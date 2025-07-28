#include <note_naga_engine/note_naga_engine.h>

#include <note_naga_engine/logger.h>
#include <note_naga_engine/note_naga_version.h>
#include <note_naga_engine/synth/synth_fluidsynth.h>

NoteNagaEngine::NoteNagaEngine()
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
    this->project = nullptr;
    this->mixer = nullptr;
    this->playback_worker = nullptr;
    NOTE_NAGA_LOG_INFO("Instance created. Version: " + std::string(NOTE_NAGA_VERSION_STR));
}

NoteNagaEngine::~NoteNagaEngine() {
    if (playback_worker) playback_worker->stop();
    if (mixer) mixer->close();

    if (mixer) {
        delete mixer;
        mixer = nullptr;
    }

    if (playback_worker) {
        delete playback_worker;
        playback_worker = nullptr;
    }

    if (project) {
        delete project;
        project = nullptr;
    }

    for (auto* synth : this->synthesizers) {
        delete synth;
    }

    NOTE_NAGA_LOG_INFO("Instance destroyed");
}

bool NoteNagaEngine::initialize() {
    // Initialize synthesizers
    if (this->synthesizers.empty()) {
        this->synthesizers.push_back(new NoteNagaSynthFluidSynth("FluidSynth 1", "./FluidR3_GM.sf2"));
    }

    // project
    if (!this->project) this->project = new NoteNagaProject();

    // mixer
    if (!this->mixer) {
        this->mixer = new NoteNagaMixer(this->project, &this->synthesizers);
    }
    
    // playback worker
    if (!this->playback_worker) {
        this->playback_worker = new PlaybackWorker(this->project, this->mixer, 30.0);

        this->playback_worker->addFinishedCallback([this]() {
            if (mixer) mixer->stopAllNotes();
            NN_QT_EMIT(this->playbackStopped());
        });
    }

    bool status = this->project && this->mixer && this->playback_worker && !this->synthesizers.empty();
    if (status) {
        NOTE_NAGA_LOG_INFO("Initialized successfully");
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to initialize Note Naga Engine components");
    }
    return status;
}

bool NoteNagaEngine::loadProject(const std::string &midi_file_path) {
    if (!this->project) { 
        NOTE_NAGA_LOG_ERROR("Project is not initialized");
        return false; 
    }
    this->stopPlayback();
    return this->project->loadProject(midi_file_path);
}

bool NoteNagaEngine::startPlayback() {
    if (playback_worker) {
        if (playback_worker->play()) {
            NN_QT_EMIT(this->playbackStarted());
            return true;
        }
    }
    NOTE_NAGA_LOG_WARNING("Failed to start playback");
    return false;
}

bool NoteNagaEngine::stopPlayback() {
    if (playback_worker) {
        if (playback_worker->stop()) {
            // playbackStopped is emitted from thread finished callback
            return true;
        }
    }
    if (mixer) mixer->stopAllNotes();
    NOTE_NAGA_LOG_WARNING("Failed to stop playback");
    return false;
}

void NoteNagaEngine::setPlaybackPosition(int tick) {
    if (playback_worker && playback_worker->isPlaying()) { playback_worker->stop(); }
    if (this->project) { 
        this->project->setCurrentTick(tick); 
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to set playback position: Project is not initialized");
    }
}

void NoteNagaEngine::changeTempo(int new_tempo) {
    if (this->project) { 
        this->project->setTempo(new_tempo); 
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to change tempo: Project is not initialized");
    }
    if (playback_worker) {
        playback_worker->recalculateWorkerTempo();
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to change tempo: Playback worker is not initialized");
    }
}

void NoteNagaEngine::muteTrack(NoteNagaTrack *track, bool mute) {
    if (mixer) {
        mixer->muteTrack(track, mute);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to mute track: Mixer is not initialized");
    }
}

void NoteNagaEngine::soloTrack(NoteNagaTrack *track, bool solo) {
    if (mixer) {
        mixer->soloTrack(track, solo);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to solo track: Mixer is not initialized");
    }
}

void NoteNagaEngine::enableLooping(bool enabled) {
    if (playback_worker) {
        playback_worker->enableLooping(enabled);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to enable looping: Playback worker is not initialized");
    }
}