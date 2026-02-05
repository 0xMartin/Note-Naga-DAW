#include "arrangement_timeline_ruler.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>

#include <QWheelEvent>
#include <QPainterPath>
#include <cmath>

ArrangementTimelineRuler::ArrangementTimelineRuler(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    setMinimumHeight(28);
    setMaximumHeight(28);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    
    // Default to Seconds mode for arrangement timeline
    m_timeFormat = Seconds;
    
    // Get PPQ from engine if available
    if (m_engine && m_engine->getRuntimeData()) {
        m_ppq = m_engine->getRuntimeData()->getPPQ();
        if (m_ppq <= 0) m_ppq = 480;
    }
}

void ArrangementTimelineRuler::setPixelsPerTick(double ppTick)
{
    if (m_pixelsPerTick != ppTick) {
        m_pixelsPerTick = qBound(0.01, ppTick, 2.0);
        update();
        emit zoomChanged(m_pixelsPerTick);
    }
}

void ArrangementTimelineRuler::setHorizontalOffset(int offset)
{
    if (m_horizontalOffset != offset) {
        m_horizontalOffset = offset;
        update();
    }
}

void ArrangementTimelineRuler::setPlayheadTick(int64_t tick)
{
    if (m_playheadTick != tick) {
        m_playheadTick = tick;
        update();
    }
}

void ArrangementTimelineRuler::setTimeFormat(TimeFormat format)
{
    if (m_timeFormat != format) {
        m_timeFormat = format;
        update();
    }
}

void ArrangementTimelineRuler::setTimeSignature(int numerator, int denominator)
{
    if (m_timeSignatureNumerator != numerator || m_timeSignatureDenominator != denominator) {
        m_timeSignatureNumerator = numerator;
        m_timeSignatureDenominator = denominator;
        update();
    }
}

int64_t ArrangementTimelineRuler::xToTick(int x) const
{
    // No left margin - ruler is now aligned directly with timeline
    return static_cast<int64_t>((x + m_horizontalOffset) / m_pixelsPerTick);
}

int ArrangementTimelineRuler::tickToX(int64_t tick) const
{
    // No left margin - ruler is now aligned directly with timeline
    return static_cast<int>(tick * m_pixelsPerTick) - m_horizontalOffset;
}

int64_t ArrangementTimelineRuler::getTicksPerBar() const
{
    // Ticks per bar = PPQ * numerator * (4 / denominator)
    return static_cast<int64_t>(m_ppq * m_timeSignatureNumerator * 4 / m_timeSignatureDenominator);
}

int64_t ArrangementTimelineRuler::getTicksPerBeat() const
{
    // For quarter note beats
    return static_cast<int64_t>(m_ppq * 4 / m_timeSignatureDenominator);
}

QString ArrangementTimelineRuler::formatTickLabel(int64_t tick) const
{
    if (m_timeFormat == Seconds) {
        // Use project tempo and PPQ from runtime data (like global_transport_bar.cpp)
        double tempo_us = 500000.0;  // Default 120 BPM = 500000 us per quarter
        int ppq = m_ppq;
        if (m_engine && m_engine->getRuntimeData()) {
            int projectTempo = m_engine->getRuntimeData()->getTempo();
            int projectPpq = m_engine->getRuntimeData()->getPPQ();
            if (projectTempo > 0) {
                tempo_us = static_cast<double>(projectTempo);
            }
            if (projectPpq > 0) {
                ppq = projectPpq;
            }
        }
        // Convert tick to seconds: seconds = (tick * tempo_us) / (ppq * 1000000)
        double us_per_tick = tempo_us / static_cast<double>(ppq);
        double seconds = static_cast<double>(tick) * us_per_tick / 1000000.0;
        int minutes = static_cast<int>(seconds / 60);
        int secs = static_cast<int>(seconds) % 60;
        return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
    } else {
        // Bars:Beats (0-indexed, starting from 0.0)
        int64_t ticksPerBar = getTicksPerBar();
        int64_t ticksPerBeat = getTicksPerBeat();
        
        int bar = static_cast<int>(tick / ticksPerBar);
        int beat = static_cast<int>((tick % ticksPerBar) / ticksPerBeat);
        
        return QString("%1.%2").arg(bar).arg(beat);
    }
}

