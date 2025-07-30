#include <note_naga_engine/dsp/dsp_block_noise_gate.h>

#include <algorithm>
#include <cmath>

DSPBlockNoiseGate::DSPBlockNoiseGate(float threshold, float attack, float release)
    : threshold_(threshold), attack_(attack), release_(release)
{}

void DSPBlockNoiseGate::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    // Convert threshold to linear
    float threshLinear = std::pow(10.0f, threshold_ / 20.0f);
    float attackCoef = std::exp(-1.0f / (attack_ * 0.001f * sampleRate_));
    float releaseCoef = std::exp(-1.0f / (release_ * 0.001f * sampleRate_));

    for (size_t i = 0; i < numFrames; ++i) {
        float in = 0.5f * (std::fabs(left[i]) + std::fabs(right[i]));
        float target = (in > threshLinear) ? 1.0f : 0.0f;
        if (target > gain_)
            gain_ = attackCoef * gain_ + (1.0f - attackCoef) * target;
        else
            gain_ = releaseCoef * gain_ + (1.0f - releaseCoef) * target;

        left[i] *= gain_;
        right[i] *= gain_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockNoiseGate::getParamDescriptors() {
    return {
        { "Threshold", DSPParamType::Float, DSControlType::Dial, -60.0f, 0.0f, -40.0f },
        { "Attack", DSPParamType::Float, DSControlType::Dial, 1.0f, 50.0f, 5.0f },
        { "Release", DSPParamType::Float, DSControlType::Dial, 10.0f, 500.0f, 80.0f }
    };
}

float DSPBlockNoiseGate::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return threshold_;
        case 1: return attack_;
        case 2: return release_;
        default: return 0.0f;
    }
}

void DSPBlockNoiseGate::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: threshold_ = value; break;
        case 1: attack_ = value; break;
        case 2: release_ = value; break;
    }
}