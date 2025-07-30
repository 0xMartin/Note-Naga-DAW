#include <note_naga_engine/dsp/dsp_factory.h>

#include <note_naga_engine/dsp/dsp_block_compressor.h>
#include <note_naga_engine/dsp/dsp_block_gain.h>
#include <note_naga_engine/dsp/dsp_block_multi_eq.h>
#include <note_naga_engine/dsp/dsp_block_pan.h>
#include <note_naga_engine/dsp/dsp_block_single_eq.h>

NoteNagaDSPBlockBase *nn_create_audio_gain_block(float gain) { return new DSPBlockGain(gain); }

NoteNagaDSPBlockBase *nn_create_audio_pan_block(float pan) { return new DSPBlockPan(pan); }

NoteNagaDSPBlockBase *nn_create_single_band_eq_block(float frequency, float gain, float q) {
    return new DSPBlockSingleEQ(frequency, gain, q);
}

NoteNagaDSPBlockBase *nn_create_compressor_block(float threshold, float ratio, float attack,
                                                 float release, float makeup) {
    return new DSPBlockCompressor(threshold, ratio, attack, release, makeup);
}

NoteNagaDSPBlockBase *nn_create_multi_band_eq_block(const std::vector<float> &bands, float q) {
    return new DSPBlockMultiSimpleEQ(bands, q);
}
