#include <note_naga_engine/module/dsp_engine.h>

#include <algorithm>
#include <cstring>

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
        NN_AudioBuffer_t left, right;
        synth->renderAudio(num_frames, left, right);
        for (size_t i = 0; i < num_frames; ++i) {
            mix_left_[i] += left.data[i];
            mix_right_[i] += right.data[i];
        }
    }

    // Interleave and write to output buffer
    for (size_t i = 0; i < num_frames; ++i) {
        output[2 * i] = mix_left_[i];
        output[2 * i + 1] = mix_right_[i];
    }
}
