#include <note_naga_engine/module/spectrum_analyzer.h>

#include <algorithm>
#include <cmath>
#include <numeric>

NoteNagaSpectrumAnalyzer::NoteNagaSpectrumAnalyzer(size_t fft_size, ChannelMode mode)
    : fft_size_(fft_size), fft_current_pos_left_(0), fft_current_pos_right_(0),
      samples_buffer_left_(fft_size, 0.0f), samples_buffer_right_(fft_size, 0.0f),
      spectrum_(fft_size / 2, 0.0f), channel_mode_(mode) {
    this->enable_ = false; 
    // Reset all buffers to zero
    std::fill(samples_buffer_left_.begin(), samples_buffer_left_.end(), 0.0f);
    std::fill(samples_buffer_right_.begin(), samples_buffer_right_.end(), 0.0f);
    std::fill(spectrum_.begin(), spectrum_.end(), 0.0f);
}

void NoteNagaSpectrumAnalyzer::pushSamplesToLeftBuffer(float *samples, size_t num_samples) {
    if (!this->enable_) return; // Do not process if disabled

    size_t to_copy = std::min(num_samples, fft_size_ - fft_current_pos_left_);
    std::copy(samples, samples + to_copy, samples_buffer_left_.begin() + fft_current_pos_left_);
    fft_current_pos_left_ += to_copy;

    if (channel_mode_ == ChannelMode::Left && fft_current_pos_left_ >= fft_size_) {
        fft_current_pos_left_ = 0;
        this->pushToQueue(NN_AsyncTriggerMessage_t{});
    } else if (channel_mode_ == ChannelMode::Merged && fft_current_pos_left_ >= fft_size_ &&
               fft_current_pos_right_ >= fft_size_) {
        fft_current_pos_left_ = 0;
        fft_current_pos_right_ = 0;
        this->pushToQueue(NN_AsyncTriggerMessage_t{});
    }
}

void NoteNagaSpectrumAnalyzer::pushSamplesToRightBuffer(float *samples, size_t num_samples) {
    if (!this->enable_) return; // Do not process if disabled
    
    size_t to_copy = std::min(num_samples, fft_size_ - fft_current_pos_right_);
    std::copy(samples, samples + to_copy, samples_buffer_right_.begin() + fft_current_pos_right_);
    fft_current_pos_right_ += to_copy;

    if (channel_mode_ == ChannelMode::Right && fft_current_pos_right_ >= fft_size_) {
        fft_current_pos_right_ = 0;
        this->pushToQueue(NN_AsyncTriggerMessage_t{});
    } else if (channel_mode_ == ChannelMode::Merged && fft_current_pos_left_ >= fft_size_ &&
               fft_current_pos_right_ >= fft_size_) {
        fft_current_pos_left_ = 0;
        fft_current_pos_right_ = 0;
        this->pushToQueue(NN_AsyncTriggerMessage_t{});
    }
}

void NoteNagaSpectrumAnalyzer::onItem(const NN_AsyncTriggerMessage_t &) {
    processSampleBuffer();
    NN_QT_EMIT(spectrumUpdated(spectrum_));
}

void NoteNagaSpectrumAnalyzer::processSampleBuffer() {
    std::vector<float> working_buffer(fft_size_, 0.0f);

    if (channel_mode_ == ChannelMode::Left) {
        std::copy(samples_buffer_left_.begin(), samples_buffer_left_.end(), working_buffer.begin());
    } else if (channel_mode_ == ChannelMode::Right) {
        std::copy(samples_buffer_right_.begin(), samples_buffer_right_.end(), working_buffer.begin());
    } else if (channel_mode_ == ChannelMode::Merged) {
        for (size_t i = 0; i < fft_size_; ++i)
            working_buffer[i] = 0.5f * (samples_buffer_left_[i] + samples_buffer_right_[i]);
    }

    // DC offset removal
    float mean = std::accumulate(working_buffer.begin(), working_buffer.end(), 0.0f) /
                 float(working_buffer.size());
    for (size_t i = 0; i < working_buffer.size(); ++i)
        working_buffer[i] -= mean;

    // Hann window
    for (size_t i = 0; i < working_buffer.size(); ++i)
        working_buffer[i] *=
            0.5f * (1.0f - std::cos(2.0f * M_PI * i / (working_buffer.size() - 1)));

    // FFT
    std::vector<std::complex<float>> fft_in(fft_size_);
    for (size_t i = 0; i < fft_size_; ++i)
        fft_in[i] = std::complex<float>(working_buffer[i], 0.0f);

    nn_fft(fft_in);

    // Magnitude spectrum
    std::vector<float> mag(fft_size_ / 2, 0.0f);
    for (size_t k = 1; k < fft_size_ / 2; ++k)
        mag[k] = std::abs(fft_in[k]);

    // THRESHOLD: pokud je maxMag menší než 1e-5, považuj za ticho!
    float maxMag = *std::max_element(mag.begin() + 1, mag.end()); // ignoruj DC
    const float noiseFloor = 1e-5f;
    if (maxMag > noiseFloor) {
        for (size_t k = 1; k < mag.size(); ++k)
            mag[k] /= maxMag;
    } else {
        for (size_t k = 1; k < mag.size(); ++k)
            mag[k] = 0.0f;
    }
    mag[0] = 0.0f;

    std::lock_guard<std::mutex> lock(this->spectrum_mutex_);
    spectrum_ = mag;
}
