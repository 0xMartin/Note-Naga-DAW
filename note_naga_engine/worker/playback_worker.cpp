#include "playback_worker.h"
#include <algorithm>
#include <iostream>

/*******************************************************************************************************/
// Playback Thread Worker
/*******************************************************************************************************/

PlaybackThreadWorker::PlaybackThreadWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval)
    : project(project), mixer(mixer), timer_interval(timer_interval), ms_per_tick(1.0), start_tick_at_start(0), last_id(0) {}

PlaybackThreadWorker::CallbackId PlaybackThreadWorker::add_finished_callback(FinishedCallback cb) {
    CallbackId id = ++last_id;
    finished_callbacks.emplace_back(id, std::move(cb));
    return id;
}

PlaybackThreadWorker::CallbackId PlaybackThreadWorker::add_position_changed_callback(PositionChangedCallback cb) {
    CallbackId id = ++last_id;
    position_changed_callbacks.emplace_back(id, std::move(cb));
    return id;
}

void PlaybackThreadWorker::remove_finished_callback(CallbackId id) {
    finished_callbacks.erase(
        std::remove_if(finished_callbacks.begin(), finished_callbacks.end(),
            [id](const auto& pair){ return pair.first == id; }),
        finished_callbacks.end());
}

void PlaybackThreadWorker::remove_position_changed_callback(CallbackId id) {
    position_changed_callbacks.erase(
        std::remove_if(position_changed_callbacks.begin(), position_changed_callbacks.end(),
            [id](const auto& pair){ return pair.first == id; }),
        position_changed_callbacks.end());
}

void PlaybackThreadWorker::recalculate_tempo() {
    int current_tick = this->project->get_current_tick();
    double us_per_tick = static_cast<double>(this->project->get_tempo()) / this->project->get_ppq();
    ms_per_tick = us_per_tick / 1000.0;
    start_time_point = std::chrono::high_resolution_clock::now();
    start_tick_at_start = current_tick;
}

void PlaybackThreadWorker::emit_finished() {
    for (auto &cb : finished_callbacks)
        cb.second();
}

void PlaybackThreadWorker::emit_position_changed(int tick) {
    for (auto &cb : position_changed_callbacks)
        cb.second(tick);
}

void PlaybackThreadWorker::run() {
    NoteNagaMIDISeq *active_sequence = this->project->get_active_sequence();
    if (!active_sequence) {
        emit_finished();
        return;
    }

    int current_tick = this->project->get_current_tick();
    recalculate_tempo();

    using clock = std::chrono::high_resolution_clock;

    while (!should_stop) {
        auto now = clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(now - start_time_point).count();
        int target_tick = start_tick_at_start + static_cast<int>(elapsed_ms / ms_per_tick);
        int tick_advance = std::max(1, target_tick - current_tick);
        int last_tick = current_tick;
        current_tick += tick_advance;
        this->project->set_current_tick(current_tick);

        // Stop playback on reaching max tick
        if (current_tick >= active_sequence->get_max_tick()) {
            current_tick = active_sequence->get_max_tick();
            should_stop = true;
        }

        if (active_sequence->get_solo_track()) {
            auto track = active_sequence->get_solo_track();
            if (track) {
                for (const auto &note : track->get_notes()) {
                    if (note.start.has_value() && note.length.has_value()) {
                        if (last_tick < note.start.value() && note.start.value() <= current_tick)
                            mixer->note_play(note);
                        if (last_tick < note.start.value() + note.length.value() &&
                            note.start.value() + note.length.value() <= current_tick)
                            mixer->note_stop(note);
                    }
                }
            }
        } else {
            for (const auto &track : active_sequence->get_tracks()) {
                if (track->is_muted()) continue;
                for (const auto &note : track->get_notes()) {
                    if (note.start.has_value() && note.length.has_value()) {
                        if (last_tick < note.start.value() && note.start.value() <= current_tick)
                            mixer->note_play(note);
                        if (last_tick < note.start.value() + note.length.value() &&
                            note.start.value() + note.length.value() <= current_tick)
                            mixer->note_stop(note);
                    }
                }
            }
        }

        emit_position_changed(current_tick);

        std::this_thread::sleep_for(std::chrono::duration<double>(timer_interval));
    }

    emit_finished();
}

