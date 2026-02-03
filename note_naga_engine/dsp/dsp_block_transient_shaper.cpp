#include <note_naga_engine/dsp/dsp_block_transient_shaper.h>
#include <cmath>
#include <algorithm>

DSPBlockTransientShaper::DSPBlockTransientShaper(float attack, float sustain)
    : attack_(attack), sustain_(sustain)
{
}

void DSPBlockTransientShaper::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    // Envelope follower time constants
    float fastAttack = std::exp(-1.0f / (sampleRate_ * 0.001f));  // 1ms
    float fastRelease = std::exp(-1.0f / (sampleRate_ * 0.01f));  // 10ms
    float slowAttack = std::exp(-1.0f / (sampleRate_ * 0.02f));   // 20ms
    float slowRelease = std::exp(-1.0f / (sampleRate_ * 0.2f));   // 200ms
    
    // Convert percentage to multiplier (-100% = 0.0, 0% = 1.0, +100% = 2.0)
    float attackMult = 1.0f + attack_ / 100.0f;
    float sustainMult = 1.0f + sustain_ / 100.0f;
    
    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        float absL = std::abs(inL);
        float absR = std::abs(inR);
        float absSum = (absL + absR) * 0.5f;
        
        // Fast envelope (follows transients)
        if (absSum > fastEnvL_) {
            fastEnvL_ = fastAttack * fastEnvL_ + (1.0f - fastAttack) * absSum;
        } else {
            fastEnvL_ = fastRelease * fastEnvL_ + (1.0f - fastRelease) * absSum;
        }
        
        // Slow envelope (follows sustain)
        if (absSum > slowEnvL_) {
            slowEnvL_ = slowAttack * slowEnvL_ + (1.0f - slowAttack) * absSum;
        } else {
            slowEnvL_ = slowRelease * slowEnvL_ + (1.0f - slowRelease) * absSum;
        }
        
        // Transient is the difference between fast and slow
        float transient = std::max(0.0f, fastEnvL_ - slowEnvL_);
        float sustain = slowEnvL_;
        
        // Avoid division by zero
        float total = transient + sustain + 0.0001f;
        
        // Calculate gain
        float transientGain = transient * attackMult;
        float sustainGain = sustain * sustainMult;
        float newTotal = transientGain + sustainGain + 0.0001f;
        
        float gain = newTotal / total;
        gain = std::clamp(gain, 0.1f, 4.0f); // Limit gain range
        
        left[i] = inL * gain;
        right[i] = inR * gain;
    }
}

std::vector<DSPParamDescriptor> DSPBlockTransientShaper::getParamDescriptors() {
    return {
        { "Attack", DSPParamType::Float, DSControlType::DialCentered, -100.0f, 100.0f, 0.0f },
        { "Sustain", DSPParamType::Float, DSControlType::DialCentered, -100.0f, 100.0f, 0.0f }
    };
}

float DSPBlockTransientShaper::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return attack_;
        case 1: return sustain_;
        default: return 0.0f;
    }
}

void DSPBlockTransientShaper::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: attack_ = value; break;
        case 1: sustain_ = value; break;
    }
}
