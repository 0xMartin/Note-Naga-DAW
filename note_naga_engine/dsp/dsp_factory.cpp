#include <note_naga_engine/dsp/dsp_factory.h>

#include <note_naga_engine/dsp/dsp_block_gain.h>

NoteNagaDSPBlockBase* nn_create_audio_gain_block(float gain) {
    return new DSPBlockGain(gain);
}
