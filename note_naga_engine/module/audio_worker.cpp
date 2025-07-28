#include <note_naga_engine/module/audio_worker.h>

#include <cstring>
#include <iostream>

NoteNagaAudioWorker::NoteNagaAudioWorker(NoteNagaDSPEngine* dsp) {
    this->setDSPEngine(dsp);
}

NoteNagaAudioWorker::~NoteNagaAudioWorker() {
    stop();
}

void NoteNagaAudioWorker::setDSPEngine(NoteNagaDSPEngine* dsp) {
    dspEngine_ = dsp;
}

void NoteNagaAudioWorker::start(unsigned int sampleRate, unsigned int blockSize) {
    // Pokud už běží, nejdřív zastav
    stop();

    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    RtAudio::StreamParameters params;
    params.deviceId = audio_.getDefaultOutputDevice();
    params.nChannels = 2;
    params.firstChannel = 0;

    audio_.openStream(&params, nullptr, RTAUDIO_FLOAT32, sampleRate_, &blockSize_, &NoteNagaAudioWorker::audioCallback, this);
    audio_.startStream();
    stream_open_ = true;
}

void NoteNagaAudioWorker::stop() {
    if (stream_open_) {
        try {
            if (audio_.isStreamRunning()) {
                audio_.stopStream();
            }
            if (audio_.isStreamOpen()) {
                audio_.closeStream();
            }
        } catch (const std::exception& e) {
            std::cerr << "Audio stop error: " << e.what() << std::endl;
        }
        stream_open_ = false;
    }
}

int NoteNagaAudioWorker::audioCallback(void* outputBuffer, void*, unsigned int nFrames, double, RtAudioStreamStatus, void* userData) {
    NoteNagaAudioWorker* self = static_cast<NoteNagaAudioWorker*>(userData);
    float* out = static_cast<float*>(outputBuffer);

    if (self->dspEngine_) {
        self->dspEngine_->renderBlock(out, nFrames);
    } else {
        std::memset(out, 0, sizeof(float) * nFrames * 2);
    }
    return 0;
}