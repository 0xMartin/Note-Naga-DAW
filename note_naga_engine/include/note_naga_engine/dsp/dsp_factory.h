#pragma once

#include <functional>
#include <note_naga_engine/core/dsp_block_base.h>
#include <string>
#include <vector>

/**
 * @brief Factory function to create a simple audio gain block.
 *
 * This function creates a DSP block that applies a gain factor to the audio signal.
 *
 * @param gain Gain factor to apply (default is 0.0, which means no change).
 * @return Pointer to the created DSP block.
 */
NoteNagaDSPBlockBase *nn_create_audio_gain_block(float gain = 0.0f);

/**
 * @brief Factory function to create a simple audio pan block.
 *
 * This function creates a DSP block that applies panning to the audio signal.
 *
 * @param pan Pan value to apply (default is 0.0, which means center).
 * @return Pointer to the created DSP block.
 */
NoteNagaDSPBlockBase *nn_create_audio_pan_block(float pan = 0.0f);

/**
 * @brief Factory function to create a single EQ audio block.
 *
 * This function creates a DSP block that applies a single band EQ to the audio signal.
 *
 * @param frequency Center frequency of the EQ band (default is 1000.0 Hz).
 * @param gain Gain applied at the center frequency (default is 0.0 dB).
 * @param q Quality factor of the EQ band (default is 1.0).
 * @return Pointer to the created DSP block.
 */
NoteNagaDSPBlockBase *nn_create_single_band_eq_block(float freq = 1000.0f, float gain = 0.0f,
                                                     float q = 1.0f);

/**
 * @brief Factory function to create a compressor audio block. This function creates a DSP block
 * that applies compression to the audio signal.
 * @param threshold Threshold level for compression in dB (default is -18.0 dB).
 * @param ratio Compression ratio (default is 4.0).
 * @param attack Attack time in milliseconds (default is 10.0 ms).
 * @param release Release time in milliseconds (default is 80.0 ms).
 * @param makeup Makeup gain in dB (default is 0.0 dB).
 * @return Pointer to the created DSP block.
 */
NoteNagaDSPBlockBase *nn_create_compressor_block(float threshold = -18.0f, float ratio = 4.0f,
                                                 float attack = 10.0f, float release = 80.0f,
                                                 float makeup = 0.0f);

/**
 * @brief Factory function to create a multi-band EQ audio block.
 * This function creates a DSP block that applies a multi-band EQ to the audio signal.
 * @param bands Frequencies of the EQ bands (default is {100, 500, 1000, 3000, 8000} Hz).
 * @param q Quality factor for the EQ bands (default is 1.0).
 * @return Pointer to the created DSP block. 
 */                                                 
NoteNagaDSPBlockBase *
nn_create_multi_band_eq_block(const std::vector<float> &bands = {100, 500, 1000, 3000, 8000},
                              float q = 1.0f);

////////////////////////////////////////////////////////////////////////////////////////////////////////
// List of all factory functions for DSP blocks
////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Factory entry for creating DSP blocks.
 */
struct DSPBlockFactoryEntry {
    std::string name;
    std::function<NoteNagaDSPBlockBase *()> create;
};

/**
 * @brief Factory class for creating DSP blocks.
 */
class DSPBlockFactory {
public:
    static const std::vector<DSPBlockFactoryEntry> &allBlocks() {
        static std::vector<DSPBlockFactoryEntry> blocks = {
            {"Gain", []() { return nn_create_audio_gain_block(); }},
            {"Pan", []() { return nn_create_audio_pan_block(); }},
            {"Single EQ", []() { return nn_create_single_band_eq_block(); }},
            {"Compressor", []() { return nn_create_compressor_block(); }},
            {"Multi Band EQ", []() { return nn_create_multi_band_eq_block(); }}
        };
        return blocks;
    }
};