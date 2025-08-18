#include <note_naga_engine/dsp/dsp_block_multi_eq.h>

DSPBlockMultiSimpleEQ::DSPBlockMultiSimpleEQ(const std::vector<float>& freqs, float q) {
    for (float f : freqs) {
        Band b;
        b.freq = f;
        b.gain = 0.0f;
        b.q = q;
        bands_.push_back(b);
    }
    for (size_t i = 0; i < bands_.size(); ++i)
        recalcCoeffs(int(i));
}

void DSPBlockMultiSimpleEQ::setSampleRate(float sr) {
    sampleRate_ = sr;
    for (size_t i = 0; i < bands_.size(); ++i)
        recalcCoeffs(int(i));
}

void DSPBlockMultiSimpleEQ::recalcCoeffs(int bandIdx) {
    auto& band = bands_[bandIdx];
    float A = std::pow(10.0f, band.gain / 40.0f);
    float omega = 2.0f * float(M_PI) * band.freq / sampleRate_;
    float sn = std::sin(omega);
    float cs = std::cos(omega);
    float alpha = sn / (2.0f * band.q);

    float b0 = 1 + alpha*A;
    float b1 = -2*cs;
    float b2 = 1 - alpha*A;
    float a0 = 1 + alpha/A;
    float a1 = -2*cs;
    float a2 = 1 - alpha/A;

    // Normalize!
    band.b0 = b0 / a0;
    band.b1 = b1 / a0;
    band.b2 = b2 / a0;
    band.a1 = a1 / a0;
    band.a2 = a2 / a0;
    band.a0 = 1.0f;

    // clear state to avoid clicks at param change
    band.x1l = band.x2l = band.y1l = band.y2l = 0.0f;
    band.x1r = band.x2r = band.y1r = band.y2r = 0.0f;
}

void DSPBlockMultiSimpleEQ::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;

    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];

        // Process all bands in series (cascaded)
        float outL = inL;
        float outR = inR;

        for (auto& band : bands_) {
            // Left
            float x0l = outL;
            float y0l = band.b0*x0l + band.b1*band.x1l + band.b2*band.x2l
                        - band.a1*band.y1l - band.a2*band.y2l;
            outL = y0l;
            band.x2l = band.x1l;
            band.x1l = x0l;
            band.y2l = band.y1l;
            band.y1l = y0l;

            // Right
            float x0r = outR;
            float y0r = band.b0*x0r + band.b1*band.x1r + band.b2*band.x2r
                        - band.a1*band.y1r - band.a2*band.y2r;
            outR = y0r;
            band.x2r = band.x1r;
            band.x1r = x0r;
            band.y2r = band.y1r;
            band.y1r = y0r;
        }

        left[i] = outL;
        right[i] = outR;
    }
}

std::vector<DSPParamDescriptor> DSPBlockMultiSimpleEQ::getParamDescriptors() {
    std::vector<DSPParamDescriptor> descs;
    for (size_t i = 0; i < bands_.size(); ++i) {
        char name[32];
        if (bands_[i].freq >= 1000.0f)
            snprintf(name, sizeof(name), "%.0f kHz", bands_[i].freq / 1000.0f);
        else
            snprintf(name, sizeof(name), "%.0f Hz", bands_[i].freq);
        descs.push_back({ name, DSPParamType::Float, DSControlType::SliderVertical, -10.0f, 10.0f, bands_[i].gain });
    }
    return descs;
}

float DSPBlockMultiSimpleEQ::getParamValue(size_t idx) const {
    if (idx < bands_.size())
        return bands_[idx].gain;
    return 0.0f;
}

void DSPBlockMultiSimpleEQ::setParamValue(size_t idx, float value) {
    if (idx < bands_.size()) {
        bands_[idx].gain = value;
        recalcCoeffs(int(idx));
    }
}