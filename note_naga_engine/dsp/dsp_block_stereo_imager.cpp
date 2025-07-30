#include <note_naga_engine/dsp/dsp_block_stereo_imager.h>

#include <algorithm>
#include <cmath>

DSPBlockStereoImager::DSPBlockStereoImager(float width)
    : width_(width)
{}

void DSPBlockStereoImager::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;

    float sideGain = 1.0f + width_; // -1 (mono), 1 (max wide)
    float midGain = 1.0f;

    for (size_t i = 0; i < numFrames; ++i) {
        float mid = 0.5f * (left[i] + right[i]);
        float side = 0.5f * (left[i] - right[i]);

        side *= sideGain;
        mid *= midGain;

        left[i]  = mid + side;
        right[i] = mid - side;
    }
}

std::vector<DSPParamDescriptor> DSPBlockStereoImager::getParamDescriptors() {
    return {
        { "Width", DSPParamType::Float, DSControlType::DialCentered, -1.0f, 1.0f, 0.0f }
    };
}

float DSPBlockStereoImager::getParamValue(size_t idx) const {
    if (idx == 0) return width_;
    return 0.0f;
}

void DSPBlockStereoImager::setParamValue(size_t idx, float value) {
    if (idx == 0) width_ = value;
}