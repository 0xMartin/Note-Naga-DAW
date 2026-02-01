#include "midi_editor_note_handler.h"
#include "midi_editor_widget.h"

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <algorithm>
#include <cmath>

MidiEditorNoteHandler::MidiEditorNoteHandler(MidiEditorWidget *editor, QObject *parent)
    : QObject(parent), m_editor(editor)
{
}

// --- Selection ---

void MidiEditorNoteHandler::selectNote(NoteGraphics *noteGraphics, bool clearPrevious) {
    if (!noteGraphics) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    if (clearPrevious) {
        for (NoteGraphics *ng : m_selectedNotes) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng->item);
            if (shapeItem) {
                bool is_active_track = seq->getActiveTrack() &&
                                 seq->getActiveTrack()->getId() == ng->track->getId();
                shapeItem->setPen(m_editor->getNotePen(ng->track, is_active_track, false));
                shapeItem->setZValue(ng->track->getId() + 10);
            }
        }
        m_selectedNotes.clear();
    }
    
    if (!m_selectedNotes.contains(noteGraphics)) {
        m_selectedNotes.append(noteGraphics);
        
        QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(noteGraphics->item);
        if (shapeItem) {
            shapeItem->setPen(m_editor->getNotePen(noteGraphics->track, false, true));
            shapeItem->setZValue(999);
        }
        
        emit selectionChanged();
    }
}

void MidiEditorNoteHandler::deselectNote(NoteGraphics *noteGraphics) {
    if (!noteGraphics) return;
    if (!m_selectedNotes.contains(noteGraphics)) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    m_selectedNotes.removeOne(noteGraphics);
    
    QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(noteGraphics->item);
    if (shapeItem) {
        bool is_active_track = seq->getActiveTrack() &&
                         seq->getActiveTrack()->getId() == noteGraphics->track->getId();
        shapeItem->setPen(m_editor->getNotePen(noteGraphics->track, is_active_track, false));
        shapeItem->setZValue(noteGraphics->track->getId() + 10);
    }
    
    emit selectionChanged();
}

void MidiEditorNoteHandler::clearSelection() {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    
    for (NoteGraphics *ng : m_selectedNotes) {
        QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng->item);
        if (shapeItem && seq) {
            bool is_active_track = seq->getActiveTrack() &&
                             seq->getActiveTrack()->getId() == ng->track->getId();
            shapeItem->setPen(m_editor->getNotePen(ng->track, is_active_track, false));
            shapeItem->setZValue(ng->track->getId() + 10);
        }
    }
    m_selectedNotes.clear();
    
    emit selectionChanged();
}

void MidiEditorNoteHandler::selectNotesInRect(const QRectF &rect) {
    int countBefore = m_selectedNotes.size();
    
    for (auto &trackPair : m_noteItems) {
        for (auto &ng : trackPair) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng.item);
            if (shapeItem) {
                QRectF noteRect = getRealNoteRect(&ng);
                if (rect.intersects(noteRect)) {
                    if (!m_selectedNotes.contains(&ng)) {
                        m_selectedNotes.append(&ng);
                        shapeItem->setPen(m_editor->getNotePen(ng.track, false, true));
                        shapeItem->setZValue(999);
                    }
                }
            }
        }
    }
    
    if (m_selectedNotes.size() != countBefore) {
        emit selectionChanged();
    }
}

void MidiEditorNoteHandler::selectAll() {
    clearSelection();
    
    for (auto &trackPair : m_noteItems) {
        for (auto &ng : trackPair) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng.item);
            if (shapeItem) {
                m_selectedNotes.append(&ng);
                shapeItem->setPen(m_editor->getNotePen(ng.track, false, true));
                shapeItem->setZValue(999);
            }
        }
    }
    
    emit selectionChanged();
}

void MidiEditorNoteHandler::invertSelection() {
    QList<NoteGraphics*> newSelection;
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    for (auto &trackPair : m_noteItems) {
        for (auto &ng : trackPair) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng.item);
            if (!shapeItem) continue;
            
            bool wasSelected = m_selectedNotes.contains(&ng);
            bool is_active_track = seq->getActiveTrack() &&
                             seq->getActiveTrack()->getId() == ng.track->getId();
            
            if (wasSelected) {
                // Deselect
                shapeItem->setPen(m_editor->getNotePen(ng.track, is_active_track, false));
                shapeItem->setZValue(ng.track->getId() + 10);
            } else {
                // Select
                newSelection.append(&ng);
                shapeItem->setPen(m_editor->getNotePen(ng.track, false, true));
                shapeItem->setZValue(999);
            }
        }
    }
    
    m_selectedNotes = newSelection;
    emit selectionChanged();
}

