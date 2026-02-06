#include "arrangement_timeline_widget.h"
#include "arrangement_timeline_ruler.h"
#include "arrangement_track_headers_widget.h"
#include "../components/track_stereo_meter.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/module/dsp_engine.h>

#include <QDragEnterEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QApplication>
#include <QColorDialog>
#include <QInputDialog>
#include <cmath>
#include <algorithm>

static const int RESIZE_HANDLE_WIDTH = 6;
static const int HEADER_BUTTON_SIZE = 20;
static const int HEADER_BUTTON_PADDING = 4;
static const char* MIME_TYPE_MIDI_SEQUENCE = "application/x-notenaga-midi-sequence";

ArrangementTimelineWidget::ArrangementTimelineWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    setMinimumSize(400, 200);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setMouseTracking(true);
}

QRect ArrangementTimelineWidget::contentRect() const
{
    // The entire widget is now content area since headers are in a separate widget
    return rect();
}

void ArrangementTimelineWidget::setPixelsPerTick(double ppTick)
{
    if (m_pixelsPerTick != ppTick) {
        m_pixelsPerTick = qBound(0.01, ppTick, 2.0);
        if (m_ruler) {
            m_ruler->setPixelsPerTick(m_pixelsPerTick);
        }
        update();
        emit zoomChanged(m_pixelsPerTick);
    }
}

void ArrangementTimelineWidget::setHorizontalOffset(int offset)
{
    if (m_horizontalOffset != offset) {
        m_horizontalOffset = qMax(0, offset);
        if (m_ruler) {
            m_ruler->setHorizontalOffset(m_horizontalOffset);
        }
        update();
        emit horizontalOffsetChanged(m_horizontalOffset);
    }
}

void ArrangementTimelineWidget::setVerticalOffset(int offset)
{
    // Clamp offset to valid range
    int trackCount = 0;
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
        if (arrangement) {
            trackCount = static_cast<int>(arrangement->getTrackCount());
        }
    }
    
    // Calculate max offset - add extra 100px for context menu area at bottom
    int totalTracksHeight = trackCount * m_trackHeight + 100;
    int maxOffset = qMax(0, totalTracksHeight - height());
    offset = qBound(0, offset, maxOffset);
    
    if (m_verticalOffset != offset) {
        m_verticalOffset = offset;
        // Sync vertical offset with headers widget
        if (m_trackHeadersWidget) {
            m_trackHeadersWidget->setVerticalOffset(m_verticalOffset);
        }
        update();
    }
}

void ArrangementTimelineWidget::setSnapEnabled(bool enabled)
{
    m_snapEnabled = enabled;
}

void ArrangementTimelineWidget::setSnapResolution(int ticksPerSnap)
{
    m_snapResolution = ticksPerSnap;
}

void ArrangementTimelineWidget::setTrackHeight(int height)
{
    m_trackHeight = qBound(40, height, 120);
    update();
}

void ArrangementTimelineWidget::setPlayheadTick(int64_t tick)
{
    if (m_playheadTick != tick) {
        m_playheadTick = tick;
        if (m_ruler) {
            m_ruler->setPlayheadTick(tick);
        }
        update();
    }
}

void ArrangementTimelineWidget::clearSelection()
{
    m_selectedClipIds.clear();
    update();
    emit selectionChanged();
}

void ArrangementTimelineWidget::selectClip(int clipId, bool addToSelection)
{
    if (!addToSelection) {
        m_selectedClipIds.clear();
    }
    m_selectedClipIds.insert(clipId);
    update();
    emit selectionChanged();
}

void ArrangementTimelineWidget::deleteSelectedClips()
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;

    for (int clipId : m_selectedClipIds) {
        for (auto *track : arrangement->getTracks()) {
            if (track->removeClip(clipId)) {
                break;
            }
        }
    }
    
    // Update arrangement max tick after deletion
    arrangement->updateMaxTick();
    
    m_selectedClipIds.clear();
    update();
    emit selectionChanged();
}

void ArrangementTimelineWidget::refreshFromArrangement()
{
    // Ensure at least 1 track exists when arrangement is empty
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
        if (arrangement && arrangement->getTrackCount() == 0) {
            arrangement->addTrack("Track 1");
        }
    }
    // Refresh track headers widget if connected
    if (m_trackHeadersWidget) {
        m_trackHeadersWidget->refreshFromArrangement();
    }
    update();
}

void ArrangementTimelineWidget::setTrackHeadersWidget(ArrangementTrackHeadersWidget *headersWidget)
{
    m_trackHeadersWidget = headersWidget;
    if (m_trackHeadersWidget) {
        m_trackHeadersWidget->setEngine(m_engine);
        m_trackHeadersWidget->setTrackHeight(m_trackHeight);
        m_trackHeadersWidget->setVerticalOffset(m_verticalOffset);
        m_trackHeadersWidget->refreshFromArrangement();
    }
}

void ArrangementTimelineWidget::updateTrackMeters()
{
    // Delegate to the track headers widget
    if (m_trackHeadersWidget) {
        m_trackHeadersWidget->updateTrackMeters();
    }
}

void ArrangementTimelineWidget::setRuler(ArrangementTimelineRuler *ruler)
{
    m_ruler = ruler;
    if (m_ruler) {
        m_ruler->setPixelsPerTick(m_pixelsPerTick);
        m_ruler->setHorizontalOffset(m_horizontalOffset);
        m_ruler->setPlayheadTick(m_playheadTick);
        
        connect(m_ruler, &ArrangementTimelineRuler::seekRequested, 
                this, &ArrangementTimelineWidget::seekRequested);
    }
}

// Coordinate conversion (now simpler since headers are in a separate widget)
int64_t ArrangementTimelineWidget::xToTick(int x) const
{
    // Content starts at x=0 since headers are in separate widget
    return static_cast<int64_t>((x + m_horizontalOffset) / m_pixelsPerTick);
}

int ArrangementTimelineWidget::tickToX(int64_t tick) const
{
    // Content starts at x=0 since headers are in separate widget
    return static_cast<int>(tick * m_pixelsPerTick) - m_horizontalOffset;
}

int ArrangementTimelineWidget::yToTrackIndex(int y) const
{
    return (y + m_verticalOffset) / m_trackHeight;
}

int ArrangementTimelineWidget::trackIndexToY(int trackIndex) const
{
    return trackIndex * m_trackHeight - m_verticalOffset;
}

bool ArrangementTimelineWidget::isInHeaderArea(const QPoint &pos) const
{
    // Headers are now in a separate widget, so nothing in this widget is "header area"
    Q_UNUSED(pos);
    return false;
}

int ArrangementTimelineWidget::headerTrackAtY(int y) const
{
    return yToTrackIndex(y);
}

