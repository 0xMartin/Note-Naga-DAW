#include "arrangement_minimap_widget.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

ArrangementMinimapWidget::ArrangementMinimapWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    setMinimumHeight(30);
    setMaximumHeight(50);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void ArrangementMinimapWidget::setHorizontalOffset(int offset)
{
    Q_UNUSED(offset);
    update();
}

void ArrangementMinimapWidget::setVisibleTickRange(int64_t startTick, int64_t endTick)
{
    m_visibleStartTick = startTick;
    m_visibleEndTick = endTick;
    update();
}

void ArrangementMinimapWidget::setPlayheadTick(int64_t tick)
{
    if (m_playheadTick != tick) {
        m_playheadTick = tick;
        update();
    }
}

void ArrangementMinimapWidget::setLoopRegion(int64_t startTick, int64_t endTick, bool enabled)
{
    m_loopStartTick = startTick;
    m_loopEndTick = endTick;
    m_loopEnabled = enabled;
    update();
}

void ArrangementMinimapWidget::setTotalTicks(int64_t ticks)
{
    m_totalTicks = ticks;
    update();
}

int64_t ArrangementMinimapWidget::xToTick(int x) const
{
    if (width() <= 0 || m_totalTicks <= 0) return 0;
    return static_cast<int64_t>(static_cast<double>(x) / width() * m_totalTicks);
}

int ArrangementMinimapWidget::tickToX(int64_t tick) const
{
    if (m_totalTicks <= 0) return 0;
    return static_cast<int>(static_cast<double>(tick) / m_totalTicks * width());
}

void ArrangementMinimapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), QColor("#151518"));
    
    // Border
    painter.setPen(QColor("#3a3a42"));
    painter.drawRect(0, 0, width() - 1, height() - 1);
    
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    // Update total ticks from arrangement
    int64_t maxTick = arrangement->getMaxTick();
    if (maxTick <= 0) maxTick = 480 * 16; // Default 4 bars
    
    // Add some padding beyond the last clip
    int64_t displayTotalTicks = maxTick + (maxTick / 4);
    if (m_totalTicks != displayTotalTicks) {
        m_totalTicks = displayTotalTicks;
    }
    
    // Draw clips for each track
    int trackCount = static_cast<int>(arrangement->getTrackCount());
    if (trackCount == 0) return;
    
    int trackHeight = std::max(3, (height() - 4) / std::max(1, trackCount));
    int trackIndex = 0;
    
    for (auto *track : arrangement->getTracks()) {
        if (!track) {
            ++trackIndex;
            continue;
        }
        
        int trackY = 2 + trackIndex * trackHeight;
        QColor trackColor = track->getColor().toQColor();
        
        for (const auto &clip : track->getClips()) {
            if (clip.muted) continue;
            
            int clipX = tickToX(clip.startTick);
            int clipWidth = std::max(2, tickToX(clip.startTick + clip.durationTicks) - clipX);
            
            // Clip rectangle
            QColor fillColor = trackColor.darker(130);
            painter.fillRect(clipX, trackY, clipWidth, trackHeight - 1, fillColor);
        }
        
        ++trackIndex;
    }
    
    // Draw loop region (if enabled)
    if (m_loopEnabled && m_loopEndTick > m_loopStartTick) {
        int loopStartX = tickToX(m_loopStartTick);
        int loopEndX = tickToX(m_loopEndTick);
        
        painter.fillRect(loopStartX, 0, loopEndX - loopStartX, height(),
                         QColor(34, 197, 94, 40)); // Green with transparency
        
        // Loop markers
        painter.setPen(QPen(QColor("#22c55e"), 1));
        painter.drawLine(loopStartX, 0, loopStartX, height());
        painter.drawLine(loopEndX, 0, loopEndX, height());
    }
    
    // Draw visible area indicator
    int visibleStartX = tickToX(m_visibleStartTick);
    int visibleEndX = tickToX(m_visibleEndTick);
    int visibleWidth = std::max(10, visibleEndX - visibleStartX);
    
    // Semi-transparent overlay for non-visible areas
    painter.fillRect(0, 0, visibleStartX, height(), QColor(0, 0, 0, 100));
    painter.fillRect(visibleStartX + visibleWidth, 0, width() - visibleStartX - visibleWidth, height(),
                     QColor(0, 0, 0, 100));
    
    // Visible area border
    painter.setPen(QPen(QColor("#ffffff"), 1));
    painter.drawRect(visibleStartX, 1, visibleWidth - 1, height() - 3);
    
    // Draw playhead
    int playheadX = tickToX(m_playheadTick);
    painter.setPen(QPen(QColor("#ef4444"), 2));
    painter.drawLine(playheadX, 0, playheadX, height());
}

void ArrangementMinimapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragStartX = event->pos().x();
        m_dragStartTick = xToTick(event->pos().x());
        
        // Seek to clicked position
        emit seekRequested(m_dragStartTick);
        
        // Also center the visible area on click
        int64_t visibleRange = m_visibleEndTick - m_visibleStartTick;
        int64_t newStartTick = m_dragStartTick - visibleRange / 2;
        newStartTick = std::max(static_cast<int64_t>(0), newStartTick);
        emit visibleRangeChangeRequested(newStartTick);
    }
}

void ArrangementMinimapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        int64_t tick = xToTick(event->pos().x());
        tick = std::max(static_cast<int64_t>(0), tick);
        
        emit seekRequested(tick);
        
        // Update visible area
        int64_t visibleRange = m_visibleEndTick - m_visibleStartTick;
        int64_t newStartTick = tick - visibleRange / 2;
        newStartTick = std::max(static_cast<int64_t>(0), newStartTick);
        emit visibleRangeChangeRequested(newStartTick);
    }
}

void ArrangementMinimapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
}

void ArrangementMinimapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}
