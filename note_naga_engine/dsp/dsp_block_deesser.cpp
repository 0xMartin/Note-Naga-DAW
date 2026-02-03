#include <note_naga_engine/dsp/dsp_block_deesser.h>
#include <cmath>
#include <algorithm>

DSPBlockDeEsser::DSPBlockDeEsser(float freq, float threshold, float reduction)
    : freq_(freq), threshold_(threshold), reduction_(reduction)
{
}

void DSPBlockDeEsser::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    // High-pass filter coefficient for sibilance detection
    float hpCoeff = std::exp(-2.0f * 3.14159f * freq_ / sampleRate_);
    
    // Envelope follower coefficients
    float attackCoeff = std::exp(-1.0f / (sampleRate_ * 0.0005f)); // 0.5ms attack
    float releaseCoeff = std::exp(-1.0f / (sampleRate_ * 0.05f));  // 50ms release
    
    // Convert threshold from dB
    float threshLin = std::pow(10.0f, threshold_ / 20.0f);
    float maxReduction = std::pow(10.0f, -reduction_ / 20.0f);
    
    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        // High-pass filter for sibilance detection
        float hpL = inL - hpCoeff * hpStateL_;
        hpStateL_ = inL;
        
        float hpR = inR - hpCoeff * hpStateR_;
        hpStateR_ = inR;
        
        // Envelope follower on high-passed signal
        float absL = std::abs(hpL);
        float absR = std::abs(hpR);
        
        if (absL > envL_) {
            envL_ = attackCoeff * envL_ + (1.0f - attackCoeff) * absL;
        } else {
            envL_ = releaseCoeff * envL_ + (1.0f - releaseCoeff) * absL;
        }
        
        if (absR > envR_) {
            envR_ = attackCoeff * envR_ + (1.0f - attackCoeff) * absR;
        } else {
            envR_ = releaseCoeff * envR_ + (1.0f - releaseCoeff) * absR;
        }
        
        // Calculate gain reduction
        float gainL = 1.0f;
        float gainR = 1.0f;
        
        if (envL_ > threshLin) {
            float overThresh = envL_ / threshLin;
            gainL = 1.0f / std::max(1.0f, overThresh);
            gainL = std::max(gainL, maxReduction);
        }
        
        if (envR_ > threshLin) {
            float overThresh = envR_ / threshLin;
            gainR = 1.0f / std::max(1.0f, overThresh);
            gainR = std::max(gainR, maxReduction);
        }
        
        // Apply gain reduction only to high frequencies (blend approach)
        // Mix reduced HF with original LF
        float lfL = inL - hpL;
        float lfR = inR - hpR;
        
        left[i] = lfL + hpL * gainL;
        right[i] = lfR + hpR * gainR;
    }
}

std::vector<DSPParamDescriptor> DSPBlockDeEsser::getParamDescriptors() {
    return {
        { "Frequency", DSPParamType::Float, DSControlType::Dial, 4000.0f, 10000.0f, 6000.0f },
        { "Threshold", DSPParamType::Float, DSControlType::Dial, -40.0f, 0.0f, -20.0f },
        { "Reduction", DSPParamType::Float, DSControlType::Dial, 0.0f, 12.0f, 6.0f }
    };
}

float DSPBlockDeEsser::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return freq_;
        case 1: return threshold_;
        case 2: return reduction_;
        default: return 0.0f;
    }
}

void DSPBlockDeEsser::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: freq_ = value; break;
        case 1: threshold_ = value; break;
        case 2: reduction_ = value; break;
    }
}
