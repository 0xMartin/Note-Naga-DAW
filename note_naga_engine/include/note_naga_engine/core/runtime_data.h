#pragma once

#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_api.h>

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

#include <atomic>
#include <vector>

/*******************************************************************************************************/
// Note Naga Runtime Data
/*******************************************************************************************************/

/**
 * @brief Represents a Note Naga runtime data container holding MIDI sequences and tracks.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaRuntimeData : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaRuntimeData {
#endif

public:
    /**
     * @brief Constructor for Note Naga runtime data.
     */
    NoteNagaRuntimeData();

    /**
     * @brief Destructor for Note Naga runtime data.
     */
    virtual ~NoteNagaRuntimeData();

    /**
     * @brief Loads a project from the specified path.
     * @param project_path Path to the project file.
     * @return True on success, false otherwise.
     */
    bool loadProject(const std::string &project_path);

    /**
     * @brief Adds a MIDI sequence to the project.
     * @param sequence Pointer to the sequence to add.
     */
    void addSequence(NoteNagaMidiSeq *sequence);

    /**
     * @brief Removes a MIDI sequence from the project.
     * @param sequence Pointer to the sequence to remove.
     */
    void removeSequence(NoteNagaMidiSeq *sequence);

    /**
     * @brief Returns the project's PPQ (pulses per quarter note).
     * @return PPQ value.
     */
    int getPPQ() const;

    /**
     * @brief Returns the project's tempo.
     * @return Tempo (BPM).
     * @note int bpm = 60'000'000.0 / project->getTempo(); // Convert to BPM
     */
    int getTempo() const;

    /**
     * @brief Returns the current tick of the project.
     * @return Current tick.
     */
    int getCurrentTick() const { return current_tick; }

    /**
     * @brief Returns the maximum tick in the project.
     * @return Maximum tick.
     */
    int getMaxTick() const;

    /**
     * @brief Returns the currently active MIDI sequence.
     * @return Pointer to the active sequence.
     */
    NoteNagaMidiSeq *getActiveSequence() const { return this->active_sequence; }

    /**
     * @brief Returns a sequence by its ID.
     * @param sequence_id Sequence ID.
     * @return Pointer to the found sequence, or nullptr.
     */
    NoteNagaMidiSeq *getSequenceById(int sequence_id);

    /**
     * @brief Returns all sequences in the project.
     * @return Vector of pointers to sequences.
     */
    std::vector<NoteNagaMidiSeq *> getSequences() const { return sequences; }

    /**
     * @brief Sets the project's PPQ value.
     * @param ppq New PPQ value.
     */
    void setPPQ(int ppq) { this->ppq = ppq; }

    /**
     * @brief Sets the project's tempo.
     * @param tempo New tempo (BPM).
     */
    void setTempo(int tempo) { this->tempo = tempo; }

    /**
     * @brief Sets the current tick of the project.
     * @param tick New tick value.
     */
    void setCurrentTick(int tick);

    /**
     * @brief Sets the active sequence.
     * @param sequence Pointer to the sequence.
     * @return True on success, false otherwise.
     */
    bool setActiveSequence(NoteNagaMidiSeq *sequence);

    // ARRANGEMENT METHODS
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Gets the arrangement object.
     * @return Pointer to the arrangement.
     */
    NoteNagaArrangement* getArrangement() { return arrangement_; }
    const NoteNagaArrangement* getArrangement() const { return arrangement_; }

    /**
     * @brief Gets the current arrangement tick (separate from sequence tick).
     * @return Current arrangement tick.
     */
    int getCurrentArrangementTick() const { return current_arrangement_tick_; }

    /**
     * @brief Sets the current arrangement tick.
     * @param tick The tick value.
     */
    void setCurrentArrangementTick(int tick);

    /**
     * @brief Gets the maximum tick in the arrangement.
     * @return Maximum arrangement tick.
     */
    int getArrangementMaxTick() const;

protected:
    std::vector<NoteNagaMidiSeq *> sequences; ///< All MIDI sequences in the project
    NoteNagaMidiSeq *active_sequence; ///< Pointer to the currently active sequence
    NoteNagaArrangement *arrangement_; ///< The arrangement/composition timeline

    int ppq;                       ///< Pulses per quarter note (PPQ)
    int tempo;                     ///< Tempo of the project (BPM)
    std::atomic<int> current_tick; ///< Current position in ticks (for sequence mode)
    std::atomic<int> current_arrangement_tick_; ///< Current position in arrangement mode
    int max_tick;                  ///< Maximum tick in the project

    // SIGNALS
    // ////////////////////////////////////////////////////////////////////////////////
    
#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when the project file is successfully loaded.
     */
    void projectFileLoaded();

    /**
     * @brief Signal emitted when the current tick changes.
     * @param tick The current tick.
     */
    void currentTickChanged(int tick);

    /**
     * @brief Signal emitted when the effective tempo changes during playback.
     * @param bpm The current BPM value.
     */
    void currentTempoChanged(double bpm);

    /**
     * @brief Signal emitted when sequence metadata is changed.
     * @param seq Pointer to the sequence.
     * @param param Name of the parameter.
     */
    void sequenceMetadataChanged(NoteNagaMidiSeq *seq, const std::string &param);

    /**
     * @brief Signal emitted when track metadata is changed.
     * @param track Pointer to the track.
     * @param param Name of the parameter.
     */
    void trackMetaChanged(NoteNagaTrack *track, const std::string &param);

    /**
     * @brief Signal emitted when the active sequence is changed.
     * @param seq Pointer to the new active sequence.
     */
    void activeSequenceChanged(NoteNagaMidiSeq *seq);

    /**
     * @brief Signal emitted when a sequence is added or removed.
     */
    void sequenceListChanged();

    /**
     * @brief Signal emitted when the track list changes.
     * @param track Pointer to the track.
     */
    void activeSequenceTrackListChanged(NoteNagaMidiSeq *seq);

    /**
     * @brief Signal emitted when the arrangement tick changes.
     * @param tick The current arrangement tick.
     */
    void currentArrangementTickChanged(int tick);

    /**
     * @brief Signal emitted when arrangement tracks/clips change.
     */
    void arrangementChanged();
#endif
};