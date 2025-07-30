#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>

/**
 * @brief DSP Block for a pan effect.
 *
 * This block implements a basic panning effect with adjustable parameters.
 */
class NOTE_NAGA_ENGINE_API DSPBlockPan : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the pan block.
     *
     * @param pan The pan value, where -1 is full left, 0 is center, and 1 is full right.
     */
    DSPBlockPan(float pan) : pan_(pan) {}

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Pan"; }

private:
    float pan_ = 0.0f; // -1 = Left, 0 = Center, 1 = Right
};