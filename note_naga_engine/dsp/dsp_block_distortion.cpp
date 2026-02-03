#include <note_naga_engine/dsp/dsp_block_distortion.h>
#include <cmath>
#include <algorithm>

DSPBlockDistortion::DSPBlockDistortion(int type, float drive, float tone, float mix)
    : type_(static_cast<DistortionType>(type)), drive_(drive), tone_(tone), mix_(mix)
{
}

float DSPBlockDistortion::processDistortion(float sample) {
    float driven = sample * drive_;
    float output;
    
    switch (type_) {
        case DistortionType::SoftClip:
            // Soft clip using tanh
            output = std::tanh(driven);
            break;
            
        case DistortionType::HardClip:
            // Hard clip
            output = std::clamp(driven, -1.0f, 1.0f);
            break;
            
        case DistortionType::Tube:
            // Asymmetric tube-like distortion
            if (driven >= 0.0f) {
                output = 1.0f - std::exp(-driven);
            } else {
                output = -1.0f + std::exp(driven);
            }
            break;
            
        case DistortionType::Fuzz:
            // Fuzz with more aggressive clipping
            output = std::tanh(driven * 2.0f);
            if (std::abs(output) > 0.7f) {
                output = std::copysign(0.7f + 0.3f * std::tanh((std::abs(output) - 0.7f) * 5.0f), output);
            }
            break;
            
        default:
            output = driven;
    }
    
    return output;
}

void DSPBlockDistortion::process(float* left, float* right, size_t numFrames) {
    if (!isActive()) return;
    
    // Tone control filter coefficient
    float lpCoeff = std::exp(-2.0f * 3.14159f * (800.0f + tone_ * 15000.0f) / 44100.0f);
    float hpAmount = 1.0f - tone_;
    
    for (size_t i = 0; i < numFrames; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        // Process distortion
        float distL = processDistortion(inL);
        float distR = processDistortion(inR);
        
        // Apply tone control (low-pass filter blend)
        lpStateL_ = lpStateL_ + (1.0f - lpCoeff) * (distL - lpStateL_);
        lpStateR_ = lpStateR_ + (1.0f - lpCoeff) * (distR - lpStateR_);
        
        float toneL = lpStateL_ * (1.0f - hpAmount) + distL * hpAmount;
        float toneR = lpStateR_ * (1.0f - hpAmount) + distR * hpAmount;
        
        // Mix
        left[i] = inL * (1.0f - mix_) + toneL * mix_;
        right[i] = inR * (1.0f - mix_) + toneR * mix_;
    }
}

std::vector<DSPParamDescriptor> DSPBlockDistortion::getParamDescriptors() {
    return {
        { "Type", DSPParamType::Int, DSControlType::Dial, 0.0f, 3.0f, 0.0f, {"Soft Clip", "Hard Clip", "Tube", "Fuzz"} },
        { "Drive", DSPParamType::Float, DSControlType::Dial, 1.0f, 20.0f, 4.0f },
        { "Tone", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.5f },
        { "Mix", DSPParamType::Float, DSControlType::DialCentered, 0.0f, 1.0f, 0.8f }
    };
}

float DSPBlockDistortion::getParamValue(size_t idx) const {
    switch (idx) {
        case 0: return static_cast<float>(type_);
        case 1: return drive_;
        case 2: return tone_;
        case 3: return mix_;
        default: return 0.0f;
    }
}

void DSPBlockDistortion::setParamValue(size_t idx, float value) {
    switch (idx) {
        case 0: type_ = static_cast<DistortionType>(static_cast<int>(value)); break;
        case 1: drive_ = value; break;
        case 2: tone_ = value; break;
        case 3: mix_ = value; break;
    }
}
