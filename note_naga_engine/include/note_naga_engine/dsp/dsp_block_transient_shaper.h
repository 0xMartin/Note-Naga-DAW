#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for transient shaper effect.
 *
 * This block allows enhancing or reducing the attack and sustain
 * portions of the audio signal independently.
 */
class NOTE_NAGA_ENGINE_API DSPBlockTransientShaper : public NoteNagaDSPBlockBase {
public:
    /**
     * @brief Constructor for the transient shaper block.
     * @param attack Attack amount (-100 .. +100 %, negative reduces, positive enhances).
     * @param sustain Sustain amount (-100 .. +100 %).
     */
    DSPBlockTransientShaper(float attack = 0.0f, float sustain = 0.0f);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Transient Shaper"; }

    void setSampleRate(float sr) { sampleRate_ = sr; }

private:
    float attack_ = 0.0f;
    float sustain_ = 0.0f;
    
    float sampleRate_ = 44100.0f;
    
    // Envelope followers with different time constants
    float fastEnvL_ = 0.0f, fastEnvR_ = 0.0f;
    float slowEnvL_ = 0.0f, slowEnvR_ = 0.0f;
};
