#pragma once

#include <note_naga_engine/core/types.h>
#include <QList>
#include <QPair>
#include <tuple>
#include <QSet>

class NoteNagaTrack;
class NoteNagaMidiSeq;

/**
 * @brief Describes a note difference for preview visualization.
 */
enum class PreviewNoteState {
    Original,   ///< Note exists in original and preview (unchanged)
    Added,      ///< Note was added by AI
    Removed,    ///< Note was removed by AI (shown as ghost)
    Modified    ///< Note was modified by AI (position, length, velocity, etc.)
};

/**
 * @brief Represents a track's state changes for preview.
 */
struct PreviewTrackState {
    int trackId = -1;
    bool isNewTrack = false;        ///< Track was added by AI
    bool isRemovedTrack = false;    ///< Track was removed by AI
    
    // For property changes
    std::optional<std::string> originalName;
    std::optional<int> originalInstrument;
    std::optional<int> originalChannel;
};

/**
 * @brief Stores all the data needed for AI preview mode.
 *        Tracks original state and AI modifications for visual diff.
 */
class AiPreviewState {
public:
    AiPreviewState() = default;
    
    /**
     * @brief Check if preview mode is active.
     */
    bool isActive() const { return m_isActive; }
    
    /**
     * @brief Set preview mode active state directly.
     */
    void setActive(bool active) { m_isActive = active; }
    
    /**
     * @brief Start preview mode for a sequence.
     * @param sequence The sequence being modified.
     */
    void beginPreview(NoteNagaMidiSeq *sequence);
    
    /**
     * @brief End preview mode and clear all stored state.
     */
    void endPreview();
    
    /**
     * @brief Record a note that was added by AI.
     */
    void addAddedNote(NoteNagaTrack *track, const NN_Note_t &note);
    
    /**
     * @brief Record a note that was removed by AI.
     */
    void addRemovedNote(NoteNagaTrack *track, const NN_Note_t &note);
    
    /**
     * @brief Record a note that was modified by AI.
     * @param track The track containing the note.
     * @param oldNote Original note state.
     * @param newNote New note state after modification.
     */
    void addModifiedNote(NoteNagaTrack *track, const NN_Note_t &oldNote, const NN_Note_t &newNote);
    
    /**
     * @brief Record a track that was added by AI.
     */
    void addAddedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track);
    
    /**
     * @brief Record a track that was removed by AI.
     */
    void addRemovedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track, int originalIndex);
    
    /**
     * @brief Get the preview state of a note.
     * @param track Track containing the note.
     * @param note The note to check.
     * @return The preview state (Original, Added, Modified, Removed).
     */
    PreviewNoteState getNoteState(NoteNagaTrack *track, const NN_Note_t &note) const;
    
    /**
     * @brief Check if a track was added by AI.
     */
    bool isAddedTrack(NoteNagaTrack *track) const;
    
    /**
     * @brief Get all removed notes (for rendering as ghost outlines).
     */
    const QList<QPair<NoteNagaTrack*, NN_Note_t>>& getRemovedNotes() const { return m_removedNotes; }
    
    /**
     * @brief Get all removed tracks (for displaying in track list).
     */
    const QList<std::tuple<NoteNagaMidiSeq*, NoteNagaTrack*, int>>& getRemovedTracks() const { return m_removedTracks; }
    
    // Statistics
    int getAddedNotesCount() const { return m_addedNotes.size(); }
    int getRemovedNotesCount() const { return m_removedNotes.size(); }
    int getModifiedNotesCount() const { return m_modifiedNotes.size(); }
    int getAddedTracksCount() const { return m_addedTracks.size(); }
    int getRemovedTracksCount() const { return m_removedTracks.size(); }
    
    /**
     * @brief Get the number of undo operations needed to discard changes.
     */
    int getPendingUndoCount() const { return m_pendingUndoCount; }
    
    /**
     * @brief Set the number of undo operations needed to discard changes.
     */
    void setPendingUndoCount(int count) { m_pendingUndoCount = count; }
    
    /**
     * @brief Clear all preview state data.
     */
    void clear();

private:
    bool m_isActive = false;
    NoteNagaMidiSeq *m_sequence = nullptr;
    
    // Added notes: <track, note>
    QList<QPair<NoteNagaTrack*, NN_Note_t>> m_addedNotes;
    
    // Removed notes: <track, note>
    QList<QPair<NoteNagaTrack*, NN_Note_t>> m_removedNotes;
    
    // Modified notes: <track, oldNote, newNote>
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_modifiedNotes;
    
    // For quick lookup of added notes by ID
    QSet<int> m_addedNoteIds;
    
    // For quick lookup of modified note old IDs  
    QSet<int> m_modifiedNoteOldIds;
    
    // Added tracks: <sequence, track>
    QList<QPair<NoteNagaMidiSeq*, NoteNagaTrack*>> m_addedTracks;
    
    // Removed tracks: <sequence, track, originalIndex>
    QList<std::tuple<NoteNagaMidiSeq*, NoteNagaTrack*, int>> m_removedTracks;
    
    // Number of undo operations needed to discard preview changes
    int m_pendingUndoCount = 0;
};
