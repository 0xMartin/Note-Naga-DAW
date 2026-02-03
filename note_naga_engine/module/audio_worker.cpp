#include <note_naga_engine/module/audio_worker.h>

#include <cstring>
#include <vector>

NoteNagaAudioWorker::NoteNagaAudioWorker(NoteNagaDSPEngine *dsp) {
    this->setDSPEngine(dsp);
    NOTE_NAGA_LOG_INFO("Audio worker initialized");
}

NoteNagaAudioWorker::~NoteNagaAudioWorker() { stop(); }

void NoteNagaAudioWorker::setDSPEngine(NoteNagaDSPEngine *dsp) { this->dsp_engine = dsp; }

bool NoteNagaAudioWorker::start(unsigned int sampleRate, unsigned int blockSize) {
    if (this->stream_open) {
        NOTE_NAGA_LOG_WARNING("Audio worker is already running");
        return false;
    }

    this->sample_rate = sampleRate;
    this->block_size = blockSize;

    // Get list of available devices
    std::vector<unsigned int> deviceIds = this->audio.getDeviceIds();
    if (deviceIds.empty()) {
        NOTE_NAGA_LOG_ERROR("No audio output devices found");
        return false;
    }

    // Try default device first, then fall back to other devices
    unsigned int defaultDevice = this->audio.getDefaultOutputDevice();
    std::vector<unsigned int> devicesToTry;
    devicesToTry.push_back(defaultDevice);
    for (unsigned int id : deviceIds) {
        if (id != defaultDevice) {
            devicesToTry.push_back(id);
        }
    }

    for (unsigned int deviceId : devicesToTry) {
        try {
            RtAudio::DeviceInfo info = this->audio.getDeviceInfo(deviceId);
            if (info.outputChannels < 1) {
                continue; // Skip devices with no output channels
            }

            RtAudio::StreamParameters params;
            params.deviceId = deviceId;
            params.nChannels = std::min(2u, info.outputChannels); // Use available channels, max 2
            params.firstChannel = 0;

            NOTE_NAGA_LOG_INFO("Trying audio device: " + info.name + 
                             " (channels: " + std::to_string(params.nChannels) + ")");

            // Close any existing stream before trying new device
            if (this->audio.isStreamOpen()) {
                this->audio.closeStream();
            }

            this->audio.openStream(&params, nullptr, RTAUDIO_FLOAT32, 
                                   this->sample_rate, &this->block_size,
                                   &NoteNagaAudioWorker::audioCallback, this);
            
            // RtAudio 6.x doesn't throw exceptions - check if stream actually opened
            if (!this->audio.isStreamOpen()) {
                NOTE_NAGA_LOG_WARNING("Failed to open stream on device: " + info.name);
                continue;
            }

            this->audio.startStream();
            
            // Check if stream is actually running
            if (!this->audio.isStreamRunning()) {
                NOTE_NAGA_LOG_WARNING("Failed to start stream on device: " + info.name);
                this->audio.closeStream();
                continue;
            }

            this->stream_open = true;
            this->output_channels = params.nChannels;

            NOTE_NAGA_LOG_INFO("Audio worker started on device: " + info.name);
            return true;
        } catch (const std::exception &e) {
            NOTE_NAGA_LOG_WARNING("Failed to open device " + std::to_string(deviceId) + 
                                 ": " + std::string(e.what()));
            // Close stream if it was partially opened
            if (this->audio.isStreamOpen()) {
                try {
                    this->audio.closeStream();
                } catch (...) {}
            }
            continue; // Try next device
        } catch (...) {
            NOTE_NAGA_LOG_WARNING("Failed to open device " + std::to_string(deviceId) + 
                                 ": unknown error");
            if (this->audio.isStreamOpen()) {
                try {
                    this->audio.closeStream();
                } catch (...) {}
            }
            continue;
        }
    }

    NOTE_NAGA_LOG_ERROR("Failed to open any audio output device");
    return false;
}

bool NoteNagaAudioWorker::stop() {
    if (this->stream_open) {
        try {
            if (this->audio.isStreamRunning()) {
                this->audio.stopStream();
                NOTE_NAGA_LOG_INFO("Audio stream stopped");
            } else {
                NOTE_NAGA_LOG_WARNING("Audio stream was not running");
            }
            if (this->audio.isStreamOpen()) {
                this->audio.closeStream();
                NOTE_NAGA_LOG_INFO("Audio stream closed");
            } else {
                NOTE_NAGA_LOG_WARNING("Audio stream was not running or already closed");
            }
        } catch (const std::exception &e) { NOTE_NAGA_LOG_ERROR(e.what()); }
        this->stream_open = false;
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
    unsigned int channels = self->output_channels;

    // Zkontrolujeme, zda je worker ztlumený (muted) nebo nemá DSP engine.
    // .load() bezpečně přečte hodnotu z atomické proměnné.
    if (self->is_muted.load(std::memory_order_relaxed) || !self->dsp_engine) {
        // Pokud ano, vyplníme buffer tichem (nulami).
        std::memset(out, 0, sizeof(float) * nFrames * channels);
    } else {
        // Pokud není ztlumený, renderujeme normálně.
        // DSP engine always renders stereo, so we need temp buffer if mono output
        if (channels == 1) {
            // Render to temp stereo buffer, then mix down to mono
            std::vector<float> stereoBuffer(nFrames * 2);
            self->dsp_engine->render(stereoBuffer.data(), nFrames, true);
            // Mix stereo to mono
            for (unsigned int i = 0; i < nFrames; ++i) {
                out[i] = (stereoBuffer[i * 2] + stereoBuffer[i * 2 + 1]) * 0.5f;
            }
        } else {
            self->dsp_engine->render(out, nFrames, true);
        }
    }
    return 0;
}

void NoteNagaAudioWorker::mute() {
    this->is_muted.store(true, std::memory_order_relaxed);
}

void NoteNagaAudioWorker::unmute() {
    this->is_muted.store(false, std::memory_order_relaxed);
}

bool NoteNagaAudioWorker::isMuted() const {
    return this->is_muted.load(std::memory_order_relaxed);
}