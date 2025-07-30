#pragma once

#include <functional>
#include <string>
#include <vector>

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/dsp_block_base.h>

#include <note_naga_engine/dsp/dsp_block_bitcrusher.h>
#include <note_naga_engine/dsp/dsp_block_chorus.h>
#include <note_naga_engine/dsp/dsp_block_compressor.h>
#include <note_naga_engine/dsp/dsp_block_delay.h>
#include <note_naga_engine/dsp/dsp_block_exciter.h>
#include <note_naga_engine/dsp/dsp_block_filter.h>
#include <note_naga_engine/dsp/dsp_block_flanger.h>
#include <note_naga_engine/dsp/dsp_block_gain.h>
#include <note_naga_engine/dsp/dsp_block_limiter.h>
#include <note_naga_engine/dsp/dsp_block_multi_eq.h>
#include <note_naga_engine/dsp/dsp_block_noise_gate.h>
#include <note_naga_engine/dsp/dsp_block_pan.h>
#include <note_naga_engine/dsp/dsp_block_phaser.h>
#include <note_naga_engine/dsp/dsp_block_reverb.h>
#include <note_naga_engine/dsp/dsp_block_saturator.h>
#include <note_naga_engine/dsp/dsp_block_single_eq.h>
#include <note_naga_engine/dsp/dsp_block_tremolo.h>
#include <note_naga_engine/dsp/dsp_block_stereo_imager.h>

