#include <note_naga_engine/module/playback_worker.h>

#include <algorithm>
#include <note_naga_engine/logger.h>

/*******************************************************************************************************/
// Playback Worker
/*******************************************************************************************************/

PlaybackWorker::PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer,
                               double timer_interval_ms)
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
    this->project = project;
    this->mixer = mixer;
    this->timer_interval = timer_interval_ms / 1000.0;
    this->last_id = 0;
    this->pending_cleanup = false;
    NOTE_NAGA_LOG_INFO("Initialized successfully with timer interval: " +
                       std::to_string(timer_interval_ms) + " ms");
}

PlaybackWorker::CallbackId PlaybackWorker::addFinishedCallback(FinishedCallback cb) {
    CallbackId id = ++last_id;
    finished_callbacks.emplace_back(id, std::move(cb));
    NOTE_NAGA_LOG_INFO("Added finished callback with ID: " + std::to_string(id));
    return id;
}

PlaybackWorker::CallbackId
PlaybackWorker::addPositionChangedCallback(PositionChangedCallback cb) {
    CallbackId id = ++last_id;
    position_changed_callbacks.emplace_back(id, std::move(cb));
    NOTE_NAGA_LOG_INFO("Added position changed callback with ID: " + std::to_string(id));
    return id;
}

PlaybackWorker::CallbackId
PlaybackWorker::addPlayingStateCallback(PlayingStateCallback cb) {
    CallbackId id = ++last_id;
    playing_state_callbacks.emplace_back(id, std::move(cb));
    NOTE_NAGA_LOG_INFO("Added playing state callback with ID: " + std::to_string(id));
    return id;
}

void PlaybackWorker::removeFinishedCallback(CallbackId id) {
    auto it = std::remove_if(finished_callbacks.begin(), finished_callbacks.end(),
                             [id](const auto &pair) { return pair.first == id; });
    bool removed = (it != finished_callbacks.end());
    finished_callbacks.erase(it, finished_callbacks.end());
    if (removed) {
        NOTE_NAGA_LOG_INFO("Removed finished callback with ID: " + std::to_string(id));
    } else {
        NOTE_NAGA_LOG_INFO("No finished callback found with ID: " + std::to_string(id));
    }
}

void PlaybackWorker::removePositionChangedCallback(CallbackId id) {
    auto it = std::remove_if(position_changed_callbacks.begin(),
                             position_changed_callbacks.end(),
                             [id](const auto &pair) { return pair.first == id; });
    bool removed = (it != position_changed_callbacks.end());
    position_changed_callbacks.erase(it, position_changed_callbacks.end());
    if (removed) {
        NOTE_NAGA_LOG_INFO("Removed position changed callback with ID: " +
                           std::to_string(id));
    } else {
        NOTE_NAGA_LOG_INFO("No position changed callback found with ID: " +
                           std::to_string(id));
    }
}

void PlaybackWorker::removePlayingStateCallback(CallbackId id) {
    auto it =
        std::remove_if(playing_state_callbacks.begin(), playing_state_callbacks.end(),
                       [id](const auto &pair) { return pair.first == id; });
    bool removed = (it != playing_state_callbacks.end());
    playing_state_callbacks.erase(it, playing_state_callbacks.end());
    if (removed) {
        NOTE_NAGA_LOG_INFO("Removed playing state callback with ID: " +
                           std::to_string(id));
    } else {
        NOTE_NAGA_LOG_INFO("No playing state callback found with ID: " +
                           std::to_string(id));
    }
}

void PlaybackWorker::recalculateWorkerTempo() {
    if (worker) {
        worker->recalculateTempo();
    } else {
        NOTE_NAGA_LOG_ERROR("Worker is not running, unable to recalculate tempo");
    }
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
    // Check if we need to clean up the previous worker
    if (pending_cleanup) {
        if (worker_thread.joinable()) worker_thread.join();
        cleanupThread();
        pending_cleanup = false;
    }

    if (playing) {
        NOTE_NAGA_LOG_WARNING("Already playing");
        return false;
    }
    if (!project) {
        NOTE_NAGA_LOG_ERROR("No project available");
        return false;
    }

    should_stop = false;
    worker = new PlaybackThreadWorker(project, mixer, timer_interval);
    worker->enableLooping(this->looping);

    // Forward events from thread worker to this worker
    worker->addPositionChangedCallback([this](int tick) { emitPositionChanged(tick); });
    worker->addFinishedCallback([this]() {
        playing = false;
        emitPlayingState(false);
        pending_cleanup = true;
        emitFinished();
    });

    playing = true;
    emitPlayingState(true);

    worker_thread = std::thread(&PlaybackThreadWorker::run, worker);

    NOTE_NAGA_LOG_INFO("Playback worker started");
    return true;
}

