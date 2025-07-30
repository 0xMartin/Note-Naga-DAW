#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <note_naga_engine/module/metronome.h>
#include <note_naga_engine/module/spectrum_analyzer.h>
#include <note_naga_engine/core/project_data.h>

#include <vector>
#include <mutex>

/** 
 * @brief NoteNagaDSPEngine is the main DSP engine for the Note Naga project.
 * It manages audio rendering, synthesizers, and the metronome.
 * It provides methods to add/remove synthesizers and render audio blocks.
 */
class NOTE_NAGA_ENGINE_API NoteNagaDSPEngine {
public:
    /**
     * @brief Construct a new NoteNagaDSPEngine object.
     * @param metronome Pointer to the metronome instance.
     * @param spectrum_analyzer Pointer to the spectrum analyzer instance.
     */
    NoteNagaDSPEngine(NoteNagaMetronome* metronome, NoteNagaSpectrumAnalyzer * spectrum_analyzer);

    /**
     * @brief Render audio for the current project.
     * @param output Pointer to the output buffer (interleaved left/right).
     * @param num_frames Number of frames to render.
     */
    void render(float* output, size_t num_frames);

    /**
     * @brief Enable or disable DSP processing.
     * @param enable True to enable, false to disable.
     */
    void setEnableDSP(bool enable);

    /**
     * @brief Check if DSP processing is enabled.
     * @return True if enabled, false otherwise.
     */
    bool isDSPEnabled() const { return enable_dsp_; }

    /**
     * @brief Add a synthesizer to the DSP engine.
     * @param synth Pointer to the synthesizer to add.
     */
    void addSynth(INoteNagaSoftSynth* synth);

    /**
     * @brief Remove a synthesizer from the DSP engine.
     * @param synth Pointer to the synthesizer to remove.
     */
    void removeSynth(INoteNagaSoftSynth* synth);

    /**
     * @brief Add a DSP block to the engine.
     * @param block Pointer to the DSP block to add.
     */
    void addDSPBlock(NoteNagaDSPBlockBase* block);

    /**
     * @brief Remove a DSP block from the engine.
     * @param block Pointer to the DSP block to remove.
     */
    void removeDSPBlock(NoteNagaDSPBlockBase* block);

    /**
     * @brief Reorder DSP block in the processing chain.
     * @param from_idx Index of block to move.
     * @param to_idx Target index.
     */
    void reorderDSPBlock(int from_idx, int to_idx);

    /**
     * @brief Get the current project.
     * @return Pointer to the NoteNagaProject instance.
     */
    void setOutputVolume(float volume); 

    /**
     * @brief Get the last RMS values for left and right channels.
     * @return Pair of RMS values (left, right).
     */
    std::pair<float, float> getCurrentVolumeDb() const;

private:
    NoteNagaProject *project_; ///< Pointer to the associated project
    std::vector<INoteNagaSoftSynth*> synths_; ///< List of synthesizers managed by the engine
    std::vector<NoteNagaDSPBlockBase*> dsp_blocks_;
    std::mutex dsp_engine_mutex_; ///< Mutex for thread-safe access to the DSP blocks list, synths, and configuration

    bool enable_dsp_ = true; ///< Flag to enable/disable DSP processing

    // Output volume level (0.0 to 1.0)
    float output_volume_ = 1.0f; ///< Output volume level (0.0 to 1.0)

    // Mixing buffers (always resized as needed)
    std::vector<float> mix_left_;
    std::vector<float> mix_right_;

    // Last RMS values for visualization
    float last_rms_left_ = -100.0f;
    float last_rms_right_ = -100.0f;

    // Pointer to engine components
    NoteNagaMetronome* metronome_;
    NoteNagaSpectrumAnalyzer *spectrum_analyzer_; 

    /**
     * @brief Calculate RMS values for the current mix.
     * @param left Pointer to the left channel buffer.
     * @param right Pointer to the right channel buffer.
     * @param numFrames Number of frames in the buffers.
     */
    void calculateRMS(float* left, float* right, size_t numFrames);
};