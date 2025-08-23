#include <note_naga_engine/module/dsp_engine.h>

#include <note_naga_engine/core/types.h>

#include <cmath>
#include <algorithm>
#include <cstring>

NoteNagaDSPEngine::NoteNagaDSPEngine(NoteNagaMetronome* metronome, NoteNagaSpectrumAnalyzer * spectrum_analyzer) {
    this->metronome_ = metronome;
    this->spectrum_analyzer_ = spectrum_analyzer;
    this->enable_dsp_ = true;
    NOTE_NAGA_LOG_INFO("DSP Engine initialized");
}

void NoteNagaDSPEngine::render(float *output, size_t num_frames) {
    // Prepare mix buffers
    if (mix_left_.size() < num_frames) mix_left_.resize(num_frames, 0.0f);
    if (mix_right_.size() < num_frames) mix_right_.resize(num_frames, 0.0f);
    if (temp_left_.size() < num_frames) temp_left_.resize(num_frames, 0.0f);
    if (temp_right_.size() < num_frames) temp_right_.resize(num_frames, 0.0f);
    
    std::fill(mix_left_.begin(), mix_left_.begin() + num_frames, 0.0f);
    std::fill(mix_right_.begin(), mix_right_.begin() + num_frames, 0.0f);

    // Render all synths and mix
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    for (INoteNagaSoftSynth *synth : this->synths_) {
        // Clear temporary buffers
        std::fill(temp_left_.begin(), temp_left_.begin() + num_frames, 0.0f);
        std::fill(temp_right_.begin(), temp_right_.begin() + num_frames, 0.0f);
        
        // Render this synth to temporary buffers
        synth->renderAudio(temp_left_.data(), temp_right_.data(), num_frames);
        
        // Add to mix buffers
        for (size_t i = 0; i < num_frames; i++) {
            mix_left_[i] += temp_left_[i];
            mix_right_[i] += temp_right_[i];
        }
    }

    // DSP blocks processing
    if (this->enable_dsp_) {
        for (NoteNagaDSPBlockBase *block : this->dsp_blocks_) {
            block->process(mix_left_.data(), mix_right_.data(), num_frames);
        }
    }

    // Metronome rendering
    if (this->metronome_) {
        this->metronome_->render(mix_left_.data(), mix_right_.data(), num_frames);
    }

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

    // push to spectrum analyzer
    if (this->spectrum_analyzer_) {
        this->spectrum_analyzer_->pushSamplesToLeftBuffer(mix_left_.data(), num_frames);
        this->spectrum_analyzer_->pushSamplesToRightBuffer(mix_right_.data(), num_frames);
    }

    // Interleave left and right channels using pointer arithmetic for efficiency
    float *left = mix_left_.data();
    float *right = mix_right_.data();
    float *out = output;
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

void NoteNagaDSPEngine::setEnableDSP(bool enable) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    this->enable_dsp_ = enable;
}

void NoteNagaDSPEngine::addSynth(INoteNagaSoftSynth *synth) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    synths_.push_back(synth);
}

void NoteNagaDSPEngine::removeSynth(INoteNagaSoftSynth *synth) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    synths_.erase(std::remove(synths_.begin(), synths_.end(), synth), synths_.end());
}

void NoteNagaDSPEngine::addDSPBlock(NoteNagaDSPBlockBase *block) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    dsp_blocks_.push_back(block);
}

void NoteNagaDSPEngine::removeDSPBlock(NoteNagaDSPBlockBase *block) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    dsp_blocks_.erase(std::remove(dsp_blocks_.begin(), dsp_blocks_.end(), block),
                      dsp_blocks_.end());
}

void NoteNagaDSPEngine::reorderDSPBlock(int from_idx, int to_idx) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    if (from_idx < 0 || from_idx >= int(dsp_blocks_.size()) || to_idx < 0 ||
        to_idx >= int(dsp_blocks_.size()) || from_idx == to_idx)
        return;
    auto it_from = dsp_blocks_.begin() + from_idx;
    auto block = *it_from;
    dsp_blocks_.erase(it_from);
    dsp_blocks_.insert(dsp_blocks_.begin() + to_idx, block);
}

void NoteNagaDSPEngine::setOutputVolume(float volume) {
    // Ensure volume is within [0.0, 1.0] range
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    this->output_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

std::pair<float, float> NoteNagaDSPEngine::getCurrentVolumeDb() const {
    return {last_rms_left_, last_rms_right_};
}

void NoteNagaDSPEngine::calculateRMS(float *left, float *right, size_t numFrames) {
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