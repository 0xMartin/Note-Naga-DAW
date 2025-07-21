#include "note_naga_engine.h"

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

bool NoteNagaEngine::init() {
    if (!this->project) this->project = new NoteNagaProject();
    if (!this->mixer) this->mixer = new NoteNagaMixer(this->project);
    if (!this->playback_worker) {
        this->playback_worker = new PlaybackWorker(this->project, this->mixer, 30.0);

        this->playback_worker->add_finished_callback([this]() {
            if (mixer) mixer->stop_all_notes();
        });
    }
    return this->project && this->mixer && this->playback_worker;
}

bool NoteNagaEngine::load_project(const std::string &midi_file_path) {
    if (!this->project) {
#ifndef QT_DEACTIVATED
        qWarning("NoteNagaEngine: Project is not initialized.");
#else
        std::cerr << "NoteNagaEngine: Project is not initialized." << std::endl;
#endif
        return false;
    }
    this->stop_playback();
    return this->project->load_project(midi_file_path);
}

void NoteNagaEngine::start_playback() {
    if (playback_worker) playback_worker->play();
}

void NoteNagaEngine::stop_playback() {
    if (playback_worker) playback_worker->stop();
    if (mixer) mixer->stop_all_notes();
}

void NoteNagaEngine::set_playback_position(int tick) {
    if (playback_worker && playback_worker->is_playing()) {
        playback_worker->stop();
    }
    if (this->project) {
        this->project->set_current_tick(tick);
    }
}

void NoteNagaEngine::change_tempo(int new_tempo) {
    if (this->project) {
        this->project->set_tempo(new_tempo);
    }
    if (playback_worker) playback_worker->recalculate_worker_tempo();
}

void NoteNagaEngine::mute_track(NoteNagaTrack *track, bool mute) {
    if (mixer) mixer->mute_track(track, mute);
}

void NoteNagaEngine::solo_track(NoteNagaTrack *track, bool solo) {
    if (mixer) mixer->solo_track(track, solo);
}