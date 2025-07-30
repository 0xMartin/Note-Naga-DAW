#include <note_naga_engine/dsp/dsp_block_limiter.h>

#include <cmath>

void DSPBlockLimiter::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    float threshold = dB_to_linear(threshold_db_);
    float makeup = dB_to_linear(makeup_db_);
    float release_coeff = expf(-1.0f / (release_ms_ * 0.001f * 44100.0f));

    for (size_t i = 0; i < numFrames; ++i) {
        float peak = std::max(std::fabs(left[i]), std::fabs(right[i]));
        float gain = 1.0f;
        if (peak > threshold) {
            gain = threshold / (peak + 1e-20f);
        }
        // Envelope follower for smooth release
        if (gain < gainSmooth_)
            gainSmooth_ = gain;
        else
            gainSmooth_ = gainSmooth_ * release_coeff + gain * (1.0f - release_coeff);

        left[i] *= gainSmooth_ * makeup;
        right[i] *= gainSmooth_ * makeup;
    }
}

std::vector<DSPParamDescriptor> DSPBlockLimiter::getParamDescriptors() {
    return {
        { "Threshold", DSPParamType::Float, DSControlType::SliderVertical, -40.0f, 0.0f, -5.0f },
        { "Release",   DSPParamType::Float, DSControlType::DialCentered, 5.0f, 200.0f, 50.0f },
        { "Makeup",    DSPParamType::Float, DSControlType::DialCentered, -12.0f, 12.0f, 0.0f }
    };
}

float DSPBlockLimiter::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return threshold_db_;
        case 1: return release_ms_;
        case 2: return makeup_db_;
        default: return 0.0f;
    }
}

void DSPBlockLimiter::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: threshold_db_ = value; break;
        case 1: release_ms_ = value; break;
        case 2: makeup_db_ = value; break;
    }
}