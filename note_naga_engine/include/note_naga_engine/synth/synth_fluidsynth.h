#pragma once

#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <fluidsynth.h>
#include <string>
#include <unordered_map>

/**
 * FluidSynth syntetizér pro NoteNagaEngine.
 */
class NoteNagaSynthFluidSynth : public NoteNagaSynthesizer {
public:
    NoteNagaSynthFluidSynth(const std::string &name, const std::string &sf2_path);
    ~NoteNagaSynthFluidSynth() override;

    void playNote(const NN_Note_t &note, float pan = 0.0) override;
    void stopNote(const NN_Note_t &note) override;
    void stopAllNotes(NoteNagaMidiSeq *seq = nullptr, NoteNagaTrack *track = nullptr) override;

protected:
    void ensureFluidsynth();

    fluid_settings_t *synth_settings_;
    fluid_synth_t *fluidsynth_;
    fluid_audio_driver_t *audio_driver_;

    // playing notes map: track -> note ID -> playing note
    typedef std::unordered_map<long, NN_Note_t> TrackNotesMap;
    std::unordered_map<NoteNagaTrack*, TrackNotesMap> playing_notes_;

    // current channel programs
    std::unordered_map<int, int> channel_programs_;

    // curent channel pan values
    std::unordered_map<int, float> channel_pan_;
};