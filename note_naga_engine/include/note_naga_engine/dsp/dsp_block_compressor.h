#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <cmath>

/**
 * @brief DSP Block for a compressor effect.
 *
 * This block implements a basic compressor with adjustable parameters.
 */
class NOTE_NAGA_ENGINE_API DSPBlockCompressor : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the compressor block.
     *
     * @param threshold The threshold in dB.
     * @param ratio The compression ratio.
     * @param attack The attack time in milliseconds.
     * @param release The release time in milliseconds.
     * @param makeup The makeup gain in dB.
     */
    DSPBlockCompressor(float threshold, float ratio, float attack, float release, float makeup)
        : threshold_db_(threshold), ratio_(ratio), attack_ms_(attack), release_ms_(release), makeup_db_(makeup) {}

    void process(float* left, float* right, size_t numFrames) override;
    void resetState() override { gainSmooth_ = 1.0f; }

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Compressor"; }

private:
    // Parameters
    float threshold_db_ = -18.0f;
    float ratio_ = 4.0f;
    float attack_ms_ = 10.0f;
    float release_ms_ = 80.0f;
    float makeup_db_ = 0.0f;

    // Internal state
    float gainSmooth_ = 1.0f;

    // Helper
    inline float dB_to_linear(float db) const { return powf(10.0f, db / 20.0f); }
};