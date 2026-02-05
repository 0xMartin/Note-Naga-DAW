#include "arrangement_timeline_widget.h"
#include "arrangement_timeline_ruler.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <QDragEnterEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QApplication>
#include <QColorDialog>
#include <QInputDialog>
#include <cmath>

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
    return QRect(TRACK_HEADER_WIDTH, 0, width() - TRACK_HEADER_WIDTH, height());
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
    
    // Calculate max offset (don't scroll past all tracks)
    int totalTracksHeight = trackCount * m_trackHeight;
    int maxOffset = qMax(0, totalTracksHeight - height());
    offset = qBound(0, offset, maxOffset);
    
    if (m_verticalOffset != offset) {
        m_verticalOffset = offset;
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
    update();
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

// Coordinate conversion (x coordinates offset by track header width)
int64_t ArrangementTimelineWidget::xToTick(int x) const
{
    // Subtract header width since content starts after headers
    int contentX = x - TRACK_HEADER_WIDTH;
    return static_cast<int64_t>((contentX + m_horizontalOffset) / m_pixelsPerTick);
}

int ArrangementTimelineWidget::tickToX(int64_t tick) const
{
    // Add header width since content starts after headers
    return TRACK_HEADER_WIDTH + static_cast<int>(tick * m_pixelsPerTick) - m_horizontalOffset;
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
    return pos.x() < TRACK_HEADER_WIDTH;
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
        
        QRect nameRect(8, y + 4, TRACK_HEADER_WIDTH - 80, m_trackHeight - 8);
        painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, 
                         painter.fontMetrics().elidedText(trackName, Qt::ElideRight, nameRect.width()));
        
        // Buttons: Color, Mute, Solo (right side)
        int buttonsX = TRACK_HEADER_WIDTH - (3 * (HEADER_BUTTON_SIZE + HEADER_BUTTON_PADDING));
        int buttonY = y + (m_trackHeight - HEADER_BUTTON_SIZE) / 2;
        
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
    
    // If no tracks, draw a hint/placeholder in content area
    if (trackCount == 0) {
        QRect contentArea = contentRect();
        painter.fillRect(contentArea, QColor("#1a1a20"));
        painter.setPen(QColor("#555555"));
        QFont font = painter.font();
        font.setPointSize(11);
        painter.setFont(font);
        painter.drawText(contentArea, Qt::AlignCenter, tr("Drag a MIDI sequence here to create a track"));
        return;
    }
    
    // Draw track lanes in content area only
    for (int i = 0; i < trackCount; ++i) {
        int y = trackIndexToY(i);
        
        // Alternating background - only in content area
        QColor bgColor = (i % 2 == 0) ? QColor("#1a1a20") : QColor("#1e1e24");
        painter.fillRect(TRACK_HEADER_WIDTH, y, width() - TRACK_HEADER_WIDTH, m_trackHeight, bgColor);
        
        // Track separator
        painter.setPen(QColor("#3a3a42"));
        painter.drawLine(TRACK_HEADER_WIDTH, y + m_trackHeight - 1, width(), y + m_trackHeight - 1);
    }
    
    // Fill remaining space below tracks
    int bottomY = trackIndexToY(trackCount);
    if (bottomY < height()) {
        painter.fillRect(TRACK_HEADER_WIDTH, bottomY, width() - TRACK_HEADER_WIDTH, 
                        height() - bottomY, QColor("#151518"));
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
    
    // Draw content area first (track lanes, clips, etc.)
    drawTrackLanes(painter);
    drawClips(painter);
    drawSelectionRect(painter);
    drawDropPreview(painter);
    drawPlayhead(painter);
    
    // Draw track headers on top (they stay fixed on the left)
    drawTrackHeaders(painter);
}

// Mouse events
void ArrangementTimelineWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    m_dragStartPos = event->pos();
    
    if (event->button() == Qt::LeftButton) {
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
            m_originalClipStart = clip->startTick;
            m_originalClipDuration = clip->durationTicks;
            
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
                // TODO: Show clip context menu
            } else {
                // Show add track menu for empty content area
                showEmptyAreaContextMenu(event->globalPosition().toPoint());
            }
        }
    }
    
    QWidget::mousePressEvent(event);
}

void ArrangementTimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
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
            int64_t newStart = snapTick(m_originalClipStart + tickDelta);
            newStart = qMax(static_cast<int64_t>(0), newStart);
            
            // Find and move the clip
            for (auto *track : arrangement->getTracks()) {
                for (auto &clip : track->getClips()) {
                    if (clip.id == m_dragClipId) {
                        clip.startTick = newStart;
                        update();
                        break;
                    }
                }
            }
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
        } else if (m_interactionMode == ResizingClipLeft || m_interactionMode == ResizingClipRight) {
            // Emit resize signal if needed
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
            
        case Qt::Key_Escape:
            clearSelection();
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
    
    QAction *selected = menu.exec(globalPos);
    
    if (selected == addTrackAction) {
        QString name = tr("Track %1").arg(arrangement->getTrackCount() + 1);
        arrangement->addTrack(name.toStdString());
        update();
    }
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