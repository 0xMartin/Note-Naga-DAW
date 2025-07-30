#include <note_naga_engine/dsp/dsp_block_tremolo.h>

#include <cmath>

DSPBlockTremolo::DSPBlockTremolo(float speed, float depth, float mix)
    : speed_(speed), depth_(depth), mix_(mix) {}

void DSPBlockTremolo::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    float phaseInc = 2.0f * M_PI * speed_ / sampleRate_;
    for (size_t i = 0; i < numFrames; ++i) {
        float lfo = (1.0f + std::sin(phase_)) * 0.5f; // 0..1
        float gain = 1.0f - depth_ + lfo * depth_;
        float dryGain = 1.0f - mix_;
        left[i]  = left[i] * dryGain + left[i] * gain * mix_;
        right[i] = right[i] * dryGain + right[i] * gain * mix_;
        phase_ += phaseInc;
        if (phase_ > 2.0f * M_PI) phase_ -= 2.0f * M_PI;
    }
}

std::vector<DSPParamDescriptor> DSPBlockTremolo::getParamDescriptors() {
    return {
        { "Speed", DSPParamType::Float, DSControlType::Dial, 0.1f, 20.0f, 5.0f },
        { "Depth", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.8f },
        { "Mix",   DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 1.0f }
    };
}

float DSPBlockTremolo::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return speed_;
        case 1: return depth_;
        case 2: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockTremolo::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: speed_ = value; break;
        case 1: depth_ = value; break;
        case 2: mix_ = value; break;
    }
}

void DSPBlockTremolo::setSampleRate(float sr) {
    sampleRate_ = sr;
}