std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> MidiEditorNoteHandler::getSelectedNotesData() const {
    std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> result;
    result.reserve(m_selectedNotes.size());
    for (const NoteGraphics *ng : m_selectedNotes) {
        if (ng && ng->track) {
            result.push_back({ng->track, ng->note});
        }
    }
    return result;
}

// --- Note lookup ---

NoteGraphics* MidiEditorNoteHandler::findNoteUnderCursor(const QPointF &scenePos) {
    for (auto &trackPair : m_noteItems) {
        for (auto &ng : trackPair) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng.item);
            if (shapeItem) {
                QRectF noteRect = getRealNoteRect(&ng);
                if (noteRect.contains(scenePos)) {
                    return &ng;
                }
            }
        }
    }
    return nullptr;
}

bool MidiEditorNoteHandler::isNoteEdge(NoteGraphics *ng, const QPointF &scenePos) {
    if (!ng) return false;
    QRectF rect = getRealNoteRect(ng);
    return (scenePos.x() >= rect.right() - RESIZE_EDGE_MARGIN && 
            scenePos.x() <= rect.right() + RESIZE_EDGE_MARGIN);
}

QRectF MidiEditorNoteHandler::getRealNoteRect(const NoteGraphics *ng) const {
    QRectF rect;
    if (QGraphicsRectItem *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(ng->item)) {
        rect = rectItem->sceneBoundingRect();
    } else if (QGraphicsEllipseItem *ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(ng->item)) {
        rect = ellipseItem->sceneBoundingRect();
    }
    return rect;
}

// --- Note creation ---

void MidiEditorNoteHandler::addNewNote(const QPointF &scenePos) {
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    NoteNagaTrack *activeTrack = seq->getActiveTrack();
    if (!activeTrack) return;
    
    int tick = m_editor->sceneXToTick(scenePos.x());
    int noteValue = m_editor->sceneYToNote(scenePos.y());
    noteValue = std::max(0, std::min(127, noteValue));
    
    NN_Note_t newNote;
    newNote.note = noteValue;
    newNote.start = m_editor->snapTickToGrid(tick);
    newNote.velocity = 64;
    newNote.parent = activeTrack;
    
    int ppq = seq->getPPQ();
    NoteDuration duration = m_editor->getNoteDuration();
    int noteLength = ppq;

    switch (duration) {
        case NoteDuration::Whole:       noteLength = ppq * 4; break;
        case NoteDuration::Half:        noteLength = ppq * 2; break;
        case NoteDuration::Quarter:     noteLength = ppq; break;
        case NoteDuration::Eighth:      noteLength = ppq / 2; break;
        case NoteDuration::Sixteenth:   noteLength = ppq / 4; break;
        case NoteDuration::ThirtySecond:noteLength = ppq / 8; break;
    }
    newNote.length = std::max(1, noteLength);

    // Check for overlap
    for (const auto& existingNote : activeTrack->getNotes()) {
        if (existingNote.note == newNote.note) {
            int newNoteStart = newNote.start.value_or(0);
            int newNoteEnd = newNoteStart + newNote.length.value_or(0);
            int existingNoteStart = existingNote.start.value_or(0);
            int existingNoteEnd = existingNoteStart + existingNote.length.value_or(0);

            if (newNoteStart < existingNoteEnd && existingNoteStart < newNoteEnd) {
                return; // Overlap found, don't add
            }
        }
    }

    activeTrack->addNote(newNote);
    seq->computeMaxTick();
    
    emit notesModified();
}

// --- Note manipulation ---

void MidiEditorNoteHandler::moveSelectedNotes(const QPointF &delta) {
    if (m_selectedNotes.isEmpty() || (delta.x() == 0 && delta.y() == 0)) return;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng->item);
        if (shapeItem) {
            shapeItem->moveBy(delta.x(), delta.y());
            if (ng->label) {
                ng->label->moveBy(delta.x(), delta.y());
            }
        }
    }
}

void MidiEditorNoteHandler::resizeSelectedNotes(const QPointF &delta) {
    if (m_selectedNotes.isEmpty() || delta.x() == 0) return;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        if (QGraphicsRectItem *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(ng->item)) {
            QRectF rect = rectItem->rect();
            qreal newWidth = std::max(1.0, rect.width() + delta.x());
            rectItem->setRect(rect.x(), rect.y(), newWidth, rect.height());
        }
    }
}

