#pragma once

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

#include <note_naga_engine/core/async_queue_component.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_api.h>

#include <array>
#include <mutex>
#include <vector>

/**
 * @brief Number of angular segments for pan visualization (semicircle divided into segments)
 */
constexpr int PAN_NUM_SEGMENTS = 12;

/**
 * @brief Pan analysis data - RMS levels for each angular segment
 */
struct NOTE_NAGA_ENGINE_API NN_PanData_t {
    std::array<float, PAN_NUM_SEGMENTS> segments; // RMS level for each segment (0=left, 6=center, 11=right)
    float leftRms;   // Overall left channel RMS
    float rightRms;  // Overall right channel RMS
    float pan;       // Computed pan position (-1 = left, 0 = center, 1 = right)
};

/**
 * @brief NoteNagaPanAnalyzer analyzes stereo audio to visualize pan/stereo field.
 * Uses a semicircle divided into segments to show where sound is coming from.
 * Efficient: only updates a few times per second.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaPanAnalyzer
    : public QObject,
      public AsyncQueueComponent<NN_AsyncTriggerMessage_t, 16> {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaPanAnalyzer
    : public AsyncQueueComponent<NN_AsyncTriggerMessage_t, 16> {
#endif
public:
    explicit NoteNagaPanAnalyzer(size_t buffer_size = 2048);

    /**
     * @brief Enable or disable pan analysis.
     */
    void setEnabled(bool enable) { enabled_ = enable; }
    
    /**
     * @brief Check if pan analysis is enabled.
     */
    bool isEnabled() const { return enabled_; }

    /**
     * @brief Push audio samples to left channel buffer.
     */
    void pushSamplesToLeftBuffer(float* samples, size_t num_samples);

    /**
     * @brief Push audio samples to right channel buffer.
     */
    void pushSamplesToRightBuffer(float* samples, size_t num_samples);

    /**
     * @brief Get the latest pan analysis data.
     */
    NN_PanData_t getPanData() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return pan_data_;
    }

private:
    bool enabled_ = false;
    size_t buffer_size_;
    
    std::vector<float> left_buffer_;
    std::vector<float> right_buffer_;
    size_t left_pos_ = 0;
    size_t right_pos_ = 0;
    
    mutable std::mutex data_mutex_;
    NN_PanData_t pan_data_;
    
    void onItem(const NN_AsyncTriggerMessage_t &message) override;
    void processBuffers();

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when pan data is updated.
     */
    void panDataUpdated(const NN_PanData_t &data);
#endif
};
