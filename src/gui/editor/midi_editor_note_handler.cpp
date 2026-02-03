#include "midi_editor_note_handler.h"
#include "midi_editor_widget.h"
#include "../undo/undo_manager.h"
#include "../undo/midi_note_commands.h"
#include <note_naga_engine/note_naga_engine.h>

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>

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
                // Active track notes get higher z-value (500+) so they're always on top
                shapeItem->setZValue(is_active_track ? 500 + ng->track->getId() : ng->track->getId() + 10);
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
        
        // If single note selection (clearPrevious=true), emit track selection signal
        if (clearPrevious && noteGraphics->track) {
            emit noteTrackSelected(noteGraphics->track);
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
        // Active track notes get higher z-value (500+) so they're always on top
        shapeItem->setZValue(is_active_track ? 500 + noteGraphics->track->getId() : noteGraphics->track->getId() + 10);
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
            // Active track notes get higher z-value (500+) so they're always on top
            shapeItem->setZValue(is_active_track ? 500 + ng->track->getId() : ng->track->getId() + 10);
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
    auto *seq = m_editor->getSequence();
    NoteNagaTrack *activeTrack = seq ? seq->getActiveTrack() : nullptr;
    
    NoteGraphics *bestNote = nullptr;
    qreal bestZValue = -9999;
    
    for (auto &trackPair : m_noteItems) {
        for (auto &ng : trackPair) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng.item);
            if (shapeItem) {
                QRectF noteRect = getRealNoteRect(&ng);
                if (noteRect.contains(scenePos)) {
                    // Always prefer note with highest z-value (active track notes have z >= 500)
                    qreal zValue = shapeItem->zValue();
                    if (zValue > bestZValue) {
                        bestZValue = zValue;
                        bestNote = &ng;
                    }
                }
            }
        }
    }
    
    return bestNote;
}

bool MidiEditorNoteHandler::isNoteEdge(NoteGraphics *ng, const QPointF &scenePos) {
    if (!ng || !ng->item) return false;
    QRectF rect = getRealNoteRect(ng);
    return (scenePos.x() >= rect.right() - RESIZE_EDGE_MARGIN && 
            scenePos.x() <= rect.right() + RESIZE_EDGE_MARGIN);
}

