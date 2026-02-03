#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for ducking/sidechain compression effect.
 *
 * This block reduces the signal level based on its own envelope,
 * creating a pumping effect similar to sidechain compression.
 */
class NOTE_NAGA_ENGINE_API DSPBlockDucker : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the ducking compressor block.
     * @param threshold Threshold in dB (-40 .. 0).
     * @param ratio Compression ratio (1.0 .. 20.0).
     * @param attack Attack time in ms (0.1 .. 100).
     * @param release Release time in ms (50 .. 1000).
     * @param depth Maximum ducking depth in dB (0 .. 40).
     */
    DSPBlockDucker(float threshold = -20.0f, float ratio = 8.0f, 
                   float attack = 5.0f, float release = 200.0f, float depth = 20.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Ducker"; }

    void setSampleRate(float sr) { sampleRate_ = sr; }

private:
    float threshold_ = -20.0f;
    float ratio_ = 8.0f;
    float attack_ = 5.0f;
    float release_ = 200.0f;
    float depth_ = 20.0f;
    
    float sampleRate_ = 44100.0f;
    
    // Envelope follower state
    float envelope_ = 0.0f;
    
    // Gain reduction state
    float gainReduction_ = 0.0f;
};
