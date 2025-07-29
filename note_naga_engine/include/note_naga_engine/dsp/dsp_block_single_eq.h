#pragma once

#include <note_naga_engine/core/dsp_block_base.h>
#include <cmath>

/**
 * @brief DSP Block for a single band EQ effect (peak filter, biquad).
 */
class DSPBlockSingleEQ : public NoteNagaDSPBlockBase {
public:
    DSPBlockSingleEQ(float freq, float gain, float q);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Single Band EQ"; }

    void recalcCoeffs();

    void setSampleRate(float sr);

private:
    float freq_ = 1000.0f;
    float gain_ = 0.0f;
    float q_ = 1.0f;

    // biquad coefficients (RBJ cookbook)
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a0_ = 1.0f, a1_ = 0.0f, a2_ = 0.0f;

    // filter state for each channel
    float x1l_ = 0, x2l_ = 0, y1l_ = 0, y2l_ = 0;
    float x1r_ = 0, x2r_ = 0, y1r_ = 0, y2r_ = 0;

    float sampleRate_ = 44100.0f;
};