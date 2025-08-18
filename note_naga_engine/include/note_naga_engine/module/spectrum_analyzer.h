#pragma once

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

#include <note_naga_engine/core/async_queue_component.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_api.h>

/** Channel mode for spectrum analysis */
enum class NOTE_NAGA_ENGINE_API ChannelMode { Left, Right, Merged };

/**
 * @brief NoteNagaSpectrumAnalyzer is a component for analyzing audio spectrum.
 * It processes audio samples and computes the frequency spectrum using FFT.
 * It uses an asynchronous queue to handle incoming audio data.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaSpectrumAnalyzer
    : public QObject,
      public AsyncQueueComponent<NN_AsyncTriggerMessage_t, 16> {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaSpectrumAnalyzer
    : public AsyncQueueComponent<NN_AsyncTriggerMessage_t> {
#endif
public:
    explicit NoteNagaSpectrumAnalyzer(size_t fft_size, ChannelMode mode = ChannelMode::Merged);

    /**
     * @brief Enable or disable spectrum analysis.
     * @param enable True to enable, false to disable.
     */
    void setEnableSpectrumAnalysis(bool enable) {
        this->enable_ = enable;
    }    
    
    /**
     * @brief Check if spectrum analysis is enabled.
     * @return True if enabled, false otherwise.
     */
    bool isEnabled() const { return enable_; }

    /**
     * @brief Push audio samples to the analyzer.
     * @param samples Pointer to the audio samples buffer.
     * @param num_samples Number of samples in the buffer.
     */
    void pushSamplesToLeftBuffer(float* samples, size_t num_samples);

    /**
     * @brief Push audio samples to the analyzer.
     * @param samples Pointer to the audio samples buffer.
     * @param num_samples Number of samples in the buffer.
     */
    void pushSamplesToRightBuffer(float* samples, size_t num_samples);

    /**
     * @brief Set the channel mode for spectrum analysis.
     * @param mode The channel mode to set (Left, Right, Merged).
     */
    void setChannelMode(ChannelMode mode) { channel_mode_ = mode; }

    /**
     * @brief Get the current channel mode.
     * @return The current channel mode (Left, Right, Merged).
     */
    ChannelMode getChannelMode() const { return channel_mode_; }

    /**
     * @brief Get the current frequency spectrum.
     * @return Vector of float values representing the frequency spectrum.
     */
    std::vector<float> getSpectrum() {
        std::lock_guard<std::mutex> lock(this->spectrum_mutex_);
        return spectrum_;
    }

private:
    bool enable_; /// Enable/disable spectrum analysis
    size_t fft_size_;        /// Required FFT size for the spectrum analysis

    // Buffers for audio samples
    std::vector<float> samples_buffer_left_;
    std::vector<float> samples_buffer_right_;
    size_t fft_current_pos_left_;
    size_t fft_current_pos_right_;
    ChannelMode channel_mode_;

    mutable std::mutex spectrum_mutex_; // Mutex for thread-safe access to spectrum data
    std::vector<float> spectrum_;       // Frequency spectrum data

    void onItem(const NN_AsyncTriggerMessage_t &message) override;

    void processSampleBuffer();

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when the spectrum is updated.
     * @param spectrum The computed frequency spectrum.
     */
    void spectrumUpdated(const std::vector<float> &spectrum);
#endif
};