ArrangementTimelineWidget::HeaderButton ArrangementTimelineWidget::headerButtonAtPos(
    const QPoint &pos, int &outTrackIndex) const
{
    if (!isInHeaderArea(pos)) {
        outTrackIndex = -1;
        return NoButton;
    }
    
    outTrackIndex = headerTrackAtY(pos.y());
    if (!m_engine || !m_engine->getRuntimeData()) return NoButton;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement || outTrackIndex < 0 || 
        outTrackIndex >= static_cast<int>(arrangement->getTrackCount())) {
        return NoButton;
    }
    
    int trackY = trackIndexToY(outTrackIndex);
    int buttonY = trackY + (m_trackHeight - HEADER_BUTTON_SIZE) / 2;
    int buttonsAreaStart = TRACK_HEADER_WIDTH - (3 * (HEADER_BUTTON_SIZE + HEADER_BUTTON_PADDING));
    
    // Check which button was clicked (right to left: Solo, Mute, Color)
    if (pos.y() >= buttonY && pos.y() <= buttonY + HEADER_BUTTON_SIZE) {
        int relX = pos.x() - buttonsAreaStart;
        int buttonIndex = relX / (HEADER_BUTTON_SIZE + HEADER_BUTTON_PADDING);
        if (relX >= 0 && buttonIndex >= 0 && buttonIndex < 3) {
            switch (buttonIndex) {
                case 0: return ColorButton;
                case 1: return MuteButton;
                case 2: return SoloButton;
            }
        }
    }
    
    return NoButton;
}

int64_t ArrangementTimelineWidget::snapTick(int64_t tick) const
{
    if (!m_snapEnabled || m_snapResolution <= 0) {
        return tick;
    }
    return (tick / m_snapResolution) * m_snapResolution;
}

NN_MidiClip_t* ArrangementTimelineWidget::clipAtPosition(const QPoint &pos, int &outTrackIndex)
{
    if (!m_engine || !m_engine->getRuntimeData()) return nullptr;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return nullptr;

    outTrackIndex = yToTrackIndex(pos.y());
    if (outTrackIndex < 0 || outTrackIndex >= static_cast<int>(arrangement->getTrackCount())) {
        return nullptr;
    }

    auto *track = arrangement->getTracks()[outTrackIndex];
    if (!track) return nullptr;

    int64_t tick = xToTick(pos.x());
    
    // Search clips in reverse order (top clips have priority)
    for (auto &clip : track->getClips()) {
        if (tick >= clip.startTick && tick < clip.startTick + clip.durationTicks) {
            return &clip;
        }
    }
    
    return nullptr;
}

ArrangementTimelineWidget::HitZone ArrangementTimelineWidget::hitTestClip(
    NN_MidiClip_t *clip, int trackIndex, const QPoint &pos)
{
    if (!clip) return NoHit;
    
    int clipX = tickToX(clip->startTick);
    int clipWidth = static_cast<int>(clip->durationTicks * m_pixelsPerTick);
    int clipY = trackIndexToY(trackIndex);
    
    QRect clipRect(clipX, clipY, clipWidth, m_trackHeight);
    
    if (!clipRect.contains(pos)) {
        return NoHit;
    }
    
    // Check resize handles
    if (pos.x() - clipX < RESIZE_HANDLE_WIDTH) {
        return LeftEdgeHit;
    }
    if (clipX + clipWidth - pos.x() < RESIZE_HANDLE_WIDTH) {
        return RightEdgeHit;
    }
    
    return BodyHit;
}

void ArrangementTimelineWidget::onSequenceDropped(int midiSequenceIndex, const QPoint &pos)
{
    int trackIndex = yToTrackIndex(pos.y());
    int64_t tick = snapTick(xToTick(pos.x()));
    
    emit clipDropped(trackIndex, tick, midiSequenceIndex);
}

// Drawing
void ArrangementTimelineWidget::drawTrackHeaders(QPainter &painter)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    
    // Draw header background
    painter.fillRect(0, 0, TRACK_HEADER_WIDTH, height(), QColor("#1e1e24"));
    
    // Draw separator line between headers and content
    painter.setPen(QColor("#3a3a42"));
    painter.drawLine(TRACK_HEADER_WIDTH - 1, 0, TRACK_HEADER_WIDTH - 1, height());
    
    if (!arrangement) return;

    int trackCount = static_cast<int>(arrangement->getTrackCount());
    auto tracks = arrangement->getTracks();
    
    for (int i = 0; i < trackCount; ++i) {
        if (i >= static_cast<int>(tracks.size()) || !tracks[i]) continue;
        
        NoteNagaArrangementTrack *track = tracks[i];
        int y = trackIndexToY(i);
        
        // Skip tracks outside visible area
        if (y + m_trackHeight < 0 || y > height()) continue;
        
        QRect headerRect(0, y, TRACK_HEADER_WIDTH - 1, m_trackHeight);
        
        // Selected track highlight
        if (m_selectedTrackIndex == i) {
            painter.fillRect(headerRect, QColor("#2a2a35"));
        }
        
        // Track color indicator on the left
        QColor trackColor = track->getColor().toQColor();
        painter.fillRect(0, y, 4, m_trackHeight, trackColor);
        
        // Track name
        painter.setPen(QColor("#cccccc"));
        QFont font = painter.font();
        font.setPointSize(11);
        font.setBold(false);
        painter.setFont(font);
        
        QString trackName = QString::fromStdString(track->getName());
        if (trackName.isEmpty()) {
            trackName = tr("Track %1").arg(i + 1);
        }
        
        // Track name in top portion, leaving space for meter at bottom (14px meter + 4px padding)
        QRect nameRect(8, y + 2, TRACK_HEADER_WIDTH - 80, 20);
        painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, 
                         painter.fontMetrics().elidedText(trackName, Qt::ElideRight, nameRect.width()));
        
        // Buttons: Color, Mute, Solo (right side, positioned higher to not overlap meter)
        int buttonsX = TRACK_HEADER_WIDTH - (3 * (HEADER_BUTTON_SIZE + HEADER_BUTTON_PADDING));
        int buttonY = y + 22;  // Position buttons below track name
        
        // Color button
        QRect colorBtnRect(buttonsX, buttonY, HEADER_BUTTON_SIZE, HEADER_BUTTON_SIZE);
        painter.fillRect(colorBtnRect, trackColor);
        painter.setPen(QColor("#555555"));
        painter.drawRect(colorBtnRect);
        
        // Mute button
        QRect muteBtnRect(buttonsX + HEADER_BUTTON_SIZE + HEADER_BUTTON_PADDING, buttonY, 
                          HEADER_BUTTON_SIZE, HEADER_BUTTON_SIZE);
        QColor muteColor = track->isMuted() ? QColor("#ef4444") : QColor("#3a3a42");
        painter.fillRect(muteBtnRect, muteColor);
        painter.setPen(track->isMuted() ? Qt::white : QColor("#888888"));
        painter.drawText(muteBtnRect, Qt::AlignCenter, "M");
        
        // Solo button
        QRect soloBtnRect(buttonsX + 2 * (HEADER_BUTTON_SIZE + HEADER_BUTTON_PADDING), buttonY,
                          HEADER_BUTTON_SIZE, HEADER_BUTTON_SIZE);
        QColor soloColor = track->isSolo() ? QColor("#22c55e") : QColor("#3a3a42");
        painter.fillRect(soloBtnRect, soloColor);
        painter.setPen(track->isSolo() ? Qt::white : QColor("#888888"));
        painter.drawText(soloBtnRect, Qt::AlignCenter, "S");
        
        // Track separator
        painter.setPen(QColor("#3a3a42"));
        painter.drawLine(0, y + m_trackHeight - 1, TRACK_HEADER_WIDTH, y + m_trackHeight - 1);
    }
}

