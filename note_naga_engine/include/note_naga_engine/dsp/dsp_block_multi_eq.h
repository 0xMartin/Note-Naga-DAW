#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <cmath>
#include <string>

/**
 * @brief DSP Block for a multi-band EQ with fixed frequencies and only gain controls.
 */
class NOTE_NAGA_ENGINE_API DSPBlockMultiSimpleEQ : public NoteNagaDSPBlockBase {
public:
    DSPBlockMultiSimpleEQ(const std::vector<float>& freqs, float q = 1.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Multi Band EQ"; }

    void recalcCoeffs(int band);
    void setSampleRate(float sr);

private:
    struct Band {
        float freq;
        float gain;
        float q;

        // biquad coefficients (RBJ cookbook)
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

        // filter state for each channel
        float x1l = 0, x2l = 0, y1l = 0, y2l = 0;
        float x1r = 0, x2r = 0, y1r = 0, y2r = 0;
    };

    std::vector<Band> bands_;
    float sampleRate_ = 44100.0f;
};