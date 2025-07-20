#pragma once

#include <QObject>
#include <QThread>
#include <functional>
#include <chrono>

#include "../note_naga_api.h"
#include "../core/project_data.h"
#include "../core/mixer.h"

// PlaybackWorker manages playback in a separate thread.
// It emits signals when playback finishes and playing state changes.

class NOTE_NAGA_ENGINE_API PlaybackWorker : public QObject
{
    Q_OBJECT
public:
    explicit PlaybackWorker(NoteNagaProject *project, NoteNagaMixer * mixer, double timer_interval_ms, QObject* parent = nullptr);

    bool is_playing() const { return playing; }
    void recalculate_worker_tempo();
    bool play();
    bool stop();

signals:
    void finished_signal();
    void on_position_changed_signal(int current_tick);
    void playing_state_changed_signal(bool playing);

private slots:
    void cleanup_thread();

private:
    NoteNagaProject *project;
    NoteNagaMixer *mixer;

    double timer_interval;
    bool playing;
    QThread* thread;
    class PlaybackThreadWorker* worker;
    bool should_stop;
};

class NOTE_NAGA_ENGINE_API PlaybackThreadWorker : public QObject
{
    Q_OBJECT
public:
    PlaybackThreadWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval);
    void recalculate_tempo();
    void stop();

public slots:
    void run();

signals:
    void finished_signal();
    void on_position_changed_signal(int current_tick);

private:
    NoteNagaProject *project;
    NoteNagaMixer *mixer;
    
    double timer_interval;
    double ms_per_tick;
    std::chrono::high_resolution_clock::time_point start_time_point;
    int start_tick_at_start;
    bool should_stop;
};