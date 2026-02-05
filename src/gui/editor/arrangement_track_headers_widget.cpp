#include "arrangement_track_headers_widget.h"
#include "arrangement_track_header_widget.h"
#include "../components/track_stereo_meter.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/module/dsp_engine.h>

#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QColorDialog>

ArrangementTrackHeadersWidget::ArrangementTrackHeadersWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    // Width is controlled by splitter, just set minimum
    setMinimumWidth(120);
    setMinimumHeight(100);
    setFocusPolicy(Qt::StrongFocus);  // Enable keyboard focus for Delete key
}

void ArrangementTrackHeadersWidget::setEngine(NoteNagaEngine *engine)
{
    m_engine = engine;
}

void ArrangementTrackHeadersWidget::refreshFromArrangement()
{
    if (!m_engine || !m_engine->getRuntimeData()) {
        clearHeaders();
        return;
    }
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) {
        clearHeaders();
        return;
    }
    
    int trackCount = static_cast<int>(arrangement->getTrackCount());
    auto tracks = arrangement->getTracks();
    
    // Remove excess header widgets
    while (m_headerWidgets.size() > trackCount) {
        ArrangementTrackHeaderWidget *widget = m_headerWidgets.takeLast();
        widget->deleteLater();
    }
    
    // Add or update header widgets
    for (int i = 0; i < trackCount; ++i) {
        NoteNagaArrangementTrack *track = (i < static_cast<int>(tracks.size())) ? tracks[i] : nullptr;
        
        if (i < m_headerWidgets.size()) {
            // Update existing widget
            m_headerWidgets[i]->setTrack(track);
            m_headerWidgets[i]->setTrackIndex(i);
            m_headerWidgets[i]->updateFromTrack();
        } else {
            // Create new widget
            ArrangementTrackHeaderWidget *headerWidget = new ArrangementTrackHeaderWidget(track, i, this);
            headerWidget->setFixedHeight(m_trackHeight);
            
            // Connect signals
            connect(headerWidget, &ArrangementTrackHeaderWidget::muteToggled,
                    this, &ArrangementTrackHeadersWidget::trackMuteToggled);
            connect(headerWidget, &ArrangementTrackHeaderWidget::soloToggled,
                    this, &ArrangementTrackHeadersWidget::trackSoloToggled);
            connect(headerWidget, &ArrangementTrackHeaderWidget::colorChangeRequested,
                    this, &ArrangementTrackHeadersWidget::trackColorChangeRequested);
            connect(headerWidget, &ArrangementTrackHeaderWidget::trackSelected,
                    this, &ArrangementTrackHeadersWidget::trackSelected);
            connect(headerWidget, &ArrangementTrackHeaderWidget::nameChanged,
                    this, &ArrangementTrackHeadersWidget::trackNameChanged);
            
            m_headerWidgets.append(headerWidget);
        }
    }
    
    updateHeaderPositions();
    update();
}

void ArrangementTrackHeadersWidget::clearHeaders()
{
    for (ArrangementTrackHeaderWidget *widget : m_headerWidgets) {
        widget->deleteLater();
    }
    m_headerWidgets.clear();
}

void ArrangementTrackHeadersWidget::setVerticalOffset(int offset)
{
    if (m_verticalOffset != offset) {
        m_verticalOffset = qMax(0, offset);
        updateHeaderPositions();
        update();
    }
}

void ArrangementTrackHeadersWidget::setTrackHeight(int height)
{
    if (m_trackHeight != height) {
        m_trackHeight = qBound(40, height, 120);
        for (ArrangementTrackHeaderWidget *widget : m_headerWidgets) {
            widget->setFixedHeight(m_trackHeight);
        }
        updateHeaderPositions();
        update();
    }
}

void ArrangementTrackHeadersWidget::setSelectedTrack(int trackIndex)
{
    if (m_selectedTrackIndex != trackIndex) {
        // Deselect old
        if (m_selectedTrackIndex >= 0 && m_selectedTrackIndex < m_headerWidgets.size()) {
            m_headerWidgets[m_selectedTrackIndex]->setSelected(false);
        }
        
        m_selectedTrackIndex = trackIndex;
        
        // Select new
        if (m_selectedTrackIndex >= 0 && m_selectedTrackIndex < m_headerWidgets.size()) {
            m_headerWidgets[m_selectedTrackIndex]->setSelected(true);
        }
    }
}

void ArrangementTrackHeadersWidget::updateHeaderPositions()
{
    for (int i = 0; i < m_headerWidgets.size(); ++i) {
        ArrangementTrackHeaderWidget *widget = m_headerWidgets[i];
        int y = i * m_trackHeight - m_verticalOffset;
        widget->move(0, y);
        widget->setFixedWidth(width());
        
        // Show/hide based on visibility
        bool visible = (y + m_trackHeight > 0) && (y < height());
        widget->setVisible(visible);
    }
}

