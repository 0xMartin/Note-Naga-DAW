#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <note_naga_engine/audio/audio_resource.h>
#include <note_naga_engine/module/metronome.h>
#include <note_naga_engine/module/spectrum_analyzer.h>
#include <note_naga_engine/module/pan_analyzer.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/module/playback_worker.h>

#include <vector>
#include <mutex>
#include <map>

/** 
 * @brief NoteNagaDSPEngine is the main DSP engine for the Note Naga project.
 * It manages audio rendering, synthesizers, and the metronome.
 * It provides methods to add/remove synthesizers and render audio blocks.
 */
class NOTE_NAGA_ENGINE_API NoteNagaDSPEngine {
public:
    /**
     * @brief Construct a new NoteNagaDSPEngine object.
     * 
     * @param metronome Pointer to the metronome module.
     * @param spectrum_analyzer Pointer to the spectrum analyzer module.
     * @param pan_analyzer Pointer to the pan analyzer module.
     */
    NoteNagaDSPEngine(NoteNagaMetronome* metronome = nullptr, NoteNagaSpectrumAnalyzer* spectrum_analyzer = nullptr, NoteNagaPanAnalyzer* pan_analyzer = nullptr);
    ~NoteNagaDSPEngine() = default;

    /**
     * @brief Render audio output.
     * 
     * @param output Pointer to the output buffer.
     * @param num_frames Number of frames to render.
     * @param compute_rms Whether to compute RMS levels for volume metering.
     */
    void render(float *output, size_t num_frames, bool compute_rms = true);

    /**
     * @brief Add a synthesizer to the DSP engine.
     * 
     * @param synth Pointer to the synthesizer to add.
     */
    void addSynth(INoteNagaSoftSynth *synth);

    /**
     * @brief Remove a synthesizer from the DSP engine.
     * 
     * @param synth Pointer to the synthesizer to remove.
     */
    void removeSynth(INoteNagaSoftSynth *synth);

    /**
     * @brief Add a DSP block to the master channel.
     * 
     * @param block Pointer to the DSP block to add.
     */
    void addDSPBlock(NoteNagaDSPBlockBase *block);

    /**
     * @brief Remove a DSP block from the master channel.
     * 
     * @param block Pointer to the DSP block to remove.
     */
    void removeDSPBlock(NoteNagaDSPBlockBase *block);

    /**
     * @brief Reorder a DSP block in the master channel.
     * 
     * @param from_idx Index of the DSP block to move.
     * @param to_idx New index for the DSP block.
     */
    void reorderDSPBlock(int from_idx, int to_idx);

    /**
     * @brief Add a DSP block to a specific synthesizer.
     * 
     * @param synth Pointer to the synthesizer.
     * @param block Pointer to the DSP block to add.
     */
    void addSynthDSPBlock(INoteNagaSoftSynth *synth, NoteNagaDSPBlockBase *block);

    /**
     * @brief Remove a DSP block from a specific synthesizer.
     * 
     * @param synth Pointer to the synthesizer.
     * @param block Pointer to the DSP block to remove.
     */
    void removeSynthDSPBlock(INoteNagaSoftSynth *synth, NoteNagaDSPBlockBase *block);

    /**
     * @brief Reorder a DSP block in a specific synthesizer.
     * 
     * @param synth Pointer to the synthesizer.
     * @param from_idx Index of the DSP block to move.
     * @param to_idx New index for the DSP block.
     */
    void reorderSynthDSPBlock(INoteNagaSoftSynth *synth, int from_idx, int to_idx);

    /**
     * @brief Get all DSP blocks for the master channel.
     * 
     * @return std::vector<NoteNagaDSPBlockBase*> List of DSP blocks.
     */
    std::vector<NoteNagaDSPBlockBase*> getDSPBlocks() const { return dsp_blocks_; }

    /**
     * @brief Get all DSP blocks for a specific synthesizer.
     * 
     * @param synth Pointer to the synthesizer.
     * @return std::vector<NoteNagaDSPBlockBase*> List of DSP blocks.
     */
    std::vector<NoteNagaDSPBlockBase*> getSynthDSPBlocks(INoteNagaSoftSynth *synth) const;

    /**
     * @brief Enable or disable DSP processing.
     * 
     * @param enable True to enable DSP, false to disable.
     */
    void setEnableDSP(bool enable);

    /**
     * @brief Check if DSP processing is enabled.
     * 
     * @return True if DSP is enabled, false otherwise.
     */
    bool isDSPEnabled() const { return enable_dsp_; }

    /**
     * @brief Get all synthesizers managed by this DSP engine.
     * 
     * @return std::vector<INoteNagaSoftSynth*> List of synthesizers.
     */
    std::vector<INoteNagaSoftSynth*> getAllSynths() const { return synths_; }

    /**
     * @brief Set the runtime data for track-based rendering.
     * 
     * @param runtime_data Pointer to the runtime data.
     */
    void setRuntimeData(NoteNagaRuntimeData* runtime_data) { runtime_data_ = runtime_data; }

    /**
     * @brief Get the runtime data.
     * 
     * @return NoteNagaRuntimeData* Pointer to the runtime data.
     */
    NoteNagaRuntimeData* getRuntimeData() const { return runtime_data_; }

