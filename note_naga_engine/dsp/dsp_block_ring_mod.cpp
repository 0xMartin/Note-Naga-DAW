#include <note_naga_engine/dsp/dsp_block_ring_mod.h>
#include <cmath>

DSPBlockRingMod::DSPBlockRingMod(float freq, float mix)
    : freq_(freq), mix_(mix)
{
}

void DSPBlockRingMod::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    const float twoPi = 2.0f * 3.14159265359f;
    const float phaseInc = freq_ / sampleRate_;
    
    for (size_t i = 0; i < numFrames; ++i) {
        // Generate carrier signal
        float carrier = std::sin(twoPi * phase_);
        phase_ += phaseInc;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        
        float inL = left[i];
        float inR = right[i];
        
        // Ring modulation (multiply)
        float modL = inL * carrier;
        float modR = inR * carrier;
        
        // Mix
        left[i] = inL * (1.0f - mix_) + modL * mix_;
        right[i] = inR * (1.0f - mix_) + modR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockRingMod::getParamDescriptors() {
    return {
        { "Frequency", DSPParamType::Float, DSControlType::Dial, 20.0f, 2000.0f, 440.0f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.5f }
    };
}

float DSPBlockRingMod::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return freq_;
        case 1: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockRingMod::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: freq_ = value; break;
        case 1: mix_ = value; break;
    }
}
