#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for simple pitch shifting effect.
 *
 * This block shifts the pitch of the audio signal up or down
 * using a granular approach.
 */
class NOTE_NAGA_ENGINE_API DSPBlockPitchShifter : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the pitch shifter block.
     * @param semitones Pitch shift in semitones (-12 .. +12).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockPitchShifter(float semitones = 0.0f, float mix = 1.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Pitch Shifter"; }

    void setSampleRate(float sr);

private:
    float semitones_ = 0.0f;
    float mix_ = 1.0f;
    
    float sampleRate_ = 44100.0f;
    
    // Delay buffer and crossfade for granular pitch shifting
    std::vector<float> bufferL_, bufferR_;
    size_t bufferSize_ = 8192;
    float readPosL_ = 0.0f;
    float readPosR_ = 0.0f;
    size_t writeIdx_ = 0;
    
    void resizeBuffers();
};