QRectF MidiEditorNoteHandler::getRealNoteRect(const NoteGraphics *ng) const {
    QRectF rect;
    if (!ng || !ng->item) return rect;
    // Check if item is still valid (has scene)
    if (!ng->item->scene()) return rect;
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
    
    // Cannot insert notes into tempo track
    if (activeTrack->isTempoTrack()) return;
    
    int tick = m_editor->sceneXToTick(scenePos.x());
    int noteValue = m_editor->sceneYToNote(scenePos.y());
    noteValue = std::max(0, std::min(127, noteValue));
    
    NN_Note_t newNote;
    newNote.note = noteValue;
    newNote.start = m_editor->snapTickToGrid(tick);
    newNote.velocity = 100;
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

    // Play the note for audio feedback BEFORE adding to track
    // This ensures the note ID matches what synth expects
    NoteNagaEngine *engine = m_editor->getEngine();
    NN_Note_t noteToPlay = newNote;  // Copy for playback
    if (engine) {
        engine->playSingleNote(noteToPlay);
        
        // Stop the note after its actual duration (minimum 150ms for audibility)
        // getTempo() returns microseconds per beat, so:
        // us_per_tick = tempo / ppq, then total_us = note_length * us_per_tick
        // time_ms = total_us / 1000
        int tempo = seq->getTempo();  // microseconds per beat
        double usPerTick = static_cast<double>(tempo) / ppq;
        double totalUs = noteToPlay.length.value_or(ppq) * usPerTick;
        int previewDuration = std::max(150, static_cast<int>(totalUs / 1000.0));
        
        QTimer::singleShot(previewDuration, this, [engine, noteToPlay]() {
            engine->stopSingleNote(noteToPlay);
        });
    }

    // Use undo command to add the note to track
    auto cmd = std::make_unique<AddNoteCommand>(m_editor, activeTrack, newNote);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
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
    int gridStep = m_editor->getGridStepTicks();
    int minNoteLength = std::max(1, gridStep); // Minimum note length = grid step

    QPointF totalDelta = m_lastDragPos - m_dragStartPos;
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> noteChanges;

    // For group operations, calculate common delta in ticks and semitones ONCE
    // This ensures all notes move by the same amount
    int deltaTicks = static_cast<int>(totalDelta.x() / config->time_scale);
    int deltaNotes = static_cast<int>(-round(totalDelta.y() / config->key_height));
    
    // For move operations, snap the delta itself to grid (not each note individually)
    // This prevents notes from "spreading apart" due to individual snapping
    if (m_dragMode == NoteDragMode::Move && m_selectedNotes.size() > 1) {
        // Find the first note's original start and calculate where it would snap to
        NoteGraphics *firstNote = m_selectedNotes.first();
        if (m_dragStartNoteStates.contains(firstNote)) {
            NN_Note_t firstOriginal = m_dragStartNoteStates.value(firstNote);
            if (firstOriginal.start.has_value()) {
                int rawNewStart = firstOriginal.start.value() + deltaTicks;
                int snappedNewStart = m_editor->snapTickToGridNearest(rawNewStart);
                // Use the snapped delta for all notes
                deltaTicks = snappedNewStart - firstOriginal.start.value();
            }
        }
    }

    for (NoteGraphics *ng : m_selectedNotes) {
        if (!m_dragStartNoteStates.contains(ng)) continue;

        NN_Note_t originalNote = m_dragStartNoteStates.value(ng);
        NN_Note_t newNote = originalNote;

        if (m_dragMode == NoteDragMode::Move) {
            if (newNote.start.has_value()) {
                if (m_selectedNotes.size() == 1) {
                    // Single note: snap to grid normally
                    newNote.start = m_editor->snapTickToGridNearest(originalNote.start.value() + deltaTicks);
                } else {
                    // Multiple notes: apply same delta to maintain relative positions
                    newNote.start = std::max(0, originalNote.start.value() + deltaTicks);
                }
            }
            // Apply same note delta to all
            newNote.note = std::clamp(originalNote.note + deltaNotes, 0, 127);
        } else if (m_dragMode == NoteDragMode::Resize) {
            int deltaLength = static_cast<int>(totalDelta.x() / config->time_scale);
            if (newNote.length.has_value() && newNote.start.has_value()) {
                int originalEndTick = originalNote.start.value() + originalNote.length.value();
                int newEndTick = originalEndTick + deltaLength;
                int snappedEndTick = m_editor->snapTickToGridNearest(newEndTick);
                // Enforce minimum length based on grid
                int newLength = snappedEndTick - originalNote.start.value();
                newNote.length = std::max(minNoteLength, newLength);
            }
        }
        noteChanges.append({ng->track, originalNote, newNote});
    }

    // Check for overlaps - if ANY overlap, cancel the ENTIRE operation
    // Group changes by track for overlap checking
    QMap<NoteNagaTrack*, QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>>> changesByTrack;
    for (const auto& change : noteChanges) {
        changesByTrack[std::get<0>(change)].append(change);
    }
    
    bool anyOverlap = false;
    
    // Validate each track's changes for overlaps
    for (auto trackIt = changesByTrack.begin(); trackIt != changesByTrack.end() && !anyOverlap; ++trackIt) {
        NoteNagaTrack *track = trackIt.key();
        auto &trackChanges = trackIt.value();
        
        // Get existing notes that are NOT being moved
        std::vector<NN_Note_t> existingNotes = track->getNotes();
        QSet<unsigned long> movingNoteIds;
        for (const auto& change : trackChanges) {
            movingNoteIds.insert(std::get<1>(change).id);
        }
        
        // Check each new note position for overlaps
        for (const auto& change : trackChanges) {
            const NN_Note_t& newNote = std::get<2>(change);
            
            // Check against existing notes (not being moved)
            for (const auto& existing : existingNotes) {
                if (movingNoteIds.contains(existing.id)) continue; // Skip notes being moved
                if (existing.note != newNote.note) continue; // Different pitch, no overlap
                
                int newStart = newNote.start.value_or(0);
                int newEnd = newStart + newNote.length.value_or(minNoteLength);
                int existingStart = existing.start.value_or(0);
                int existingEnd = existingStart + existing.length.value_or(1);
                
                if (newStart < existingEnd && existingStart < newEnd) {
                    anyOverlap = true;
                    break;
                }
            }
            if (anyOverlap) break;
            
            // Check against other moving notes (in case they overlap each other after move)
            for (const auto& otherChange : trackChanges) {
                if (&otherChange == &change) continue;
                const NN_Note_t& otherNote = std::get<2>(otherChange);
                if (otherNote.note != newNote.note) continue;
                
                int newStart = newNote.start.value_or(0);
                int newEnd = newStart + newNote.length.value_or(minNoteLength);
                int otherStart = otherNote.start.value_or(0);
                int otherEnd = otherStart + otherNote.length.value_or(1);
                
                if (newStart < otherEnd && otherStart < newEnd) {
                    anyOverlap = true;
                    break;
                }
            }
            if (anyOverlap) break;
        }
    }
    
    // If overlap detected, cancel entire operation
    if (anyOverlap) {
        // Reset visual positions
        for (NoteGraphics *ng : m_selectedNotes) {
            if (ng->item) ng->item->setPos(0, 0);
            if (ng->label) ng->label->setPos(0, 0);
        }
        clearGhostPreview();
        m_dragStartNoteStates.clear();
        return;
    }
    
    // Reset visual positions before refresh (they were moved via setPos during drag)
    for (NoteGraphics *ng : m_selectedNotes) {
        if (ng->item) ng->item->setPos(0, 0);
        if (ng->label) ng->label->setPos(0, 0);
    }
    
    // Clear ghost preview
    clearGhostPreview();
    
    // Use undo command based on drag mode
    if (m_dragMode == NoteDragMode::Move) {
        auto cmd = std::make_unique<MoveNotesCommand>(m_editor, noteChanges);
        m_editor->getUndoManager()->executeCommand(std::move(cmd));
    } else if (m_dragMode == NoteDragMode::Resize) {
        auto cmd = std::make_unique<ResizeNotesCommand>(m_editor, noteChanges);
        m_editor->getUndoManager()->executeCommand(std::move(cmd));
    }
    
    m_dragStartNoteStates.clear();

    emit notesModified();

    clearSelection();
}