void ArrangementTrackHeadersWidget::updateTrackMeters()
{
    if (!m_engine) return;
    
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    if (!dspEngine) return;
    
    NoteNagaRuntimeData *runtimeData = m_engine->getRuntimeData();
    if (!runtimeData) return;
    
    NoteNagaArrangement *arrangement = runtimeData->getArrangement();
    if (!arrangement) return;
    
    int64_t currentTick = runtimeData->getCurrentArrangementTick();
    
    for (int i = 0; i < m_headerWidgets.size(); ++i) {
        ArrangementTrackHeaderWidget *headerWidget = m_headerWidgets[i];
        TrackStereoMeter *meter = headerWidget->getStereoMeter();
        if (!meter) continue;
        
        NoteNagaArrangementTrack *arrTrack = headerWidget->getTrack();
        if (!arrTrack || arrTrack->isMuted()) {
            meter->reset();
            continue;
        }
        
        // Aggregate RMS from all active clips on this track
        float maxLeftDb = -100.0f;
        float maxRightDb = -100.0f;
        
        for (const auto& clip : arrTrack->getClips()) {
            if (clip.muted) continue;
            if (!clip.containsTick(static_cast<int>(currentTick))) continue;
            
            // Get the referenced sequence
            NoteNagaMidiSeq *seq = runtimeData->getSequenceById(clip.sequenceId);
            if (!seq) continue;
            
            int seqLength = seq->getMaxTick();
            if (seqLength <= 0) continue;
            
            // Calculate current sequence tick for this clip
            int seqTick = clip.toSequenceTickLooped(static_cast<int>(currentTick), seqLength);
            
            // Check if any notes are currently playing in this clip's sequence tick
            bool hasActiveNotes = false;
            for (NoteNagaTrack *midiTrack : seq->getTracks()) {
                if (!midiTrack || midiTrack->isMuted() || midiTrack->isTempoTrack()) continue;
                
                // Check if any note is active at this sequence tick
                for (const auto& note : midiTrack->getNotes()) {
                    if (!note.start.has_value() || !note.length.has_value()) continue;
                    int noteStart = note.start.value();
                    int noteEnd = noteStart + note.length.value();
                    if (seqTick >= noteStart && seqTick < noteEnd) {
                        hasActiveNotes = true;
                        break;
                    }
                }
                if (hasActiveNotes) break;
            }
            
            // Only show RMS if this clip has active notes
            if (!hasActiveNotes) continue;
            
            // Get RMS from all tracks in this sequence
            for (NoteNagaTrack *midiTrack : seq->getTracks()) {
                if (!midiTrack || midiTrack->isMuted() || midiTrack->isTempoTrack()) continue;
                
                auto rms = dspEngine->getTrackVolumeDb(midiTrack);
                maxLeftDb = std::max(maxLeftDb, rms.first);
                maxRightDb = std::max(maxRightDb, rms.second);
            }
        }
        
        meter->setVolumesDb(maxLeftDb, maxRightDb);
    }
}

int ArrangementTrackHeadersWidget::contentHeight() const
{
    return m_headerWidgets.size() * m_trackHeight;
}

void ArrangementTrackHeadersWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Background
    painter.fillRect(rect(), QColor("#1e1e24"));
    
    // Right border
    painter.setPen(QColor("#3a3a42"));
    painter.drawLine(width() - 1, 0, width() - 1, height());
}

void ArrangementTrackHeadersWidget::wheelEvent(QWheelEvent *event)
{
    // Forward to parent for synchronized scrolling
    event->ignore();
}

void ArrangementTrackHeadersWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    
    // Find which track was right-clicked
    int clickedTrack = -1;
    for (int i = 0; i < m_headerWidgets.size(); ++i) {
        QRect widgetRect = m_headerWidgets[i]->geometry();
        if (widgetRect.contains(event->pos())) {
            clickedTrack = i;
            break;
        }
    }
    
    QAction *addTrackAction = menu.addAction(tr("Add New Track"));
    QAction *deleteTrackAction = nullptr;
    
    if (clickedTrack >= 0) {
        menu.addSeparator();
        deleteTrackAction = menu.addAction(tr("Delete Track"));
        deleteTrackAction->setShortcut(QKeySequence::Delete);
    }
    
    QAction *selected = menu.exec(event->globalPos());
    
    if (selected == addTrackAction) {
        emit addTrackRequested();
    } else if (deleteTrackAction && selected == deleteTrackAction) {
        emit deleteTrackRequested(clickedTrack);
    }
}

void ArrangementTrackHeadersWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_selectedTrackIndex >= 0) {
            emit deleteTrackRequested(m_selectedTrackIndex);
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void ArrangementTrackHeadersWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Update header widget widths when splitter is moved
    updateHeaderPositions();
}
