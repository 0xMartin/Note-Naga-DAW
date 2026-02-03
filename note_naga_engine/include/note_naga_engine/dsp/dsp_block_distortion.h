#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for distortion/overdrive effect.
 *
 * This block implements various distortion types including soft clip,
 * hard clip, tube, and fuzz.
 */
class NOTE_NAGA_ENGINE_API DSPBlockDistortion : public NoteNagaDSPBlockBase {
public:
    enum class DistortionType { SoftClip = 0, HardClip, Tube, Fuzz };

    /**
     * @brief Constructor for the distortion block.
     * @param type Distortion type (0-3).
     * @param drive Drive amount (1.0 .. 20.0).
     * @param tone Tone control (0.0 .. 1.0, affects high frequencies).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockDistortion(int type = 0, float drive = 4.0f, float tone = 0.5f, float mix = 0.8f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Distortion"; }

private:
    DistortionType type_ = DistortionType::SoftClip;
    float drive_ = 4.0f;
    float tone_ = 0.5f;
    float mix_ = 0.8f;
    
    // Low-pass filter state for tone control
    float lpStateL_ = 0.0f;
    float lpStateR_ = 0.0f;
    
    float processDistortion(float sample);
};
