#include <note_naga_engine/note_naga_engine.h>

NoteNagaEngine::NoteNagaEngine()
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
    this->project = nullptr;
    this->mixer = nullptr;
    this->playback_worker = nullptr;
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
}

bool NoteNagaEngine::initialize() {
    if (!this->project) this->project = new NoteNagaProject();
    if (!this->mixer) this->mixer = new NoteNagaMixer(this->project, "./FluidR3_GM.sf2");
    if (!this->playback_worker) {
        this->playback_worker = new PlaybackWorker(this->project, this->mixer, 30.0);

        this->playback_worker->addFinishedCallback([this]() {
            if (mixer) mixer->stopAllNotes();
        });
    }
    return this->project && this->mixer && this->playback_worker;
}

bool NoteNagaEngine::loadProject(const std::string &midi_file_path) {
    if (!this->project) {
        return false;
    }
    this->stopPlayback();
    return this->project->loadProject(midi_file_path);
}

void NoteNagaEngine::startPlayback() {
    if (playback_worker) playback_worker->play();
}

void NoteNagaEngine::stopPlayback() {
    if (playback_worker) playback_worker->stop();
    if (mixer) mixer->stopAllNotes();
}

void NoteNagaEngine::setPlaybackPosition(int tick) {
    if (playback_worker && playback_worker->isPlaying()) {
        playback_worker->stop();
    }
    if (this->project) {
        this->project->setCurrentTick(tick);
    }
}

void NoteNagaEngine::changeTempo(int new_tempo) {
    if (this->project) {
        this->project->setTempo(new_tempo);
    }
    if (playback_worker) playback_worker->recalculateWorkerTempo();
}

void NoteNagaEngine::muteTrack(NoteNagaTrack *track, bool mute) {
    if (mixer) mixer->muteTrack(track, mute);
}

void NoteNagaEngine::soloTrack(NoteNagaTrack *track, bool solo) {
    if (mixer) mixer->soloTrack(track, solo);
}