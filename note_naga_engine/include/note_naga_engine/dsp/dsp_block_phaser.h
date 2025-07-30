#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>
#include <cmath>

/**
 * @brief DSP Block for a phaser effect.
 *
 * Classic stereo phaser using modulated all-pass filters.
 */
class NOTE_NAGA_ENGINE_API DSPBlockPhaser : public NoteNagaDSPBlockBase {
public:
    DSPBlockPhaser(float speed = 0.6f, float depth = 0.8f, float feedback = 0.4f, float mix = 0.5f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Phaser"; }

private:
    float speed_;      // Hz, 0.1 .. 3.0
    float depth_;      // Sweep depth, 0..1
    float feedback_;   // Feedback amount, 0..0.95
    float mix_;        // Dry/Wet, 0..1

    float sampleRate_ = 44100.0f; // Should be set from engine!
    float lfoPhase_ = 0.0f;

    // Phaser state: 6 all-pass stages per channel
    static constexpr int stages_ = 6;
    float zL_[stages_] = {0}; // Memory for left all-pass
    float zR_[stages_] = {0}; // Memory for right all-pass

    float prevOutL_ = 0.0f, prevOutR_ = 0.0f; // Last output for feedback
};