#include <note_naga_engine/dsp/dsp_block_chorus.h>

#include <cmath>

DSPBlockChorus::DSPBlockChorus(float speed, float depth, float mix)
    : speed_(speed), depth_(depth), mix_(mix)
{
    // Max delay for chorus typicky 25 ms
    maxDelaySamples_ = static_cast<size_t>(sampleRate_ * 0.025f); // 25 ms max
    delayBufferL_.resize(maxDelaySamples_, 0.0f);
    delayBufferR_.resize(maxDelaySamples_, 0.0f);
}

void DSPBlockChorus::prepareDelayBuffer(size_t numFrames) {
    // Ensure buffer is large enough
    if (delayBufferL_.size() < maxDelaySamples_ + numFrames)
        delayBufferL_.resize(maxDelaySamples_ + numFrames, 0.0f);
    if (delayBufferR_.size() < maxDelaySamples_ + numFrames)
        delayBufferR_.resize(maxDelaySamples_ + numFrames, 0.0f);
}

void DSPBlockChorus::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;

    prepareDelayBuffer(numFrames);

    for (size_t i = 0; i < numFrames; ++i) {
        // LFO phase (correct increment for frequency)
        float lfo = std::sin(lfoPhase_);
        lfoPhase_ += 2.0f * M_PI * speed_ / sampleRate_;
        if (lfoPhase_ > 2.0f * M_PI) lfoPhase_ -= 2.0f * M_PI;

        // Modulated delay (10 .. 10+depth ms)
        float delayMs = 10.0f + lfo * depth_; // min 10ms, max (10+depth) ms
        float delaySamples = delayMs * sampleRate_ / 1000.0f;

        // Clamp delaySamples to valid range for safety
        if (delaySamples < 0.0f) delaySamples = 0.0f;
        if (delaySamples > (float)(maxDelaySamples_ - 2)) delaySamples = (float)(maxDelaySamples_ - 2);

        // Write input to delay buffer
        delayBufferL_[delayIdx_] = left[i];
        delayBufferR_[delayIdx_] = right[i];

        // Read delayed sample with linear interpolation
        float readPos = (float)delayIdx_ - delaySamples;
        if (readPos < 0.0f) readPos += (float)maxDelaySamples_;

        size_t idx0 = static_cast<size_t>(readPos) % maxDelaySamples_;
        size_t idx1 = (idx0 + 1) % maxDelaySamples_;
        float frac = readPos - std::floor(readPos);

        float chorusL = delayBufferL_[idx0] * (1.0f - frac) + delayBufferL_[idx1] * frac;
        float chorusR = delayBufferR_[idx0] * (1.0f - frac) + delayBufferR_[idx1] * frac;

        // Mix
        left[i]  = left[i]  * (1.0f - mix_) + chorusL * mix_;
        right[i] = right[i] * (1.0f - mix_) + chorusR * mix_;

        delayIdx_ = (delayIdx_ + 1) % maxDelaySamples_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockChorus::getParamDescriptors() {
    return {
        { "Speed", DSPParamType::Float, DSControlType::Dial, 0.2f, 5.0f, 1.2f },
        { "Depth", DSPParamType::Float, DSControlType::Dial, 4.0f, 16.0f, 6.0f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.25f }
    };
}

float DSPBlockChorus::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return speed_;
        case 1: return depth_;
        case 2: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockChorus::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: speed_ = value; break;
        case 1: depth_ = value; break;
        case 2: mix_ = value; break;
    }
}