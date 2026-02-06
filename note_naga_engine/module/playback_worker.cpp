#include <note_naga_engine/module/playback_worker.h>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <note_naga_engine/logger.h>

/*******************************************************************************************************/
// Playback Worker
/*******************************************************************************************************/

NoteNagaPlaybackWorker::NoteNagaPlaybackWorker(NoteNagaRuntimeData *project,
                               double timer_interval_ms)
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
    this->project = project;
    this->timer_interval = timer_interval_ms / 1000.0;
    this->last_id = 0;
    this->pending_cleanup = false;
    this->playback_mode_ = PlaybackMode::Sequence;
    NOTE_NAGA_LOG_INFO("Initialized successfully with timer interval: " +
                       std::to_string(timer_interval_ms) + " ms");
}

NoteNagaPlaybackWorker::CallbackId NoteNagaPlaybackWorker::addFinishedCallback(FinishedCallback cb) {
    CallbackId id = ++last_id;
    finished_callbacks.emplace_back(id, std::move(cb));
    NOTE_NAGA_LOG_INFO("Added finished callback with ID: " + std::to_string(id));
    return id;
}

NoteNagaPlaybackWorker::CallbackId NoteNagaPlaybackWorker::addPositionChangedCallback(PositionChangedCallback cb) {
    CallbackId id = ++last_id;
    position_changed_callbacks.emplace_back(id, std::move(cb));
    NOTE_NAGA_LOG_INFO("Added position changed callback with ID: " + std::to_string(id));
    return id;
}

NoteNagaPlaybackWorker::CallbackId NoteNagaPlaybackWorker::addPlayingStateCallback(PlayingStateCallback cb) {
    CallbackId id = ++last_id;
    playing_state_callbacks.emplace_back(id, std::move(cb));
    NOTE_NAGA_LOG_INFO("Added playing state callback with ID: " + std::to_string(id));
    return id;
}

