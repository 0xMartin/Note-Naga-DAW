#pragma once

#include <RtAudio.h>
#include <note_naga_engine/module/dsp_engine.h>

/**
 * NoteNagaAudioWorker - zajišťuje audio výstup a callback pro DSP řetězec.
 */
class NoteNagaAudioWorker {
public:
    NoteNagaAudioWorker(NoteNagaDSPEngine* dsp);
    ~NoteNagaAudioWorker();

    void setDSPEngine(NoteNagaDSPEngine* dsp);

    // Startuje audio zařízení s daným sample rate a block size.
    void start(unsigned int sampleRate, unsigned int blockSize);

    // Zastaví audio zařízení (stream). Lze volat opakovaně.
    void stop();

    // Callback volaný RtAudio, naplňuje výstupní buffer audio daty.
    static int audioCallback(void* outputBuffer, void*, unsigned int nFrames,
                            double, RtAudioStreamStatus, void* userData);

private:
    RtAudio audio_;
    NoteNagaDSPEngine* dspEngine_ = nullptr;
    unsigned int sampleRate_ = 44100;
    unsigned int blockSize_ = 512;
    bool stream_open_ = false;
};