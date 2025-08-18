#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <string>
#include <vector>

/**
 * @brief DSP Block for multimode filter (lowpass, highpass, bandpass).
 * Implements a simple biquad filter.
 */
enum class NOTE_NAGA_ENGINE_API FilterType {
    Lowpass = 0,
    Highpass = 1,
    Bandpass = 2
};

/**
 * @brief DSP Block for a filter effect.
 *
 * This block implements a basic multimode filter with adjustable cutoff, resonance and mix.
 */
class NOTE_NAGA_ENGINE_API DSPBlockFilter : public NoteNagaDSPBlockBase {
public:
    DSPBlockFilter(FilterType type, float cutoff, float resonance, float mix);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Filter"; }

    void setSampleRate(float sr);

private:
    // Parameters
    FilterType type_ = FilterType::Lowpass;
    float cutoff_ = 800.0f;      // Hz
    float resonance_ = 0.7f;     // Q (0.1 ... 2.0)
    float mix_ = 1.0f;           // 0 ... 1

    float sampleRate_ = 44100.0f;

    // Biquad coefficients and state
    float a0_, a1_, a2_, b1_, b2_;
    float z1L_ = 0.0f, z2L_ = 0.0f;
    float z1R_ = 0.0f, z2R_ = 0.0f;

    void calcCoeffs();
};