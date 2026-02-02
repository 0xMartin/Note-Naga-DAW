#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/runtime_data.h>

/**
 * @brief Sample-accurate metronome driven by project ticks.
 * Generates 4 ticks per beat (16th notes). Tick sounds are triggered precisely
 * according to project->getCurrentTick(). Tempo and tick positions are always read from project.
 */
class NOTE_NAGA_ENGINE_API NoteNagaMetronome {
public:
    NoteNagaMetronome();

    void setProject(NoteNagaRuntimeData* project) { project_ = project; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setSampleRate(unsigned int sr) { sampleRate_ = sr; }

    bool isEnabled() const { return enabled_; }

    // Musí být voláno při změně pozice v sekvenceru!
    void reset() { lastTick_ = -1; phase_ = 0; tickSample_ = 0; }

    // Vykreslí tiky do stereo bufferu podle projektu
    void render(float* left, float* right, size_t numFrames);

private:
    NoteNagaRuntimeData* project_ = nullptr;
    bool enabled_ = false;
    unsigned int sampleRate_ = 44100;

    int lastTick_ = -1;      // poslední tick, při kterém došlo k renderu
    int phase_ = 0;          // sample uvnitř aktuálního tiku
    int tickSample_ = 0;     // sample uvnitř fáze tiknutí
    int ticksPerBeat_ = 4;   // počet "metronomických" tiků na beat (např. 4 = šestnáctiny)

    // pomocné: sample délka jednoho tiku (pro zvuk), počet ticků na míru z projektu
    int tickLength_ = 200;

    // Generuje sample pro aktuální fázi tiku
    float tickSample(bool accent, int tickSample) const;
};