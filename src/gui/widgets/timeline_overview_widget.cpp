#include "timeline_overview_widget.h"

#include <QPainter>
#include <QMouseEvent>
#include <note_naga_engine/note_naga_engine.h>

TimelineOverviewWidget::TimelineOverviewWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent),
      m_engine(engine),
      m_sequence(nullptr),
      m_activeTrack(nullptr),
      m_playbackTick(0),
      m_viewportStartTick(0),
      m_viewportEndTick(0),
      m_maxTick(0),
      m_timeScale(0.2),
      m_isDragging(false),
      m_dragViewport(false),
      // Colors matching the editor theme
      m_backgroundColor(QColor(0x28, 0x2a, 0x30)),
      m_borderColor(QColor(0x3d, 0x42, 0x4d)),
      m_noteBlockColor(QColor(0x50, 0x80, 0xc0, 180)),
      m_playbackMarkerColor(QColor(0xff, 0x50, 0x50)),
      m_viewportColor(QColor(0x70, 0xa7, 0xff, 60)),
      m_startEndMarkerColor(QColor(0x6f, 0xa5, 0xff, 100))
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(20);
    setCursor(Qt::PointingHandCursor);

    connectSignals();
}

void TimelineOverviewWidget::connectSignals()
{
    if (!m_engine) return;

    // Connect to project/sequence changes
    if (m_engine->getProject()) {
        connect(m_engine->getProject(), &NoteNagaProject::activeSequenceChanged,
                this, &TimelineOverviewWidget::onSequenceChanged);

        m_sequence = m_engine->getProject()->getActiveSequence();
        if (m_sequence) {
            connect(m_sequence, &NoteNagaMidiSeq::activeTrackChanged,
                    this, &TimelineOverviewWidget::onActiveTrackChanged);
            m_activeTrack = m_sequence->getActiveTrack();
        }
    }

    // Connect to playback position changes
    if (m_engine->getProject()) {
        connect(m_engine->getProject(), &NoteNagaProject::currentTickChanged,
                this, &TimelineOverviewWidget::onPlaybackPositionChanged);
    }

    updateMaxTick();
}

void TimelineOverviewWidget::onSequenceChanged(NoteNagaMidiSeq *seq)
{
    if (m_sequence) {
        disconnect(m_sequence, nullptr, this, nullptr);
    }

    m_sequence = seq;
    m_activeTrack = nullptr;
    m_playbackTick = 0;

    if (m_sequence) {
        connect(m_sequence, &NoteNagaMidiSeq::activeTrackChanged,
                this, &TimelineOverviewWidget::onActiveTrackChanged);
        connect(m_sequence, &NoteNagaMidiSeq::trackListChanged,
                this, &TimelineOverviewWidget::refresh);
        m_activeTrack = m_sequence->getActiveTrack();
    }

    updateMaxTick();
    
    // Initialize viewport to start of timeline
    // Assume a reasonable default visible range (will be updated when scroll happens)
    m_viewportStartTick = 0;
    int defaultVisibleTicks = static_cast<int>(800 / m_timeScale);  // ~800px width equivalent
    m_viewportEndTick = qMin(defaultVisibleTicks, m_maxTick);
    
    update();
}

void TimelineOverviewWidget::onActiveTrackChanged(NoteNagaTrack *track)
{
    m_activeTrack = track;
    updateMaxTick();
    update();
}

void TimelineOverviewWidget::onPlaybackPositionChanged(int tick)
{
    m_playbackTick = tick;
    update();
}

void TimelineOverviewWidget::setViewportRange(int startTick, int endTick)
{
    m_viewportStartTick = startTick;
    m_viewportEndTick = endTick;
    update();
}

void TimelineOverviewWidget::setTimeScale(double scale)
{
    m_timeScale = scale;
    update();
}

void TimelineOverviewWidget::setMaxTick(int maxTick)
{
    m_maxTick = maxTick;
    
    // Ensure minimum timeline length
    if (m_maxTick < 1920) {  // At least 1 bar at 480 PPQ
        m_maxTick = 1920;
    }
    
    update();
}

void TimelineOverviewWidget::refresh()
{
    updateMaxTick();
    update();
}

void TimelineOverviewWidget::updateMaxTick()
{
    m_maxTick = 0;

    if (m_sequence) {
        m_maxTick = m_sequence->computeMaxTick();
    }

    // Ensure minimum timeline length
    if (m_maxTick < 1920) {  // At least 1 bar at 480 PPQ
        m_maxTick = 1920;
    }
}

