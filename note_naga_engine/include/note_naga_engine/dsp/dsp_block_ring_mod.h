#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for ring modulation effect.
 *
 * This block multiplies the audio signal with a carrier oscillator,
 * creating metallic, bell-like tones.
 */
class NOTE_NAGA_ENGINE_API DSPBlockRingMod : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the ring modulator block.
     * @param freq Carrier frequency in Hz (20 .. 2000).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockRingMod(float freq = 440.0f, float mix = 0.5f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Ring Modulator"; }

    void setSampleRate(float sr) { sampleRate_ = sr; }

private:
    float freq_ = 440.0f;
    float mix_ = 0.5f;
    
    float sampleRate_ = 44100.0f;
    float phase_ = 0.0f;
};
