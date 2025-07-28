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
    NoteNagaSynthFluidSynth(const std::string &sf2_path);
    ~NoteNagaSynthFluidSynth() override;

    void playNote(const NN_Note_t &note) override;
    void stopNote(const NN_Note_t &note) override;
    void stopAllNotes(NoteNagaMidiSeq *seq = nullptr, NoteNagaTrack *track = nullptr) override;
    void setParam(const std::string &param, float value) override;

protected:
    void ensureFluidsynth();

    fluid_settings_t *synth_settings_;
    fluid_synth_t *fluidsynth_;
    fluid_audio_driver_t *audio_driver_;

    // Evidence hraných not apod. (dle potřeby)
    std::unordered_map<NoteNagaTrack*, std::vector<int>> playing_notes_;
    float master_volume_ = 1.0f;
};