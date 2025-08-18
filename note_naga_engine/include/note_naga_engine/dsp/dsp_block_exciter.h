#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <string>
#include <vector>

/**
 * @brief DSP Block for an exciter effect.
 *
 * Adds brightness and harmonics to high frequencies.
 */
class NOTE_NAGA_ENGINE_API DSPBlockExciter : public NoteNagaDSPBlockBase {
public:
    DSPBlockExciter(float freq = 4000.0f, float drive = 4.0f, float mix = 0.6f);

    void process(float *left, float *right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Exciter"; }

private:
    float freq_;  // Hz, 1000...12000
    float drive_; // 1.0..10.0
    float mix_;   // 0..1

    // Internal state for single-pole HP filter
    float hpL_ = 0.0f, hpR_ = 0.0f;
    float sampleRate_ = 44100.0f;
};