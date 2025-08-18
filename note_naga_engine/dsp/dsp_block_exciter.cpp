#include <note_naga_engine/dsp/dsp_block_exciter.h>

#include <cmath>

DSPBlockExciter::DSPBlockExciter(float freq, float drive, float mix)
    : freq_(freq), drive_(drive), mix_(mix) {}

inline float saturate(float x, float drive) {
    // Soft clipping: tanh drive
    return std::tanh(x * drive);
}

void DSPBlockExciter::process(float *left, float *right, size_t numFrames) {
    if (!isActive()) return;
    float alpha = freq_ / (freq_ + sampleRate_); // HP filter coeff

    for (size_t i = 0; i < numFrames; ++i) {
        // Simple HP filter for highs
        hpL_ = alpha * (hpL_ + left[i] - left[i]);
        hpR_ = alpha * (hpR_ + right[i] - right[i]);
        float highL = left[i] - hpL_;
        float highR = right[i] - hpR_;

        // Excite: saturate highs
        float excL = saturate(highL, drive_);
        float excR = saturate(highR, drive_);

        // Mix
        left[i] = left[i] * (1.0f - mix_) + excL * mix_;
        right[i] = right[i] * (1.0f - mix_) + excR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockExciter::getParamDescriptors() {
    return {{"Freq", DSPParamType::Float, DSControlType::Dial, 1000.0f, 12000.0f, 4000.0f},
            {"Drive", DSPParamType::Float, DSControlType::Dial, 1.0f, 10.0f, 4.0f},
            {"Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.6f}};
}

float DSPBlockExciter::getParamValue(size_t idx) const {
    switch (idx) {
    case 0:
        return freq_;
    case 1:
        return drive_;
    case 2:
        return mix_;
    default:
        return 0.0f;
    }
}

void DSPBlockExciter::setParamValue(size_t idx, float value) {
    switch (idx) {
    case 0:
        freq_ = value;
        break;
    case 1:
        drive_ = value;
        break;
    case 2:
        mix_ = value;
        break;
    }
}