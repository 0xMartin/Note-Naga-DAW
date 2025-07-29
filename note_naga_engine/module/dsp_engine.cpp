#include <note_naga_engine/module/dsp_engine.h>

#include <algorithm>
#include <cstring>

NoteNagaDSPEngine::NoteNagaDSPEngine(NoteNagaProject *project){
    this->project_ = project;
    metronome_.setProject(project);
    metronome_.setSampleRate(44100);
    NOTE_NAGA_LOG_INFO("DSP Engine initialized");
}

void NoteNagaDSPEngine::addSynth(INoteNagaSoftSynth* synth) {
    std::lock_guard<std::mutex> lock(synths_mutex_);
    synths_.push_back(synth);
}

void NoteNagaDSPEngine::removeSynth(INoteNagaSoftSynth* synth) {
    std::lock_guard<std::mutex> lock(synths_mutex_);
    synths_.erase(std::remove(synths_.begin(), synths_.end(), synth), synths_.end());
}

void NoteNagaDSPEngine::renderBlock(float* output, size_t num_frames) {
    // Prepare mix buffers
    if (mix_left_.size() < num_frames) mix_left_.resize(num_frames, 0.0f);
    if (mix_right_.size() < num_frames) mix_right_.resize(num_frames, 0.0f);
    std::fill(mix_left_.begin(), mix_left_.begin() + num_frames, 0.0f);
    std::fill(mix_right_.begin(), mix_right_.begin() + num_frames, 0.0f);

    // Render all synths and mix
    std::lock_guard<std::mutex> lock(synths_mutex_);
    for (auto* synth : synths_) {
        synth->renderAudio(mix_left_.data(), mix_right_.data(), num_frames);
    }

    // Metronome rendering
    metronome_.render(mix_left_.data(), mix_right_.data(), num_frames);

    // apply master volume with logarithmic effect
    if (output_volume_ < 1.0f) {
        // Use a simple logarithmic curve for perceptual loudness
        float log_volume = powf(output_volume_, 2.0f); // or use another exponent for desired curve
        for (size_t i = 0; i < num_frames; ++i) {
            mix_left_[i] *= log_volume;
            mix_right_[i] *= log_volume;
        }
    }

    // Calculate RMS for visualization
    this->calculateRMS(mix_left_.data(), mix_right_.data(), num_frames);

    // Interleave left and right channels using pointer arithmetic for efficiency
    float* left = mix_left_.data();
    float* right = mix_right_.data();
    float* out = output;
    size_t i = 0;
    size_t n = num_frames;
    // Unroll loop for better performance
    for (; i + 3 < n; i += 4) {
        out[0] = left[i];
        out[1] = right[i];
        out[2] = left[i + 1];
        out[3] = right[i + 1];
        out[4] = left[i + 2];
        out[5] = right[i + 2];
        out[6] = left[i + 3];
        out[7] = right[i + 3];
        out += 8;
    }
    // Handle remaining frames
    for (; i < n; ++i) {
        *out++ = left[i];
        *out++ = right[i];
    }
}

void NoteNagaDSPEngine::setOutputVolume(float volume) {
    // Ensure volume is within [0.0, 1.0] range
    std::lock_guard<std::mutex> lock(synths_mutex_);
    this->output_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

std::pair<float, float> NoteNagaDSPEngine::getCurrentVolumeDb() const {
    return {last_rms_left_, last_rms_right_};
}

void NoteNagaDSPEngine::calculateRMS(float* left, float* right, size_t numFrames) {
        // Výpočet RMS pro left/right
    double sum_left = 0.0, sum_right = 0.0;
    for (size_t i = 0; i < numFrames; ++i) {
        sum_left += left[i] * left[i];
        sum_right += right[i] * right[i];
    }
    float rms_left = sqrt(sum_left / numFrames);
    float rms_right = sqrt(sum_right / numFrames);

    // Konverze na dBFS (0 dB = max, typicky -60 až 0)
    this->last_rms_left_ = (rms_left > 0.000001f) ? 20.0f * log10(rms_left) : -100.0f;
    this->last_rms_right_ = (rms_right > 0.000001f) ? 20.0f * log10(rms_right) : -100.0f;
}