void ArrangementTimelineWidget::drawTrackLanes(QPainter &painter)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;

    int trackCount = static_cast<int>(arrangement->getTrackCount());
    
    // If no tracks, draw a hint/placeholder
    if (trackCount == 0) {
        painter.fillRect(rect(), QColor("#1a1a20"));
        painter.setPen(QColor("#555555"));
        QFont font = painter.font();
        font.setPointSize(11);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, tr("Drag a MIDI sequence here to create a track"));
        return;
    }
    
    // Draw track lanes (headers are now in separate widget)
    for (int i = 0; i < trackCount; ++i) {
        int y = trackIndexToY(i);
        
        // Alternating background
        QColor bgColor = (i % 2 == 0) ? QColor("#1a1a20") : QColor("#1e1e24");
        painter.fillRect(0, y, width(), m_trackHeight, bgColor);
        
        // Track separator
        painter.setPen(QColor("#3a3a42"));
        painter.drawLine(0, y + m_trackHeight - 1, width(), y + m_trackHeight - 1);
    }
    
    // Fill remaining space below tracks
    int bottomY = trackIndexToY(trackCount);
    if (bottomY < height()) {
        painter.fillRect(0, bottomY, width(), height() - bottomY, QColor("#151518"));
    }
}

void ArrangementTimelineWidget::drawClips(QPainter &painter)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    // Get sequences for lookup
    auto sequences = m_engine->getRuntimeData()->getSequences();

    int trackIndex = 0;
    for (auto *track : arrangement->getTracks()) {
        if (!track) {
            ++trackIndex;
            continue;
        }
        
        int trackY = trackIndexToY(trackIndex);
        QColor trackColor = track->getColor().toQColor();
        
        for (const auto &clip : track->getClips()) {
            int clipX = tickToX(clip.startTick);
            int clipWidth = static_cast<int>(clip.durationTicks * m_pixelsPerTick);
            
            // Skip clips outside visible area
            if (clipX + clipWidth < 0 || clipX > width()) {
                continue;
            }
            
            QRect clipRect(clipX + 1, trackY + 4, clipWidth - 2, m_trackHeight - 8);
            
            // Clip background
            bool isSelected = m_selectedClipIds.contains(clip.id);
            QColor fillColor = isSelected ? trackColor.lighter(130) : trackColor.darker(120);
            painter.fillRect(clipRect, fillColor);
            
            // Find the source sequence
            NoteNagaMidiSeq *sourceSeq = nullptr;
            for (auto *seq : sequences) {
                if (seq && seq->getId() == clip.sequenceId) {
                    sourceSeq = seq;
                    break;
                }
            }
            
            // Draw note preview if sequence found
            if (sourceSeq) {
                int seqDuration = sourceSeq->getMaxTick();
                if (seqDuration <= 0) seqDuration = 480 * 4; // Fallback
                
                // Calculate number of loops
                int numLoops = (clip.durationTicks + seqDuration - 1) / seqDuration;
                
                // Find note range for scaling
                int minNote = 127, maxNote = 0;
                for (auto *t : sourceSeq->getTracks()) {
                    if (!t || t->isTempoTrack()) continue;
                    for (const auto &n : t->getNotes()) {
                        if (n.start.has_value()) {
                            minNote = std::min(minNote, n.note);
                            maxNote = std::max(maxNote, n.note);
                        }
                    }
                }
                if (minNote > maxNote) {
                    minNote = 48; maxNote = 84; // Default range if no notes
                }
                int noteRange = std::max(12, maxNote - minNote + 1);
                
                // Draw notes for each loop
                painter.setClipRect(clipRect);
                QColor noteColor = trackColor.lighter(180);
                
                for (int loop = 0; loop < numLoops; ++loop) {
                    int loopOffset = loop * seqDuration;
                    
                    // Draw loop separator line (except for first loop)
                    if (loop > 0) {
                        int separatorX = clipX + 1 + static_cast<int>(loopOffset * m_pixelsPerTick);
                        if (separatorX < clipRect.right()) {
                            painter.setPen(QPen(QColor("#ffffff"), 1, Qt::DashLine));
                            painter.drawLine(separatorX, clipRect.top() + 2, separatorX, clipRect.bottom() - 2);
                        }
                    }
                    
                    for (auto *t : sourceSeq->getTracks()) {
                        if (!t || t->isTempoTrack() || t->isMuted()) continue;
                        
                        for (const auto &n : t->getNotes()) {
                            if (!n.start.has_value()) continue;
                            
                            int noteStart = n.start.value() + loopOffset;
                            int noteLength = n.length.value_or(120);
                            int noteKey = n.note;
                            
                            // Check if note is within clip duration
                            if (noteStart >= clip.durationTicks) continue;
                            if (noteStart + noteLength > clip.durationTicks) {
                                noteLength = clip.durationTicks - noteStart;
                            }
                            
                            // Calculate position
                            int noteX = clipX + 1 + static_cast<int>(noteStart * m_pixelsPerTick);
                            int noteW = std::max(2, static_cast<int>(noteLength * m_pixelsPerTick));
                            
                            // Vertical position (scaled)
                            float noteRelY = 1.0f - float(noteKey - minNote) / float(noteRange);
                            int noteY = clipRect.top() + 14 + static_cast<int>(noteRelY * (clipRect.height() - 18));
                            int noteH = std::max(2, (clipRect.height() - 18) / noteRange);
                            
                            painter.fillRect(noteX, noteY, noteW, noteH, noteColor);
                        }
                    }
                }
                
                painter.setClipping(false);
            }
            
            // Clip border
            painter.setPen(QPen(isSelected ? QColor("#ffffff") : trackColor.lighter(150), isSelected ? 2 : 1));
            painter.drawRect(clipRect);
            
            // Clip name (use sequence name from file path if clip name is empty)
            QString clipName = QString::fromStdString(clip.name);
            if (clipName.isEmpty() && sourceSeq) {
                // Use file path as display name if available
                if (!sourceSeq->getFilePath().empty()) {
                    QString path = QString::fromStdString(sourceSeq->getFilePath());
                    int lastSlash = path.lastIndexOf('/');
                    if (lastSlash >= 0) {
                        clipName = path.mid(lastSlash + 1);
                    } else {
                        clipName = path;
                    }
                    // Remove extension
                    int lastDot = clipName.lastIndexOf('.');
                    if (lastDot > 0) {
                        clipName = clipName.left(lastDot);
                    }
                } else {
                    clipName = QString("Sequence %1").arg(sourceSeq->getId());
                }
            }
            
            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setPixelSize(11);
            font.setBold(true);
            painter.setFont(font);
            
            QRect textRect = clipRect.adjusted(4, 2, -4, -clipRect.height() + 16);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, 
                           painter.fontMetrics().elidedText(clipName, Qt::ElideRight, textRect.width()));
            font.setBold(false);
            painter.setFont(font);
        }
        
        ++trackIndex;
    }
}

