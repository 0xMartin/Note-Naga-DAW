#include <note_naga_engine/module/audio_worker.h>

#include <cstring>

NoteNagaAudioWorker::NoteNagaAudioWorker(NoteNagaDSPEngine *dsp) {
    this->setDSPEngine(dsp);
    NOTE_NAGA_LOG_INFO("Audio worker initialized");
}

NoteNagaAudioWorker::~NoteNagaAudioWorker() { stop(); }

void NoteNagaAudioWorker::setDSPEngine(NoteNagaDSPEngine *dsp) { dspEngine_ = dsp; }

bool NoteNagaAudioWorker::start(unsigned int sampleRate, unsigned int blockSize) {
    if (stream_open_) {
        NOTE_NAGA_LOG_WARNING("Audio worker is already running");
        return false;
    }

    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    RtAudio::StreamParameters params;
    params.deviceId = audio_.getDefaultOutputDevice();
    params.nChannels = 2;
    params.firstChannel = 0;

    audio_.openStream(&params, nullptr, RTAUDIO_FLOAT32, sampleRate_, &blockSize_,
                      &NoteNagaAudioWorker::audioCallback, this);
    audio_.startStream();
    stream_open_ = true;

    NOTE_NAGA_LOG_INFO("Audio worker started");
    return true;
}

bool NoteNagaAudioWorker::stop() {
    if (stream_open_) {
        try {
            if (audio_.isStreamRunning()) {
                audio_.stopStream();
                NOTE_NAGA_LOG_INFO("Audio stream stopped");
            } else {
                NOTE_NAGA_LOG_WARNING("Audio stream was not running");
            }
            if (audio_.isStreamOpen()) {
                audio_.closeStream();
                NOTE_NAGA_LOG_INFO("Audio stream closed");
            } else {
                NOTE_NAGA_LOG_WARNING("Audio stream was not running or already closed");
            }
        } catch (const std::exception &e) { NOTE_NAGA_LOG_ERROR(e.what()); }
        stream_open_ = false;
        NOTE_NAGA_LOG_INFO("Audio worker stopped");
        return true;
    } else {
        NOTE_NAGA_LOG_WARNING("Audio worker is not running");
    }

    return false;
}

int NoteNagaAudioWorker::audioCallback(void *outputBuffer, void *, unsigned int nFrames, double,
                                       RtAudioStreamStatus, void *userData) {
    NoteNagaAudioWorker *self = static_cast<NoteNagaAudioWorker *>(userData);
    float *out = static_cast<float *>(outputBuffer);

    if (self->dspEngine_) {
        self->dspEngine_->render(out, nFrames);
    } else {
        std::memset(out, 0, sizeof(float) * nFrames * 2);
    }
    return 0;
}