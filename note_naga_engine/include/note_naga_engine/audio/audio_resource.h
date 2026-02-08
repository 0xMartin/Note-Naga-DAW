#ifndef NOTE_NAGA_AUDIO_RESOURCE_H
#define NOTE_NAGA_AUDIO_RESOURCE_H

#include "../note_naga_api.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

/**
 * @brief Waveform peak data for efficient rendering.
 * Each peak represents min/max values over a range of samples.
 */
struct NOTE_NAGA_ENGINE_API NN_WaveformPeak_t {
    float minLeft = 0.0f;
    float maxLeft = 0.0f;
    float minRight = 0.0f;
    float maxRight = 0.0f;
};

/**
 * @brief Audio clip reference in arrangement.
 */
struct NOTE_NAGA_ENGINE_API NN_AudioClip_t {
    int id = -1;                    ///< Unique clip ID
    int audioResourceId = -1;       ///< Reference to audio resource
    int startTick = 0;              ///< Start position in arrangement (ticks)
    int durationTicks = 0;          ///< Duration in ticks
    int offsetSamples = 0;          ///< Offset into audio file (samples)
    int offsetTicks = 0;            ///< Offset into audio file (ticks) - for waveform display
    int clipLengthSamples = 0;      ///< Length of clip in samples (for looping calculation)
    int fadeInTicks = 0;            ///< Fade in duration in ticks
    int fadeOutTicks = 0;           ///< Fade out duration in ticks
    bool muted = false;             ///< Mute state
    bool looping = false;           ///< Enable looping within clip duration
    float gain = 1.0f;              ///< Clip-specific gain (0.0 - 1.0)
    
    /**
     * @brief Check if this clip contains a given tick.
     */
    bool containsTick(int tick) const {
        return tick >= startTick && tick < (startTick + durationTicks);
    }
};

/**
 * @brief Represents an audio resource loaded from disk.
 * Handles streaming, caching, and waveform generation.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaAudioResource : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaAudioResource {
#endif

public:
    /**
     * @brief Constructor.
     * @param filePath Path to the audio file.
     */
    explicit NoteNagaAudioResource(const std::string &filePath);
    
    /**
     * @brief Destructor.
     */
    virtual ~NoteNagaAudioResource();
    
    // GETTERS
    int getId() const { return id_; }
    const std::string& getFilePath() const { return filePath_; }
    const std::string& getFileName() const { return fileName_; }
    int getSampleRate() const { return sampleRate_; }
    int getChannels() const { return channels_; }
    int64_t getTotalSamples() const { return totalSamples_; }
    double getDurationSeconds() const { return durationSeconds_; }
    bool isLoaded() const { return loaded_; }
    bool hasError() const { return hasError_; }
    const std::string& getErrorMessage() const { return errorMessage_; }
    
    /**
     * @brief Get waveform peaks for rendering.
     * @return Vector of peak data.
     */
    const std::vector<NN_WaveformPeak_t>& getWaveformPeaks() const { return waveformPeaks_; }
    
    /**
     * @brief Get number of samples per peak.
     */
    int getSamplesPerPeak() const { return samplesPerPeak_; }
    
    /**
     * @brief Load the audio file and prepare for streaming.
     * @param targetSampleRate Target sample rate for resampling.
     * @return True if successful.
     */
    bool load(int targetSampleRate);
    
    /**
     * @brief Get audio samples for a given range. Handles streaming buffer.
     * @param startSample Start sample index.
     * @param numSamples Number of samples to get.
     * @param outLeft Output buffer for left channel.
     * @param outRight Output buffer for right channel.
     * @return Number of samples actually read.
     */
    int getSamples(int64_t startSample, int numSamples, 
                   float* outLeft, float* outRight);
    
    /**
     * @brief Prepare streaming buffer for upcoming playback position.
     * Call this ahead of time to ensure data is ready.
     * @param startSample Sample position to prepare for.
     */
    void prepareForPosition(int64_t startSample);
    
    /**
     * @brief Set the unique ID.
     */
    void setId(int id) { id_ = id; }

private:
    int id_ = -1;
    std::string filePath_;
    std::string fileName_;
    
    // Audio format info (after resampling)
    int sampleRate_ = 44100;
    int channels_ = 2;
    int64_t totalSamples_ = 0;
    double durationSeconds_ = 0.0;
    
    // Original file info
    int originalSampleRate_ = 0;
    int originalChannels_ = 0;
    int64_t originalTotalSamples_ = 0;
    
    // State
    bool loaded_ = false;
    bool hasError_ = false;
    std::string errorMessage_;
    
    // Waveform peaks for visualization
    std::vector<NN_WaveformPeak_t> waveformPeaks_;
    int samplesPerPeak_ = 256;
    
    // Streaming buffer (4 seconds of audio)
    static constexpr int BUFFER_SECONDS = 4;
    std::vector<float> streamBufferLeft_;
    std::vector<float> streamBufferRight_;
    int64_t bufferStartSample_ = 0;
    int64_t bufferEndSample_ = 0;
    std::mutex bufferMutex_;
    
    // Background loading
    std::thread loadThread_;
    std::atomic<bool> loadThreadRunning_{false};
    std::atomic<int64_t> requestedPosition_{0};
    std::condition_variable loadCondition_;
    std::mutex loadMutex_;
    
    // Full audio data (for small files) or file handle for streaming
    std::vector<float> fullAudioLeft_;
    std::vector<float> fullAudioRight_;
    bool useFullAudioCache_ = false;
    
    // Internal methods
    bool loadWavFile(int targetSampleRate);
    void generateWaveformPeaks();
    void resampleAudio(const std::vector<float>& input, std::vector<float>& output,
                       int inputRate, int outputRate);
    void streamingThreadFunc();
    void loadBufferRange(int64_t startSample, int64_t endSample);
    
#ifndef QT_DEACTIVATED
Q_SIGNALS:
    void loadComplete(bool success);
    void loadProgress(int percent);
#endif
};

#endif // NOTE_NAGA_AUDIO_RESOURCE_H
