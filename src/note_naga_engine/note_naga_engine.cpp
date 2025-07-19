#include "note_naga_engine.h"

NoteNagaEngine::NoteNagaEngine(QObject* parent)
    : QObject(parent)
{
    // set default configuration ...
}

NoteNagaEngine::~NoteNagaEngine()
{
    if (playback_worker) playback_worker->stop();
    if (mixer) mixer->close();
}

bool NoteNagaEngine::init()
{
    this->project_data = std::make_shared<NoteNagaProjectData>();
    this->mixer = std::make_unique<Mixer>(this->project_data);
    this->playback_worker = std::make_unique<PlaybackWorker>(this->project_data, this->mixer.get(), 30.0);
    // configuration loading ...
    return true;
}

bool NoteNagaEngine::load_project(const QString &midi_file_path)
{
    return this->project_data->load_project(midi_file_path);
}

void NoteNagaEngine::start_playback()
{
    if (playback_worker)playback_worker->play();
}

void NoteNagaEngine::stop_playback()
{
    if (playback_worker) playback_worker->stop();
}

void NoteNagaEngine::set_playback_position(int tick)
{
    // get active sequence from project data
    std::shared_ptr<NoteNagaMIDISequence> active_sequence = this->project_data->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot set playback position.";
        return;
    }

    playback_worker->stop();
    active_sequence->set_current_tick(tick);
}

void NoteNagaEngine::mute_track(int track_id, bool mute)
{
    if (mixer)
        mixer->mute_track(track_id, mute);
}

void NoteNagaEngine::solo_track(int track_id, bool solo)
{
    if (mixer)
        mixer->solo_track(track_id, solo);
}

void NoteNagaEngine::set_project_data(const std::shared_ptr<NoteNagaProjectData>& data)
{
    project_data = data;
    // Reinitialize mixer and playback worker with new project data if needed
}