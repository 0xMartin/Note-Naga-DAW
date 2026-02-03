#include <note_naga_engine/dsp/dsp_block_ducker.h>
#include <cmath>
#include <algorithm>

DSPBlockDucker::DSPBlockDucker(float threshold, float ratio, float attack, float release, float depth)
    : threshold_(threshold), ratio_(ratio), attack_(attack), release_(release), depth_(depth)
{
}

void DSPBlockDucker::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    // Envelope follower coefficients
    float attackCoeff = std::exp(-1.0f / (sampleRate_ * attack_ * 0.001f));
    float releaseCoeff = std::exp(-1.0f / (sampleRate_ * release_ * 0.001f));
    
    // Convert threshold from dB to linear
    float threshLin = std::pow(10.0f, threshold_ / 20.0f);
    float maxReduction = std::pow(10.0f, -depth_ / 20.0f);
    
    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        // Peak detection
        float peak = std::max(std::abs(inL), std::abs(inR));
        
        // Envelope follower
        if (peak > envelope_) {
            envelope_ = attackCoeff * envelope_ + (1.0f - attackCoeff) * peak;
        } else {
            envelope_ = releaseCoeff * envelope_ + (1.0f - releaseCoeff) * peak;
        }
        
        // Calculate gain reduction
        float targetGain = 1.0f;
        if (envelope_ > threshLin) {
            // dB above threshold
            float dbOver = 20.0f * std::log10(envelope_ / threshLin);
            // Compressed dB
            float dbReduction = dbOver * (1.0f - 1.0f / ratio_);
            // Convert back to linear gain
            targetGain = std::pow(10.0f, -dbReduction / 20.0f);
            targetGain = std::max(targetGain, maxReduction);
        }
        
        // Smooth gain changes
        if (targetGain < gainReduction_) {
            gainReduction_ = attackCoeff * gainReduction_ + (1.0f - attackCoeff) * targetGain;
        } else {
            gainReduction_ = releaseCoeff * gainReduction_ + (1.0f - releaseCoeff) * targetGain;
        }
        
        // Apply gain
        left[i] = inL * gainReduction_;
        right[i] = inR * gainReduction_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockDucker::getParamDescriptors() {
    return {
        { "Threshold", DSPParamType::Float, DSControlType::Dial, -40.0f, 0.0f, -20.0f },
        { "Ratio", DSPParamType::Float, DSControlType::Dial, 1.0f, 20.0f, 8.0f },
        { "Attack", DSPParamType::Float, DSControlType::Dial, 0.1f, 100.0f, 5.0f },
        { "Release", DSPParamType::Float, DSControlType::Dial, 50.0f, 1000.0f, 200.0f },
        { "Depth", DSPParamType::Float, DSControlType::Dial, 0.0f, 40.0f, 20.0f }
    };
}

float DSPBlockDucker::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return threshold_;
        case 1: return ratio_;
        case 2: return attack_;
        case 3: return release_;
        case 4: return depth_;
        default: return 0.0f;
    }
}

void DSPBlockDucker::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: threshold_ = value; break;
        case 1: ratio_ = value; break;
        case 2: attack_ = value; break;
        case 3: release_ = value; break;
        case 4: depth_ = value; break;
    }
}
