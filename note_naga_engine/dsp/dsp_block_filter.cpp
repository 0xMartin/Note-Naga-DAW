#include <note_naga_engine/dsp/dsp_block_filter.h>

#include <algorithm>
#include <cmath>

DSPBlockFilter::DSPBlockFilter(FilterType type, float cutoff, float resonance, float mix)
    : type_(type), cutoff_(cutoff), resonance_(resonance), mix_(mix) {
    setSampleRate(44100.0f);
    calcCoeffs();
}

void DSPBlockFilter::process(float *left, float *right, size_t numFrames) {
    if (!isActive()) return;
    for (size_t i = 0; i < numFrames; ++i) {
        // Left
        float inL = left[i];
        float outL = a0_ * inL + a1_ * z1L_ + a2_ * z2L_ - b1_ * z1L_ - b2_ * z2L_;
        z2L_ = z1L_;
        z1L_ = outL;
        left[i] = inL * (1.0f - mix_) + outL * mix_;

        // Right
        float inR = right[i];
        float outR = a0_ * inR + a1_ * z1R_ + a2_ * z2R_ - b1_ * z1R_ - b2_ * z2R_;
        z2R_ = z1R_;
        z1R_ = outR;
        right[i] = inR * (1.0f - mix_) + outR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockFilter::getParamDescriptors() {
    return {{"Type",
             DSPParamType::Int,
             DSControlType::Dial,
             0.0f,
             2.0f,
             0.0f,
             {"Lowpass", "Highpass", "Bandpass"}}, // 0=LP, 1=HP, 2=BP
            {"Cutoff", DSPParamType::Float, DSControlType::Dial, 20.0f, 18000.0f, 800.0f},
            {"Resonance", DSPParamType::Float, DSControlType::DialCentered, 0.1f, 2.0f, 0.7f},
            {"Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 1.0f}};
}

float DSPBlockFilter::getParamValue(size_t idx) const {
    switch (idx) {
    case 0:
        return static_cast<float>(type_);
    case 1:
        return cutoff_;
    case 2:
        return resonance_;
    case 3:
        return mix_;
    default:
        return 0.0f;
    }
}

void DSPBlockFilter::setParamValue(size_t idx, float value) {
    switch (idx) {
    case 0:
        type_ = static_cast<FilterType>(static_cast<int>(value));
        calcCoeffs();
        break;
    case 1:
        cutoff_ = value;
        calcCoeffs();
        break;
    case 2:
        resonance_ = value;
        calcCoeffs();
        break;
    case 3:
        mix_ = value;
        break;
    }
}

void DSPBlockFilter::setSampleRate(float sr) {
    sampleRate_ = sr;
    calcCoeffs();
}

// Biquad filter design (TPT, simple RBJ formula)
void DSPBlockFilter::calcCoeffs() {
    float freq = std::clamp(cutoff_, 20.0f, sampleRate_ * 0.45f);
    float Q = std::clamp(resonance_, 0.1f, 2.0f);
    float omega = 2.0f * M_PI * freq / sampleRate_;
    float sn = std::sin(omega);
    float cs = std::cos(omega);
    float alpha = sn / (2.0f * Q);

    switch (type_) {
    case FilterType::Lowpass: {
        float norm = 1.0f / (1.0f + alpha);
        a0_ = (1.0f - cs) * 0.5f * norm;
        a1_ = (1.0f - cs) * norm;
        a2_ = a0_;
        b1_ = -2.0f * cs * norm;
        b2_ = (1.0f - alpha) * norm;
        break;
    }
    case FilterType::Highpass: {
        float norm = 1.0f / (1.0f + alpha);
        a0_ = (1.0f + cs) * 0.5f * norm;
        a1_ = -(1.0f + cs) * norm;
        a2_ = a0_;
        b1_ = -2.0f * cs * norm;
        b2_ = (1.0f - alpha) * norm;
        break;
    }
    case FilterType::Bandpass: {
        float norm = 1.0f / (1.0f + alpha);
        a0_ = alpha * norm;
        a1_ = 0.0f;
        a2_ = -alpha * norm;
        b1_ = -2.0f * cs * norm;
        b2_ = (1.0f - alpha) * norm;
        break;
    }
    }
    // Clear state after parameter change
    z1L_ = z2L_ = z1R_ = z2R_ = 0.0f;
}