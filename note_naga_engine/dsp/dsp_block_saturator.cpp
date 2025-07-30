#include <note_naga_engine/dsp/dsp_block_saturator.h>
#include <algorithm>
#include <cmath>

DSPBlockSaturator::DSPBlockSaturator(float drive, float mix)
    : drive_(drive), mix_(mix)
{}

inline float saturate(float x, float drive) {
    // Soft clipping: tanh drive
    return std::tanh(x * drive);
}

void DSPBlockSaturator::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    for (size_t i = 0; i < numFrames; ++i) {
        float satL = saturate(left[i], drive_);
        float satR = saturate(right[i], drive_);
        left[i]  = left[i]  * (1.0f - mix_) + satL * mix_;
        right[i] = right[i] * (1.0f - mix_) + satR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockSaturator::getParamDescriptors() {
    return {
        { "Drive", DSPParamType::Float, DSControlType::Dial, 1.0f, 10.0f, 2.0f },
        { "Mix",   DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.7f }
    };
}

float DSPBlockSaturator::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return drive_;
        case 1: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockSaturator::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: drive_ = value; break;
        case 1: mix_ = value; break;
    }
}