#include <note_naga_engine/dsp/dsp_block_delay.h>

#include <algorithm>

DSPBlockDelay::DSPBlockDelay(float time_ms, float feedback, float mix)
    : time_ms_(time_ms), feedback_(feedback), mix_(mix)
{
    setSampleRate(44100.0f); // default
}

void DSPBlockDelay::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    const float sampleRate = sampleRate_;
    size_t delaySamples = static_cast<size_t>(std::clamp(time_ms_ * 0.001f * sampleRate, 1.0f, (float)(maxDelaySamples_)));

    for (size_t i = 0; i < numFrames; ++i) {
        // Left channel
        float delayedL = delayBufferL_[delayIdx_];
        float inL = left[i];
        float outL = inL * (1.0f - mix_) + delayedL * mix_;
        delayBufferL_[delayIdx_] = inL + delayedL * feedback_;
        left[i] = outL;

        // Right channel
        float delayedR = delayBufferR_[delayIdx_];
        float inR = right[i];
        float outR = inR * (1.0f - mix_) + delayedR * mix_;
        delayBufferR_[delayIdx_] = inR + delayedR * feedback_;
        right[i] = outR;

        // Increment and wrap delay index
        delayIdx_ = (delayIdx_ + 1) % delaySamples;
    }
}

std::vector<DSPParamDescriptor> DSPBlockDelay::getParamDescriptors() {
    return {
        { "Time", DSPParamType::Float, DSControlType::Dial, 10.0f, 1000.0f, 400.0f },
        { "Feedback", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 0.95f, 0.25f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.5f }
    };
}

float DSPBlockDelay::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return time_ms_;
        case 1: return feedback_;
        case 2: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockDelay::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: 
            time_ms_ = value; 
            resizeDelayBuffers();
            break;
        case 1: feedback_ = value; break;
        case 2: mix_ = value; break;
    }
}

void DSPBlockDelay::setSampleRate(float sr) {
    sampleRate_ = sr;
    resizeDelayBuffers();
}

void DSPBlockDelay::resizeDelayBuffers() {
    size_t samples = static_cast<size_t>(std::clamp(time_ms_ * 0.001f * sampleRate_, 1.0f, 2.0f * sampleRate_));
    maxDelaySamples_ = samples;
    delayBufferL_.resize(maxDelaySamples_, 0.0f);
    delayBufferR_.resize(maxDelaySamples_, 0.0f);
    delayIdx_ = 0;
}