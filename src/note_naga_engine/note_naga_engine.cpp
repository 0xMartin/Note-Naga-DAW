#include "note_naga_engine.h"

NoteNagaEngine::NoteNagaEngine(QObject* parent)
    : QObject(parent)
{
    this->project = nullptr;
    this->mixer = nullptr;
    this->playback_worker = nullptr;
}

NoteNagaEngine::~NoteNagaEngine()
{
    if (playback_worker) playback_worker->stop();
    if (mixer) mixer->close();

    if (project) 
    {
        delete project;
        project = nullptr;
    }
    if (mixer) 
    {
        delete mixer;
        mixer = nullptr;
    }
    if (playback_worker) 
    {
        delete playback_worker;
        playback_worker = nullptr;
    }
}

bool NoteNagaEngine::init()
{
    this->project = new NoteNagaProject();
    this->mixer = new NoteNagaMixer(this->project);
    this->playback_worker = new PlaybackWorker(this->project, this->mixer, 30.0);
    return true;
}

bool NoteNagaEngine::load_project(const QString &midi_file_path)
{
    return this->project->load_project(midi_file_path);
}

void NoteNagaEngine::start_playback()
{
    if (playback_worker) playback_worker->play();
}

void NoteNagaEngine::stop_playback()
{
    if (playback_worker) playback_worker->stop();
}

void NoteNagaEngine::set_playback_position(int tick)
{
    // get active sequence from project data
    NoteNagaMIDISeq *active_sequence = this->project->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot set playback position.";
        return;
    }

    playback_worker->stop();
    active_sequence->set_current_tick(tick);
}

void NoteNagaEngine::mute_track(NoteNagaTrack* track, bool mute)
{
    if (mixer)
        mixer->mute_track(track, mute);
}

void NoteNagaEngine::solo_track(NoteNagaTrack* track, bool solo)
{
    if (mixer)
        mixer->solo_track(track, solo);
}