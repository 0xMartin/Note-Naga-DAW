#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>
#include <cmath>

/**
 * @brief DSP Block for a noise gate effect.
 *
 * Silences audio below threshold, useful for cleaning up noise.
 */
class NOTE_NAGA_ENGINE_API DSPBlockNoiseGate : public NoteNagaDSPBlockBase {
public:
    DSPBlockNoiseGate(float threshold = -40.0f, float attack = 5.0f, float release = 80.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Noise Gate"; }

private:
    float threshold_; // dBFS, -60..0 dB
    float attack_;    // ms, 1..50 ms
    float release_;   // ms, 10..500 ms

    // Internal state
    float gain_ = 0.0f;
    float sampleRate_ = 44100.0f; // Should be set from engine!
};