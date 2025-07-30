#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>
#include <cmath>

/**
 * @brief DSP Block for a chorus effect.
 *
 * Simple stereo chorus using modulated delay (typicky 10–25 ms).
 */
class NOTE_NAGA_ENGINE_API DSPBlockChorus : public NoteNagaDSPBlockBase {
public:
    DSPBlockChorus(float speed = 1.2f, float depth = 8.0f, float mix = 0.5f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Chorus"; }

private:
    float speed_;   // Hz, 0.2 ... 5.0
    float depth_;   // ms, 4 ... 16
    float mix_;     // 0 ... 1

    // Internal state
    float lfoPhase_ = 0.0f;
    float sampleRate_ = 44100.0f; // Should be set from engine!
    std::vector<float> delayBufferL_, delayBufferR_;
    size_t delayIdx_ = 0;
    size_t maxDelaySamples_ = 0;

    void prepareDelayBuffer(size_t numFrames);
};