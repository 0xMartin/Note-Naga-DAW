#pragma once

#include <note_naga_engine/core/dsp_block_base.h>

/**
 * @brief Factory function to create a simple audio gain block.
 * 
 * This function creates a DSP block that applies a gain factor to the audio signal.
 * 
 * @param gain Gain factor to apply (default is 1.0, which means no change).
 * @return Pointer to the created DSP block.
 */
NoteNagaDSPBlockBase* nn_create_audio_gain_block(float gain = 1.0f);