void MidiEditorNoteHandler::deleteSelectedNotes() {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    // Collect notes to delete
    QList<QPair<NoteNagaTrack*, NN_Note_t>> notesToDelete;
    for (NoteGraphics *ng : m_selectedNotes) {
        notesToDelete.append({ng->track, ng->note});
    }
    
    clearSelection();
    
    // Use undo command
    auto cmd = std::make_unique<DeleteNotesCommand>(m_editor, notesToDelete);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
    emit notesModified();
}

void MidiEditorNoteHandler::duplicateSelectedNotes() {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    int ppq = seq->getPPQ();
    int offset = ppq; // Offset by one quarter note
    
    // Collect duplicated notes
    QList<QPair<NoteNagaTrack*, NN_Note_t>> duplicatedNotes;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t newNote;
        // Generate new unique ID for the duplicate
        newNote.id = nn_generate_unique_note_id();
        newNote.parent = ng->track;  // Set parent to same track
        newNote.note = ng->note.note;
        newNote.velocity = ng->note.velocity;
        newNote.length = ng->note.length;
        newNote.pan = ng->note.pan;
        if (ng->note.start.has_value()) {
            newNote.start = ng->note.start.value() + offset;
        }
        duplicatedNotes.append({ng->track, newNote});
    }
    
    // Clear selection after duplicate
    clearSelection();
    
    // Use undo command
    auto cmd = std::make_unique<DuplicateNotesCommand>(m_editor, duplicatedNotes);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
    emit notesModified();
}

