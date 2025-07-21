#include <note_naga_engine/worker/playback_worker.h>
#include <algorithm>
#include <iostream>

/*******************************************************************************************************/
// Playback Thread Worker
/*******************************************************************************************************/

PlaybackThreadWorker::PlaybackThreadWorker(NoteNagaProject *project, NoteNagaMixer *mixer,
                                           double timer_interval)
    : project(project), mixer(mixer), timer_interval(timer_interval), ms_per_tick(1.0),
      start_tick_at_start(0), last_id(0) {}

PlaybackThreadWorker::CallbackId
PlaybackThreadWorker::addFinishedCallback(FinishedCallback cb) {
    CallbackId id = ++last_id;
    finished_callbacks.emplace_back(id, std::move(cb));
    return id;
}

PlaybackThreadWorker::CallbackId
PlaybackThreadWorker::addPositionChangedCallback(PositionChangedCallback cb) {
    CallbackId id = ++last_id;
    position_changed_callbacks.emplace_back(id, std::move(cb));
    return id;
}

void PlaybackThreadWorker::removeFinishedCallback(CallbackId id) {
    finished_callbacks.erase(
        std::remove_if(finished_callbacks.begin(), finished_callbacks.end(),
                       [id](const auto &pair) { return pair.first == id; }),
        finished_callbacks.end());
}

void PlaybackThreadWorker::removePositionChangedCallback(CallbackId id) {
    position_changed_callbacks.erase(
        std::remove_if(position_changed_callbacks.begin(),
                       position_changed_callbacks.end(),
                       [id](const auto &pair) { return pair.first == id; }),
        position_changed_callbacks.end());
}

void PlaybackThreadWorker::recalculateTempo() {
    int current_tick = this->project->getCurrentTick();
    double us_per_tick =
        static_cast<double>(this->project->getTempo()) / this->project->getPPQ();
    ms_per_tick = us_per_tick / 1000.0;
    start_time_point = std::chrono::high_resolution_clock::now();
    start_tick_at_start = current_tick;
}

void PlaybackThreadWorker::emitFinished() {
    for (auto &cb : finished_callbacks)
        cb.second();
}

void PlaybackThreadWorker::emitPositionChanged(int tick) {
    for (auto &cb : position_changed_callbacks)
        cb.second(tick);
}

void PlaybackThreadWorker::run() {
    NoteNagaMidiSeq *active_sequence = this->project->getActiveSequence();
    if (!active_sequence) {
        emitFinished();
        return;
    }

    int current_tick = this->project->getCurrentTick();
    recalculateTempo();

    using clock = std::chrono::high_resolution_clock;

    while (!should_stop) {
        auto now = clock::now();
        double elapsed_ms =
            std::chrono::duration<double, std::milli>(now - start_time_point).count();
        int target_tick =
            start_tick_at_start + static_cast<int>(elapsed_ms / ms_per_tick);
        int tick_advance = std::max(1, target_tick - current_tick);
        int last_tick = current_tick;
        current_tick += tick_advance;
        this->project->setCurrentTick(current_tick);

        // Stop playback on reaching max tick
        if (current_tick >= active_sequence->getMaxTick()) {
            current_tick = active_sequence->getMaxTick();
            should_stop = true;
        }

        if (active_sequence->getSoloTrack()) {
            auto track = active_sequence->getSoloTrack();
            if (track) {
                for (const auto &note : track->getNotes()) {
                    if (note.start.has_value() && note.length.has_value()) {
                        if (last_tick < note.start.value() &&
                            note.start.value() <= current_tick)
                            mixer->playNote(note);
                        if (last_tick < note.start.value() + note.length.value() &&
                            note.start.value() + note.length.value() <= current_tick)
                            mixer->stopNote(note);
                    }
                }
            }
        } else {
            for (const auto &track : active_sequence->getTracks()) {
                if (track->isMuted()) continue;
                for (const auto &note : track->getNotes()) {
                    if (note.start.has_value() && note.length.has_value()) {
                        if (last_tick < note.start.value() &&
                            note.start.value() <= current_tick)
                            mixer->playNote(note);
                        if (last_tick < note.start.value() + note.length.value() &&
                            note.start.value() + note.length.value() <= current_tick)
                            mixer->stopNote(note);
                    }
                }
            }
        }

        emitPositionChanged(current_tick);

        std::this_thread::sleep_for(std::chrono::duration<double>(timer_interval));
    }

    emitFinished();
}