void MidiEditorNoteHandler::applyNoteChanges() {
    if (m_selectedNotes.isEmpty() || m_dragStartNoteStates.isEmpty()) {
        return;
    }

    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    auto *config = m_editor->getConfig();

    QPointF totalDelta = m_lastDragPos - m_dragStartPos;
    QSet<NoteNagaTrack*> affectedTracks;
    QList<QPair<NoteGraphics*, NN_Note_t>> changesToApply;

    for (NoteGraphics *ng : m_selectedNotes) {
        if (!m_dragStartNoteStates.contains(ng)) continue;

        NN_Note_t originalNote = m_dragStartNoteStates.value(ng);
        NN_Note_t newNote = originalNote;

        if (m_dragMode == NoteDragMode::Move) {
            int deltaTicks = totalDelta.x() / config->time_scale;
            int deltaNotes = -round(totalDelta.y() / config->key_height);
            if (newNote.start.has_value()) {
                newNote.start = m_editor->snapTickToGrid(originalNote.start.value() + deltaTicks);
            }
            newNote.note = originalNote.note + deltaNotes;
            newNote.note = std::max(0, std::min(127, newNote.note));
        } else if (m_dragMode == NoteDragMode::Resize) {
            int deltaLength = totalDelta.x() / config->time_scale;
            if (newNote.length.has_value() && newNote.start.has_value()) {
                int originalEndTick = originalNote.start.value() + originalNote.length.value();
                int snappedEndTick = m_editor->snapTickToGridNearest(originalEndTick + deltaLength);
                newNote.length = std::max(1, snappedEndTick - originalNote.start.value());
            }
        }
        changesToApply.append({ng, newNote});
    }

    auto project = m_editor->getEngine()->getProject();
    bool signalsWereBlocked = project->signalsBlocked();
    project->blockSignals(true);

    for (const auto& change : changesToApply) {
        NoteGraphics* ng = change.first;
        NN_Note_t newNote = change.second;
        NN_Note_t originalNote = m_dragStartNoteStates.value(ng);
        
        ng->track->removeNote(originalNote);
        ng->track->addNote(newNote);
        
        affectedTracks.insert(ng->track);
    }
    
    for (const auto& change : changesToApply) {
        change.first->note = change.second;
    }

    project->blockSignals(signalsWereBlocked);
    
    m_dragStartNoteStates.clear();
    seq->computeMaxTick();

    emit notesModified();

    for (NoteNagaTrack* track : affectedTracks) {
        m_editor->refreshTrack(track);
    }

    clearSelection();
}

void MidiEditorNoteHandler::deleteSelectedNotes() {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    QSet<NoteNagaTrack*> affectedTracks;
    
    auto project = m_editor->getEngine()->getProject();
    bool signalsWereBlocked = project->signalsBlocked();
    project->blockSignals(true);
    
    for (NoteGraphics *ng : m_selectedNotes) {
        ng->track->removeNote(ng->note);
        affectedTracks.insert(ng->track);
    }
    
    project->blockSignals(signalsWereBlocked);
    
    clearSelection();
    
    seq->computeMaxTick();
    emit notesModified();
    
    for (NoteNagaTrack *track : affectedTracks) {
        m_editor->refreshTrack(track);
    }
}

void MidiEditorNoteHandler::duplicateSelectedNotes() {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    int ppq = seq->getPPQ();
    int offset = ppq; // Offset by one quarter note
    
    QSet<NoteNagaTrack*> affectedTracks;
    
    auto project = m_editor->getEngine()->getProject();
    bool signalsWereBlocked = project->signalsBlocked();
    project->blockSignals(true);
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t newNote = ng->note;
        if (newNote.start.has_value()) {
            newNote.start = newNote.start.value() + offset;
        }
        ng->track->addNote(newNote);
        affectedTracks.insert(ng->track);
    }
    
    project->blockSignals(signalsWereBlocked);
    
    seq->computeMaxTick();
    emit notesModified();
    
    for (NoteNagaTrack *track : affectedTracks) {
        m_editor->refreshTrack(track);
    }
}