void MidiEditorNoteHandler::moveSelectedNotesToTrack(NoteNagaTrack *targetTrack) {
    if (m_selectedNotes.isEmpty() || !targetTrack) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    // Get existing notes on target track for overlap check
    std::vector<NN_Note_t> targetExisting = targetTrack->getNotes();
    
    // Collect notes that can be moved (no overlap)
    // Format: sourceTrack, targetTrack, originalNote, newNote
    QList<std::tuple<NoteNagaTrack*, NoteNagaTrack*, NN_Note_t, NN_Note_t>> moves;
    QList<QPair<NoteNagaTrack*, NN_Note_t>> notesToMoveTemp;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        // Skip if already on target track
        if (ng->track == targetTrack) continue;
        
        NN_Note_t note = ng->note;
        bool hasOverlap = false;
        
        // Check for overlap with existing notes on target track
        for (const auto &existing : targetExisting) {
            if (existing.note != note.note) continue;
            
            int noteStart = note.start.value_or(0);
            int noteEnd = noteStart + note.length.value_or(1);
            int existingStart = existing.start.value_or(0);
            int existingEnd = existingStart + existing.length.value_or(1);
            
            if (noteStart < existingEnd && existingStart < noteEnd) {
                hasOverlap = true;
                break;
            }
        }
        
        // Also check against notes we're about to move
        if (!hasOverlap) {
            for (const auto &other : notesToMoveTemp) {
                if (other.second.note != note.note) continue;
                
                int noteStart = note.start.value_or(0);
                int noteEnd = noteStart + note.length.value_or(1);
                int otherStart = other.second.start.value_or(0);
                int otherEnd = otherStart + other.second.length.value_or(1);
                
                if (noteStart < otherEnd && otherStart < noteEnd) {
                    hasOverlap = true;
                    break;
                }
            }
        }
        
        if (!hasOverlap) {
            notesToMoveTemp.append({ng->track, note});
            
            // Create new note for target track
            NN_Note_t newNote;
            newNote.id = nn_generate_unique_note_id();
            newNote.parent = targetTrack;
            newNote.note = note.note;
            newNote.velocity = note.velocity;
            newNote.length = note.length;
            newNote.start = note.start;
            newNote.pan = note.pan;
            
            moves.append({ng->track, targetTrack, note, newNote});
        }
    }
    
    if (moves.isEmpty()) return;
    
    clearSelection();
    
    // Use undo command
    auto cmd = std::make_unique<MoveNotesToTrackCommand>(m_editor, moves);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
    emit notesModified();
}

void MidiEditorNoteHandler::quantizeSelectedNotes() {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    // Collect note changes
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> noteChanges;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t originalNote = ng->note;
        NN_Note_t newNote = originalNote;
        
        if (newNote.start.has_value()) {
            newNote.start = m_editor->snapTickToGridNearest(newNote.start.value());
        }
        
        noteChanges.append({ng->track, originalNote, newNote});
    }
    
    // Use undo command
    auto cmd = std::make_unique<QuantizeNotesCommand>(m_editor, noteChanges);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
    emit notesModified();
}

void MidiEditorNoteHandler::transposeSelectedNotes(int semitones) {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    // Collect note changes
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> noteChanges;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t originalNote = ng->note;
        NN_Note_t newNote = originalNote;
        
        int newNoteValue = newNote.note + semitones;
        newNote.note = std::max(0, std::min(127, newNoteValue));
        
        noteChanges.append({ng->track, originalNote, newNote});
    }
    
    // Use undo command
    auto cmd = std::make_unique<TransposeNotesCommand>(m_editor, noteChanges, semitones);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
    emit notesModified();
}

void MidiEditorNoteHandler::setSelectedNotesVelocity(int velocity) {
    if (m_selectedNotes.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) return;
    
    velocity = std::max(1, std::min(127, velocity));
    
    // Collect note changes
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> noteChanges;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        NN_Note_t originalNote = ng->note;
        NN_Note_t newNote = originalNote;
        newNote.velocity = velocity;
        
        noteChanges.append({ng->track, originalNote, newNote});
    }
    
    // Use undo command
    auto cmd = std::make_unique<ChangeVelocityCommand>(m_editor, noteChanges, velocity);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
    emit notesModified();
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
    clearGhostPreview();
}

// --- Ghost preview ---

