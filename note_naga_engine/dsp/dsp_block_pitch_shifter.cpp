#include <note_naga_engine/dsp/dsp_block_pitch_shifter.h>
#include <cmath>
#include <algorithm>

DSPBlockPitchShifter::DSPBlockPitchShifter(float semitones, float mix)
    : semitones_(semitones), mix_(mix)
{
    resizeBuffers();
}

void DSPBlockPitchShifter::setSampleRate(float sr) {
    sampleRate_ = sr;
    resizeBuffers();
}

void DSPBlockPitchShifter::resizeBuffers() {
    bufferSize_ = static_cast<size_t>(sampleRate_ * 0.2f); // 200ms buffer
    bufferL_.resize(bufferSize_, 0.0f);
    bufferR_.resize(bufferSize_, 0.0f);
    writeIdx_ = 0;
    readPosL_ = 0.0f;
    readPosR_ = 0.0f;
}

void DSPBlockPitchShifter::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    // Calculate pitch ratio from semitones
    float ratio = std::pow(2.0f, semitones_ / 12.0f);
    
    // Grain size for crossfade (samples)
    const float grainSize = sampleRate_ * 0.02f; // 20ms grains
    
    for (size_t i = 0; i < numFrames; ++i) {
        // Write input to circular buffer
        bufferL_[writeIdx_] = left[i];
        bufferR_[writeIdx_] = right[i];
        
        // Calculate read positions for two overlapping grains
        float readPos1 = readPosL_;
        float readPos2 = std::fmod(readPosL_ + grainSize, static_cast<float>(bufferSize_));
        
        // Crossfade position within grain
        float grainPos = std::fmod(readPosL_, grainSize);
        float fade1 = 0.5f - 0.5f * std::cos(3.14159f * grainPos / grainSize);
        float fade2 = 1.0f - fade1;
        
        // Read with interpolation - grain 1
        size_t idx1_0 = static_cast<size_t>(readPos1) % bufferSize_;
        size_t idx1_1 = (idx1_0 + 1) % bufferSize_;
        float frac1 = readPos1 - std::floor(readPos1);
        
        float sampleL1 = bufferL_[idx1_0] * (1.0f - frac1) + bufferL_[idx1_1] * frac1;
        float sampleR1 = bufferR_[idx1_0] * (1.0f - frac1) + bufferR_[idx1_1] * frac1;
        
        // Read with interpolation - grain 2
        size_t idx2_0 = static_cast<size_t>(readPos2) % bufferSize_;
        size_t idx2_1 = (idx2_0 + 1) % bufferSize_;
        float frac2 = readPos2 - std::floor(readPos2);
        
        float sampleL2 = bufferL_[idx2_0] * (1.0f - frac2) + bufferL_[idx2_1] * frac2;
        float sampleR2 = bufferR_[idx2_0] * (1.0f - frac2) + bufferR_[idx2_1] * frac2;
        
        // Crossfade grains
        float shiftedL = sampleL1 * fade1 + sampleL2 * fade2;
        float shiftedR = sampleR1 * fade1 + sampleR2 * fade2;
        
        // Advance read position
        readPosL_ += ratio;
        if (readPosL_ >= bufferSize_) readPosL_ -= bufferSize_;
        if (readPosL_ < 0.0f) readPosL_ += bufferSize_;
        
        // Mix
        left[i] = left[i] * (1.0f - mix_) + shiftedL * mix_;
        right[i] = right[i] * (1.0f - mix_) + shiftedR * mix_;
        
        writeIdx_ = (writeIdx_ + 1) % bufferSize_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockPitchShifter::getParamDescriptors() {
    return {
        { "Semitones", DSPParamType::Float, DSControlType::DialCentered, -12.0f, 12.0f, 0.0f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 1.0f }
    };
}

float DSPBlockPitchShifter::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return semitones_;
        case 1: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockPitchShifter::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: semitones_ = value; break;
        case 1: mix_ = value; break;
    }
}