/**
 * @brief Factory function to create a simple audio gain block.
 *
 * This function creates a DSP block that applies a gain factor to the audio signal.
 *
 * @param gain Gain factor to apply (default is 0.0, which means no change).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_audio_gain_block(float gain = 0.0f) {
    return new DSPBlockGain(gain);
}

/**
 * @brief Factory function to create a simple audio pan block.
 *
 * This function creates a DSP block that applies panning to the audio signal.
 *
 * @param pan Pan value to apply (default is 0.0, which means center).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_audio_pan_block(float pan = 0.0f) {
    return new DSPBlockPan(pan);
}

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
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_single_band_eq_block(float freq = 1000.0f, float gain = 0.0f,
                                                            float q = 1.0f) {
    return new DSPBlockSingleEQ(freq, gain, q);
}

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
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_compressor_block(float threshold = -18.0f,
                                                        float ratio = 4.0f, float attack = 10.0f,
                                                        float release = 80.0f,
                                                        float makeup = 0.0f) {
    return new DSPBlockCompressor(threshold, ratio, attack, release, makeup);
}

/**
 * @brief Factory function to create a multi-band EQ audio block.
 * This function creates a DSP block that applies a multi-band EQ to the audio signal.
 * @param bands Frequencies of the EQ bands (default is {100, 500, 1000, 3000, 8000} Hz).
 * @param q Quality factor for the EQ bands (default is 1.0).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_multi_band_eq_block(
    const std::vector<float> &bands = {31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000},
    float q = 1.0f) {
    return new DSPBlockMultiSimpleEQ(bands, q);
}

/**
 * @brief Factory function to create a simple limiter audio block.
 *
 * This function creates a DSP block that applies limiting to the audio signal.
 *
 * @param threshold Threshold in dB (default -0.5 dB).
 * @param release Release time in ms (default 50.0 ms).
 * @param makeup Makeup gain in dB (default 0.0 dB).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_limiter_block(float threshold = -0.5f, float release = 50.0f,
                                                     float makeup = 0.0f) {
    return new DSPBlockLimiter(threshold, release, makeup);
}

/**
 * @brief Factory function to create a delay audio block.
 *
 * This function creates a DSP block that applies a delay effect to the audio signal.
 *
 * @param time_ms Delay time in milliseconds (default 400.0 ms).
 * @param feedback Feedback amount (0.0 .. 0.95, default 0.25).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 0.5).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_delay_block(float time_ms = 400.0f, float feedback = 0.25f,
                                                   float mix = 0.5f) {
    return new DSPBlockDelay(time_ms, feedback, mix);
}

/**
 * @brief Factory function to create a reverb audio block.
 *
 * This function creates a DSP block that applies a reverb effect to the audio signal.
 *
 * @param roomsize Room size (0.0 .. 1.0, default 0.7).
 * @param damping Damping (0.0 .. 1.0, default 0.5).
 * @param wet Wet mix (0.0 .. 1.0, default 0.3).
 * @param predelay Predelay in milliseconds (default 40.0 ms).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_reverb_block(float roomsize = 0.7f, float damping = 0.5f,
                                                    float wet = 0.3f, float predelay = 40.0f) {
    return new DSPBlockReverb(roomsize, damping, wet, predelay);
}

/**
 * @brief Factory function to create a bitcrusher audio block.
 *
 * This function creates a DSP block that applies a bitcrusher effect to the audio signal.
 *
 * @param bitDepth Bit depth (4 .. 16, default 8).
 * @param sampleRateReduce Sample rate reduction factor (1 .. 32, default 8).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 1.0).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *
nn_create_bitcrusher_block(float bitDepth = 8.0f, int sampleRateReduce = 8, float mix = 1.0f) {
    return new DSPBlockBitcrusher(bitDepth, sampleRateReduce, mix);
}

/**
 * @brief Factory function to create a tremolo audio block.
 *
 * This function creates a DSP block that applies a tremolo effect (volume modulation) to the audio
 * signal.
 *
 * @param speed Speed of the LFO in Hz (default 5.0 Hz).
 * @param depth Depth of the tremolo effect (0.0 .. 1.0, default 0.8).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 1.0).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_tremolo_block(float speed = 5.0f, float depth = 0.8f,
                                                     float mix = 1.0f) {
    return new DSPBlockTremolo(speed, depth, mix);
}

/**
 * @brief Factory function to create a filter audio block.
 *
 * This function creates a DSP block that applies a multimode filter (lowpass, highpass, bandpass)
 * to the audio signal.
 *
 * @param type Filter type (0: Lowpass, 1: Highpass, 2: Bandpass, default 0).
 * @param cutoff Cutoff frequency in Hz (default 800.0 Hz).
 * @param resonance Resonance (Q factor) of the filter (default 0.7f).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 1.0).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_filter_block(int type = 0, float cutoff = 800.0f,
                                                    float resonance = 0.7f, float mix = 1.0f) {
    return new DSPBlockFilter(static_cast<FilterType>(type), cutoff, resonance, mix);
}

/**
 * @brief Factory function to create a chorus audio block.
 *
 * This function creates a DSP block that applies a chorus effect to the audio signal.
 *
 * @param speed Speed of the LFO in Hz (default 1.2 Hz).
 * @param depth Depth of the chorus effect in milliseconds (default 12.0 ms).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 0.5).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_chorus_block(float speed = 1.2f, float depth = 6.0f,
                                                    float mix = 0.25f) {
    return new DSPBlockChorus(speed, depth, mix);
}

/**
 * @brief Factory function to create a phaser audio block.
 *
 * This function creates a DSP block that applies a phaser effect to the audio signal.
 *
 * @param speed Speed of the LFO in Hz (default 0.6 Hz).
 * @param depth Depth of the phaser effect (0.0 .. 1.0, default 0.8).
 * @param feedback Feedback amount (0.0 .. 0.95, default 0.4).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 0.5).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_phaser_block(float speed = 0.6f, float depth = 0.8f,
                                                    float feedback = 0.4f, float mix = 0.5f) {
    return new DSPBlockPhaser(speed, depth, feedback, mix);
}

/**
 * @brief Factory function to create a flanger audio block.
 *
 * This function creates a DSP block that applies a flanger effect to the audio signal.
 *
 * @param speed Speed of the LFO in Hz (default 0.3 Hz).
 * @param depth Depth of the flanger effect in milliseconds (default 3.0 ms).
 * @param feedback Feedback amount (0.0 .. 0.95, default 0.3).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 0.5).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_flanger_block(float speed = 0.3f, float depth = 3.0f,
                                                     float feedback = 0.3f, float mix = 0.5f) {
    return new DSPBlockFlanger(speed, depth, feedback, mix);
}

/**
 * @brief Factory function to create a noise gate audio block.
 *
 * This function creates a DSP block that applies a noise gate effect to the audio signal.
 *
 * @param threshold Threshold in dBFS (default -40.0 dB).
 * @param attack Attack time in milliseconds (default 5.0 ms).
 * @param release Release time in milliseconds (default 80.0 ms).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *
nn_create_noise_gate_block(float threshold = -40.0f, float attack = 5.0f, float release = 80.0f) {
    return new DSPBlockNoiseGate(threshold, attack, release);
}

/**
 * @brief Factory function to create a saturator audio block.
 *
 * This function creates a DSP block that applies soft clipping saturation to the audio signal.
 *
 * @param drive Drive amount (1.0 .. 10.0, default 2.0).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 0.7).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_saturator_block(float drive = 2.0f, float mix = 0.7f) {
    return new DSPBlockSaturator(drive, mix);
}

/**
 * @brief Factory function to create an exciter audio block.
 *
 * This function creates a DSP block that applies an exciter effect to the audio signal,
 * enhancing high frequencies.
 *
 * @param freq Center frequency for the exciter (default 4000.0 Hz).
 * @param drive Drive amount (1.0 .. 10.0, default 4.0).
 * @param mix Dry/Wet mix (0.0 .. 1.0, default 0.6).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_exciter_block(float freq = 4000.0f, float drive = 4.0f,
                                                     float mix = 0.6f) {
    return new DSPBlockExciter(freq, drive, mix);
}

/**
 * @brief Factory function to create a stereo imager audio block.
 *
 * This function creates a DSP block that applies mid/side processing to control stereo width.
 *
 * @param width Stereo width (-1.0 = mono, 0.0 = normal, 1.0 = super wide, default 0.0).
 * @return Pointer to the created DSP block.
 */
