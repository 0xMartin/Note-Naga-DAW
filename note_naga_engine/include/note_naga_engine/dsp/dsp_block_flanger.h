#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for a flanger effect.
 *
 * Simple stereo flanger using modulated short delay and feedback.
 */
class NOTE_NAGA_ENGINE_API DSPBlockFlanger : public NoteNagaDSPBlockBase {
public:
    DSPBlockFlanger(float speed = 0.3f, float depth = 3.0f, float feedback = 0.3f, float mix = 0.5f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Flanger"; }

private:
    float speed_;   // Hz, 0.05 ... 2.0
    float depth_;   // ms, 0.5 ... 8.0
    float feedback_;// 0 ... 0.95
    float mix_;     // 0 ... 1

    // Internal state
    float lfoPhase_ = 0.0f;
    float sampleRate_ = 44100.0f; // Should be set from engine!
    std::vector<float> delayBufferL_, delayBufferR_;
    size_t delayIdx_ = 0;
    size_t maxDelaySamples_ = 0;
};