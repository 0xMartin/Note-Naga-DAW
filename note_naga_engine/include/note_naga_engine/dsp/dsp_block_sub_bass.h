#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for sub-bass enhancement effect.
 *
 * This block generates harmonics below the original bass frequencies
 * to add weight and power to the low end.
 */
class NOTE_NAGA_ENGINE_API DSPBlockSubBass : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the sub-bass enhancer block.
     * @param freq Cutoff frequency for sub-bass generation (40 .. 120 Hz).
     * @param amount Amount of sub-bass to add (0.0 .. 1.0).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockSubBass(float freq = 80.0f, float amount = 0.5f, float mix = 0.5f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Sub Bass"; }

    void setSampleRate(float sr) { sampleRate_ = sr; }

private:
    float freq_ = 80.0f;
    float amount_ = 0.5f;
    float mix_ = 0.5f;
    
    float sampleRate_ = 44100.0f;
    
    // Low-pass filter state for isolating bass
    float lpStateL1_ = 0.0f, lpStateL2_ = 0.0f;
    float lpStateR1_ = 0.0f, lpStateR2_ = 0.0f;
    
    // Phase for sub-harmonic generation
    float phaseL_ = 0.0f;
    float phaseR_ = 0.0f;
    float lastSampleL_ = 0.0f;
    float lastSampleR_ = 0.0f;
};
