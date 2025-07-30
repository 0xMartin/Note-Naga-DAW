#include <note_naga_engine/dsp/dsp_block_flanger.h>

#include <algorithm>
#include <cmath>

DSPBlockFlanger::DSPBlockFlanger(float speed, float depth, float feedback, float mix)
    : speed_(speed), depth_(depth), feedback_(feedback), mix_(mix)
{
    maxDelaySamples_ = static_cast<size_t>(sampleRate_ * 0.008f); // 8 ms max delay
    delayBufferL_.resize(maxDelaySamples_, 0.0f);
    delayBufferR_.resize(maxDelaySamples_, 0.0f);
}

void DSPBlockFlanger::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;

    for (size_t i = 0; i < numFrames; ++i) {
        // LFO phase
        float lfo = std::sin(lfoPhase_);
        lfoPhase_ += 2.0f * M_PI * speed_ / sampleRate_;
        if (lfoPhase_ > 2.0f * M_PI) lfoPhase_ -= 2.0f * M_PI;

        // Modulated delay (0.5 .. 0.5+depth ms)
        float delayMs = 0.5f + lfo * depth_;
        float delaySamples = delayMs * sampleRate_ / 1000.0f;

        // Clamp delaySamples
        if (delaySamples < 0.0f) delaySamples = 0.0f;
        if (delaySamples > (float)(maxDelaySamples_ - 2)) delaySamples = (float)(maxDelaySamples_ - 2);

        // Write input to delay buffer (with feedback)
        float inL = left[i] + feedback_ * delayBufferL_[delayIdx_];
        float inR = right[i] + feedback_ * delayBufferR_[delayIdx_];
        delayBufferL_[delayIdx_] = inL;
        delayBufferR_[delayIdx_] = inR;

        // Read delayed sample with linear interpolation
        float readPos = (float)delayIdx_ - delaySamples;
        if (readPos < 0.0f) readPos += (float)maxDelaySamples_;

        size_t idx0 = static_cast<size_t>(readPos) % maxDelaySamples_;
        size_t idx1 = (idx0 + 1) % maxDelaySamples_;
        float frac = readPos - std::floor(readPos);

        float flangedL = delayBufferL_[idx0] * (1.0f - frac) + delayBufferL_[idx1] * frac;
        float flangedR = delayBufferR_[idx0] * (1.0f - frac) + delayBufferR_[idx1] * frac;

        // Mix
        left[i]  = left[i]  * (1.0f - mix_) + flangedL * mix_;
        right[i] = right[i] * (1.0f - mix_) + flangedR * mix_;

        delayIdx_ = (delayIdx_ + 1) % maxDelaySamples_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockFlanger::getParamDescriptors() {
    return {
        { "Speed", DSPParamType::Float, DSControlType::Dial, 0.05f, 2.0f, 0.3f },
        { "Depth", DSPParamType::Float, DSControlType::Dial, 0.5f, 8.0f, 3.0f },
        { "Feedback", DSPParamType::Float, DSControlType::Dial, 0.0f, 0.95f, 0.3f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.5f }
    };
}

float DSPBlockFlanger::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return speed_;
        case 1: return depth_;
        case 2: return feedback_;
        case 3: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockFlanger::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: speed_ = value; break;
        case 1: depth_ = value; break;
        case 2: feedback_ = value; break;
        case 3: mix_ = value; break;
    }
}