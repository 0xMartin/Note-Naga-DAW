#include <note_naga_engine/dsp/dsp_block_single_eq.h>

DSPBlockSingleEQ::DSPBlockSingleEQ(float freq, float gain, float q)
    : freq_(freq), gain_(gain), q_(q)
{
    recalcCoeffs();
}

void DSPBlockSingleEQ::setSampleRate(float sr) {
    sampleRate_ = sr;
    recalcCoeffs();
}

void DSPBlockSingleEQ::recalcCoeffs() {
    // RBJ peak EQ
    float A = powf(10.0f, gain_ / 40.0f);
    float omega = 2.0f * float(M_PI) * freq_ / sampleRate_;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * q_);

    float b0 = 1 + alpha*A;
    float b1 = -2*cs;
    float b2 = 1 - alpha*A;
    float a0 = 1 + alpha/A;
    float a1 = -2*cs;
    float a2 = 1 - alpha/A;

    // Normalize!
    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
    a0_ = 1.0f; // always 1

    // clear state to avoid clicks at param change
    x1l_ = x2l_ = y1l_ = y2l_ = 0.0f;
    x1r_ = x2r_ = y1r_ = y2r_ = 0.0f;
}

void DSPBlockSingleEQ::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;

    // coefficients should only be recalculated when param changes
    // -> recalcCoeffs() volat jen v setParamValue!
    // recalcCoeffs();

    for (size_t i = 0; i < numFrames; ++i) {
        // Left
        float x0l = left[i];
        float y0l = b0_*x0l + b1_*x1l_ + b2_*x2l_ - a1_*y1l_ - a2_*y2l_;
        left[i] = y0l;
        x2l_ = x1l_;
        x1l_ = x0l;
        y2l_ = y1l_;
        y1l_ = y0l;

        // Right
        float x0r = right[i];
        float y0r = b0_*x0r + b1_*x1r_ + b2_*x2r_ - a1_*y1r_ - a2_*y2r_;
        right[i] = y0r;
        x2r_ = x1r_;
        x1r_ = x0r;
        y2r_ = y1r_;
        y1r_ = y0r;
    }
}

std::vector<DSPParamDescriptor> DSPBlockSingleEQ::getParamDescriptors() {
    return {
        { "Freq", DSPParamType::Float, DSControlType::Dial, 20.0f, 20000.0f, 1000.0f },
        { "Gain", DSPParamType::Float, DSControlType::SliderVertical, -24.0f, 24.0f, 0.0f },
        { "Q",    DSPParamType::Float, DSControlType::Dial, 0.1f, 10.0f, 1.0f }
    };
}

float DSPBlockSingleEQ::getParamValue(size_t idx) const {
    if (idx == 0) return freq_;
    if (idx == 1) return gain_;
    if (idx == 2) return q_;
    return 0.0f;
}

void DSPBlockSingleEQ::setParamValue(size_t idx, float value) {
    if (idx == 0) freq_ = value;
    if (idx == 1) gain_ = value;
    if (idx == 2) q_ = value;
    recalcCoeffs();
}