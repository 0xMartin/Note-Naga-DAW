#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <note_naga_engine/module/metronome.h>
#include <note_naga_engine/module/spectrum_analyzer.h>
#include <note_naga_engine/module/pan_analyzer.h>
#include <note_naga_engine/core/runtime_data.h>

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

private:
    std::mutex dsp_engine_mutex_;
    std::vector<INoteNagaSoftSynth*> synths_;
    std::vector<NoteNagaDSPBlockBase*> dsp_blocks_;
    
    // Mapping from synth to its DSP blocks
    std::map<INoteNagaSoftSynth*, std::vector<NoteNagaDSPBlockBase*>> synth_dsp_blocks_;
    
    std::vector<float> mix_left_;
    std::vector<float> mix_right_;
    std::vector<float> temp_left_;
    std::vector<float> temp_right_;
    
    float output_volume_ = 1.0f;
    float last_rms_left_ = -100.0f;
    float last_rms_right_ = -100.0f;
    bool enable_dsp_ = true;
    
    NoteNagaMetronome* metronome_ = nullptr;
    NoteNagaSpectrumAnalyzer* spectrum_analyzer_ = nullptr;
    NoteNagaPanAnalyzer* pan_analyzer_ = nullptr;
    
    void calculateRMS(float *left, float *right, size_t numFrames);
};