bool PlaybackWorker::stop() {
    if (!playing && !pending_cleanup) {
        NOTE_NAGA_LOG_WARNING("Playback worker not currently playing");
        return false;
    }

    should_stop = true;
    if (worker) worker->stop();
    // Pokud thread ještě běží, joinuj; pokud už doběhl, joinable je false
    if (worker_thread.joinable()) worker_thread.join();
    cleanupThread();
    pending_cleanup = false;

    NOTE_NAGA_LOG_INFO("Playback worker stopped");
    return true;
}

void PlaybackWorker::enableLooping(bool enabled) {
    this->looping = enabled;
    if (worker) {
        worker->enableLooping(this->looping);
    } else {
        NOTE_NAGA_LOG_ERROR("Worker is not running, cannot enable looping");
    }
}

void PlaybackWorker::cleanupThread() {
    if (worker) {
        delete worker;
        worker = nullptr;
    }
    playing = false;
    emitPlayingState(false);
    NOTE_NAGA_LOG_INFO("Playback thread resources cleaned up");
}

/*******************************************************************************************************/
// Playback Thread Worker
/*******************************************************************************************************/

PlaybackThreadWorker::PlaybackThreadWorker(NoteNagaProject *project, NoteNagaMixer *mixer,
                                           double timer_interval) {
    this->project = project;
    this->mixer = mixer;
    this->timer_interval = timer_interval;
    this->start_tick_at_start = 0;
    this->last_id = 0;
    this->looping = false;
}

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

    NOTE_NAGA_LOG_INFO("Recalculated tempo: " +
                       std::to_string(60'000'000.0 / this->project->getTempo()) +
                       " BPM, PPQ: " + std::to_string(this->project->getPPQ()) +
                       ", ms per tick: " + std::to_string(ms_per_tick));
}

void PlaybackThreadWorker::enableLooping(bool enabled) {
    this->looping = enabled;
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
    // Ensure we have a valid project and active sequence
    NoteNagaMidiSeq *active_sequence = this->project->getActiveSequence();
    if (!active_sequence) {
        emitFinished();
        return;
    }

    // Ensure we start from a valid tick
    if (this->project->getCurrentTick() >= active_sequence->getMaxTick()) {
        this->project->setCurrentTick(0);
        NOTE_NAGA_LOG_WARNING("Current tick is already at or beyond max tick, go back to start");
    }

    // Initialize timing
    int current_tick = this->project->getCurrentTick();
    recalculateTempo();

    // Playback loop
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
            if (!this->looping) this->should_stop = true;
        }

        if (active_sequence->getSoloTrack()) {
            // play soloed track only
            auto track = active_sequence->getSoloTrack();
            if (track) {
                for (const auto &note : track->getNotes()) {
                    if (note.start.has_value() && note.length.has_value()) {
                        if (last_tick < note.start.value() &&
                            note.start.value() <= current_tick)
                            mixer->pushToQueue(NN_SynthMessage_t{note, true});
                        if (last_tick < note.start.value() + note.length.value() &&
                            note.start.value() + note.length.value() <= current_tick)
                            mixer->pushToQueue(NN_SynthMessage_t{note, false});
                    }
                }
            }
        } else {
            // play all tracks
            for (const auto &track : active_sequence->getTracks()) {
                if (track->isMuted()) continue;
                for (const auto &note : track->getNotes()) {
                    if (note.start.has_value() && note.length.has_value()) {
                        if (last_tick < note.start.value() &&
                            note.start.value() <= current_tick)
                            mixer->pushToQueue(NN_SynthMessage_t{note, true});
                        if (last_tick < note.start.value() + note.length.value() &&
                            note.start.value() + note.length.value() <= current_tick)
                            mixer->pushToQueue(NN_SynthMessage_t{note, false});
                    }
                }
            }
        }

        // Looping if enabled
        if (this->looping && current_tick >= active_sequence->getMaxTick()) {
            current_tick = 0; // Loop back to start
            this->project->setCurrentTick(current_tick);
            recalculateTempo();
            NOTE_NAGA_LOG_INFO("Reached max tick, looping back to start");
        } 

        // Emit position changed event
        this->emitPositionChanged(current_tick);
        // Sleep for the timer interval
        std::this_thread::sleep_for(std::chrono::duration<double>(timer_interval));
    }

    NOTE_NAGA_LOG_INFO("Playback thread finished");
    emitFinished();
}

void PlaybackThreadWorker::stop() { should_stop = true; }