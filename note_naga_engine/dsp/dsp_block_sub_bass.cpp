#include <note_naga_engine/dsp/dsp_block_sub_bass.h>
#include <cmath>
#include <algorithm>

DSPBlockSubBass::DSPBlockSubBass(float freq, float amount, float mix)
    : freq_(freq), amount_(amount), mix_(mix)
{
}

void DSPBlockSubBass::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    // Low-pass filter coefficient
    float lpCoeff = 1.0f - std::exp(-2.0f * 3.14159f * freq_ / sampleRate_);
    
    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        // Two-pole low-pass filter to isolate bass
        lpStateL1_ += lpCoeff * (inL - lpStateL1_);
        lpStateL2_ += lpCoeff * (lpStateL1_ - lpStateL2_);
        
        lpStateR1_ += lpCoeff * (inR - lpStateR1_);
        lpStateR2_ += lpCoeff * (lpStateR1_ - lpStateR2_);
        
        float bassL = lpStateL2_;
        float bassR = lpStateR2_;
        
        // Octave divider using zero-crossing detection
        // When the bass signal crosses zero, toggle the sub-harmonic phase
        if ((bassL >= 0.0f && lastSampleL_ < 0.0f) || (bassL < 0.0f && lastSampleL_ >= 0.0f)) {
            phaseL_ = 1.0f - phaseL_;
        }
        if ((bassR >= 0.0f && lastSampleR_ < 0.0f) || (bassR < 0.0f && lastSampleR_ >= 0.0f)) {
            phaseR_ = 1.0f - phaseR_;
        }
        
        lastSampleL_ = bassL;
        lastSampleR_ = bassR;
        
        // Generate sub-harmonic (square wave at half frequency, then filter)
        float subL = (phaseL_ > 0.5f ? 1.0f : -1.0f) * std::abs(bassL);
        float subR = (phaseR_ > 0.5f ? 1.0f : -1.0f) * std::abs(bassR);
        
        // Apply amount
        subL *= amount_;
        subR *= amount_;
        
        // Mix: add sub-bass to original
        left[i] = inL * (1.0f - mix_ * 0.3f) + subL * mix_;
        right[i] = inR * (1.0f - mix_ * 0.3f) + subR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockSubBass::getParamDescriptors() {
    return {
        { "Frequency", DSPParamType::Float, DSControlType::Dial, 40.0f, 120.0f, 80.0f },
        { "Amount", DSPParamType::Float, DSControlType::Dial, 0.0f, 1.0f, 0.5f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.5f }
    };
}

float DSPBlockSubBass::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return freq_;
        case 1: return amount_;
        case 2: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockSubBass::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: freq_ = value; break;
        case 1: amount_ = value; break;
        case 2: mix_ = value; break;
    }
}
