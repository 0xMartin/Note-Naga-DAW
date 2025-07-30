#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for a simple stereo delay/echo effect.
 *
 * This block implements a basic delay effect with feedback and mix.
 */
class NOTE_NAGA_ENGINE_API DSPBlockDelay : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the delay block.
     * @param time_ms Delay time in milliseconds.
     * @param feedback Feedback amount (0.0 .. 0.95).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockDelay(float time_ms, float feedback, float mix);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Delay"; }

    // Call when changing sample rate
    void setSampleRate(float sr);

private:
    // Parameters
    float time_ms_ = 400.0f;
    float feedback_ = 0.25f;
    float mix_ = 0.5f;

    // Internal state
    float sampleRate_ = 44100.0f;
    size_t delayIdx_ = 0;
    size_t maxDelaySamples_ = 44100; // 1 second max

    std::vector<float> delayBufferL_, delayBufferR_;

    void resizeDelayBuffers();
};