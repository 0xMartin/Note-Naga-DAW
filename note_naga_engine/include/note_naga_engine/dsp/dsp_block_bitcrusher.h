#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for a bitcrusher effect.
 *
 * This block implements a simple bitcrusher (bit depth + sample rate reduction).
 */
class NOTE_NAGA_ENGINE_API DSPBlockBitcrusher : public NoteNagaDSPBlockBase {
public:
    DSPBlockBitcrusher(float bitDepth, int sampleRateReduce, float mix);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Bitcrusher"; }

private:
    float bitDepth_ = 8.0f;         // 4 ... 16
    int sampleRateReduce_ = 8;      // 1 ... 32
    float mix_ = 1.0f;              // 0 ... 1

    // Internal state
    float lastL_ = 0.0f, lastR_ = 0.0f;
    int step_ = 0;
};