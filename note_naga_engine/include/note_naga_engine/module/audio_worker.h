#pragma once

#include <RtAudio.h>
#include <atomic>
#include <thread>
#include <note_naga_engine/module/dsp_engine.h>

/**
 * NoteNagaAudioWorker - provides real-time audio output using RtAudio.
 * It interfaces with the NoteNagaDSPEngine to fetch audio data for playback.
 */
class NoteNagaAudioWorker {
public:
    NoteNagaAudioWorker(NoteNagaDSPEngine* dsp);
    ~NoteNagaAudioWorker();

    /**
     * @brief Sets the DSP engine to be used for audio rendering.
     * @param dsp Pointer to the NoteNagaDSPEngine instance.
     */
    void setDSPEngine(NoteNagaDSPEngine* dsp);

    /**
     * @brief Starts the audio device (stream) asynchronously. 
     * Returns immediately, audio initialization happens in background.
     * @param sampleRate The sample rate to use for the audio stream.
     * @param blockSize The block size (number of frames) for the audio stream.
     */
    void startAsync(unsigned int sampleRate, unsigned int blockSize);

    /**
     * @brief Starts the audio device (stream) synchronously.
     * @param sampleRate The sample rate to use for the audio stream.
     * @param blockSize The block size (number of frames) for the audio stream.
     * @return True if the stream started successfully, false otherwise.
     */
    bool start(unsigned int sampleRate, unsigned int blockSize);

    /**
     * @brief Stops the audio device (stream). Can be called multiple times.
     * @return True if the stream stopped successfully, false otherwise.
     */
    bool stop();

    /**
     * @brief Mutes the audio output without stopping the stream.
     * The audio callback will start filling the buffer with zeros.
     */
    void mute();

    /**
     * @brief Unmutes the audio output.
     * The audio callback will resume rendering audio from the DSP engine.
     */
    void unmute();

    /**
     * @brief Checks if the audio worker is currently muted.
     * @return True if muted, false otherwise.
     */
    bool isMuted() const;

    /**
     * @brief Get the current sample rate.
     * @return Current sample rate.
     */
    unsigned int getSampleRate() const { return this->sample_rate; }

    /**
     * @brief Get the current block size.
     * @return Current block size.
     */
    unsigned int getBlockSize() const { return this->block_size; }

private:
    RtAudio audio;
    NoteNagaDSPEngine* dsp_engine = nullptr;
    unsigned int sample_rate = 44100;
    unsigned int block_size = 512;
    unsigned int output_channels = 2;
    std::atomic<bool> stream_open{false};
    std::atomic<bool> is_muted{false};
    std::atomic<bool> init_in_progress{false};
    std::atomic<bool> stopping_{false};  // Flag to prevent callback access during shutdown
    std::thread init_thread;

    // Callback volaný RtAudio, naplňuje výstupní buffer audio daty.
    static int audioCallback(void* outputBuffer, void*, unsigned int nFrames,
                            double, RtAudioStreamStatus, void* userData);
};