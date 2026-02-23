#include "ai_preview_state.h"
#include <note_naga_engine/core/types.h>

void AiPreviewState::beginPreview(NoteNagaMidiSeq *sequence) {
    m_isActive = true;
    m_sequence = sequence;
    
    // Clear any previous state
    m_addedNotes.clear();
    m_removedNotes.clear();
    m_modifiedNotes.clear();
    m_addedNoteIds.clear();
    m_modifiedNoteOldIds.clear();
    m_addedTracks.clear();
    m_removedTracks.clear();
}

void AiPreviewState::endPreview() {
    m_isActive = false;
    m_sequence = nullptr;
    
    m_addedNotes.clear();
    m_removedNotes.clear();
    m_modifiedNotes.clear();
    m_addedNoteIds.clear();
    m_modifiedNoteOldIds.clear();
    m_addedTracks.clear();
    m_removedTracks.clear();
}

void AiPreviewState::addAddedNote(NoteNagaTrack *track, const NN_Note_t &note) {
    m_addedNotes.append({track, note});
    m_addedNoteIds.insert(note.id);
}

void AiPreviewState::addRemovedNote(NoteNagaTrack *track, const NN_Note_t &note) {
    m_removedNotes.append({track, note});
}

void AiPreviewState::addModifiedNote(NoteNagaTrack *track, const NN_Note_t &oldNote, const NN_Note_t &newNote) {
    m_modifiedNotes.append({track, oldNote, newNote});
    m_modifiedNoteOldIds.insert(oldNote.id);
}

void AiPreviewState::addAddedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
    m_addedTracks.append({seq, track});
}

void AiPreviewState::addRemovedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track, int originalIndex) {
    m_removedTracks.append({seq, track, originalIndex});
}

PreviewNoteState AiPreviewState::getNoteState(NoteNagaTrack *track, const NN_Note_t &note) const {
    if (!m_isActive) {
        return PreviewNoteState::Original;
    }
    
    // Check if this note was added
    if (m_addedNoteIds.contains(note.id)) {
        return PreviewNoteState::Added;
    }
    
    // Check if this is a modified note (the new version)
    for (const auto &tuple : m_modifiedNotes) {
        if (std::get<0>(tuple) == track) {
            const NN_Note_t &newNote = std::get<2>(tuple);
            if (newNote.id == note.id) {
                return PreviewNoteState::Modified;
            }
        }
    }
    
    return PreviewNoteState::Original;
}

bool AiPreviewState::isAddedTrack(NoteNagaTrack *track) const {
    if (!m_isActive) return false;
    
    for (const auto &pair : m_addedTracks) {
        if (pair.second == track) {
            return true;
        }
    }
    return false;
}

void AiPreviewState::clear() {
    m_isActive = false;
    m_sequence = nullptr;
    
    m_addedNotes.clear();
    m_removedNotes.clear();
    m_modifiedNotes.clear();
    m_addedNoteIds.clear();
    m_modifiedNoteOldIds.clear();
    m_addedTracks.clear();
    m_removedTracks.clear();
    m_pendingUndoCount = 0;
}
