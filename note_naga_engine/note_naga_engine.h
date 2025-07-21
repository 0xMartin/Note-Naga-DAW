#pragma once

#ifndef QT_DEACTIVATED
#include <QObject>
#endif 

#include <vector>
#include <string>

#include "note_naga_api.h"
#include "core/types.h"
#include "core/project_data.h"
#include "core/mixer.h"
#include "worker/playback_worker.h"

#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaEngine : public QObject {
    Q_OBJECT
#else
class NoteNagaEngine {
#endif

public:
    explicit NoteNagaEngine();
    ~NoteNagaEngine();

    // --- Initialization ---
    bool init();

    // --- Playback Control ---
    void start_playback();
    void stop_playback();
    void set_playback_position(int tick);
    bool is_playing() const { return playback_worker ? playback_worker->is_playing() : false; }

    // --- Project Control ---
    bool load_project(const std::string& midi_file_path);
    void change_tempo(int new_tempo);

    // --- Mixer Control ---
    void mute_track(NoteNagaTrack* track, bool mute = true);
    void solo_track(NoteNagaTrack* track, bool solo = true);

    // --- Getters for main components ---
    NoteNagaProject* get_project() { return this->project; }
    NoteNagaMixer* get_mixer() { return this->mixer; }
    PlaybackWorker* get_playback_worker() { return this->playback_worker; }

protected:
    NoteNagaProject *project;
    NoteNagaMixer *mixer;
    PlaybackWorker *playback_worker;
};