void MidiEditorNoteHandler::quantizeSelectedNotes() {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    QSet<NoteNagaTrack*> affectedTracks;
    
    auto project = m_editor->getEngine()->getProject();
    bool signalsWereBlocked = project->signalsBlocked();
    project->blockSignals(true);
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t originalNote = ng->note;
        NN_Note_t newNote = originalNote;
        
        if (newNote.start.has_value()) {
            newNote.start = m_editor->snapTickToGridNearest(newNote.start.value());
        }
        
        ng->track->removeNote(originalNote);
        ng->track->addNote(newNote);
        affectedTracks.insert(ng->track);
    }
    
    project->blockSignals(signalsWereBlocked);
    
    emit notesModified();
    
    for (NoteNagaTrack *track : affectedTracks) {
        m_editor->refreshTrack(track);
    }
}

void MidiEditorNoteHandler::transposeSelectedNotes(int semitones) {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    QSet<NoteNagaTrack*> affectedTracks;
    
    auto project = m_editor->getEngine()->getProject();
    bool signalsWereBlocked = project->signalsBlocked();
    project->blockSignals(true);
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t originalNote = ng->note;
        NN_Note_t newNote = originalNote;
        
        int newNoteValue = newNote.note + semitones;
        newNote.note = std::max(0, std::min(127, newNoteValue));
        
        ng->track->removeNote(originalNote);
        ng->track->addNote(newNote);
        affectedTracks.insert(ng->track);
    }
    
    project->blockSignals(signalsWereBlocked);
    
    emit notesModified();
    
    for (NoteNagaTrack *track : affectedTracks) {
        m_editor->refreshTrack(track);
    }
}

void MidiEditorNoteHandler::setSelectedNotesVelocity(int velocity) {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    velocity = std::max(1, std::min(127, velocity));
    QSet<NoteNagaTrack*> affectedTracks;
    
    auto project = m_editor->getEngine()->getProject();
    bool signalsWereBlocked = project->signalsBlocked();
    project->blockSignals(true);
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t originalNote = ng->note;
        NN_Note_t newNote = originalNote;
        newNote.velocity = velocity;
        
        ng->track->removeNote(originalNote);
        ng->track->addNote(newNote);
        ng->note = newNote;
        affectedTracks.insert(ng->track);
    }
    
    project->blockSignals(signalsWereBlocked);
    
    emit notesModified();
    
    for (NoteNagaTrack *track : affectedTracks) {
        m_editor->refreshTrack(track);
    }
}

// --- Drag state ---

void MidiEditorNoteHandler::startDrag(const QPointF &pos, NoteDragMode mode) {
    m_dragMode = mode;
    m_dragStartPos = pos;
    m_lastDragPos = pos;
    
    if (mode == NoteDragMode::Move || mode == NoteDragMode::Resize) {
        m_dragStartNoteStates.clear();
        for (NoteGraphics *note : m_selectedNotes) {
            m_dragStartNoteStates[note] = note->note;
        }
    }
}

void MidiEditorNoteHandler::updateDrag(const QPointF &pos) {
    m_lastDragPos = pos;
}

void MidiEditorNoteHandler::endDrag() {
    m_dragMode = NoteDragMode::None;
    m_dragStartNoteStates.clear();
}

// --- Note items management ---

void MidiEditorNoteHandler::clearNoteItems() {
    m_selectedNotes.clear();
    
    auto *scene = m_editor->getScene();
    if (!scene) {
        m_noteItems.clear();
        return;
    }
    
    for (auto &arr : m_noteItems) {
        for (auto &ng : arr) {
            // Check if item is still valid and belongs to a scene before removing
            if (ng.item && ng.item->scene()) {
                scene->removeItem(ng.item);
                delete ng.item;
            }
            ng.item = nullptr;
            
            if (ng.label && ng.label->scene()) {
                scene->removeItem(ng.label);
                delete ng.label;
            }
            ng.label = nullptr;
        }
    }
    
    m_noteItems.clear();
}

void MidiEditorNoteHandler::clearTrackNoteItems(int trackId) {
    // First remove from selected notes
    for (int i = 0; i < m_selectedNotes.size(); ) {
        if (m_selectedNotes[i]->track->getId() == trackId) {
            m_selectedNotes.removeAt(i);
        } else {
            ++i;
        }
    }
    
    auto *scene = m_editor->getScene();
    auto it = m_noteItems.find(trackId);
    if (it != m_noteItems.end()) {
        for (auto &ng : it.value()) {
            // Check if item is still valid and belongs to a scene before removing
            if (ng.item && scene && ng.item->scene()) {
                scene->removeItem(ng.item);
                delete ng.item;
            }
            ng.item = nullptr;
            
            if (ng.label && scene && ng.label->scene()) {
                scene->removeItem(ng.label);
                delete ng.label;
            }
            ng.label = nullptr;
        }
        m_noteItems.erase(it);
    }
}
