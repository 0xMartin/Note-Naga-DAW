#pragma once

#include <note_naga_engine/core/dsp_block_base.h>

class DSPBlockGain : public NoteNagaDSPBlockBase {
public:
    DSPBlockGain(float gain) : gain_(gain) {}

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Gain"; }

private:
    float gain_ = 1.0f;
};