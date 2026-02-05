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
    
    // Get PPQ from engine if available
    if (m_engine && m_engine->getRuntimeData()) {
        // Could get PPQ from runtime data or MIDI sequence
        m_ppq = 480; // Default PPQ
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
    int contentX = x - LEFT_MARGIN;
    return static_cast<int64_t>((contentX + m_horizontalOffset) / m_pixelsPerTick);
}

int ArrangementTimelineRuler::tickToX(int64_t tick) const
{
    return LEFT_MARGIN + static_cast<int>(tick * m_pixelsPerTick) - m_horizontalOffset;
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
        // Assume 120 BPM for now
        double seconds = static_cast<double>(tick) / m_ppq / 2.0;
        int minutes = static_cast<int>(seconds / 60);
        int secs = static_cast<int>(seconds) % 60;
        return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
    } else {
        // Bars:Beats (1-indexed)
        int64_t ticksPerBar = getTicksPerBar();
        int64_t ticksPerBeat = getTicksPerBeat();
        
        int bar = static_cast<int>(tick / ticksPerBar) + 1;
        int beat = static_cast<int>((tick % ticksPerBar) / ticksPerBeat) + 1;
        
        return QString("%1.%2").arg(bar).arg(beat);
    }
}

void ArrangementTimelineRuler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background for left margin (header area)
    painter.fillRect(0, 0, LEFT_MARGIN, height(), QColor("#1e1e24"));
    
    // Background for ruler content area
    painter.fillRect(LEFT_MARGIN, 0, width() - LEFT_MARGIN, height(), QColor("#252530"));
    
    // Separator between header area and ruler
    painter.setPen(QColor("#3a3a42"));
    painter.drawLine(LEFT_MARGIN - 1, 0, LEFT_MARGIN - 1, height());
    
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
