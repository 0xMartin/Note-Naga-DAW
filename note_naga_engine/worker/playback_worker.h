#pragma once

#include "../core/mixer.h"
#include "../core/project_data.h"
#include "../note_naga_api.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include <cstdint>

/*******************************************************************************************************/
// Playback Thread Worker
/*******************************************************************************************************/

class PlaybackThreadWorker {
public:
    using CallbackId = std::uint64_t;
    using FinishedCallback = std::function<void()>;
    using PositionChangedCallback = std::function<void(int)>;

    PlaybackThreadWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval);

    void recalculate_tempo();
    void stop();
    void run();

    // Add callbacks (signals)
    CallbackId add_finished_callback(FinishedCallback cb);
    CallbackId add_position_changed_callback(PositionChangedCallback cb);

    void remove_finished_callback(CallbackId id);
    void remove_position_changed_callback(CallbackId id);

    std::atomic<bool> should_stop{false};

private:
    NoteNagaProject *project;
    NoteNagaMixer *mixer;
    double timer_interval;
    double ms_per_tick;
    std::chrono::high_resolution_clock::time_point start_time_point;
    int start_tick_at_start;

    CallbackId last_id = 0;
    std::vector<std::pair<CallbackId, FinishedCallback>> finished_callbacks;
    std::vector<std::pair<CallbackId, PositionChangedCallback>> position_changed_callbacks;

    void emit_finished();
    void emit_position_changed(int tick);
};

/*******************************************************************************************************/
// Playback Worker
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API PlaybackWorker : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API PlaybackWorker {
#endif

public:
    using CallbackId = std::uint64_t;
    using FinishedCallback = std::function<void()>;
    using PositionChangedCallback = std::function<void(int)>;
    using PlayingStateCallback = std::function<void(bool)>;

    explicit PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval_ms);

    bool is_playing() const { return playing; }
    void recalculate_worker_tempo();
    bool play();
    bool stop();

    CallbackId add_finished_callback(FinishedCallback cb);
    CallbackId add_position_changed_callback(PositionChangedCallback cb);
    CallbackId add_playing_state_callback(PlayingStateCallback cb);

    void remove_finished_callback(CallbackId id);
    void remove_position_changed_callback(CallbackId id);
    void remove_playing_state_callback(CallbackId id);

#ifndef QT_DEACTIVATED    
Q_SIGNALS:
    void finished_signal();
    void position_changed_signal(int tick);
    void playing_state_changed_signal(bool playing_val);
#endif

private:
    void thread_func();
    void cleanup_thread();

    void emit_finished();
    void emit_position_changed(int tick);
    void emit_playing_state(bool playing);

    NoteNagaProject *project;
    NoteNagaMixer *mixer;
    double timer_interval;
    std::atomic<bool> playing{false};
    std::atomic<bool> should_stop{false};
    std::thread worker_thread;
    PlaybackThreadWorker *worker{nullptr};

    CallbackId last_id = 0;
    std::vector<std::pair<CallbackId, FinishedCallback>> finished_callbacks;
    std::vector<std::pair<CallbackId, PositionChangedCallback>> position_changed_callbacks;
    std::vector<std::pair<CallbackId, PlayingStateCallback>> playing_state_callbacks;
};