void PlaybackThreadWorker::stop() { should_stop = true; }

/*******************************************************************************************************/
// Playback Worker
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED  
PlaybackWorker::PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval_ms)
    : QObject(nullptr), project(project), mixer(mixer), timer_interval(timer_interval_ms / 1000.0), last_id(0) {}
#else
PlaybackWorker::PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval_ms)
    : project(project), mixer(mixer), timer_interval(timer_interval_ms / 1000.0), last_id(0), worker(nullptr) {}
#endif

PlaybackWorker::CallbackId PlaybackWorker::add_finished_callback(FinishedCallback cb) {
    CallbackId id = ++last_id;
    finished_callbacks.emplace_back(id, std::move(cb));
    return id;
}

PlaybackWorker::CallbackId PlaybackWorker::add_position_changed_callback(PositionChangedCallback cb) {
    CallbackId id = ++last_id;
    position_changed_callbacks.emplace_back(id, std::move(cb));
    return id;
}

PlaybackWorker::CallbackId PlaybackWorker::add_playing_state_callback(PlayingStateCallback cb) {
    CallbackId id = ++last_id;
    playing_state_callbacks.emplace_back(id, std::move(cb));
    return id;
}

void PlaybackWorker::remove_finished_callback(CallbackId id) {
    finished_callbacks.erase(
        std::remove_if(finished_callbacks.begin(), finished_callbacks.end(),
            [id](const auto& pair){ return pair.first == id; }),
        finished_callbacks.end());
}

void PlaybackWorker::remove_position_changed_callback(CallbackId id) {
    position_changed_callbacks.erase(
        std::remove_if(position_changed_callbacks.begin(), position_changed_callbacks.end(),
            [id](const auto& pair){ return pair.first == id; }),
        position_changed_callbacks.end());
}

void PlaybackWorker::remove_playing_state_callback(CallbackId id) {
    playing_state_callbacks.erase(
        std::remove_if(playing_state_callbacks.begin(), playing_state_callbacks.end(),
            [id](const auto& pair){ return pair.first == id; }),
        playing_state_callbacks.end());
}

void PlaybackWorker::recalculate_worker_tempo() {
    if (!worker) {
        std::cerr << "PlaybackWorker: Worker is not running, unable to recalculate tempo.\n";
        return;
    }
    worker->recalculate_tempo();
}

void PlaybackWorker::emit_finished() {
    NN_QT_EMIT(this->finished_signal());
    for (auto &cb : finished_callbacks)
        cb.second();
}

void PlaybackWorker::emit_position_changed(int tick) {
    NN_QT_EMIT(this->position_changed_signal(tick));
    for (auto &cb : position_changed_callbacks)
        cb.second(tick);
}

void PlaybackWorker::emit_playing_state(bool playing_val) {
    NN_QT_EMIT(this->playing_state_changed_signal(playing_val));
    for (auto &cb : playing_state_callbacks)
        cb.second(playing_val);
}

bool PlaybackWorker::play() {
    if (playing) {
        std::cerr << "PlaybackWorker: Already playing.\n";
        return false;
    }
    if (!project) {
        std::cerr << "PlaybackWorker: No project data available.\n";
        return false;
    }
    std::cerr << "PlaybackWorker: Starting playback.\n";

    should_stop = false;
    worker = new PlaybackThreadWorker(project, mixer, timer_interval);

    // Forward events from thread worker to this worker
    worker->add_position_changed_callback([this](int tick) { emit_position_changed(tick); });
    worker->add_finished_callback([this]() {
        this->cleanup_thread();
        emit_finished();
    });

    playing = true;
    emit_playing_state(true);

    worker_thread = std::thread(&PlaybackThreadWorker::run, worker);
    return true;
}

bool PlaybackWorker::stop() {
    if (!playing) {
        std::cerr << "PlaybackWorker: Not currently playing.\n";
        return false;
    }
    std::cerr << "PlaybackWorker: Stopping playback.\n";

    should_stop = true;
    if (worker) worker->stop();
    if (worker_thread.joinable()) worker_thread.join();
    cleanup_thread(); 
    return true;
}

void PlaybackWorker::cleanup_thread() {
    if (worker) {
        delete worker;
        worker = nullptr;
    }
    playing = false;
    emit_playing_state(false);
}