void MidiEditorNoteHandler::updateGhostPreview(const QPointF &currentPos) {
    clearGhostPreview();
    
    if (m_selectedNotes.isEmpty() || m_dragMode != NoteDragMode::Move) return;
    
    auto *scene = m_editor->getScene();
    auto *config = m_editor->getConfig();
    if (!scene || !config) return;
    
    QPointF totalDelta = currentPos - m_dragStartPos;
    int deltaTicks = static_cast<int>(totalDelta.x() / config->time_scale);
    int deltaNotes = static_cast<int>(-round(totalDelta.y() / config->key_height));
    
    // For multiple notes, snap the group delta
    if (m_selectedNotes.size() > 1) {
        NoteGraphics *firstNote = m_selectedNotes.first();
        if (m_dragStartNoteStates.contains(firstNote)) {
            NN_Note_t firstOriginal = m_dragStartNoteStates.value(firstNote);
            if (firstOriginal.start.has_value()) {
                int rawNewStart = firstOriginal.start.value() + deltaTicks;
                int snappedNewStart = m_editor->snapTickToGridNearest(rawNewStart);
                deltaTicks = snappedNewStart - firstOriginal.start.value();
            }
        }
    }
    
    // Get content height for Y calculation (same as in drawNote)
    int content_height = 128 * config->key_height;
    
    // Create ghost rectangles for each selected note
    for (NoteGraphics *ng : m_selectedNotes) {
        if (!m_dragStartNoteStates.contains(ng)) continue;
        
        NN_Note_t originalNote = m_dragStartNoteStates.value(ng);
        
        int newStart;
        if (m_selectedNotes.size() == 1 && originalNote.start.has_value()) {
            newStart = m_editor->snapTickToGridNearest(originalNote.start.value() + deltaTicks);
        } else {
            newStart = std::max(0, originalNote.start.value_or(0) + deltaTicks);
        }
        
        int newNoteValue = std::clamp(originalNote.note + deltaNotes, 0, 127);
        int noteLength = originalNote.length.value_or(1);
        
        // Calculate ghost position (matching drawNote formula)
        int ghostX = newStart * config->time_scale;
        int ghostY = content_height - (newNoteValue + 1) * config->key_height;
        int ghostW = std::max(1, static_cast<int>(noteLength * config->time_scale));
        int ghostH = config->key_height;
        
        // Create ghost rectangle with semi-transparent outline
        QGraphicsRectItem *ghost = scene->addRect(
            ghostX, ghostY, ghostW, ghostH,
            QPen(QColor(255, 255, 255, 180), 2, Qt::DashLine),
            QBrush(QColor(255, 255, 255, 40))
        );
        ghost->setZValue(1000);
        m_ghostItems.append(ghost);
    }
}

