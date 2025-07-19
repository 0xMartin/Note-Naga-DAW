#pragma once

#include <QObject>
#include <vector>
#include <memory>
#include <optional>
#include <QString>

#include "core/project_data.h"
#include "core/types.h"
#include "core/mixer.h"
#include "worker/playback_worker.h"

class NoteNagaEngine : public QObject
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
    void mute_track(int track_id, bool mute = true);
    void solo_track(int track_id, bool solo = true);

    // --- Getters & Setters ---
    std::shared_ptr<NoteNagaProjectData> get_project_data() { return this->project_data; }
    void set_project_data(const std::shared_ptr<NoteNagaProjectData>& data);

    NoteNagaMixer* get_mixer() { return this->mixer.get(); }
    PlaybackWorker* get_playback_worker() { this->playback_worker.get(); }

protected:
    std::shared_ptr<NoteNagaProjectData> project_data;
    std::unique_ptr<NoteNagaMixer> mixer;
    std::unique_ptr<PlaybackWorker> playback_worker;
};
