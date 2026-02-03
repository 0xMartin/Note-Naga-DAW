#include <note_naga_engine/dsp/dsp_block_tape_saturation.h>
#include <cmath>
#include <algorithm>

DSPBlockTapeSaturation::DSPBlockTapeSaturation(float drive, float saturation, float warmth, float mix)
    : drive_(drive), saturation_(saturation), warmth_(warmth), mix_(mix)
{
}

void DSPBlockTapeSaturation::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    // Warmth low-pass filter coefficient (reduces high frequencies)
    float lpCoeff = 1.0f - std::exp(-2.0f * 3.14159f * (20000.0f - warmth_ * 15000.0f) / sampleRate_);
    
    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        // Apply drive
        float drivenL = inL * drive_;
        float drivenR = inR * drive_;
        
        // Tape-like saturation with hysteresis simulation
        // Using asymmetric soft-clipping with bias
        float biasAmount = saturation_ * 0.1f;
        
        // Update bias slowly (simulates tape magnetization)
        biasL_ = biasL_ * 0.9999f + drivenL * 0.0001f * biasAmount;
        biasR_ = biasR_ * 0.9999f + drivenR * 0.0001f * biasAmount;
        
        // Add bias
        drivenL += biasL_;
        drivenR += biasR_;
        
        // Soft saturation using sinh/tanh combination
        float satL, satR;
        float satAmount = saturation_ * 2.0f + 1.0f;
        
        // Tape-like transfer function (asymmetric)
        satL = std::tanh(drivenL * satAmount) / satAmount;
        satR = std::tanh(drivenR * satAmount) / satAmount;
        
        // Add subtle even harmonics (tape characteristic)
        float evenL = satL * satL * std::copysign(1.0f, inL) * saturation_ * 0.1f;
        float evenR = satR * satR * std::copysign(1.0f, inR) * saturation_ * 0.1f;
        
        satL += evenL;
        satR += evenR;
        
        // Apply warmth (low-pass filter)
        lpStateL_ += lpCoeff * (satL - lpStateL_);
        lpStateR_ += lpCoeff * (satR - lpStateR_);
        
        float warmL = lpStateL_ * warmth_ + satL * (1.0f - warmth_);
        float warmR = lpStateR_ * warmth_ + satR * (1.0f - warmth_);
        
        // Normalize output
        float normL = warmL / std::max(1.0f, drive_ * 0.5f);
        float normR = warmR / std::max(1.0f, drive_ * 0.5f);
        
        // Mix
        left[i] = inL * (1.0f - mix_) + normL * mix_;
        right[i] = inR * (1.0f - mix_) + normR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockTapeSaturation::getParamDescriptors() {
    return {
        { "Drive", DSPParamType::Float, DSControlType::Dial, 0.0f, 10.0f, 2.0f },
        { "Saturation", DSPParamType::Float, DSControlType::Dial, 0.0f, 1.0f, 0.5f },
        { "Warmth", DSPParamType::Float, DSControlType::Dial, 0.0f, 1.0f, 0.5f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.8f }
    };
}

float DSPBlockTapeSaturation::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return drive_;
        case 1: return saturation_;
        case 2: return warmth_;
        case 3: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockTapeSaturation::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: drive_ = value; break;
        case 1: saturation_ = value; break;
        case 2: warmth_ = value; break;
        case 3: mix_ = value; break;
    }
}
