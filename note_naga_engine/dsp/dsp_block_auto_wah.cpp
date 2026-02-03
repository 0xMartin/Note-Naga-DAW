#include <note_naga_engine/dsp/dsp_block_auto_wah.h>
#include <cmath>
#include <algorithm>

DSPBlockAutoWah::DSPBlockAutoWah(float sensitivity, float minFreq, float maxFreq, float resonance, float mix)
    : sensitivity_(sensitivity), minFreq_(minFreq), maxFreq_(maxFreq), resonance_(resonance), mix_(mix)
{
}

void DSPBlockAutoWah::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    const float twoPi = 2.0f * 3.14159265359f;
    
    // Envelope follower coefficients
    float attackCoeff = std::exp(-1.0f / (sampleRate_ * 0.001f)); // 1ms attack
    float releaseCoeff = std::exp(-1.0f / (sampleRate_ * 0.1f));  // 100ms release
    
    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        // Envelope follower
        float absL = std::abs(inL);
        float absR = std::abs(inR);
        
        if (absL > envelopeL_) {
            envelopeL_ = attackCoeff * envelopeL_ + (1.0f - attackCoeff) * absL;
        } else {
            envelopeL_ = releaseCoeff * envelopeL_ + (1.0f - releaseCoeff) * absL;
        }
        
        if (absR > envelopeR_) {
            envelopeR_ = attackCoeff * envelopeR_ + (1.0f - attackCoeff) * absR;
        } else {
            envelopeR_ = releaseCoeff * envelopeR_ + (1.0f - releaseCoeff) * absR;
        }
        
        // Map envelope to filter frequency
        float envAmount = std::min(1.0f, (envelopeL_ + envelopeR_) * 0.5f * sensitivity_);
        float freq = minFreq_ + envAmount * (maxFreq_ - minFreq_);
        
        // State-variable filter coefficients
        float f = 2.0f * std::sin(twoPi * freq / sampleRate_ * 0.5f);
        float q = 1.0f / resonance_;
        
        // Process left channel (bandpass)
        float hpL = inL - lpL_ - q * bpL_;
        bpL_ += f * hpL;
        lpL_ += f * bpL_;
        float wahL = bpL_;
        
        // Process right channel
        float hpR = inR - lpR_ - q * bpR_;
        bpR_ += f * hpR;
        lpR_ += f * bpR_;
        float wahR = bpR_;
        
        // Mix
        left[i] = inL * (1.0f - mix_) + wahL * mix_;
        right[i] = inR * (1.0f - mix_) + wahR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockAutoWah::getParamDescriptors() {
    return {
        { "Sensitivity", DSPParamType::Float, DSControlType::Dial, 0.1f, 10.0f, 2.0f },
        { "Min Freq", DSPParamType::Float, DSControlType::Dial, 100.0f, 500.0f, 200.0f },
        { "Max Freq", DSPParamType::Float, DSControlType::Dial, 1000.0f, 5000.0f, 2000.0f },
        { "Resonance", DSPParamType::Float, DSControlType::Dial, 0.5f, 10.0f, 3.0f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.8f }
    };
}

float DSPBlockAutoWah::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return sensitivity_;
        case 1: return minFreq_;
        case 2: return maxFreq_;
        case 3: return resonance_;
        case 4: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockAutoWah::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: sensitivity_ = value; break;
        case 1: minFreq_ = value; break;
        case 2: maxFreq_ = value; break;
        case 3: resonance_ = value; break;
        case 4: mix_ = value; break;
    }
}
