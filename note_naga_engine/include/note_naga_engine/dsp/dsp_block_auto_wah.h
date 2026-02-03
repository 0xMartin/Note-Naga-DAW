#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for auto-wah (envelope follower + filter) effect.
 *
 * This block uses an envelope follower to modulate a bandpass filter
 * cutoff, creating a classic auto-wah sound.
 */
class NOTE_NAGA_ENGINE_API DSPBlockAutoWah : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the auto-wah block.
     * @param sensitivity Envelope sensitivity (0.1 .. 10.0).
     * @param minFreq Minimum filter frequency in Hz (100 .. 500).
     * @param maxFreq Maximum filter frequency in Hz (1000 .. 5000).
     * @param resonance Filter resonance (0.5 .. 10.0).
     * @param mix Dry/Wet mix (0.0 .. 1.0).
     */
    DSPBlockAutoWah(float sensitivity = 2.0f, float minFreq = 200.0f, 
                    float maxFreq = 2000.0f, float resonance = 3.0f, float mix = 0.8f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Auto Wah"; }

    void setSampleRate(float sr) { sampleRate_ = sr; }

private:
    float sensitivity_ = 2.0f;
    float minFreq_ = 200.0f;
    float maxFreq_ = 2000.0f;
    float resonance_ = 3.0f;
    float mix_ = 0.8f;
    
    float sampleRate_ = 44100.0f;
    
    // Envelope follower state
    float envelopeL_ = 0.0f;
    float envelopeR_ = 0.0f;
    
    // Filter state (state-variable filter)
    float lpL_ = 0.0f, bpL_ = 0.0f;
    float lpR_ = 0.0f, bpR_ = 0.0f;
};
