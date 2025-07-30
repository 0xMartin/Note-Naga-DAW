#include <note_naga_engine/dsp/dsp_block_compressor.h>

#include <cmath>

void DSPBlockCompressor::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    float makeup = dB_to_linear(makeup_db_);
    float attack_coeff = expf(-1.0f / (attack_ms_ * 0.001f * 44100.0f));
    float release_coeff = expf(-1.0f / (release_ms_ * 0.001f * 44100.0f));

    for (size_t i = 0; i < numFrames; ++i) {
        float rms = sqrtf(0.5f * (left[i]*left[i] + right[i]*right[i]) + 1e-12f);
        float input_db = 20.0f * log10(rms + 1e-20f);

        float gain_db = 0.0f;
        if (input_db > threshold_db_) {
            float over_db = input_db - threshold_db_;
            gain_db = -over_db + over_db / ratio_; // správně: zeslab nad threshold
        }
        float gain = dB_to_linear(gain_db);

        // Envelope follower (attack/release)
        if (gain < gainSmooth_)
            gainSmooth_ = gainSmooth_ * attack_coeff + gain * (1.0f - attack_coeff);
        else
            gainSmooth_ = gainSmooth_ * release_coeff + gain * (1.0f - release_coeff);

        left[i] *= gainSmooth_ * makeup;
        right[i] *= gainSmooth_ * makeup;
    }
}

std::vector<DSPParamDescriptor> DSPBlockCompressor::getParamDescriptors() {
    return {
        { "Threshold", DSPParamType::Float, DSControlType::SliderVertical, -50.0f, 0.0f, -18.0f },
        { "Ratio",     DSPParamType::Float, DSControlType::Dial, 1.0f, 20.0f, 4.0f },
        { "Attack",    DSPParamType::Float, DSControlType::Dial, 0.5f, 100.0f, 10.0f },
        { "Release",   DSPParamType::Float, DSControlType::DialCentered, 5.0f, 500.0f, 80.0f },
        { "Makeup",    DSPParamType::Float, DSControlType::DialCentered, -12.0f, 12.0f, 0.0f }
    };
}
float DSPBlockCompressor::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return threshold_db_;
        case 1: return ratio_;
        case 2: return attack_ms_;
        case 3: return release_ms_;
        case 4: return makeup_db_;
        default: return 0.0f;
    }
}
void DSPBlockCompressor::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: threshold_db_ = value; break;
        case 1: ratio_ = value; break;
        case 2: attack_ms_ = value; break;
        case 3: release_ms_ = value; break;
        case 4: makeup_db_ = value; break;
    }
}