void ArrangementTimelineWidget::drawPlayhead(QPainter &painter)
{
    int x = tickToX(m_playheadTick);
    
    if (x >= 0 && x <= width()) {
        painter.setPen(QPen(QColor("#ef4444"), 2));
        painter.drawLine(x, 0, x, height());
    }
}

void ArrangementTimelineWidget::drawSelectionRect(QPainter &painter)
{
    if (m_interactionMode == Selecting && !m_selectionRect.isEmpty()) {
        painter.setPen(QPen(QColor("#2563eb"), 1, Qt::DashLine));
        painter.setBrush(QColor(37, 99, 235, 40));
        painter.drawRect(m_selectionRect);
    }
}

void ArrangementTimelineWidget::drawDropPreview(QPainter &painter)
{
    if (!m_showDropPreview) return;
    
    int x = tickToX(m_dropPreviewTick);
    int y = trackIndexToY(m_dropPreviewTrack);
    int w = static_cast<int>(m_dropPreviewDuration * m_pixelsPerTick);
    
    painter.setPen(QPen(QColor("#22c55e"), 2, Qt::DashLine));
    painter.setBrush(QColor(34, 197, 94, 60));
    painter.drawRect(x, y + 4, w, m_trackHeight - 8);
}

void ArrangementTimelineWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background for entire widget
    painter.fillRect(rect(), QColor("#1a1a20"));
    
    // Draw content area (track lanes, clips, etc.)
    // Note: Track headers are now in a separate ArrangementTrackHeadersWidget
    drawTrackLanes(painter);
    drawGridLines(painter);
    drawLoopRegion(painter);
    drawClips(painter);
    drawSelectionRect(painter);
    drawDropPreview(painter);
    drawPastePreview(painter);
    drawPlayhead(painter);
}

// Mouse events
void ArrangementTimelineWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    m_dragStartPos = event->pos();
    
    // Block editing when playback is active
    if (m_engine && m_engine->isPlaying()) {
        // Allow only seeking by clicking (handled elsewhere), but block all editing
        QWidget::mousePressEvent(event);
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        // If in paste mode, finalize paste at this position
        if (m_interactionMode == PastingClips) {
            // Update preview position one last time
            int64_t tick = xToTick(event->pos().x());
            int trackIdx = yToTrackIndex(event->pos().y());
            m_pastePreviewTick = snapTick(qMax(static_cast<int64_t>(0), tick));
            m_pastePreviewTrack = qMax(0, trackIdx);
            finishPaste();
            return;
        }
        
        // Check if click is in header area
        if (isInHeaderArea(event->pos())) {
            int headerTrack;
            HeaderButton button = headerButtonAtPos(event->pos(), headerTrack);
            
            if (!m_engine || !m_engine->getRuntimeData()) return;
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (!arrangement || headerTrack < 0 || 
                headerTrack >= static_cast<int>(arrangement->getTrackCount())) return;
            
            NoteNagaArrangementTrack *track = arrangement->getTracks()[headerTrack];
            if (!track) return;
            
            switch (button) {
                case MuteButton:
                    track->setMuted(!track->isMuted());
                    emit trackMuteToggled(headerTrack);
                    update();
                    return;
                case SoloButton:
                    track->setSolo(!track->isSolo());
                    emit trackSoloToggled(headerTrack);
                    update();
                    return;
                case ColorButton:
                    {
                        QColor newColor = QColorDialog::getColor(
                            track->getColor().toQColor(), this, tr("Track Color"));
                        if (newColor.isValid()) {
                            track->setColor(NN_Color_t::fromQColor(newColor));
                            update();
                        }
                    }
                    return;
                case NoButton:
                    // Select track
                    m_selectedTrackIndex = headerTrack;
                    emit trackSelected(headerTrack);
                    update();
                    return;
            }
        }
        
        // Handle clip interactions in content area
        int trackIndex;
        NN_MidiClip_t *clip = clipAtPosition(event->pos(), trackIndex);
        
        if (clip) {
            HitZone zone = hitTestClip(clip, trackIndex, event->pos());
            
            bool addToSelection = event->modifiers() & Qt::ShiftModifier;
            if (!m_selectedClipIds.contains(clip->id)) {
                selectClip(clip->id, addToSelection);
            }
            
            m_dragClipId = clip->id;
            m_dragTrackIndex = trackIndex;
            m_dragStartTick = xToTick(event->pos().x());
            m_dragStartTrackIndex = trackIndex;
            m_originalClipStart = clip->startTick;
            m_originalClipDuration = clip->durationTicks;
            
            // Store original state for ALL selected clips (for multi-clip movement)
            m_originalClipStates.clear();
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (arrangement) {
                for (int tIdx = 0; tIdx < static_cast<int>(arrangement->getTrackCount()); ++tIdx) {
                    auto *track = arrangement->getTracks()[tIdx];
                    if (!track) continue;
                    for (const auto &c : track->getClips()) {
                        if (m_selectedClipIds.contains(c.id)) {
                            ClipOriginalState state;
                            state.clipId = c.id;
                            state.trackIndex = tIdx;
                            state.startTick = c.startTick;
                            state.durationTicks = c.durationTicks;
                            m_originalClipStates.append(state);
                        }
                    }
                }
            }
            
            if (zone == LeftEdgeHit) {
                m_interactionMode = ResizingClipLeft;
                setCursor(Qt::SizeHorCursor);
            } else if (zone == RightEdgeHit) {
                m_interactionMode = ResizingClipRight;
                setCursor(Qt::SizeHorCursor);
            } else {
                m_interactionMode = MovingClip;
                setCursor(Qt::ClosedHandCursor);
            }
            
            emit clipSelected(clip);
        } else {
            // Start selection rectangle
            m_interactionMode = Selecting;
            m_selectionRect = QRect(event->pos(), QSize(0, 0));
            
            if (!(event->modifiers() & Qt::ShiftModifier)) {
                clearSelection();
            }
        }
    } else if (event->button() == Qt::RightButton) {
        // Cancel paste mode if active
        if (m_interactionMode == PastingClips) {
            cancelPasteMode();
            return;
        }
        
        // Context menu
        if (isInHeaderArea(event->pos())) {
            int trackIndex = headerTrackAtY(event->pos().y());
            NoteNagaArrangement *arrangement = m_engine ? 
                (m_engine->getRuntimeData() ? m_engine->getRuntimeData()->getArrangement() : nullptr) : nullptr;
            if (arrangement && trackIndex >= 0 && trackIndex < static_cast<int>(arrangement->getTrackCount())) {
                showTrackContextMenu(trackIndex, event->globalPosition().toPoint());
            } else {
                // Show add track menu for empty header area
                showEmptyAreaContextMenu(event->globalPosition().toPoint());
            }
        } else {
            // Content area right-click
            int trackIndex;
            NN_MidiClip_t *clip = clipAtPosition(event->pos(), trackIndex);
            if (clip) {
                // Select clip if not already selected
                if (!m_selectedClipIds.contains(clip->id)) {
                    selectClip(clip->id, false);
                }
                showClipContextMenu(event->globalPosition().toPoint());
            } else {
                // Show context menu for empty content area
                showClipContextMenu(event->globalPosition().toPoint());
            }
        }
    }
    
    QWidget::mousePressEvent(event);
}

void ArrangementTimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Block editing when playback is active
    if (m_engine && m_engine->isPlaying()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    // Update cursor based on hover
    if (m_interactionMode == None) {
        int trackIndex;
        NN_MidiClip_t *clip = clipAtPosition(event->pos(), trackIndex);
        if (clip) {
            HitZone zone = hitTestClip(clip, trackIndex, event->pos());
            if (zone == LeftEdgeHit || zone == RightEdgeHit) {
                setCursor(Qt::SizeHorCursor);
            } else {
                setCursor(Qt::OpenHandCursor);
            }
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
    
    // Handle paste preview mode - update preview position on mouse move
    if (m_interactionMode == PastingClips) {
        int64_t tick = xToTick(event->pos().x());
        int trackIdx = yToTrackIndex(event->pos().y());
        m_pastePreviewTick = snapTick(qMax(static_cast<int64_t>(0), tick));
        m_pastePreviewTrack = qMax(0, trackIdx);
        update();
        return;
    }
    
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }
    
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;

    switch (m_interactionMode) {
        case MovingClip: {
            int64_t currentTick = xToTick(event->pos().x());
            int64_t tickDelta = currentTick - m_dragStartTick;
            int currentTrack = yToTrackIndex(event->pos().y());
            int trackDelta = currentTrack - m_dragStartTrackIndex;
            
            // Move ALL selected clips based on delta from their original positions
            for (const auto &origState : m_originalClipStates) {
                // Calculate new position
                int64_t newStart = snapTick(origState.startTick + tickDelta);
                newStart = qMax(static_cast<int64_t>(0), newStart);
                int newTrackIdx = qBound(0, origState.trackIndex + trackDelta, 
                                         static_cast<int>(arrangement->getTrackCount()) - 1);
                
                // Find the clip in its original track
                bool found = false;
                for (int tIdx = 0; tIdx < static_cast<int>(arrangement->getTrackCount()); ++tIdx) {
                    auto *track = arrangement->getTracks()[tIdx];
                    if (!track) continue;
                    
                    for (int cIdx = 0; cIdx < static_cast<int>(track->getClips().size()); ++cIdx) {
                        auto &clip = track->getClips()[cIdx];
                        if (clip.id == origState.clipId) {
                            // If track changed, move clip to new track
                            if (tIdx != newTrackIdx) {
                                NN_MidiClip_t clipCopy = clip;
                                clipCopy.startTick = newStart;
                                track->removeClip(clip.id);
                                arrangement->getTracks()[newTrackIdx]->addClip(clipCopy);
                            } else {
                                clip.startTick = newStart;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
            update();
            break;
        }
        
        case ResizingClipLeft: {
            int64_t currentTick = xToTick(event->pos().x());
            int64_t newStart = snapTick(currentTick);
            newStart = qMax(static_cast<int64_t>(0), newStart);
            
            int64_t maxStart = m_originalClipStart + m_originalClipDuration - 480; // Min 1 beat
            newStart = qMin(newStart, maxStart);
            
            for (auto *track : arrangement->getTracks()) {
                for (auto &clip : track->getClips()) {
                    if (clip.id == m_dragClipId) {
                        int64_t endTick = m_originalClipStart + m_originalClipDuration;
                        clip.startTick = newStart;
                        clip.durationTicks = endTick - newStart;
                        update();
                        break;
                    }
                }
            }
            break;
        }
        
        case ResizingClipRight: {
            int64_t currentTick = xToTick(event->pos().x());
            int64_t newDuration = snapTick(currentTick) - m_originalClipStart;
            newDuration = qMax(static_cast<int64_t>(480), newDuration); // Min 1 beat
            
            for (auto *track : arrangement->getTracks()) {
                for (auto &clip : track->getClips()) {
                    if (clip.id == m_dragClipId) {
                        clip.durationTicks = newDuration;
                        update();
                        break;
                    }
                }
            }
            break;
        }
        
        case Selecting: {
            m_selectionRect = QRect(m_dragStartPos, event->pos()).normalized();
            
            // Update selection based on rectangle
            for (int trackIdx = 0; trackIdx < static_cast<int>(arrangement->getTrackCount()); ++trackIdx) {
                auto *track = arrangement->getTracks()[trackIdx];
                if (!track) continue;
                
                for (const auto &clip : track->getClips()) {
                    int clipX = tickToX(clip.startTick);
                    int clipWidth = static_cast<int>(clip.durationTicks * m_pixelsPerTick);
                    int clipY = trackIndexToY(trackIdx);
                    
                    QRect clipRect(clipX, clipY, clipWidth, m_trackHeight);
                    
                    if (m_selectionRect.intersects(clipRect)) {
                        m_selectedClipIds.insert(clip.id);
                    }
                }
            }
            
            update();
            break;
        }
        
        default:
            break;
    }
    
    QWidget::mouseMoveEvent(event);
}

void ArrangementTimelineWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_interactionMode == MovingClip) {
            emit clipMoved(m_dragClipId, xToTick(event->pos().x()));
            // Update arrangement max tick after move
            if (m_engine && m_engine->getRuntimeData()) {
                NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
                if (arrangement) {
                    arrangement->updateMaxTick();
                }
            }
        } else if (m_interactionMode == ResizingClipLeft || m_interactionMode == ResizingClipRight) {
            // Emit resize signal if needed
            // Update arrangement max tick after resize
            if (m_engine && m_engine->getRuntimeData()) {
                NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
                if (arrangement) {
                    arrangement->updateMaxTick();
                }
            }
        }
        
        m_interactionMode = None;
        m_dragClipId = -1;
        m_selectionRect = QRect();
        setCursor(Qt::ArrowCursor);
        update();
    }
    
    QWidget::mouseReleaseEvent(event);
}

void ArrangementTimelineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if double-click is in track header area (for name editing)
        if (isInHeaderArea(event->pos())) {
            int trackIndex = headerTrackAtY(event->pos().y());
            if (trackIndex >= 0) {
                // Check if click is on the track name area (not buttons)
                // Name area is roughly x < TRACK_HEADER_WIDTH - 40
                if (event->pos().x() < TRACK_HEADER_WIDTH - 40) {
                    startTrackNameEdit(trackIndex);
                    event->accept();
                    return;
                }
            }
        }
        
        // Check if double-click on a clip
        int trackIndex;
        NN_MidiClip_t *clip = clipAtPosition(event->pos(), trackIndex);
        
        if (clip) {
            // Open clip for editing - could switch to MIDI editor for the source sequence
            // emit clipDoubleClicked(clip);
        }
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void ArrangementTimelineWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double delta = event->angleDelta().y() > 0 ? 1.1 : 0.9;
        setPixelsPerTick(m_pixelsPerTick * delta);
        event->accept();
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Horizontal scroll
        int delta = event->angleDelta().y();
        setHorizontalOffset(m_horizontalOffset - delta);
        event->accept();
    } else {
        // Vertical scroll
        int delta = event->angleDelta().y();
        setVerticalOffset(m_verticalOffset - delta / 3);
        event->accept();
    }
}

void ArrangementTimelineWidget::keyPressEvent(QKeyEvent *event)
{
    // Block editing when playback is active (allow Escape for cancel)
    if (m_engine && m_engine->isPlaying() && event->key() != Qt::Key_Escape) {
        QWidget::keyPressEvent(event);
        return;
    }
    
    switch (event->key()) {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            deleteSelectedClips();
            break;
            
        case Qt::Key_A:
            if (event->modifiers() & Qt::ControlModifier) {
                // Select all clips
                if (m_engine && m_engine->getRuntimeData()) {
                    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
                    if (arrangement) {
                        for (auto *track : arrangement->getTracks()) {
                            for (const auto &clip : track->getClips()) {
                                m_selectedClipIds.insert(clip.id);
                            }
                        }
                        update();
                        emit selectionChanged();
                    }
                }
            }
            break;
            
        case Qt::Key_C:
            if (event->modifiers() & Qt::ControlModifier) {
                copySelectedClips();
            }
            break;
            
        case Qt::Key_X:
            if (event->modifiers() & Qt::ControlModifier) {
                cutSelectedClips();
            }
            break;
            
        case Qt::Key_V:
            if (event->modifiers() & Qt::ControlModifier) {
                startPasteMode();
            }
            break;
            
        case Qt::Key_Escape:
            if (m_interactionMode == PastingClips) {
                cancelPasteMode();
            } else {
                clearSelection();
            }
            break;
            
        default:
            break;
    }
    
    QWidget::keyPressEvent(event);
}

// Drag & Drop
void ArrangementTimelineWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(MIME_TYPE_MIDI_SEQUENCE)) {
        event->acceptProposedAction();
        m_showDropPreview = true;
    }
}

void ArrangementTimelineWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(MIME_TYPE_MIDI_SEQUENCE)) {
        m_dropPreviewTrack = yToTrackIndex(event->position().toPoint().y());
        m_dropPreviewTick = snapTick(xToTick(event->position().toPoint().x()));
        
        // Get duration from mime data
        QByteArray data = event->mimeData()->data(MIME_TYPE_MIDI_SEQUENCE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        int sequenceIndex;
        int64_t duration;
        stream >> sequenceIndex >> duration;
        m_dropPreviewDuration = duration;
        
        update();
        event->acceptProposedAction();
    }
}

void ArrangementTimelineWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(MIME_TYPE_MIDI_SEQUENCE)) {
        QByteArray data = event->mimeData()->data(MIME_TYPE_MIDI_SEQUENCE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        int sequenceIndex;
        int64_t duration;
        stream >> sequenceIndex >> duration;
        
        int trackIndex = yToTrackIndex(event->position().toPoint().y());
        int64_t tick = snapTick(xToTick(event->position().toPoint().x()));
        
        emit clipDropped(trackIndex, tick, sequenceIndex);
        
        m_showDropPreview = false;
        update();
        event->acceptProposedAction();
    }
}

void ArrangementTimelineWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Headers widget handles its own sizing via the parent layout
}
void ArrangementTimelineWidget::showTrackContextMenu(int trackIndex, const QPoint &globalPos)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement || trackIndex < 0 || 
        trackIndex >= static_cast<int>(arrangement->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arrangement->getTracks()[trackIndex];
    if (!track) return;
    
    QMenu menu(this);
    
    QAction *renameAction = menu.addAction(tr("Rename Track..."));
    QAction *colorAction = menu.addAction(tr("Change Color..."));
    menu.addSeparator();
    QAction *duplicateAction = menu.addAction(tr("Duplicate Track"));
    QAction *deleteAction = menu.addAction(tr("Delete Track"));
    menu.addSeparator();
    QAction *addAboveAction = menu.addAction(tr("Add Track Above"));
    QAction *addBelowAction = menu.addAction(tr("Add Track Below"));
    
    QAction *selected = menu.exec(globalPos);
    
    if (selected == renameAction) {
        QString currentName = QString::fromStdString(track->getName());
        QString newName = QInputDialog::getText(this, tr("Rename Track"),
            tr("Track name:"), QLineEdit::Normal, currentName);
        if (!newName.isEmpty()) {
            track->setName(newName.toStdString());
            update();
        }
    } else if (selected == colorAction) {
        QColor newColor = QColorDialog::getColor(
            track->getColor().toQColor(), this, tr("Track Color"));
        if (newColor.isValid()) {
            track->setColor(NN_Color_t::fromQColor(newColor));
            update();
        }
    } else if (selected == duplicateAction) {
        // TODO: Implement duplicate track
    } else if (selected == deleteAction) {
        arrangement->removeTrack(track->getId());
        update();
    } else if (selected == addAboveAction) {
        QString name = tr("Track %1").arg(arrangement->getTrackCount() + 1);
        arrangement->addTrack(name.toStdString());
        // TODO: Move new track to correct position
        update();
    } else if (selected == addBelowAction) {
        QString name = tr("Track %1").arg(arrangement->getTrackCount() + 1);
        arrangement->addTrack(name.toStdString());
        update();
    }
}

