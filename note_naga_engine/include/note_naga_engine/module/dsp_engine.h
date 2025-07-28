#pragma once

#include <vector>
#include <mutex>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/core/note_naga_synthesizer.h>

class NoteNagaDSPEngine {
public:
    void addSynth(INoteNagaSoftSynth* synth);
    void removeSynth(INoteNagaSoftSynth* synth);

    // Main mix/render function called from AudioWorker for each block
    void renderBlock(float* output, size_t num_frames);

private:
    std::vector<INoteNagaSoftSynth*> synths_;
    std::mutex synths_mutex_;

    // Mixing buffers (always resized as needed)
    std::vector<float> mix_left_;
    std::vector<float> mix_right_;
};