NOTE_NAGA_ENGINE_API inline NoteNagaDSPBlockBase *nn_create_stereo_imager_block(float width = 0.0f) {
    return new DSPBlockStereoImager(width);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// List of all factory functions for DSP blocks
////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Factory entry for creating DSP blocks.
 */
struct NOTE_NAGA_ENGINE_API DSPBlockFactoryEntry {
    std::string name;
    std::function<NoteNagaDSPBlockBase *()> create;
};

/**
 * @brief Factory class for creating DSP blocks.
 */
class NOTE_NAGA_ENGINE_API DSPBlockFactory {
public:
    static const std::vector<DSPBlockFactoryEntry> &allBlocks() {
        static std::vector<DSPBlockFactoryEntry> blocks = {
            {"Gain", []() { return nn_create_audio_gain_block(); }},
            {"Pan", []() { return nn_create_audio_pan_block(); }},
            {"Single EQ", []() { return nn_create_single_band_eq_block(); }},
            {"Multi Band EQ", []() { return nn_create_multi_band_eq_block(); }},
            {"Compressor", []() { return nn_create_compressor_block(); }},
            {"Limiter", []() { return nn_create_limiter_block(); }},
            {"Delay", []() { return nn_create_delay_block(); }},
            {"Reverb", []() { return nn_create_reverb_block(); }},
            {"Bitcrusher", []() { return nn_create_bitcrusher_block(); }},
            {"Tremolo", []() { return nn_create_tremolo_block(); }},
            {"Filter", []() { return nn_create_filter_block(); }},
            {"Chorus", []() { return nn_create_chorus_block(); }},
            {"Phaser", []() { return nn_create_phaser_block(); }},
            {"Flanger", []() { return nn_create_flanger_block(); }},
            {"Noise Gate", []() { return nn_create_noise_gate_block(); }},
            {"Saturator", []() { return nn_create_saturator_block(); }},
            {"Exciter", []() { return nn_create_exciter_block(); }},
            {"Stereo Imager", []() { return nn_create_stereo_imager_block(); }}
        };
        return blocks;
    }
};