void ArrangementTimelineWidget::showEmptyAreaContextMenu(const QPoint &globalPos)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    QMenu menu(this);
    
    QAction *addTrackAction = menu.addAction(tr("Add New Track"));
    
    // Tempo track options
    menu.addSeparator();
    QAction *addTempoTrackAction = nullptr;
    QAction *removeTempoTrackAction = nullptr;
    QAction *toggleTempoTrackAction = nullptr;
    
    if (arrangement->hasTempoTrack()) {
        removeTempoTrackAction = menu.addAction(tr("Remove Tempo Track"));
        NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
        if (tempoTrack) {
            QString toggleText = tempoTrack->isTempoTrackActive() ?
                tr("Disable Tempo Track") : tr("Enable Tempo Track");
            toggleTempoTrackAction = menu.addAction(toggleText);
        }
    } else {
        addTempoTrackAction = menu.addAction(tr("Add Tempo Track"));
    }
    
    QAction *selected = menu.exec(globalPos);
    
    if (selected == addTrackAction) {
        QString name = tr("Track %1").arg(arrangement->getTrackCount() + 1);
        arrangement->addTrack(name.toStdString());
        update();
    } else if (addTempoTrackAction && selected == addTempoTrackAction) {
        // Get current project tempo in BPM
        double projectBpm = 120.0;
        int projectTempo = m_engine->getRuntimeData()->getTempo();
        if (projectTempo > 0) {
            projectBpm = 60'000'000.0 / projectTempo;
        }
        arrangement->createTempoTrack(projectBpm);
        update();
    } else if (removeTempoTrackAction && selected == removeTempoTrackAction) {
        arrangement->removeTempoTrack();
        update();
    } else if (toggleTempoTrackAction && selected == toggleTempoTrackAction) {
        NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
        if (tempoTrack) {
            tempoTrack->setTempoTrackActive(!tempoTrack->isTempoTrackActive());
            emit arrangement->tempoTrackChanged();  // Notify global transport bar
            update();
        }
    }
}

