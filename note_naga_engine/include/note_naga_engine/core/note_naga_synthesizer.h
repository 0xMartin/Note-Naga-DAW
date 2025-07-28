#pragma once

#include <note_naga_engine/core/note_naga_component.h>
#include <note_naga_engine/core/types.h>
#include <string>

/**
 * @brief Represents a message for the Note Naga Synthesizer queue.
 */
struct NOTE_NAGA_ENGINE_API NN_SynthMessage_t {
    NN_Note_t note; /// <MIDI note to play or stop>
    bool play;      /// <True to play note, false to stop>
};

/**
 * Abstraktní třída syntetizéru pro NoteNagaEngine.
 * Dědí od NoteNagaEngineComponent, pracuje s NN_MixerMessage_t
 */
class NOTE_NAGA_ENGINE_API NoteNagaSynthesizer
    : public NoteNagaEngineComponent<NN_SynthMessage_t, 1024> {
public:
    NoteNagaSynthesizer(const std::string &name) : name(name) {}
    virtual ~NoteNagaSynthesizer() = default;

    /**
     * @brief Sets a parameter for the synthesizer.
     * @param param Parameter name.
     * @param value Value to set.
     */
    virtual std::string getName() { return this->name; }

    /**
     * @brief Plays a MIDI note.
     * @param note The note to play.
     */
    virtual void playNote(const NN_Note_t &note) = 0;

    /**
     * @brief Stops a MIDI note.
     * @param note The note to stop.
     */
    virtual void stopNote(const NN_Note_t &note) = 0;

    /**
     * @brief Stops all notes for the specified sequence and/or track.
     * @param seq Optional pointer to the MIDI sequence. If is nullptr, stops all notes in
     * all sequences.
     * @param track Optional pointer to the track. If is nullptr, stops all notes in the
     * sequence.
     */
    virtual void stopAllNotes(NoteNagaMidiSeq *seq = nullptr,
                              NoteNagaTrack *track = nullptr) = 0;

    /**
     * @brief Mutes or unmutes the specified track.
     * @param track Pointer to the track.
     * @param mute True to mute, false to unmute.
     */
    virtual void setParam(const std::string &param, float value) = 0;

protected:
    std::string name; ///< Name of the synthesizer

    void onItem(const NN_SynthMessage_t &value) override {
        if (value.play)
            playNote(value.note);
        else
            stopNote(value.note);
    }
};