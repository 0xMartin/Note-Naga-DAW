#include <note_naga_engine/dsp/dsp_block_vibrato.h>
#include <cmath>
#include <algorithm>

DSPBlockVibrato::DSPBlockVibrato(float speed, float depth, float mix)
    : speed_(speed), depth_(depth), mix_(mix)
{
    resizeBuffers();
}

void DSPBlockVibrato::setSampleRate(float sr) {
    sampleRate_ = sr;
    resizeBuffers();
}

void DSPBlockVibrato::resizeBuffers() {
    bufferSize_ = static_cast<size_t>(sampleRate_ * 0.05f); // 50ms buffer
    delayBufferL_.resize(bufferSize_, 0.0f);
    delayBufferR_.resize(bufferSize_, 0.0f);
    writeIdx_ = 0;
}

void DSPBlockVibrato::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    const float twoPi = 2.0f * 3.14159265359f;
    const float lfoInc = speed_ / sampleRate_;
    
    // Convert depth from cents to delay modulation (max ~5ms)
    const float maxDelayMs = 5.0f;
    const float delayModulation = (depth_ / 100.0f) * maxDelayMs * sampleRate_ / 1000.0f;
    const float baseDelay = delayModulation + 1.0f; // Center point
    
    for (size_t i = 0; i < numFrames; ++i) {
        // LFO
        float lfo = std::sin(twoPi * lfoPhase_);
        lfoPhase_ += lfoInc;
        if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
        
        // Variable delay time
        float delayTime = baseDelay + lfo * delayModulation;
        
        // Write to buffer
        delayBufferL_[writeIdx_] = left[i];
        delayBufferR_[writeIdx_] = right[i];
        
        // Calculate read position with linear interpolation
        float readPosF = static_cast<float>(writeIdx_) - delayTime;
        if (readPosF < 0.0f) readPosF += bufferSize_;
        
        size_t readIdx0 = static_cast<size_t>(readPosF) % bufferSize_;
        size_t readIdx1 = (readIdx0 + 1) % bufferSize_;
        float frac = readPosF - std::floor(readPosF);
        
        // Interpolated read
        float vibL = delayBufferL_[readIdx0] * (1.0f - frac) + delayBufferL_[readIdx1] * frac;
        float vibR = delayBufferR_[readIdx0] * (1.0f - frac) + delayBufferR_[readIdx1] * frac;
        
        // Mix
        left[i] = left[i] * (1.0f - mix_) + vibL * mix_;
        right[i] = right[i] * (1.0f - mix_) + vibR * mix_;
        
        writeIdx_ = (writeIdx_ + 1) % bufferSize_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockVibrato::getParamDescriptors() {
    return {
        { "Speed", DSPParamType::Float, DSControlType::Dial, 0.1f, 20.0f, 5.0f },
        { "Depth", DSPParamType::Float, DSControlType::Dial, 0.0f, 100.0f, 30.0f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 1.0f }
    };
}

float DSPBlockVibrato::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return speed_;
        case 1: return depth_;
        case 2: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockVibrato::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: speed_ = value; break;
        case 1: depth_ = value; break;
        case 2: mix_ = value; break;
    }
}
