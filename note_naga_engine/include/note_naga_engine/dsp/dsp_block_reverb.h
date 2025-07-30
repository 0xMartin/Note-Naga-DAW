#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/dsp_block_base.h>
#include <vector>
#include <string>

/**
 * @brief DSP Block for a simple algorithmic reverb effect (Schroeder/Moorer, stereo).
 */
class NOTE_NAGA_ENGINE_API DSPBlockReverb : public NoteNagaDSPBlockBase {
public:
    DSPBlockReverb(float roomsize, float damping, float wet, float predelay);

    void process(float* left, float* right, size_t numFrames) override;

    std::vector<DSPParamDescriptor> getParamDescriptors() override;
    float getParamValue(size_t idx) const override;
    void setParamValue(size_t idx, float value) override;
    std::string getBlockName() const override { return "Reverb"; }

    void setSampleRate(float sr);

private:
    // Parameters
    float roomsize_ = 0.7f;   // 0.0 ... 1.0
    float damping_  = 0.5f;   // 0.0 ... 1.0
    float wet_      = 0.3f;   // 0.0 ... 1.0
    float predelay_ = 40.0f;  // ms

    // Internal state
    float sampleRate_ = 44100.0f;

    // Comb filters (4 per channel)
    struct CombFilter {
        std::vector<float> buf;
        size_t idx = 0;
        float feedback, damp, damp1, damp2;
        float filter_store = 0.0f;

        CombFilter() : feedback(0.0f), damp(0.0f), damp1(0.0f), damp2(0.0f) {}
        void resize(size_t len) { buf.resize(len, 0.0f); idx = 0; }
        float process(float inp);
    };

    // Allpass filters (2 per channel)
    struct AllpassFilter {
        std::vector<float> buf;
        size_t idx = 0;
        float feedback;

        AllpassFilter() : feedback(0.5f) {}
        void resize(size_t len) { buf.resize(len, 0.0f); idx = 0; }
        float process(float inp);
    };

    // Filters
    std::vector<CombFilter> combL_, combR_;
    std::vector<AllpassFilter> allpassL_, allpassR_;

    // Predelay buffer
    std::vector<float> predelayBufL_, predelayBufR_;
    size_t predelayIdx_ = 0;
    size_t predelayLen_ = 0;

    void updateFilters();
};