void PlaybackThreadWorker::stop() { should_stop = true; }

/*******************************************************************************************************/
// Playback Worker
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED
PlaybackWorker::PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer,
                               double timer_interval_ms)
    : QObject(nullptr), project(project), mixer(mixer),
      timer_interval(timer_interval_ms / 1000.0), last_id(0) {}
#else
PlaybackWorker::PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer,
                               double timer_interval_ms)
    : project(project), mixer(mixer), timer_interval(timer_interval_ms / 1000.0),
      last_id(0), worker(nullptr) {}
#endif

PlaybackWorker::CallbackId PlaybackWorker::addFinishedCallback(FinishedCallback cb) {
    CallbackId id = ++last_id;
    finished_callbacks.emplace_back(id, std::move(cb));
    return id;
}

PlaybackWorker::CallbackId
PlaybackWorker::addPositionChangedCallback(PositionChangedCallback cb) {
    CallbackId id = ++last_id;
    position_changed_callbacks.emplace_back(id, std::move(cb));
    return id;
}

PlaybackWorker::CallbackId
PlaybackWorker::addPlayingStateCallback(PlayingStateCallback cb) {
    CallbackId id = ++last_id;
    playing_state_callbacks.emplace_back(id, std::move(cb));
    return id;
}

void PlaybackWorker::removeFinishedCallback(CallbackId id) {
    finished_callbacks.erase(
        std::remove_if(finished_callbacks.begin(), finished_callbacks.end(),
                       [id](const auto &pair) { return pair.first == id; }),
        finished_callbacks.end());
}

void PlaybackWorker::removePositionChangedCallback(CallbackId id) {
    position_changed_callbacks.erase(
        std::remove_if(position_changed_callbacks.begin(),
                       position_changed_callbacks.end(),
                       [id](const auto &pair) { return pair.first == id; }),
        position_changed_callbacks.end());
}

void PlaybackWorker::removePlayingStateCallback(CallbackId id) {
    playing_state_callbacks.erase(
        std::remove_if(playing_state_callbacks.begin(), playing_state_callbacks.end(),
                       [id](const auto &pair) { return pair.first == id; }),
        playing_state_callbacks.end());
}

void PlaybackWorker::recalculateWorkerTempo() {
    if (!worker) {
        std::cerr
            << "PlaybackWorker: Worker is not running, unable to recalculate tempo.\n";
        return;
    }
    worker->recalculateTempo();
}

void PlaybackWorker::emitFinished() {
    NN_QT_EMIT(this->finished());
    for (auto &cb : finished_callbacks)
        cb.second();
}

void PlaybackWorker::emitPositionChanged(int tick) {
    NN_QT_EMIT(this->currentTickChanged(tick));
    for (auto &cb : position_changed_callbacks)
        cb.second(tick);
}

void PlaybackWorker::emitPlayingState(bool playing_val) {
    NN_QT_EMIT(this->playingStateChanged(playing_val));
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
    worker->addPositionChangedCallback([this](int tick) { emitPositionChanged(tick); });
    worker->addFinishedCallback([this]() {
        this->cleanupThread();
        emitFinished();
    });

    playing = true;
    emitPlayingState(true);

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
    cleanupThread();
    return true;
}

void PlaybackWorker::cleanupThread() {
    if (worker) {
        delete worker;
        worker = nullptr;
    }
    playing = false;
    emitPlayingState(false);
}