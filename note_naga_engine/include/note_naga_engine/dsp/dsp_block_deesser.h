#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for de-esser effect.
 *
 * This block reduces sibilance (harsh 's' and 't' sounds)
 * by dynamically compressing high frequencies.
 */
class NOTE_NAGA_ENGINE_API DSPBlockDeEsser : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the de-esser block.
     * @param freq Center frequency for detection in Hz (4000 .. 10000).
     * @param threshold Threshold in dB (-40 .. 0).
     * @param reduction Maximum reduction in dB (0 .. 12).
     */
    DSPBlockDeEsser(float freq = 6000.0f, float threshold = -20.0f, float reduction = 6.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "De-Esser"; }

    void setSampleRate(float sr) { sampleRate_ = sr; }

private:
    float freq_ = 6000.0f;
    float threshold_ = -20.0f;
    float reduction_ = 6.0f;
    
    float sampleRate_ = 44100.0f;
    
    // High-pass filter state for detection
    float hpStateL_ = 0.0f;
    float hpStateR_ = 0.0f;
    
    // Envelope follower for gain reduction
    float envL_ = 0.0f;
    float envR_ = 0.0f;
};
