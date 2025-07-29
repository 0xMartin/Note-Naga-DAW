#pragma once

#include <note_naga_engine/core/types.h>
#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <note_naga_engine/module/metronome.h>
#include <note_naga_engine/core/project_data.h>
#include <vector>
#include <mutex>

/** 
 * @brief NoteNagaDSPEngine is the main DSP engine for the Note Naga project.
 * It manages audio rendering, synthesizers, and the metronome.
 * It provides methods to add/remove synthesizers and render audio blocks.
 */
class NoteNagaDSPEngine {
public:
    /**
     * @brief Construct a new NoteNagaDSPEngine object.
     * @param project Pointer to the NoteNagaProject instance.
     */
    NoteNagaDSPEngine(NoteNagaProject* project);

    /**
     * @brief Render audio for the current project.
     * @param output Pointer to the output buffer (interleaved left/right).
     * @param num_frames Number of frames to render.
     */
    void render(float* output, size_t num_frames);

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
     * @brief Get the current project.
     * @return Pointer to the NoteNagaProject instance.
     */
    void setOutputVolume(float volume); 

    /**
     * @brief Get the last RMS values for left and right channels.
     * @return Pair of RMS values (left, right).
     */
    std::pair<float, float> getCurrentVolumeDb() const;

    /**
     * @brief Get the metronome instance.
     * @return Pointer to the NoteNagaMetronome instance.
     */
    NoteNagaMetronome* metronome() { return &metronome_; }

private:
    NoteNagaProject *project_; ///< Pointer to the associated project
    std::vector<INoteNagaSoftSynth*> synths_; ///< List of synthesizers managed by the engine
    std::vector<NoteNagaDSPBlockBase*> dsp_blocks_;
    std::mutex dsp_engine_mutex_; ///< Mutex for thread-safe access to the DSP blocks list, synths, and configuration

    // Output volume level (0.0 to 1.0)
    float output_volume_ = 1.0f; ///< Output volume level (0.0 to 1.0)

    // Mixing buffers (always resized as needed)
    std::vector<float> mix_left_;
    std::vector<float> mix_right_;

    // Last RMS values for visualization
    float last_rms_left_ = -100.0f;
    float last_rms_right_ = -100.0f;

    NoteNagaMetronome metronome_;

    /**
     * @brief Calculate RMS values for the current mix.
     * @param left Pointer to the left channel buffer.
     * @param right Pointer to the right channel buffer.
     * @param numFrames Number of frames in the buffers.
     */
    void calculateRMS(float* left, float* right, size_t numFrames);
};