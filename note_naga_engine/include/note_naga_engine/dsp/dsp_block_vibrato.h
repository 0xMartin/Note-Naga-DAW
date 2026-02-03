#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for vibrato effect.
 *
 * This block applies pitch modulation using an LFO to create
 * a vibrato effect.
 */
class NOTE_NAGA_ENGINE_API DSPBlockVibrato : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the vibrato block.
     * @param speed LFO speed in Hz (0.1 .. 20.0).
     * @param depth Depth in cents (0 .. 100).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockVibrato(float speed = 5.0f, float depth = 30.0f, float mix = 1.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Vibrato"; }

    void setSampleRate(float sr);

private:
    float speed_ = 5.0f;
    float depth_ = 30.0f;
    float mix_ = 1.0f;
    
    float sampleRate_ = 44100.0f;
    float lfoPhase_ = 0.0f;
    
    // Delay buffer for pitch shifting
    std::vector<float> delayBufferL_, delayBufferR_;
    size_t bufferSize_ = 4096;
    size_t writeIdx_ = 0;
    
    void resizeBuffers();
};
