#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for tape saturation effect.
 *
 * This block emulates the warm saturation and subtle compression
 * characteristics of analog tape machines.
 */
class NOTE_NAGA_ENGINE_API DSPBlockTapeSaturation : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the tape saturation block.
     * @param drive Input drive (0.0 .. 10.0).
     * @param saturation Saturation amount (0.0 .. 1.0).
     * @param warmth High frequency rolloff / warmth (0.0 .. 1.0).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockTapeSaturation(float drive = 2.0f, float saturation = 0.5f, 
                           float warmth = 0.5f, float mix = 0.8f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Tape Saturation"; }

    void setSampleRate(float sr) { sampleRate_ = sr; }

private:
    float drive_ = 2.0f;
    float saturation_ = 0.5f;
    float warmth_ = 0.5f;
    float mix_ = 0.8f;
    
    float sampleRate_ = 44100.0f;
    
    // Low-pass filter state for warmth
    float lpStateL_ = 0.0f;
    float lpStateR_ = 0.0f;
    
    // Bias/hysteresis simulation state
    float biasL_ = 0.0f;
    float biasR_ = 0.0f;
};
