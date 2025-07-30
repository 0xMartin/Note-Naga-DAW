#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <cmath>

/**
 * @brief DSP Block for a limiter effect.
 *
 * This block implements a basic brickwall limiter.
 */
class NOTE_NAGA_ENGINE_API DSPBlockLimiter : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the limiter block.
     *
     * @param threshold The threshold in dB.
     * @param release The release time in milliseconds.
     * @param makeup The makeup gain in dB.
     */
    DSPBlockLimiter(float threshold, float release, float makeup)
        : threshold_db_(threshold), release_ms_(release), makeup_db_(makeup) {}

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Limiter"; }

private:
    // Parameters
    float threshold_db_ = -0.5f;
    float release_ms_ = 50.0f;
    float makeup_db_ = 0.0f;

    // Internal state
    float gainSmooth_ = 1.0f;

    // Helper
    inline float dB_to_linear(float db) const { return powf(10.0f, db / 20.0f); }
};