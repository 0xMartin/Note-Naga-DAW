#pragma once

#include <note_naga_engine/core/note_naga_component.h>
#include <note_naga_engine/core/types.h>
#include <string>

/*******************************************************************************************************/
// Synthesizer Queue Message
/*******************************************************************************************************/

/**
 * @brief Represents a message for the Note Naga Synthesizer queue.
 */
struct NOTE_NAGA_ENGINE_API NN_SynthMessage_t {
    NN_Note_t note; /// <MIDI note to play or stop>
    int channel;    ///< MIDI channel to use
    bool play;      /// <True to play note, false to stop>
    float pan;      /// <Stereo pan value (-1.0 left, 1.0 right)>
};

/*******************************************************************************************************/
// Synthesizer Base Class
/*******************************************************************************************************/

/**
 * Abstract base class for Note Naga Synthesizers.
 * This class defines the interface for synthesizers that can play MIDI notes.
 */
class NOTE_NAGA_ENGINE_API NoteNagaSynthesizer
    : public NoteNagaEngineComponent<NN_SynthMessage_t, 1024> {
public:
    NoteNagaSynthesizer(const std::string &name) : name(name) {}
    virtual ~NoteNagaSynthesizer() = default;

    /**
     * @brief Set the name of the synthesizer.
     * @param name Name of the synthesizer.
     */
    void setName(const std::string &name) { this->name = name; }

    /**
     * @brief Sets a parameter for the synthesizer.
     * @param param Parameter name.
     * @param value Value to set.
     */
    std::string getName() const { return this->name; }

    /**
     * @brief Plays a MIDI note.
     * @param note The note to play.
     * @param channel MIDI channel to use.
     * @param pan Stereo pan value (-1.0 left, 1.0 right).
     */
    virtual void playNote(const NN_Note_t &note, int channel = 0, float pan = 0.0f) = 0;

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
     * @brief Sets a parameter for the synthesizer.
     * @param param Parameter name.
     * @param value Value to set.
     * @note This method is optional and can be overridden by derived classes.
     */
    virtual void setParam(const std::string &param, float value) {}

protected:
    std::string name; ///< Name of the synthesizer

    // playing notes map: track -> note ID -> playing note
    struct PlayedNote_t {
        NN_Note_t note;
        int channel;
    };
    typedef std::unordered_map<long, PlayedNote_t> TrackNotesMap;
    std::unordered_map<NoteNagaTrack*, TrackNotesMap> playing_notes_;

    // current channel programs
    std::unordered_map<int, int> channel_programs_;

    // curent channel pan values
    std::unordered_map<int, float> channel_pan_;

    /**
     * @brief Called when a new item is dequeued from the component's queue.
     * This method processes the MIDI note play/stop messages.
     * @param value The message to process.
     */
    void onItem(const NN_SynthMessage_t &value) override {
        if (value.play)
            playNote(value.note, value.channel, value.pan);
        else
            stopNote(value.note);
    }
};

/*******************************************************************************************************/
// Interface for Soft Synthesizers
/*******************************************************************************************************/

class NOTE_NAGA_ENGINE_API INoteNagaSoftSynth {
public: 
    virtual void renderAudio(size_t num_frames, NN_AudioBuffer_t& left, NN_AudioBuffer_t& right) = 0;
};