void NoteNagaPlaybackWorker::removeFinishedCallback(CallbackId id) {
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

void NoteNagaPlaybackWorker::removePositionChangedCallback(CallbackId id) {
    auto it = std::remove_if(position_changed_callbacks.begin(), position_changed_callbacks.end(),
                             [id](const auto &pair) { return pair.first == id; });
    bool removed = (it != position_changed_callbacks.end());
    position_changed_callbacks.erase(it, position_changed_callbacks.end());
    if (removed) {
        NOTE_NAGA_LOG_INFO("Removed position changed callback with ID: " + std::to_string(id));
    } else {
        NOTE_NAGA_LOG_INFO("No position changed callback found with ID: " + std::to_string(id));
    }
}

void NoteNagaPlaybackWorker::removePlayingStateCallback(CallbackId id) {
    auto it = std::remove_if(playing_state_callbacks.begin(), playing_state_callbacks.end(),
                             [id](const auto &pair) { return pair.first == id; });
    bool removed = (it != playing_state_callbacks.end());
    playing_state_callbacks.erase(it, playing_state_callbacks.end());
    if (removed) {
        NOTE_NAGA_LOG_INFO("Removed playing state callback with ID: " + std::to_string(id));
    } else {
        NOTE_NAGA_LOG_INFO("No playing state callback found with ID: " + std::to_string(id));
    }
}

void NoteNagaPlaybackWorker::recalculateWorkerTempo() {
    if (worker) {
        worker->recalculateTempo();
    } else {
        NOTE_NAGA_LOG_ERROR("Worker is not running, unable to recalculate tempo");
    }
}

void NoteNagaPlaybackWorker::emitFinished() {
    NN_QT_EMIT(this->finished());
    for (auto &cb : finished_callbacks)
        cb.second();
}

void NoteNagaPlaybackWorker::emitPositionChanged(int tick) {
    NN_QT_EMIT(this->currentTickChanged(tick));
    for (auto &cb : position_changed_callbacks)
        cb.second(tick);
}

void NoteNagaPlaybackWorker::emitPlayingState(bool playing_val) {
    NN_QT_EMIT(this->playingStateChanged(playing_val));
    for (auto &cb : playing_state_callbacks)
        cb.second(playing_val);
}

void NoteNagaPlaybackWorker::emitNotePlayed(const NN_Note_t &note) {
    NN_QT_EMIT(this->notePlayed(note));
}

bool NoteNagaPlaybackWorker::play() {
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
    worker = new PlaybackThreadWorker(project, timer_interval, playback_mode_);
    worker->enableLooping(this->looping);

    // Forward events from thread worker to this worker
    worker->addPositionChangedCallback([this](int tick) { emitPositionChanged(tick); });
    worker->addFinishedCallback([this]() {
        playing = false;
        emitPlayingState(false);
        pending_cleanup = true;
        emitFinished();
    });
    worker->addNotePlayedCallback([this](const NN_Note_t &note) { emitNotePlayed(note); });

    playing = true;
    emitPlayingState(true);

    worker_thread = std::thread(&PlaybackThreadWorker::run, worker);

    NOTE_NAGA_LOG_INFO("Playback worker started");
    return true;
}

bool NoteNagaPlaybackWorker::stop() {
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

void NoteNagaPlaybackWorker::enableLooping(bool enabled) {
    this->looping = enabled;
    if (worker) {
        worker->enableLooping(this->looping);
    } else {
        NOTE_NAGA_LOG_ERROR("Worker is not running, cannot enable looping");
    }
}

void NoteNagaPlaybackWorker::setPlaybackMode(PlaybackMode mode) {
    this->playback_mode_ = mode;
    NOTE_NAGA_LOG_INFO("Playback mode set to: " + 
                       std::string(mode == PlaybackMode::Sequence ? "Sequence" : "Arrangement"));
}

void NoteNagaPlaybackWorker::cleanupThread() {
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

PlaybackThreadWorker::PlaybackThreadWorker(NoteNagaRuntimeData *project,
                                           double timer_interval,
                                           PlaybackMode mode) {
    this->project = project;
    this->timer_interval = timer_interval;
    this->start_tick_at_start = 0;
    this->last_id = 0;
    this->looping = false;
    this->playback_mode_ = mode;
}

PlaybackThreadWorker::CallbackId PlaybackThreadWorker::addFinishedCallback(FinishedCallback cb) {
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
    finished_callbacks.erase(std::remove_if(finished_callbacks.begin(), finished_callbacks.end(),
                                            [id](const auto &pair) { return pair.first == id; }),
                             finished_callbacks.end());
}

void PlaybackThreadWorker::removePositionChangedCallback(CallbackId id) {
    position_changed_callbacks.erase(
        std::remove_if(position_changed_callbacks.begin(), position_changed_callbacks.end(),
                       [id](const auto &pair) { return pair.first == id; }),
        position_changed_callbacks.end());
}

void PlaybackThreadWorker::recalculateTempo() {
    int current_tick = this->project->getCurrentTick();
    NoteNagaMidiSeq* seq = this->project->getActiveSequence();
    
    // Use effective tempo at current tick (supports tempo track)
    int effectiveTempo = seq ? seq->getEffectiveTempoAtTick(current_tick) : this->project->getTempo();
    double us_per_tick = static_cast<double>(effectiveTempo) / this->project->getPPQ();
    ms_per_tick = us_per_tick / 1000.0;
    start_time_point = std::chrono::high_resolution_clock::now();
    start_tick_at_start = current_tick;
    last_tempo_check_tick = current_tick;
    
    // Calculate and emit current BPM
    double currentBPM = 60'000'000.0 / effectiveTempo;
    NN_QT_EMIT(project->currentTempoChanged(currentBPM));
    // Log only on significant changes (tempo log spam removed)
}

void PlaybackThreadWorker::enableLooping(bool enabled) { this->looping = enabled; }

void PlaybackThreadWorker::emitFinished() {
    for (auto &cb : finished_callbacks)
        cb.second();
}

void PlaybackThreadWorker::emitPositionChanged(int tick) {
    for (auto &cb : position_changed_callbacks)
        cb.second(tick);
}

void PlaybackThreadWorker::emitNotePlayed(const NN_Note_t &note) {
    for (auto &cb : note_played_callbacks)
        cb.second(note);
}

PlaybackThreadWorker::CallbackId PlaybackThreadWorker::addNotePlayedCallback(NotePlayedCallback cb) {
    CallbackId id = ++last_id;
    note_played_callbacks.emplace_back(id, std::move(cb));
    return id;
}

void PlaybackThreadWorker::removeNotePlayedCallback(CallbackId id) {
    auto it = std::remove_if(note_played_callbacks.begin(), note_played_callbacks.end(),
                             [id](const auto &pair) { return pair.first == id; });
    note_played_callbacks.erase(it, note_played_callbacks.end());
}

void PlaybackThreadWorker::stop() { should_stop = true; }

void PlaybackThreadWorker::run() {
    if (playback_mode_ == PlaybackMode::Arrangement) {
        runArrangementMode();
    } else {
        runSequenceMode();
    }
}

void PlaybackThreadWorker::runSequenceMode() {
    // Ensure we have a valid project and active sequence
    NoteNagaMidiSeq *active_sequence = this->project->getActiveSequence();
    if (!active_sequence) {
        emitFinished();
        return;
    }

    // Stop all notes on all tracks
    for (auto* track : active_sequence->getTracks()) {
        if (track && !track->isTempoTrack()) {
            track->stopAllNotes();
        }
    }

    // Ensure we start from a valid tick
    if (this->project->getCurrentTick() >= active_sequence->getMaxTick()) {
        this->project->setCurrentTick(0);
        NOTE_NAGA_LOG_WARNING("Current tick is already at or beyond max tick, go back to start");
    }

    // Initialize timing
    int current_tick = this->project->getCurrentTick();
    recalculateTempo();

    // start and end indices for each track
    std::unordered_map<NoteNagaTrack*, size_t> trackNoteStartIndex;
    for (auto* track : active_sequence->getTracks()) trackNoteStartIndex[track] = 0;

    using clock = std::chrono::high_resolution_clock;
    auto last_iteration_time = clock::now();
    double fractional_ticks = 0.0;  // Accumulate fractional tick advances
    
    while (!should_stop) {
        // Calculate time since last iteration
        auto now = clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(now - last_iteration_time).count();
        last_iteration_time = now;
        
        // Get current tempo (supports dynamic tempo from tempo track)
        int effectiveTempo;
        if (active_sequence->hasTempoTrack()) {
            effectiveTempo = active_sequence->getEffectiveTempoAtTick(current_tick);
            
            // Emit BPM change for UI update (throttled to avoid spam)
            static int bpmEmitCounter = 0;
            if (++bpmEmitCounter % 10 == 0) {
                double currentBPM = 60'000'000.0 / effectiveTempo;
                NN_QT_EMIT(project->currentTempoChanged(currentBPM));
            }
        } else {
            effectiveTempo = this->project->getTempo();
        }
        
        // Calculate ticks to advance based on current tempo
        double us_per_tick = static_cast<double>(effectiveTempo) / this->project->getPPQ();
        double current_ms_per_tick = us_per_tick / 1000.0;
        
        // Accumulate ticks (including fractional part for precision)
        fractional_ticks += elapsed_ms / current_ms_per_tick;
        int tick_advance = static_cast<int>(fractional_ticks);
        fractional_ticks -= tick_advance;  // Keep fractional remainder
        
        if (tick_advance < 1) {
            // Not enough time passed for a tick, sleep briefly and continue
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }
        
        int last_tick = current_tick;
        current_tick += tick_advance;
        this->project->setCurrentTick(current_tick);

        // Stop playback on reaching max tick
        if (current_tick >= active_sequence->getMaxTick()) {
            current_tick = active_sequence->getMaxTick();
            if (!this->looping) this->should_stop = true;
        }

        // Helper lambda to process notes for a track
        auto processTrackNotes = [&, this](NoteNagaTrack* track) {
            if (!track || track->isMuted() || track->isTempoTrack()) return;
            
            size_t& index = trackNoteStartIndex[track];
            index = (index > 10) ? index - 10 : 0; // Start 10 before the last processed note

            const std::vector<NN_Note_t>& notes = track->getNotes();
            for (; index < notes.size(); ++index) {
                const NN_Note_t& note = notes[index];
                if (!note.start.has_value() || !note.length.has_value())
                    continue;

                // Note ON (use <= for first tick to catch notes at position 0)
                if (last_tick <= note.start.value() && note.start.value() <= current_tick) {
                    track->playNote(note);
                    emitNotePlayed(note);
                }
                // Note OFF
                int note_end = note.start.value() + note.length.value();
                if (last_tick < note_end && note_end <= current_tick) {
                    track->stopNote(note);
                }

                // If the note starts after the current tick, we can stop checking
                if (note.start.value() > current_tick)
                    break;
            }
        };

        if (active_sequence->getSoloTrack()) {
            // play soloed track only
            processTrackNotes(active_sequence->getSoloTrack());
        } else {
            // play all tracks
            for (auto* track : active_sequence->getTracks()) {
                processTrackNotes(track);
            }
        }

        // Looping if enabled
        if (this->looping && current_tick >= active_sequence->getMaxTick()) {
            // Stop all notes before looping
            for (auto* track : active_sequence->getTracks()) {
                if (track && !track->isTempoTrack()) {
                    track->stopAllNotes();
                }
            }
            current_tick = 0; // Loop back to start
            this->project->setCurrentTick(current_tick);
            recalculateTempo();
            for (auto* track : active_sequence->getTracks()) trackNoteStartIndex[track] = 0;
            NOTE_NAGA_LOG_INFO("Reached max tick, looping back to start");
        }

        // Emit position changed event
        this->emitPositionChanged(current_tick);
        // Sleep for the timer interval
        std::this_thread::sleep_for(std::chrono::duration<double>(timer_interval));
    }

    // Stop all notes on finish
    for (auto* track : active_sequence->getTracks()) {
        if (track && !track->isTempoTrack()) {
            track->stopAllNotes();
        }
    }

    NOTE_NAGA_LOG_INFO("Playback thread finished (Sequence mode)");
    emitFinished();
}

void PlaybackThreadWorker::runArrangementMode() {
    // Get arrangement from project
    NoteNagaArrangement *arrangement = this->project->getArrangement();
    if (!arrangement || arrangement->getTrackCount() == 0) {
        NOTE_NAGA_LOG_WARNING("No arrangement or empty arrangement, cannot play");
        emitFinished();
        return;
    }

    int arrangement_max_tick = arrangement->getMaxTick();
    if (arrangement_max_tick <= 0) {
        NOTE_NAGA_LOG_WARNING("Arrangement has no content (max tick = 0)");
        emitFinished();
        return;
    }

    // Stop all notes on all sequences
    for (auto* seq : this->project->getSequences()) {
        for (auto* track : seq->getTracks()) {
            if (track && !track->isTempoTrack()) {
                track->stopAllNotes();
            }
        }
    }

    // Ensure we start from a valid tick
    int current_tick = this->project->getCurrentArrangementTick();
    if (current_tick >= arrangement_max_tick) {
        current_tick = 0;
        this->project->setCurrentArrangementTick(0);
    }

    // Get project settings
    int projectPPQ = this->project->getPPQ();
    
    // Check if arrangement has active tempo track
    bool hasArrangementTempoTrack = arrangement->hasTempoTrack() && 
                                     arrangement->getTempoTrack()->isTempoTrackActive();

    using clock = std::chrono::high_resolution_clock;
    auto last_iteration_time = clock::now();
    double fractional_ticks = 0.0;
    
    // Track last processed tick - initialize to -1 so first iteration processes starting tick
    int last_tick = current_tick - 1;
    bool firstIteration = true;

    // Simple approach: for each tick range, iterate ALL tracks and ALL clips
    // and check if any notes should be played or stopped
    
    while (!should_stop) {
        auto now = clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(now - last_iteration_time).count();
        last_iteration_time = now;

        // Get effective tempo (dynamic from tempo track or fixed from project)
        int effectiveTempo;
        if (hasArrangementTempoTrack) {
            effectiveTempo = arrangement->getEffectiveTempoAtTick(current_tick);
            
            // Emit BPM change for UI update (throttled to avoid spam)
            static int bpmEmitCounter = 0;
            if (++bpmEmitCounter % 10 == 0) {
                double currentBPM = 60'000'000.0 / effectiveTempo;
                NN_QT_EMIT(project->currentTempoChanged(currentBPM));
            }
        } else {
            effectiveTempo = this->project->getTempo();
        }
        
        double us_per_tick = static_cast<double>(effectiveTempo) / projectPPQ;
        double ms_per_tick = us_per_tick / 1000.0;

        // Accumulate ticks
        fractional_ticks += elapsed_ms / ms_per_tick;
        int tick_advance = static_cast<int>(fractional_ticks);
        fractional_ticks -= tick_advance;

        // On first iteration, process the starting tick immediately (even if tick_advance is 0)
        if (firstIteration) {
            firstIteration = false;
            // last_tick is already current_tick - 1, so we'll process from last_tick+1 = current_tick
            // Don't update last_tick or current_tick here, just fall through to process
        } else {
            if (tick_advance < 1) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            last_tick = current_tick;
            current_tick += tick_advance;
        }
        this->project->setCurrentArrangementTick(current_tick);

        // Check loop region in arrangement
        bool hasLoopRegion = arrangement->isLoopEnabled() && arrangement->hasValidLoopRegion();
        int64_t loopStart = arrangement->getLoopStartTick();
        int64_t loopEnd = arrangement->getLoopEndTick();

        // Check for loop region end
        if (hasLoopRegion && current_tick >= loopEnd) {
            // Stop all notes before looping
            for (auto* seq : this->project->getSequences()) {
                for (auto* track : seq->getTracks()) {
                    if (track && !track->isTempoTrack()) {
                        track->stopAllNotes();
                    }
                }
            }
            current_tick = static_cast<int>(loopStart);
            last_tick = current_tick - 1; // Force re-processing from loop start
            this->project->setCurrentArrangementTick(current_tick);
        }
        // Check for end of arrangement (when no loop region or loop region disabled)
        else if (current_tick >= arrangement_max_tick) {
            if (!this->looping) {
                this->should_stop = true;
                current_tick = arrangement_max_tick;
            } else {
                // Stop all notes before looping
                for (auto* seq : this->project->getSequences()) {
                    for (auto* track : seq->getTracks()) {
                        if (track && !track->isTempoTrack()) {
                            track->stopAllNotes();
                        }
                    }
                }
                current_tick = 0;
                last_tick = -1; // Force re-processing from start
                this->project->setCurrentArrangementTick(current_tick);
            }
        }

        // Iterate ALL arrangement tracks
        for (size_t trackIdx = 0; trackIdx < arrangement->getTrackCount(); ++trackIdx) {
            NoteNagaArrangementTrack* arrTrack = arrangement->getTracks()[trackIdx];
            if (!arrTrack || arrTrack->isMuted()) continue;

            // Iterate ALL clips in this track
            for (const auto& clip : arrTrack->getClips()) {
                if (clip.muted) continue;

                // Check if current tick is within clip's timeline range
                // Clip covers from clip.startTick to clip.startTick + clip.durationTicks
                int clipStart = clip.startTick;
                int clipEnd = clip.startTick + clip.durationTicks;
                
                // Skip if we're completely outside this clip's range
                if (current_tick < clipStart || last_tick >= clipEnd) continue;

                // Get the referenced MIDI sequence
                NoteNagaMidiSeq* seq = this->project->getSequenceById(clip.sequenceId);
                if (!seq) continue;

                int seqLength = seq->getMaxTick();
                if (seqLength <= 0) continue;

                // For each tick in the range [last_tick+1, current_tick], calculate the
                // corresponding sequence tick (with looping) and check for notes
                for (int arrangeTick = last_tick + 1; arrangeTick <= current_tick; ++arrangeTick) {
                    // Skip if outside clip range
                    if (arrangeTick < clipStart || arrangeTick >= clipEnd) continue;

                    // Calculate sequence-local tick with looping
                    int localTick = (arrangeTick - clipStart) + clip.offsetTicks;
                    int seqTick = localTick % seqLength;
                    
                    // Check for loop boundary (when seqTick wraps to 0)
                    int prevLocalTick = localTick - 1;
                    int prevSeqTick = (prevLocalTick >= 0) ? (prevLocalTick % seqLength) : -1;
                    bool crossedLoopBoundary = (prevSeqTick >= 0 && seqTick < prevSeqTick);
                    
                    // If we crossed a loop boundary, stop all notes from this sequence
                    if (crossedLoopBoundary) {
                        for (auto* midiTrack : seq->getTracks()) {
                            if (midiTrack && !midiTrack->isTempoTrack()) {
                                midiTrack->stopAllNotes();
                            }
                        }
                    }

                    // Process notes for each MIDI track in the sequence
                    for (auto* midiTrack : seq->getTracks()) {
                        if (!midiTrack || midiTrack->isMuted() || midiTrack->isTempoTrack()) continue;

                        const auto& notes = midiTrack->getNotes();
                        for (const auto& note : notes) {
                            if (!note.start.has_value() || !note.length.has_value()) continue;

                            int noteStart = note.start.value();
                            int noteEnd = noteStart + note.length.value();

                            // Note ON: if this tick matches the note start
                            if (seqTick == noteStart) {
                                midiTrack->playNote(note);
                                emitNotePlayed(note);
                            }

                            // Note OFF: if this tick matches the note end
                            if (seqTick == noteEnd) {
                                midiTrack->stopNote(note);
                            }
                        }
                    }
                }
            }
        }

        // Emit position changed
        this->emitPositionChanged(current_tick);

        // Sleep
        std::this_thread::sleep_for(std::chrono::duration<double>(timer_interval));
    }

    // Stop all notes on finish
    for (auto* seq : this->project->getSequences()) {
        for (auto* track : seq->getTracks()) {
            if (track && !track->isTempoTrack()) {
                track->stopAllNotes();
            }
        }
    }

    NOTE_NAGA_LOG_INFO("Playback thread finished (Arrangement mode)");
    emitFinished();
}