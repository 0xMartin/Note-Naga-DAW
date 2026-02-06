#include "arrangement_timeline_ruler.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>

#include <QWheelEvent>
#include <QPainterPath>
#include <QMenu>
#include <QContextMenuEvent>
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

void ArrangementTimelineRuler::setLoopRegion(int64_t startTick, int64_t endTick)
{
    if (m_loopStartTick != startTick || m_loopEndTick != endTick) {
        m_loopStartTick = startTick;
        m_loopEndTick = endTick;
        update();
        emit loopRegionChanged(startTick, endTick);
    }
}

void ArrangementTimelineRuler::setLoopEnabled(bool enabled)
{
    if (m_loopEnabled != enabled) {
        m_loopEnabled = enabled;
        update();
        emit loopEnabledChanged(enabled);
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
    if (m_isHovered && m_hoverX >= 0 && m_dragMode == DragNone) {
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
    
    // Draw loop region
    if (m_loopEnabled && m_loopEndTick > m_loopStartTick) {
        int loopStartX = tickToX(m_loopStartTick);
        int loopEndX = tickToX(m_loopEndTick);
        
        // Loop region background
        painter.fillRect(loopStartX, 0, loopEndX - loopStartX, height(),
                         QColor(34, 197, 94, 60)); // Green with transparency
        
        // Loop start marker
        painter.setPen(QPen(QColor("#22c55e"), 2));
        painter.drawLine(loopStartX, 0, loopStartX, height());
        
        // Loop start bracket
        painter.drawLine(loopStartX, 0, loopStartX + 8, 0);
        painter.drawLine(loopStartX, height() - 1, loopStartX + 8, height() - 1);
        
        // Loop end marker
        painter.drawLine(loopEndX, 0, loopEndX, height());
        
        // Loop end bracket
        painter.drawLine(loopEndX, 0, loopEndX - 8, 0);
        painter.drawLine(loopEndX, height() - 1, loopEndX - 8, height() - 1);
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
        int x = event->pos().x();
        int64_t tick = xToTick(x);
        tick = qMax(static_cast<int64_t>(0), tick);
        
        // Check if clicking on loop markers (if loop is enabled)
        if (m_loopEnabled && m_loopEndTick > m_loopStartTick) {
            int loopStartX = tickToX(m_loopStartTick);
            int loopEndX = tickToX(m_loopEndTick);
            
            // Check loop start handle (6px tolerance)
            if (qAbs(x - loopStartX) < 6) {
                m_dragMode = DragLoopStart;
                m_dragStartX = x;
                m_dragStartLoopStart = m_loopStartTick;
                m_dragStartLoopEnd = m_loopEndTick;
                setCursor(Qt::SizeHorCursor);
                return;
            }
            
            // Check loop end handle
            if (qAbs(x - loopEndX) < 6) {
                m_dragMode = DragLoopEnd;
                m_dragStartX = x;
                m_dragStartLoopStart = m_loopStartTick;
                m_dragStartLoopEnd = m_loopEndTick;
                setCursor(Qt::SizeHorCursor);
                return;
            }
            
            // Check if inside loop region (drag entire loop)
            if (x > loopStartX + 6 && x < loopEndX - 6) {
                m_dragMode = DragLoopBody;
                m_dragStartX = x;
                m_dragStartLoopStart = m_loopStartTick;
                m_dragStartLoopEnd = m_loopEndTick;
                setCursor(Qt::SizeAllCursor);
                return;
            }
        }
        
        // Regular seek
        m_dragMode = DragSeek;
        m_isDragging = true;
        emit seekRequested(tick);
    }
    QWidget::mousePressEvent(event);
}

void ArrangementTimelineRuler::mouseMoveEvent(QMouseEvent *event)
{
    int x = event->pos().x();
    
    // Update cursor based on position when not dragging
    if (m_dragMode == DragNone && m_loopEnabled && m_loopEndTick > m_loopStartTick) {
        int loopStartX = tickToX(m_loopStartTick);
        int loopEndX = tickToX(m_loopEndTick);
        
        if (qAbs(x - loopStartX) < 6 || qAbs(x - loopEndX) < 6) {
            setCursor(Qt::SizeHorCursor);
        } else if (x > loopStartX + 6 && x < loopEndX - 6) {
            setCursor(Qt::SizeAllCursor);
        } else {
            setCursor(Qt::PointingHandCursor);
        }
    }
    
    // Update hover position for visual feedback
    m_hoverX = x;
    update();
    
    switch (m_dragMode) {
        case DragSeek: {
            int64_t tick = xToTick(x);
            tick = qMax(static_cast<int64_t>(0), tick);
            emit seekRequested(tick);
            break;
        }
        case DragLoopStart: {
            int64_t tick = xToTick(x);
            tick = qMax(static_cast<int64_t>(0), tick);
            tick = qMin(tick, m_loopEndTick - getTicksPerBeat()); // Minimum 1 beat
            setLoopRegion(tick, m_loopEndTick);
            break;
        }
        case DragLoopEnd: {
            int64_t tick = xToTick(x);
            tick = qMax(m_loopStartTick + getTicksPerBeat(), tick); // Minimum 1 beat
            setLoopRegion(m_loopStartTick, tick);
            break;
        }
        case DragLoopBody: {
            int64_t deltaTick = xToTick(x) - xToTick(m_dragStartX);
            int64_t newStart = m_dragStartLoopStart + deltaTick;
            int64_t newEnd = m_dragStartLoopEnd + deltaTick;
            
            // Keep in valid range
            if (newStart < 0) {
                newEnd -= newStart;
                newStart = 0;
            }
            setLoopRegion(newStart, newEnd);
            break;
        }
        default:
            break;
    }
    
    QWidget::mouseMoveEvent(event);
}

void ArrangementTimelineRuler::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        m_dragMode = DragNone;
        setCursor(Qt::PointingHandCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void ArrangementTimelineRuler::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int x = event->pos().x();
        int64_t tick = xToTick(x);
        tick = qMax(static_cast<int64_t>(0), tick);
        
        // Double-click to set loop region (snap to bar)
        int64_t ticksPerBar = getTicksPerBar();
        int64_t barStart = (tick / ticksPerBar) * ticksPerBar;
        int64_t barEnd = barStart + ticksPerBar * 4; // 4 bars default loop
        
        setLoopRegion(barStart, barEnd);
        setLoopEnabled(true);
    }
    QWidget::mouseDoubleClickEvent(event);
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
void ArrangementTimelineRuler::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    
    if (m_loopEnabled) {
        QAction *disableLoopAction = menu.addAction(tr("Disable Loop"));
        connect(disableLoopAction, &QAction::triggered, this, [this]() {
            setLoopEnabled(false);
        });
        
        QAction *clearLoopAction = menu.addAction(tr("Clear Loop Region"));
        connect(clearLoopAction, &QAction::triggered, this, [this]() {
            setLoopEnabled(false);
            setLoopRegion(0, 0);
        });
    } else {
        if (m_loopEndTick > m_loopStartTick) {
            QAction *enableLoopAction = menu.addAction(tr("Enable Loop"));
            connect(enableLoopAction, &QAction::triggered, this, [this]() {
                setLoopEnabled(true);
            });
        }
        
        // Create loop at clicked position
        int64_t clickTick = xToTick(event->pos().x());
        int64_t ticksPerBar = getTicksPerBar();
        int64_t barStart = (clickTick / ticksPerBar) * ticksPerBar;
        
        QAction *createLoopAction = menu.addAction(tr("Create Loop Here (4 bars)"));
        connect(createLoopAction, &QAction::triggered, this, [this, barStart, ticksPerBar]() {
            setLoopRegion(barStart, barStart + ticksPerBar * 4);
            setLoopEnabled(true);
        });
    }
    
    menu.addSeparator();
    
    // Time format options
    QMenu *formatMenu = menu.addMenu(tr("Time Format"));
    QAction *barsAction = formatMenu->addAction(tr("Bars:Beats"));
    barsAction->setCheckable(true);
    barsAction->setChecked(m_timeFormat == BarsBeats);
    connect(barsAction, &QAction::triggered, this, [this]() {
        setTimeFormat(BarsBeats);
    });
    
    QAction *secondsAction = formatMenu->addAction(tr("Time (Seconds)"));
    secondsAction->setCheckable(true);
    secondsAction->setChecked(m_timeFormat == Seconds);
    connect(secondsAction, &QAction::triggered, this, [this]() {
        setTimeFormat(Seconds);
    });
    
    menu.exec(event->globalPos());
}