void ArrangementTimelineRuler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background for ruler content area (no left margin since headers are separate)
    painter.fillRect(rect(), QColor("#252530"));
    
    // Bottom border
    painter.setPen(QColor("#3a3a42"));
    painter.drawLine(0, height() - 1, width(), height() - 1);
    
    // Calculate visible tick range
    int64_t startTick = xToTick(0);
    int64_t endTick = xToTick(width());
    
    int64_t ticksPerBar = getTicksPerBar();
    int64_t ticksPerBeat = getTicksPerBeat();
    
    // Determine marker spacing based on zoom level
    int64_t majorStep = ticksPerBar;
    int64_t minorStep = ticksPerBeat;
    
    // Adjust if too dense or sparse
    double pixelsPerBar = ticksPerBar * m_pixelsPerTick;
    while (pixelsPerBar < 50 && majorStep < ticksPerBar * 16) {
        majorStep *= 2;
        minorStep *= 2;
        pixelsPerBar *= 2;
    }
    while (pixelsPerBar > 300) {
        if (minorStep > ticksPerBeat / 2) {
            minorStep /= 2;
        } else {
            break;
        }
        pixelsPerBar /= 2;
    }
    
    // Draw minor markers (beats)
    painter.setPen(QColor("#4a4a54"));
    int64_t tick = (startTick / minorStep) * minorStep;
    while (tick <= endTick) {
        if (tick >= 0) {
            int x = tickToX(tick);
            if (tick % majorStep != 0) {
                painter.drawLine(x, height() - 6, x, height() - 1);
            }
        }
        tick += minorStep;
    }
    
    // Draw major markers (bars) with labels
    painter.setPen(QColor("#888888"));
    QFont font = painter.font();
    font.setPixelSize(10);
    painter.setFont(font);
    
    tick = (startTick / majorStep) * majorStep;
    while (tick <= endTick) {
        if (tick >= 0) {
            int x = tickToX(tick);
            painter.drawLine(x, height() - 12, x, height() - 1);
            
            QString label = formatTickLabel(tick);
            painter.drawText(x + 4, 12, label);
        }
        tick += majorStep;
    }
    
    // Draw hover indicator (before playhead so playhead is on top)
    if (m_isHovered && m_hoverX >= 0) {
        // Draw semi-transparent hint area
        painter.fillRect(QRect(m_hoverX - 1, 0, 3, height()), QColor("#ff585840"));
        
        // Draw vertical line at hover position
        painter.setPen(QPen(QColor("#ff5858"), 2));
        painter.drawLine(m_hoverX, 0, m_hoverX, height());
        
        // Draw small triangle pointer at top
        QPainterPath hoverPath;
        hoverPath.moveTo(m_hoverX - 4, 0);
        hoverPath.lineTo(m_hoverX + 4, 0);
        hoverPath.lineTo(m_hoverX, 6);
        hoverPath.closeSubpath();
        painter.fillPath(hoverPath, QColor("#ff5858"));
    }
    
    // Draw playhead
    if (m_playheadTick >= startTick && m_playheadTick <= endTick) {
        int x = tickToX(m_playheadTick);
        painter.setPen(QPen(QColor("#ef4444"), 2));
        painter.drawLine(x, 0, x, height());
        
        // Playhead triangle
        QPainterPath path;
        path.moveTo(x - 5, 0);
        path.lineTo(x + 5, 0);
        path.lineTo(x, 8);
        path.closeSubpath();
        painter.fillPath(path, QColor("#ef4444"));
    }
}

void ArrangementTimelineRuler::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        int64_t tick = xToTick(event->pos().x());
        tick = qMax(static_cast<int64_t>(0), tick);
        emit seekRequested(tick);
    }
    QWidget::mousePressEvent(event);
}

void ArrangementTimelineRuler::mouseMoveEvent(QMouseEvent *event)
{
    // Update hover position for visual feedback
    m_hoverX = event->pos().x();
    update();
    
    if (m_isDragging) {
        int64_t tick = xToTick(event->pos().x());
        tick = qMax(static_cast<int64_t>(0), tick);
        emit seekRequested(tick);
    }
    QWidget::mouseMoveEvent(event);
}

void ArrangementTimelineRuler::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void ArrangementTimelineRuler::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = true;
    update();
}

void ArrangementTimelineRuler::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = false;
    m_hoverX = -1;
    update();
}

void ArrangementTimelineRuler::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double delta = event->angleDelta().y() > 0 ? 1.1 : 0.9;
        setPixelsPerTick(m_pixelsPerTick * delta);
        event->accept();
    } else {
        // Scroll horizontally
        int delta = event->angleDelta().y();
        setHorizontalOffset(m_horizontalOffset - delta);
        event->accept();
    }
}
