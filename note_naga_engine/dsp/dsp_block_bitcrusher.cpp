#include <note_naga_engine/dsp/dsp_block_bitcrusher.h>

#include <cmath>
#include <algorithm>

DSPBlockBitcrusher::DSPBlockBitcrusher(float bitDepth, int sampleRateReduce, float mix)
    : bitDepth_(bitDepth), sampleRateReduce_(sampleRateReduce), mix_(mix)
{}

void DSPBlockBitcrusher::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;

    float levels = std::pow(2.0f, bitDepth_);
    for (size_t i = 0; i < numFrames; ++i) {
        if (step_ == 0) {
            // Quantize
            lastL_ = std::round(left[i] * levels) / levels;
            lastR_ = std::round(right[i] * levels) / levels;
        }
        left[i]  = left[i]  * (1.0f - mix_) + lastL_ * mix_;
        right[i] = right[i] * (1.0f - mix_) + lastR_ * mix_;

        step_ = (step_ + 1) % std::max(1, sampleRateReduce_);
    }
}

std::vector<DSPParamDescriptor> DSPBlockBitcrusher::getParamDescriptors() {
    return {
        { "Bit Depth", DSPParamType::Float, DSControlType::Dial, 4.0f, 16.0f, 8.0f },
        { "Rate Reduce", DSPParamType::Int, DSControlType::Dial, 1.0f, 32.0f, 8.0f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 1.0f }
    };
}

float DSPBlockBitcrusher::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return bitDepth_;
        case 1: return static_cast<float>(sampleRateReduce_);
        case 2: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockBitcrusher::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: bitDepth_ = value; break;
        case 1: sampleRateReduce_ = std::max(1, static_cast<int>(value)); break;
        case 2: mix_ = value; break;
    }
}