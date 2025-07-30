#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <cmath>
#include <note_naga_engine/core/dsp_block_base.h>
#include <string>

/**
 * @brief DSP Block for a tremolo effect (volume LFO).
 */
class NOTE_NAGA_ENGINE_API DSPBlockTremolo : public NoteNagaDSPBlockBase {
public:
    DSPBlockTremolo(float speed, float depth, float mix);

    void process(float *left, float *right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Tremolo"; }

    void setSampleRate(float sr);

private:
    float speed_ = 5.0f; // Hz
    float depth_ = 0.8f; // 0 ... 1
    float mix_ = 1.0f;   // 0 ... 1
    float sampleRate_ = 44100.0f;
    float phase_ = 0.0f;
};