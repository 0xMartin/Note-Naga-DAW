#pragma once

#include <QObject>
#include <vector>
#include <memory>
#include <optional>
#include <QString>

#include "note_naga_api.h"
#include "core/types.h"
#include "core/project_data.h"
#include "core/mixer.h"
#include "worker/playback_worker.h"

class NOTE_NAGA_ENGINE_API NoteNagaEngine : public QObject
{
    Q_OBJECT

public:
    explicit NoteNagaEngine(QObject* parent = nullptr);
    ~NoteNagaEngine();

    // --- Initialization ---
    bool init();
    bool load_project(const QString& midi_file_path);

    // --- Playback Control ---
    void start_playback();
    void stop_playback();
    void set_playback_position(int tick);

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
