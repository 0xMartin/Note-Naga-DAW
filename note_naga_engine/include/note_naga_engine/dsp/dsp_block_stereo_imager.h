#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>
#include <cmath>

/**
 * @brief DSP Block for Stereo Imager.
 *
 * Controls stereo width using mid/side processing.
 */
class NOTE_NAGA_ENGINE_API DSPBlockStereoImager : public NoteNagaDSPBlockBase {
public:
    DSPBlockStereoImager(float width = 0.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Stereo Imager"; }

private:
    float width_; // -1.0 = mono, 0.0 = normal, 1.0 = super wide
};