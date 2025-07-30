#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for a saturator effect.
 *
 * Adds analog-style harmonics via soft clipping.
 */
class NOTE_NAGA_ENGINE_API DSPBlockSaturator : public NoteNagaDSPBlockBase {
public:
    DSPBlockSaturator(float drive = 2.0f, float mix = 0.7f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Saturator"; }

private:
    float drive_; // 1.0 .. 10.0
    float mix_;   // 0 .. 1
};