#pragma once

#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/types.h>
#include <fluidsynth.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <functional>

class DSPEngine;

/**
 * FluidSynth syntetizér pro NoteNagaEngine.
 */
class NoteNagaSynthFluidSynth : public NoteNagaSynthesizer, public INoteNagaSoftSynth {
public:
    /**
     * @brief Konstruktor FluidSynth syntetizéru
     * @param name Název syntetizéru
     * @param sf2_path Cesta k SoundFont souboru (.sf2 nebo .sf3)
     * @param loadAsync If true, SoundFont is loaded asynchronously in background
     */
    NoteNagaSynthFluidSynth(const std::string &name, const std::string &sf2_path, bool loadAsync = false);
    ~NoteNagaSynthFluidSynth() override;

    virtual void playNote(const NN_Note_t &note, int channel = 0, float pan = 0.0) override;
    virtual void stopNote(const NN_Note_t &note) override;
    virtual void stopAllNotes(NoteNagaMidiSeq *seq = nullptr, NoteNagaTrack *track = nullptr) override;
    virtual void renderAudio(float* left, float* right, size_t num_frames) override;
    virtual void setMasterPan(float pan) override;

    virtual std::string getConfig(const std::string &key) const override;
    virtual bool setConfig(const std::string &key, const std::string &value) override;
    virtual std::vector<std::string> getSupportedConfigKeys() const override;

    /**
     * @brief Get the current SoundFont path
     * @return The path to the currently loaded SoundFont file
     */
    std::string getSoundFontPath() const { return sf2_path_; }

    /**
     * @brief Change the SoundFont file at runtime
     * @param sf2_path Path to the new SoundFont file
     * @return True if the SoundFont was successfully loaded
     */
    bool setSoundFont(const std::string &sf2_path);

    /**
     * @brief Check if the synthesizer is valid and ready to use
     * @return True if a SoundFont is loaded and the synth is operational
     */
    bool isValid() const { return soundfont_loaded_.load(std::memory_order_acquire); }

    /**
     * @brief Get the last error message
     * @return Error message string, empty if no error
     */
    std::string getLastError() const { return last_error_; }

    /**
     * @brief Check if the SoundFont is currently being loaded asynchronously
     * @return True if loading is in progress
     */
    bool isLoading() const { return loading_in_progress_.load(std::memory_order_acquire); }

    /**
     * @brief Set a callback to be invoked when async SoundFont loading completes
     * @param callback Function to call with success status
     */
    void setLoadCompletedCallback(std::function<void(bool)> callback) {
        load_completed_callback_ = std::move(callback);
    }

protected:
    // Mutex for thread-safe access to the synthesizer
    std::mutex synth_mutex_;
    
    // Atomic flag to indicate synth is ready for rendering
    std::atomic<bool> synth_ready_{true};
    
    // Atomic flag to indicate if SoundFont is successfully loaded
    std::atomic<bool> soundfont_loaded_{false};
    
    // Atomic flag to indicate if async loading is in progress
    std::atomic<bool> loading_in_progress_{false};

    // FluidSynth interní struktury
    fluid_settings_t *synth_settings_;
    fluid_synth_t *fluidsynth_;

    // Store the current SoundFont path
    std::string sf2_path_;
    
    // Last error message
    std::string last_error_;
    
    // Background loading thread
    std::thread load_thread_;
    
    // Callback for async load completion
    std::function<void(bool)> load_completed_callback_;

    void ensureFluidsynth();
    
    /**
     * @brief Internal method to load SoundFont (can be called from background thread)
     */
    void loadSoundFontInternal();
};