void ArrangementTimelineWidget::showClipContextMenu(const QPoint &globalPos)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    QMenu menu(this);
    
    bool hasSelection = !m_selectedClipIds.isEmpty();
    bool hasClipboard = !m_clipboardClips.isEmpty();
    
    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(hasSelection);
    
    QAction *cutAction = menu.addAction(tr("Cut"));
    cutAction->setShortcut(QKeySequence::Cut);
    cutAction->setEnabled(hasSelection);
    
    QAction *pasteAction = menu.addAction(tr("Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setEnabled(hasClipboard);
    
    menu.addSeparator();
    
    QAction *duplicateAction = menu.addAction(tr("Duplicate"));
    duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateAction->setEnabled(hasSelection);
    
    QAction *deleteAction = menu.addAction(tr("Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setEnabled(hasSelection);
    
    menu.addSeparator();
    
    QAction *selectAllAction = menu.addAction(tr("Select All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    
    QAction *selected = menu.exec(globalPos);
    
    if (selected == copyAction) {
        copySelectedClips();
    } else if (selected == cutAction) {
        cutSelectedClips();
    } else if (selected == pasteAction) {
        startPasteMode();
    } else if (selected == duplicateAction) {
        copySelectedClips();
        // Paste immediately at offset
        for (auto &clipState : m_clipboardClips) {
            clipState.startTick += 480; // Offset by 1 beat
        }
        finishPaste();
    } else if (selected == deleteAction) {
        deleteSelectedClips();
    } else if (selected == selectAllAction) {
        for (auto *track : arrangement->getTracks()) {
            for (const auto &clip : track->getClips()) {
                m_selectedClipIds.insert(clip.id);
            }
        }
        update();
        emit selectionChanged();
    }
}

void ArrangementTimelineWidget::copySelectedClips()
{
    if (m_selectedClipIds.isEmpty()) return;
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    m_clipboardClips.clear();
    m_clipboardBaseTick = INT64_MAX;
    m_clipboardBaseTrack = INT_MAX;
    
    // Collect all selected clips with full data
    for (int tIdx = 0; tIdx < static_cast<int>(arrangement->getTrackCount()); ++tIdx) {
        auto *track = arrangement->getTracks()[tIdx];
        if (!track) continue;
        
        for (const auto &clip : track->getClips()) {
            if (m_selectedClipIds.contains(clip.id)) {
                ClipOriginalState state;
                state.clipId = clip.id;
                state.trackIndex = tIdx;
                state.startTick = clip.startTick;
                state.durationTicks = clip.durationTicks;
                // Store full clip data for cut operation support
                state.sequenceId = clip.sequenceId;
                state.offsetTicks = clip.offsetTicks;
                state.muted = clip.muted;
                state.name = clip.name;
                state.color = clip.color;
                m_clipboardClips.append(state);
                
                m_clipboardBaseTick = qMin(m_clipboardBaseTick, static_cast<int64_t>(clip.startTick));
                m_clipboardBaseTrack = qMin(m_clipboardBaseTrack, tIdx);
            }
        }
    }
}

void ArrangementTimelineWidget::cutSelectedClips()
{
    copySelectedClips();
    deleteSelectedClips();
}

void ArrangementTimelineWidget::startPasteMode()
{
    if (m_clipboardClips.isEmpty()) return;
    
    m_interactionMode = PastingClips;
    m_pastePreviewTrack = 0;
    m_pastePreviewTick = 0;
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    update();
}

void ArrangementTimelineWidget::cancelPasteMode()
{
    m_interactionMode = None;
    m_pastePreviewTrack = -1;
    m_pastePreviewTick = 0;
    setCursor(Qt::ArrowCursor);
    update();
}

void ArrangementTimelineWidget::finishPaste()
{
    if (m_clipboardClips.isEmpty()) return;
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    // Calculate offset from original clipboard position to paste position
    int64_t tickOffset = m_pastePreviewTick - m_clipboardBaseTick;
    int trackOffset = m_pastePreviewTrack - m_clipboardBaseTrack;
    
    // Clear selection for new clips
    m_selectedClipIds.clear();
    
    // Create new clips from clipboard
    for (const auto &clipState : m_clipboardClips) {
        // Calculate target track
        int targetTrackIdx = qBound(0, clipState.trackIndex + trackOffset, 
                                    static_cast<int>(arrangement->getTrackCount()) - 1);
        auto *targetTrack = arrangement->getTracks()[targetTrackIdx];
        if (!targetTrack) continue;
        
        // Create new clip from stored state (works for both copy and cut)
        NN_MidiClip_t newClip;
        newClip.sequenceId = clipState.sequenceId;
        newClip.startTick = clipState.startTick + tickOffset;
        newClip.durationTicks = clipState.durationTicks;
        newClip.offsetTicks = clipState.offsetTicks;
        newClip.muted = clipState.muted;
        newClip.name = clipState.name;
        newClip.color = clipState.color;
        
        targetTrack->addClip(newClip);
        m_selectedClipIds.insert(newClip.id);
    }
    
    m_interactionMode = None;
    m_pastePreviewTrack = -1;
    m_pastePreviewTick = 0;
    setCursor(Qt::ArrowCursor);
    update();
    emit selectionChanged();
}

void ArrangementTimelineWidget::drawPastePreview(QPainter &painter)
{
    if (m_interactionMode != PastingClips || m_clipboardClips.isEmpty()) return;
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    int64_t tickOffset = m_pastePreviewTick - m_clipboardBaseTick;
    int trackOffset = m_pastePreviewTrack - m_clipboardBaseTrack;
    
    painter.setOpacity(0.5);
    
    for (const auto &clipState : m_clipboardClips) {
        int targetTrackIdx = qBound(0, clipState.trackIndex + trackOffset, 
                                    static_cast<int>(arrangement->getTrackCount()) - 1);
        int64_t targetTick = clipState.startTick + tickOffset;
        
        int x = tickToX(targetTick);
        int y = trackIndexToY(targetTrackIdx);
        int w = static_cast<int>(clipState.durationTicks * m_pixelsPerTick);
        int h = m_trackHeight - 4;
        
        // Ghost clip rectangle
        painter.fillRect(x, y + 2, w, h, QColor(100, 149, 237, 128)); // Cornflower blue
        painter.setPen(QPen(QColor(100, 149, 237), 2, Qt::DashLine));
        painter.drawRect(x, y + 2, w, h);
    }
    
    painter.setOpacity(1.0);
}

// === Inline Track Name Editing ===

void ArrangementTimelineWidget::startTrackNameEdit(int trackIndex)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement || trackIndex < 0 || 
        trackIndex >= static_cast<int>(arrangement->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arrangement->getTracks()[trackIndex];
    if (!track) return;
    
    // Cancel any existing edit
    if (m_trackNameEdit) {
        cancelTrackNameEdit();
    }
    
    m_editingTrackIndex = trackIndex;
    
    // Create line edit widget
    m_trackNameEdit = new QLineEdit(this);
    m_trackNameEdit->setText(QString::fromStdString(track->getName()));
    m_trackNameEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #2a2a30;
            color: #ffffff;
            border: 1px solid #2563eb;
            border-radius: 3px;
            padding: 2px 4px;
            font-size: 11px;
        }
    )");
    
    // Position the line edit over the track name area
    int y = trackIndexToY(trackIndex);
    int editHeight = 24;
    int editY = y + (m_trackHeight - editHeight) / 2;
    m_trackNameEdit->setGeometry(8, editY, TRACK_HEADER_WIDTH - 50, editHeight);
    
    m_trackNameEdit->show();
    m_trackNameEdit->setFocus();
    m_trackNameEdit->selectAll();
    
    // Connect signals
    connect(m_trackNameEdit, &QLineEdit::returnPressed, this, &ArrangementTimelineWidget::finishTrackNameEdit);
    connect(m_trackNameEdit, &QLineEdit::editingFinished, this, &ArrangementTimelineWidget::finishTrackNameEdit);
}

void ArrangementTimelineWidget::finishTrackNameEdit()
{
    if (!m_trackNameEdit || m_editingTrackIndex < 0) return;
    
    if (!m_engine || !m_engine->getRuntimeData()) {
        cancelTrackNameEdit();
        return;
    }
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement || m_editingTrackIndex >= static_cast<int>(arrangement->getTrackCount())) {
        cancelTrackNameEdit();
        return;
    }
    
    NoteNagaArrangementTrack *track = arrangement->getTracks()[m_editingTrackIndex];
    if (track) {
        QString newName = m_trackNameEdit->text().trimmed();
        if (!newName.isEmpty()) {
            track->setName(newName.toStdString());
        }
    }
    
    // Cleanup
    m_trackNameEdit->deleteLater();
    m_trackNameEdit = nullptr;
    m_editingTrackIndex = -1;
    
    update();
}

void ArrangementTimelineWidget::cancelTrackNameEdit()
{
    if (m_trackNameEdit) {
        m_trackNameEdit->deleteLater();
        m_trackNameEdit = nullptr;
    }
    m_editingTrackIndex = -1;
    update();
}

void ArrangementTimelineWidget::setLoopRegion(int64_t startTick, int64_t endTick)
{
    if (m_loopStartTick != startTick || m_loopEndTick != endTick) {
        m_loopStartTick = startTick;
        m_loopEndTick = endTick;
        update();
        emit loopRegionChanged(startTick, endTick);
    }
}

void ArrangementTimelineWidget::setLoopEnabled(bool enabled)
{
    if (m_loopEnabled != enabled) {
        m_loopEnabled = enabled;
        update();
        emit loopEnabledChanged(enabled);
    }
}

void ArrangementTimelineWidget::setShowGrid(bool show)
{
    if (m_showGrid != show) {
        m_showGrid = show;
        update();
    }
}

void ArrangementTimelineWidget::drawGridLines(QPainter &painter)
{
    if (!m_showGrid) return;
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    // Get PPQ and time signature info
    int ppq = m_engine->getRuntimeData()->getPPQ();
    if (ppq <= 0) ppq = 480;
    
    int64_t ticksPerBeat = ppq;  // Quarter note = 1 beat
    int64_t ticksPerBar = ticksPerBeat * 4;  // 4/4 time signature
    
    // Calculate visible tick range
    int64_t startTick = xToTick(0);
    int64_t endTick = xToTick(width());
    
    // Determine grid spacing based on zoom level
    double pixelsPerBar = ticksPerBar * m_pixelsPerTick;
    
    int64_t majorStep = ticksPerBar;
    int64_t minorStep = ticksPerBeat;
    
    // Adjust spacing if too dense
    while (pixelsPerBar < 30) {
        majorStep *= 2;
        minorStep *= 2;
        pixelsPerBar *= 2;
    }
    
    // Draw minor grid lines (beats)
    if (pixelsPerBar > 60) {  // Only show beat lines if zoomed in enough
        painter.setPen(QPen(QColor("#2a2a32"), 1));
        int64_t tick = (startTick / minorStep) * minorStep;
        while (tick <= endTick) {
            if (tick >= 0 && tick % majorStep != 0) {
                int x = tickToX(tick);
                painter.drawLine(x, 0, x, height());
            }
            tick += minorStep;
        }
    }
    
    // Draw major grid lines (bars)
    painter.setPen(QPen(QColor("#3a3a45"), 1));
    int64_t tick = (startTick / majorStep) * majorStep;
    while (tick <= endTick) {
        if (tick >= 0) {
            int x = tickToX(tick);
            painter.drawLine(x, 0, x, height());
        }
        tick += majorStep;
    }
}

void ArrangementTimelineWidget::drawLoopRegion(QPainter &painter)
{
    if (!m_loopEnabled || m_loopEndTick <= m_loopStartTick) return;
    
    int loopStartX = tickToX(m_loopStartTick);
    int loopEndX = tickToX(m_loopEndTick);
    
    // Skip if completely outside visible area
    if (loopEndX < 0 || loopStartX > width()) return;
    
    // Draw semi-transparent overlay
    painter.fillRect(loopStartX, 0, loopEndX - loopStartX, height(),
                     QColor(34, 197, 94, 25)); // Green with transparency
    
    // Draw loop markers
    painter.setPen(QPen(QColor("#22c55e"), 2));
    painter.drawLine(loopStartX, 0, loopStartX, height());
    painter.drawLine(loopEndX, 0, loopEndX, height());
}