void MidiEditorNoteHandler::clearGhostPreview() {
    auto *scene = m_editor->getScene();
    if (!scene) {
        m_ghostItems.clear();
        return;
    }
    
    for (QGraphicsItem *item : m_ghostItems) {
        if (item && item->scene()) {
            scene->removeItem(item);
            delete item;
        }
    }
    m_ghostItems.clear();
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

// --- Copy/Paste ---

void MidiEditorNoteHandler::copySelectedNotes() {
    if (m_selectedNotes.isEmpty()) return;
    
    m_clipboard.clear();
    
    // Find the minimum start time to use as reference
    int minStart = std::numeric_limits<int>::max();
    int sumNotes = 0;
    
    for (NoteGraphics *ng : m_selectedNotes) {
        if (ng->note.start.has_value()) {
            minStart = std::min(minStart, ng->note.start.value());
        }
        sumNotes += ng->note.note;
    }
    
    if (minStart == std::numeric_limits<int>::max()) minStart = 0;
    m_clipboardBaseNote = m_selectedNotes.isEmpty() ? 64 : sumNotes / m_selectedNotes.size();
    
    // Copy notes with relative positions and track info
    for (NoteGraphics *ng : m_selectedNotes) {
        CopiedNote copied;
        copied.trackId = ng->track->getId();
        copied.relativeStart = ng->note.start.value_or(0) - minStart;
        copied.note = ng->note.note;
        copied.length = ng->note.length.value_or(480);
        copied.velocity = ng->note.velocity.value_or(100);
        copied.pan = ng->note.pan;
        m_clipboard.append(copied);
    }
}

void MidiEditorNoteHandler::startPasteMode() {
    if (m_clipboard.isEmpty()) return;
    
    m_pasteMode = true;
    emit pasteModeChanged(true);
}

void MidiEditorNoteHandler::cancelPasteMode() {
    if (!m_pasteMode) return;
    
    clearGhostPreview();
    m_pasteMode = false;
    emit pasteModeChanged(false);
}

void MidiEditorNoteHandler::updatePastePreview(const QPointF &scenePos) {
    if (!m_pasteMode || m_clipboard.isEmpty()) return;
    
    clearGhostPreview();
    
    auto *scene = m_editor->getScene();
    auto *config = m_editor->getConfig();
    if (!scene || !config) return;
    
    int baseTick = m_editor->snapTickToGridNearest(m_editor->sceneXToTick(scenePos.x()));
    int baseNote = m_editor->sceneYToNote(scenePos.y());
    int noteDelta = baseNote - m_clipboardBaseNote;
    
    int content_height = 128 * config->key_height;
    
    for (const CopiedNote &copied : m_clipboard) {
        int newStart = baseTick + copied.relativeStart;
        int newNoteValue = std::clamp(copied.note + noteDelta, 0, 127);
        
        int ghostX = newStart * config->time_scale;
        int ghostY = content_height - (newNoteValue + 1) * config->key_height;
        int ghostW = std::max(1, static_cast<int>(copied.length * config->time_scale));
        int ghostH = config->key_height;
        
        QGraphicsRectItem *ghost = scene->addRect(
            ghostX, ghostY, ghostW, ghostH,
            QPen(QColor(100, 255, 100, 200), 2, Qt::DashLine),
            QBrush(QColor(100, 255, 100, 60))
        );
        ghost->setZValue(1000);
        m_ghostItems.append(ghost);
    }
}

void MidiEditorNoteHandler::commitPaste(const QPointF &scenePos) {
    if (!m_pasteMode || m_clipboard.isEmpty()) return;
    
    auto *seq = m_editor->getSequence();
    if (!seq) {
        cancelPasteMode();
        return;
    }
    
    int baseTick = m_editor->snapTickToGridNearest(m_editor->sceneXToTick(scenePos.x()));
    int baseNote = m_editor->sceneYToNote(scenePos.y());
    int noteDelta = baseNote - m_clipboardBaseNote;
    int gridStep = m_editor->getGridStepTicks();
    
    // Group notes by track
    QMap<int, QList<NN_Note_t>> notesByTrack;
    
    for (const CopiedNote &copied : m_clipboard) {
        NN_Note_t newNote;
        newNote.id = nn_generate_unique_note_id();
        newNote.parent = nullptr;  // Will be set when adding to track
        newNote.start = baseTick + copied.relativeStart;
        newNote.note = std::clamp(copied.note + noteDelta, 0, 127);
        newNote.length = copied.length;
        newNote.velocity = copied.velocity;
        newNote.pan = copied.pan;
        notesByTrack[copied.trackId].append(newNote);
    }
    
    // Check for overlaps on each track
    bool anyOverlap = false;
    
    for (auto it = notesByTrack.begin(); it != notesByTrack.end() && !anyOverlap; ++it) {
        NoteNagaTrack *track = seq->getTrackById(it.key());
        if (!track) continue;
        
        std::vector<NN_Note_t> existingNotes = track->getNotes();
        const QList<NN_Note_t> &notesToAdd = it.value();
        
        for (const NN_Note_t &newNote : notesToAdd) {
            // Check against existing notes
            for (const auto &existing : existingNotes) {
                if (existing.note != newNote.note) continue;
                
                int newStart = newNote.start.value_or(0);
                int newEnd = newStart + newNote.length.value_or(gridStep);
                int existingStart = existing.start.value_or(0);
                int existingEnd = existingStart + existing.length.value_or(1);
                
                if (newStart < existingEnd && existingStart < newEnd) {
                    anyOverlap = true;
                    break;
                }
            }
            if (anyOverlap) break;
            
            // Check against other notes being pasted to same track
            for (const NN_Note_t &otherNew : notesToAdd) {
                if (&otherNew == &newNote) continue;
                if (otherNew.note != newNote.note) continue;
                
                int newStart = newNote.start.value_or(0);
                int newEnd = newStart + newNote.length.value_or(gridStep);
                int otherStart = otherNew.start.value_or(0);
                int otherEnd = otherStart + otherNew.length.value_or(1);
                
                if (newStart < otherEnd && otherStart < newEnd) {
                    anyOverlap = true;
                    break;
                }
            }
            if (anyOverlap) break;
        }
    }
    
    if (anyOverlap) {
        // Don't paste if there's overlap, but stay in paste mode
        return;
    }
    
    // Collect notes for undo command
    QList<QPair<NoteNagaTrack*, NN_Note_t>> pastedNotes;
    for (auto it = notesByTrack.begin(); it != notesByTrack.end(); ++it) {
        NoteNagaTrack *track = seq->getTrackById(it.key());
        if (!track) continue;
        
        for (NN_Note_t &newNote : it.value()) {
            newNote.parent = track;
            pastedNotes.append({track, newNote});
        }
    }
    
    // Clear ghost preview BEFORE any refresh that could clear the scene
    // to prevent double-delete of ghost items
    clearGhostPreview();
    
    m_pasteMode = false;
    emit pasteModeChanged(false);
    
    // Use undo command
    auto cmd = std::make_unique<PasteNotesCommand>(m_editor, pastedNotes);
    m_editor->getUndoManager()->executeCommand(std::move(cmd));
    
    emit notesModified();
}
