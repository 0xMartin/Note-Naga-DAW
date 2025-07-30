#include <note_naga_engine/dsp/dsp_block_phaser.h>
#include <algorithm>
#include <cmath>

DSPBlockPhaser::DSPBlockPhaser(float speed, float depth, float feedback, float mix)
    : speed_(speed), depth_(depth), feedback_(feedback), mix_(mix)
{}

void DSPBlockPhaser::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;

    for (size_t i = 0; i < numFrames; ++i) {
        // LFO: sweep center freq between ~400Hz .. 1600Hz
        float lfo = std::sin(2.0f * M_PI * lfoPhase_);
        lfoPhase_ += speed_ / sampleRate_;
        if (lfoPhase_ > 1.0f) lfoPhase_ -= 1.0f;

        float minF = 400.0f, maxF = 1600.0f;
        float centerF = minF + (maxF - minF) * (depth_ * (lfo + 1.0f) * 0.5f);
        float omega = 2.0f * M_PI * centerF / sampleRate_;
        float a = (1.0f - std::sin(omega)) / std::cos(omega);

        // Left channel
        float xL = left[i] + prevOutL_ * feedback_;
        for (int st = 0; st < stages_; ++st) {
            float yL = -a * xL + zL_[st];
            zL_[st] = xL + a * yL;
            xL = yL;
        }
        prevOutL_ = xL;

        // Right channel
        float xR = right[i] + prevOutR_ * feedback_;
        for (int st = 0; st < stages_; ++st) {
            float yR = -a * xR + zR_[st];
            zR_[st] = xR + a * yR;
            xR = yR;
        }
        prevOutR_ = xR;

        // Mix
        left[i]  = left[i]  * (1.0f - mix_) + xL * mix_;
        right[i] = right[i] * (1.0f - mix_) + xR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockPhaser::getParamDescriptors() {
    return {
        { "Speed", DSPParamType::Float, DSControlType::Dial, 0.1f, 3.0f, 0.6f },
        { "Depth", DSPParamType::Float, DSControlType::Dial, 0.0f, 1.0f, 0.8f },
        { "Feedback", DSPParamType::Float, DSControlType::Dial, 0.0f, 0.95f, 0.4f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.5f }
    };
}

float DSPBlockPhaser::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return speed_;
        case 1: return depth_;
        case 2: return feedback_;
        case 3: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockPhaser::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: speed_ = value; break;
        case 1: depth_ = value; break;
        case 2: feedback_ = value; break;
        case 3: mix_ = value; break;
    }
}