#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>

/** 
 * @brief DSP Block for a gain effect.
 */
class NOTE_NAGA_ENGINE_API DSPBlockGain : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the gain block.
     *
     * @param gain The gain value in dB.
     */
    DSPBlockGain(float gain) : gain_(gain) {}

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Gain"; }

private:
    float gain_ = 0.0f;
};