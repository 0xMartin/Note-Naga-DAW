#include <note_naga_engine/module/pan_analyzer.h>

#include <algorithm>
#include <cmath>
#include <numeric>

NoteNagaPanAnalyzer::NoteNagaPanAnalyzer(size_t buffer_size)
    : buffer_size_(buffer_size),
      left_buffer_(buffer_size, 0.0f),
      right_buffer_(buffer_size, 0.0f)
{
    pan_data_.segments.fill(0.0f);
    pan_data_.leftRms = 0.0f;
    pan_data_.rightRms = 0.0f;
    pan_data_.pan = 0.0f;
}

void NoteNagaPanAnalyzer::pushSamplesToLeftBuffer(float *samples, size_t num_samples) {
    if (!enabled_) return;

    size_t to_copy = std::min(num_samples, buffer_size_ - left_pos_);
    std::copy(samples, samples + to_copy, left_buffer_.begin() + left_pos_);
    left_pos_ += to_copy;

    // Trigger processing when both buffers are full
    if (left_pos_ >= buffer_size_ && right_pos_ >= buffer_size_) {
        left_pos_ = 0;
        right_pos_ = 0;
        this->pushToQueue(NN_AsyncTriggerMessage_t{});
    }
}

void NoteNagaPanAnalyzer::pushSamplesToRightBuffer(float *samples, size_t num_samples) {
    if (!enabled_) return;
    
    size_t to_copy = std::min(num_samples, buffer_size_ - right_pos_);
    std::copy(samples, samples + to_copy, right_buffer_.begin() + right_pos_);
    right_pos_ += to_copy;

    // Trigger processing when both buffers are full
    if (left_pos_ >= buffer_size_ && right_pos_ >= buffer_size_) {
        left_pos_ = 0;
        right_pos_ = 0;
        this->pushToQueue(NN_AsyncTriggerMessage_t{});
    }
}

void NoteNagaPanAnalyzer::onItem(const NN_AsyncTriggerMessage_t &) {
    processBuffers();
    NN_QT_EMIT(panDataUpdated(pan_data_));
}

void NoteNagaPanAnalyzer::processBuffers() {
    // Calculate RMS for each channel
    float leftSumSq = 0.0f;
    float rightSumSq = 0.0f;
    
    for (size_t i = 0; i < buffer_size_; ++i) {
        leftSumSq += left_buffer_[i] * left_buffer_[i];
        rightSumSq += right_buffer_[i] * right_buffer_[i];
    }
    
    float leftRms = std::sqrt(leftSumSq / float(buffer_size_));
    float rightRms = std::sqrt(rightSumSq / float(buffer_size_));
    
    // Calculate overall pan position (-1 to 1)
    float totalRms = leftRms + rightRms;
    float pan = 0.0f;
    if (totalRms > 0.0001f) {
        pan = (rightRms - leftRms) / totalRms;
    }
    
    // Divide into angular segments
    // Segment 0 = hard left (180°), Segment 6 = center (90°), Segment 11 = hard right (0°)
    std::array<float, PAN_NUM_SEGMENTS> segments;
    segments.fill(0.0f);
    
    // Process in small windows to capture dynamic pan changes
    const size_t windowSize = buffer_size_ / 16;
    
    for (size_t w = 0; w < 16; ++w) {
        size_t start = w * windowSize;
        size_t end = start + windowSize;
        
        float wLeftSq = 0.0f;
        float wRightSq = 0.0f;
        
        for (size_t i = start; i < end; ++i) {
            wLeftSq += left_buffer_[i] * left_buffer_[i];
            wRightSq += right_buffer_[i] * right_buffer_[i];
        }
        
        float wLeftRms = std::sqrt(wLeftSq / float(windowSize));
        float wRightRms = std::sqrt(wRightSq / float(windowSize));
        float wTotal = wLeftRms + wRightRms;
        
        if (wTotal > 0.0001f) {
            // Calculate pan for this window (-1 to 1)
            float wPan = (wRightRms - wLeftRms) / wTotal;
            
            // Map pan to segment index (0-11)
            // pan -1 -> segment 0 (left)
            // pan 0 -> segment 6 (center)
            // pan 1 -> segment 11 (right)
            float segFloat = (wPan + 1.0f) * 0.5f * float(PAN_NUM_SEGMENTS - 1);
            int segIdx = std::clamp(int(segFloat + 0.5f), 0, PAN_NUM_SEGMENTS - 1);
            
            // Add amplitude to this segment
            float amplitude = wTotal * 0.5f; // Normalize
            segments[segIdx] = std::max(segments[segIdx], amplitude);
        }
    }
    
    // Apply smoothing/decay to existing data
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        // Smooth transition
        const float smoothing = 0.3f;
        for (int i = 0; i < PAN_NUM_SEGMENTS; ++i) {
            pan_data_.segments[i] = pan_data_.segments[i] * (1.0f - smoothing) + segments[i] * smoothing;
        }
        
        pan_data_.leftRms = pan_data_.leftRms * (1.0f - smoothing) + leftRms * smoothing;
        pan_data_.rightRms = pan_data_.rightRms * (1.0f - smoothing) + rightRms * smoothing;
        pan_data_.pan = pan_data_.pan * (1.0f - smoothing) + pan * smoothing;
    }
}
