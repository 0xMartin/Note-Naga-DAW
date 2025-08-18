#include <note_naga_engine/dsp/dsp_block_reverb.h>

#include <algorithm>

// Comb filter implementation
float DSPBlockReverb::CombFilter::process(float inp) {
    float y = buf[idx];
    filter_store = y * damp2 + filter_store * damp1;
    buf[idx] = inp + filter_store * feedback;
    if (++idx >= buf.size()) idx = 0;
    return y;
}

// Allpass filter implementation
float DSPBlockReverb::AllpassFilter::process(float inp) {
    float bufout = buf[idx];
    float y = -inp + bufout;
    buf[idx] = inp + bufout * feedback;
    if (++idx >= buf.size()) idx = 0;
    return y;
}

DSPBlockReverb::DSPBlockReverb(float roomsize, float damping, float wet, float predelay)
    : roomsize_(roomsize), damping_(damping), wet_(wet), predelay_(predelay) {
    setSampleRate(44100.0f);
    updateFilters();
}

void DSPBlockReverb::process(float *left, float *right, size_t numFrames) {
    if (!isActive()) return;

    for (size_t i = 0; i < numFrames; ++i) {
        // predelay
        predelayBufL_[predelayIdx_] = left[i];
        predelayBufR_[predelayIdx_] = right[i];
        float inL = predelayBufL_[(predelayIdx_ + 1) % predelayLen_];
        float inR = predelayBufR_[(predelayIdx_ + 1) % predelayLen_];
        if (++predelayIdx_ >= predelayLen_) predelayIdx_ = 0;

        // Process combs (parallel, sum)
        float outL = 0.0f, outR = 0.0f;
        for (size_t c = 0; c < combL_.size(); ++c)
            outL += combL_[c].process(inL);
        for (size_t c = 0; c < combR_.size(); ++c)
            outR += combR_[c].process(inR);

        // Process allpass (serial)
        for (size_t a = 0; a < allpassL_.size(); ++a)
            outL = allpassL_[a].process(outL);
        for (size_t a = 0; a < allpassR_.size(); ++a)
            outR = allpassR_[a].process(outR);

        // Mix dry/wet
        left[i] = left[i] * (1.0f - wet_) + outL * wet_ * 0.3f;
        right[i] = right[i] * (1.0f - wet_) + outR * wet_ * 0.3f;
    }
}

std::vector<DSPParamDescriptor> DSPBlockReverb::getParamDescriptors() {
    return {{"Room Size", DSPParamType::Float, DSControlType::DialCentered, 0.1f, 1.0f, 0.7f},
            {"Damping", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.5f},
            {"Wet", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.3f},
            {"Predelay", DSPParamType::Float, DSControlType::Dial, 0.0f, 100.0f, 40.0f}};
}

float DSPBlockReverb::getParamValue(size_t idx) const {
    switch (idx) {
    case 0:
        return roomsize_;
    case 1:
        return damping_;
    case 2:
        return wet_;
    case 3:
        return predelay_;
    default:
        return 0.0f;
    }
}

void DSPBlockReverb::setParamValue(size_t idx, float value) {
    switch (idx) {
    case 0:
        roomsize_ = value;
        updateFilters();
        break;
    case 1:
        damping_ = value;
        updateFilters();
        break;
    case 2:
        wet_ = value;
        break;
    case 3:
        predelay_ = value;
        updateFilters();
        break;
    }
}

void DSPBlockReverb::setSampleRate(float sr) {
    sampleRate_ = sr;
    updateFilters();
}

void DSPBlockReverb::updateFilters() {
    // Comb filter lengths (in samples, prime numbers for less resonance)
    static const size_t combLensL[4] = {1116, 1188, 1277, 1356};
    static const size_t combLensR[4] = {1139, 1211, 1300, 1387};
    static const size_t allpassLens[2] = {225, 556};

    // Calculate predelay length
    predelayLen_ = std::clamp<size_t>(predelay_ * 0.001f * sampleRate_, 1, sampleRate_);
    predelayBufL_.resize(predelayLen_, 0.0f);
    predelayBufR_.resize(predelayLen_, 0.0f);
    predelayIdx_ = 0;

    // Comb filters
    combL_.resize(4);
    combR_.resize(4);
    for (int i = 0; i < 4; ++i) {
        size_t lenL = static_cast<size_t>(combLensL[i] * roomsize_);
        size_t lenR = static_cast<size_t>(combLensR[i] * roomsize_);
        combL_[i].resize(std::max<size_t>(lenL, 1));
        combR_[i].resize(std::max<size_t>(lenR, 1));
        // Feedback and damping
        combL_[i].feedback = 0.7f + roomsize_ * 0.25f;
        combR_[i].feedback = 0.7f + roomsize_ * 0.25f;
        combL_[i].damp = damping_;
        combR_[i].damp = damping_;
        combL_[i].damp1 = damping_;
        combR_[i].damp1 = damping_;
        combL_[i].damp2 = 1.0f - damping_;
        combR_[i].damp2 = 1.0f - damping_;
    }

    // Allpass filters
    allpassL_.resize(2);
    allpassR_.resize(2);
    for (int i = 0; i < 2; ++i) {
        size_t len = allpassLens[i];
        allpassL_[i].resize(len);
        allpassR_[i].resize(len);
        allpassL_[i].feedback = 0.5f;
        allpassR_[i].feedback = 0.5f;
    }
}