    /**
     * @brief Set the playback mode (Sequence or Arrangement).
     * In Sequence mode, only the active sequence's tracks are rendered.
     * In Arrangement mode, all sequences' tracks are rendered.
     * 
     * @param mode The playback mode.
     */
    void setPlaybackMode(PlaybackMode mode) { playback_mode_ = mode; }

    /**
     * @brief Get the current playback mode.
     * 
     * @return PlaybackMode Current playback mode.
     */
    PlaybackMode getPlaybackMode() const { return playback_mode_; }

    /**
     * @brief Set the output volume.
     * 
     * @param volume New output volume (0.0 to 1.0).
     */
    void setOutputVolume(float volume);

    /**
     * @brief Get the current output volume.
     * 
     * @return float Current output volume (0.0 to 1.0).
     */
    float getOutputVolume() const { return output_volume_; }

    /**
     * @brief Get the current volume in dB.
     * 
     * @return std::pair<float, float> Current volume in dB (left, right).
     */
    std::pair<float, float> getCurrentVolumeDb() const;

    /**
     * @brief Get the current volume in dB for a specific track.
     * 
     * @param track Pointer to the track.
     * @return std::pair<float, float> Current volume in dB (left, right).
     */
    std::pair<float, float> getTrackVolumeDb(NoteNagaTrack* track) const;

    /**
     * @brief Reset internal state of all DSP blocks.
     * Call this when playback restarts to prevent state bleed.
     */
    void resetAllBlocks();

    /**
     * @brief Set the sample rate for audio calculations.
     * @param sampleRate Sample rate in Hz.
     */
    void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }

    /**
     * @brief Get the current sample rate.
     * @return Sample rate in Hz.
     */
    int getSampleRate() const { return sampleRate_; }

    /**
     * @brief Set the current audio sample position (called by playback worker).
     * @param samplePosition Current sample position in the arrangement/sequence.
     */
    void setAudioSamplePosition(int64_t samplePosition) { 
        audioSamplePosition_.store(samplePosition, std::memory_order_relaxed); 
    }

    /**
     * @brief Get the current audio sample position.
     * @return Current sample position.
     */
    int64_t getAudioSamplePosition() const { 
        return audioSamplePosition_.load(std::memory_order_relaxed); 
    }
    
    /**
     * @brief Set audio playback active state. When false, audio clips won't advance.
     * @param active True if playback is active.
     */
    void setAudioPlaybackActive(bool active) {
        audioPlaybackActive_.store(active, std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if audio playback is active.
     * @return True if playback is active.
     */
    bool isAudioPlaybackActive() const {
        return audioPlaybackActive_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Convert tick position to sample position.
     * @param tick Tick position.
     * @param tempo Tempo in microseconds per quarter note.
     * @param ppq Pulses per quarter note.
     * @return Sample position.
     */
    int64_t tickToSamples(int tick, int tempo, int ppq) const;

    /**
     * @brief Convert sample position to tick position.
     * @param sample Sample position.
     * @param tempo Tempo in microseconds per quarter note.
     * @param ppq Pulses per quarter note.
     * @return Tick position.
     */
    int sampleToTicks(int64_t sample, int tempo, int ppq) const;

private:
    std::mutex dsp_engine_mutex_;
    std::vector<INoteNagaSoftSynth*> synths_;
    std::vector<NoteNagaDSPBlockBase*> dsp_blocks_;
    
    // Mapping from synth to its DSP blocks (for per-track synths)
    std::map<INoteNagaSoftSynth*, std::vector<NoteNagaDSPBlockBase*>> synth_dsp_blocks_;
    
    // Runtime data for track-based rendering
    NoteNagaRuntimeData* runtime_data_ = nullptr;
    
    // Playback mode: Sequence (render active sequence) or Arrangement (render all)
    PlaybackMode playback_mode_ = PlaybackMode::Sequence;
    
    std::vector<float> mix_left_;
    std::vector<float> mix_right_;
    std::vector<float> temp_left_;
    std::vector<float> temp_right_;
    std::vector<float> track_left_;
    std::vector<float> track_right_;
    
    float output_volume_ = 1.0f;
    float last_rms_left_ = -100.0f;
    float last_rms_right_ = -100.0f;
    std::map<NoteNagaTrack*, std::pair<float, float>> track_rms_values_; ///< Per-track RMS in dB
    bool enable_dsp_ = true;
    
    NoteNagaMetronome* metronome_ = nullptr;
    NoteNagaSpectrumAnalyzer* spectrum_analyzer_ = nullptr;
    NoteNagaPanAnalyzer* pan_analyzer_ = nullptr;
    
    // Audio rendering state
    int sampleRate_ = 44100;
    std::atomic<int64_t> audioSamplePosition_{0}; ///< Current sample position in arrangement
    std::atomic<bool> audioPlaybackActive_{false}; ///< True when playback is active
    std::vector<float> audioClipBuffer_; ///< Temporary buffer for audio clip samples
    
    void calculateRMS(float *left, float *right, size_t numFrames);
    std::pair<float, float> calculateTrackRMS(float *left, float *right, size_t numFrames);
    
    /**
     * @brief Render audio clips from arrangement tracks into the mix buffers.
     * @param numFrames Number of frames to render.
     */
    void renderAudioClips(size_t numFrames);
};