int TimelineOverviewWidget::tickToX(int tick) const
{
    if (m_maxTick <= 0) return 0;

    int margin = 4;
    int availableWidth = width() - 2 * margin;

    return margin + static_cast<int>((static_cast<double>(tick) / m_maxTick) * availableWidth);
}

int TimelineOverviewWidget::xToTick(int x) const
{
    if (m_maxTick <= 0) return 0;

    int margin = 4;
    int availableWidth = width() - 2 * margin;

    if (availableWidth <= 0) return 0;

    int tick = static_cast<int>((static_cast<double>(x - margin) / availableWidth) * m_maxTick);
    return qBound(0, tick, m_maxTick);
}

void TimelineOverviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int margin = 4;

    // Background
    painter.fillRect(rect(), m_backgroundColor);

    // Border
    painter.setPen(m_borderColor);
    painter.drawRect(0, 0, w - 1, h - 1);

    if (m_maxTick <= 0) return;

    // Draw note blocks for active track
    if (m_activeTrack) {
        QColor trackColor = m_activeTrack->getColor().toQColor();
        trackColor.setAlpha(160);

        const auto& notes = m_activeTrack->getNotes();

        // Group notes into density blocks for visualization
        // We'll divide the timeline into segments and show where notes exist
        int numSegments = qMax(1, w - 2 * margin);
        std::vector<bool> segments(numSegments, false);

        for (const auto& note : notes) {
            int noteTick = note.start.value_or(0);
            int noteLength = note.length.value_or(480);
            
            int startX = tickToX(noteTick) - margin;
            int endX = tickToX(noteTick + noteLength) - margin;

            startX = qBound(0, startX, numSegments - 1);
            endX = qBound(0, endX, numSegments - 1);

            for (int i = startX; i <= endX; ++i) {
                segments[i] = true;
            }
        }

        // Draw note density blocks
        painter.setPen(Qt::NoPen);
        painter.setBrush(trackColor);

        int blockStart = -1;
        for (int i = 0; i < numSegments; ++i) {
            if (segments[i] && blockStart < 0) {
                blockStart = i;
            } else if (!segments[i] && blockStart >= 0) {
                // Draw block
                painter.drawRect(margin + blockStart, 4, i - blockStart, h - 8);
                blockStart = -1;
            }
        }
        // Draw final block if needed
        if (blockStart >= 0) {
            painter.drawRect(margin + blockStart, 4, numSegments - blockStart, h - 8);
        }
    }

    // Draw start marker (thin line at beginning)
    painter.setPen(QPen(m_startEndMarkerColor, 1));
    painter.drawLine(margin, 2, margin, h - 3);

    // Draw end marker (at max tick)
    int endX = tickToX(m_maxTick);
    painter.drawLine(endX, 2, endX, h - 3);

    // Draw viewport indicator (semi-transparent box)
    if (m_viewportEndTick > m_viewportStartTick) {
        int vpStartX = tickToX(m_viewportStartTick);
        int vpEndX = tickToX(m_viewportEndTick);

        painter.setPen(QPen(QColor(0x70, 0xa7, 0xff, 150), 1));
        painter.setBrush(m_viewportColor);
        painter.drawRect(vpStartX, 1, vpEndX - vpStartX, h - 3);
    }

    // Draw playback position marker (vertical red line)
    if (m_playbackTick > 0) {
        int playX = tickToX(m_playbackTick);
        painter.setPen(QPen(m_playbackMarkerColor, 2));
        painter.drawLine(playX, 1, playX, h - 2);
    }
}

void TimelineOverviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Don't allow viewport navigation during playback
        if (m_engine && m_engine->isPlaying()) {
            return;
        }
        
        m_isDragging = true;

        int tick = xToTick(event->pos().x());

        // Check if clicking inside viewport - drag viewport
        if (tick >= m_viewportStartTick && tick <= m_viewportEndTick) {
            m_dragViewport = true;
            emit viewportNavigationRequested(tick);
        } else {
            // Click outside viewport - seek
            m_dragViewport = false;
            emit viewportNavigationRequested(tick);
        }
    }
}

void TimelineOverviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Don't allow viewport navigation during playback
    if (m_engine && m_engine->isPlaying()) {
        return;
    }
    
    if (m_isDragging) {
        int tick = xToTick(event->pos().x());
        emit viewportNavigationRequested(tick);
    }
}

void TimelineOverviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_isDragging = false;
    m_dragViewport = false;
}
