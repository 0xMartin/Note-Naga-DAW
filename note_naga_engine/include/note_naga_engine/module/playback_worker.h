#pragma once

#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_api.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

// Forward declarations
class NOTE_NAGA_ENGINE_API PlaybackThreadWorker;

/*******************************************************************************************************/
// Playback Mode Enum
/*******************************************************************************************************/

/**
 * @brief Playback mode determines what content is being played.
 */
enum class PlaybackMode {
    Sequence,    ///< Play only the selected MIDI sequence (loop in editor)
    Arrangement  ///< Play the full timeline/arrangement (Compose mode)
};

/*******************************************************************************************************/
// Playback Worker
/*******************************************************************************************************/

/**
 * @brief Playback worker supporting Qt signals for GUI integration.
 *
 * This class manages a playback thread and provides signals/callbacks for playback state.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaPlaybackWorker : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaPlaybackWorker {
#endif

public:
    using CallbackId = std::uint64_t;               ///< Type for callback identifier
    using FinishedCallback = std::function<void()>; ///< Callback type for finished event
    using PositionChangedCallback =
        std::function<void(int)>; ///< Callback type for position changed event
    using PlayingStateCallback =
        std::function<void(bool)>; ///< Callback type for playing state change

    /**
     * @brief Constructs the playback worker.
     * @param project Pointer to NoteNagaRuntimeData.
     * @param timer_interval_ms Worker timer interval in milliseconds.
     */
    explicit NoteNagaPlaybackWorker(NoteNagaRuntimeData *project,
                            double timer_interval_ms);

    /**
     * @brief Returns whether playback is currently running.
     * @return True if playing, false otherwise.
     */
    bool isPlaying() const { return playing; }

    /**
     * @brief Recalculates the tempo in the playback thread worker.
     */
    void recalculateWorkerTempo();

    /**
     * @brief Starts playback.
     * @return True if playback started successfully.
     */
    bool play();

    /**
     * @brief Stops playback.
     * @return True if playback was running and is now stopped.
     */
    bool stop();

    /**
     * @brief Enables or disables looping.
     * @param enabled True to enable looping, false to disable.
     */
    void enableLooping(bool enabled);

    /**
     * @brief Sets the playback mode (Sequence or Arrangement).
     * @param mode The playback mode to use.
     */
    void setPlaybackMode(PlaybackMode mode);

    /**
     * @brief Gets the current playback mode.
     * @return Current playback mode.
     */
    PlaybackMode getPlaybackMode() const { return playback_mode_; }

    /**
     * @brief Sets the DSP engine for audio sample position synchronization.
     * @param dsp Pointer to the DSP engine.
     */
    void setDSPEngine(class NoteNagaDSPEngine* dsp) { dsp_engine_ = dsp; }

    /**
     * @brief Adds a callback for the finished event.
     * @param cb Callback function.
     * @return Unique callback ID.
     */
    CallbackId addFinishedCallback(FinishedCallback cb);

    /**
     * @brief Adds a callback for the position changed event.
     * @param cb Callback function.
     * @return Unique callback ID.
     */
    CallbackId addPositionChangedCallback(PositionChangedCallback cb);

    /**
     * @brief Adds a callback for the playing state changed event.
     * @param cb Callback function.
     * @return Unique callback ID.
     */
    CallbackId addPlayingStateCallback(PlayingStateCallback cb);

    /**
     * @brief Removes a finished callback by its ID.
     * @param id Callback ID.
     */
    void removeFinishedCallback(CallbackId id);

    /**
     * @brief Removes a position changed callback by its ID.
     * @param id Callback ID.
     */
    void removePositionChangedCallback(CallbackId id);

    /**
     * @brief Removes a playing state callback by its ID.
     * @param id Callback ID.
     */
    void removePlayingStateCallback(CallbackId id);

private:
    NoteNagaRuntimeData *project; ///< Pointer to project data (not owned)
    class NoteNagaDSPEngine* dsp_engine_ = nullptr; ///< DSP engine for audio sync

    // Internal State
    // ////////////////////////////////////////////////////////////////////////////////

    double timer_interval;                 ///< Timer interval in milliseconds
    bool looping;                          ///< Looping is enabled
    PlaybackMode playback_mode_;           ///< Current playback mode
    std::atomic<bool> playing{false};      ///< Whether playback is currently running
    std::atomic<bool> should_stop{false};  ///< Flag to signal playback should stop
    std::thread worker_thread;             ///< Thread running the playback logic
    PlaybackThreadWorker *worker{nullptr}; ///< Pointer to the thread worker
    std::atomic<bool> pending_cleanup{false}; ///< Flag: worker needs to be deleted in next play()

    // Callbacks
    // ////////////////////////////////////////////////////////////////////////////////

    CallbackId last_id = 0; ///< Last assigned callback ID
    std::vector<std::pair<CallbackId, FinishedCallback>>
        finished_callbacks; ///< List of finished callbacks
    std::vector<std::pair<CallbackId, PositionChangedCallback>>
        position_changed_callbacks; ///< List of position changed callbacks
    std::vector<std::pair<CallbackId, PlayingStateCallback>>
        playing_state_callbacks; ///< List of playing state change callbacks

    // Private Methods
    // ////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Cleans up the worker thread.
     */
    void cleanupThread();

    /**
     * @brief Emits all registered finished callbacks and the Qt signal.
     */
    void emitFinished();

    /**
     * @brief Emits all registered position changed callbacks and the Qt signal.
     * @param tick Current playback tick.
     */
    void emitPositionChanged(int tick);

    /**
     * @brief Emits all registered playing state callbacks and the Qt signal.
     * @param playing True if playing, false otherwise.
     */
    void emitPlayingState(bool playing);

    /**
     * @brief Emits the Qt signal for a played note.
     * @param note The note that was played.
     */
    void emitNotePlayed(const NN_Note_t &note);

    // SIGNALS
    // ////////////////////////////////////////////////////////////////////////////////

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Qt signal emitted when playback is finished.
     */
    void finished();
    /**
     * @brief Qt signal emitted when playback position changes.
     * @param tick Current tick.
     */
    void currentTickChanged(int tick);
    /**
     * @brief Qt signal emitted when playing state changes.
     * @param playing_val New playing state.
     */
    void playingStateChanged(bool playing_val);
    /**
     * @brief Qt signal emitted when a note is played during playback.
     * @param note The note that was played.
     */
    void notePlayed(const NN_Note_t &note);
#endif
};

/*******************************************************************************************************/
// Playback Thread Worker
/*******************************************************************************************************/

/**
 * @brief Worker class responsible for running playback logic in a separate thread.
 */
class NOTE_NAGA_ENGINE_API PlaybackThreadWorker {
public:
    using CallbackId = std::uint64_t;               ///< Type for callback identifier
    using FinishedCallback = std::function<void()>; ///< Callback type for finished event
    using PositionChangedCallback =
        std::function<void(int)>; ///< Callback type for position changed event
    using NotePlayedCallback =
        std::function<void(const NN_Note_t&)>; ///< Callback type for note played event

    /**
     * @brief Constructs the worker.
     * @param project Pointer to NoteNagaRuntimeData instance.
     * @param timer_interval Timer interval in milliseconds.
     * @param mode Playback mode (Sequence or Arrangement).
     */
    PlaybackThreadWorker(NoteNagaRuntimeData *project,
                         double timer_interval,
                         PlaybackMode mode = PlaybackMode::Sequence);

    /**
     * @brief Recalculates the tempo based on current project settings.
     */
    void recalculateTempo();

    /**
     * @brief Requests playback to stop (thread-safe).
     */
    void stop();

    /**
     * @brief Main execution loop to be run in a separate thread.
     */
    void run();

    /**
     * @brief Enables or disables looping.
     * @param enabled True to enable looping, false to disable.
     */
    void enableLooping(bool enabled);

    /**
     * @brief Adds a callback for when playback finishes.
     * @param cb Callback function.
     * @return Unique callback ID.
     */
    CallbackId addFinishedCallback(FinishedCallback cb);

    /**
     * @brief Adds a callback for when playback position changes.
     * @param cb Callback function taking the tick position.
     * @return Unique callback ID.
     */
    CallbackId addPositionChangedCallback(PositionChangedCallback cb);

    /**
     * @brief Removes a finished callback by its ID.
     * @param id Callback ID to remove.
     */
    void removeFinishedCallback(CallbackId id);

    /**
     * @brief Removes a position changed callback by its ID.
     * @param id Callback ID to remove.
     */
    void removePositionChangedCallback(CallbackId id);

    /**
     * @brief Adds a callback for when a note is played during playback.
     * @param cb Callback function taking the note.
     * @return Unique callback ID.
     */
    CallbackId addNotePlayedCallback(NotePlayedCallback cb);

    /**
     * @brief Removes a note played callback by its ID.
     * @param id Callback ID to remove.
     */
    void removeNotePlayedCallback(CallbackId id);

    std::atomic<bool> should_stop{false}; ///< Flag to signal worker thread should stop

    /**
     * @brief Sets the DSP engine for audio sample position synchronization.
     * @param dsp Pointer to the DSP engine.
     */
    void setDSPEngine(class NoteNagaDSPEngine* dsp) { dsp_engine_ = dsp; }

private:
    NoteNagaRuntimeData *project; ///< Pointer to project data (not owned)
    class NoteNagaDSPEngine* dsp_engine_ = nullptr; ///< DSP engine for audio sync

    // Timing
    // ////////////////////////////////////////////////////////////////////////////////

    double timer_interval; ///< Timer interval in milliseconds
    double ms_per_tick;    ///< Milliseconds per tick, for timing
    std::chrono::high_resolution_clock::time_point
        start_time_point;    ///< Start time of playback
    int start_tick_at_start; ///< Tick at which playback started
    int last_tempo_check_tick{0}; ///< Last tick where tempo was checked (for dynamic tempo)
    bool looping; ///< Looping is enabled
    PlaybackMode playback_mode_; ///< Current playback mode (Sequence or Arrangement)

    // Callbacks
    // ////////////////////////////////////////////////////////////////////////////////

    CallbackId last_id = 0; ///< Last assigned callback ID
    std::vector<std::pair<CallbackId, FinishedCallback>>
        finished_callbacks; ///< List of finished callbacks
    std::vector<std::pair<CallbackId, PositionChangedCallback>>
        position_changed_callbacks; ///< List of position changed callbacks
    std::vector<std::pair<CallbackId, NotePlayedCallback>>
        note_played_callbacks; ///< List of note played callbacks

    // Private Methods
    // //////////////////////////////////////////////////////////////////////////

    /**
     * @brief Emits all registered finished callbacks.
     */
    void emitFinished();

    /**
     * @brief Emits all registered position changed callbacks with the given tick.
     * @param tick Current playback tick.
     */
    void emitPositionChanged(int tick);

    /**
     * @brief Emits all registered note played callbacks with the given note.
     * @param note The note that was played.
     */
    void emitNotePlayed(const NN_Note_t &note);

    /**
     * @brief Run playback in Sequence mode (single MIDI sequence).
     */
    void runSequenceMode();

    /**
     * @brief Run playback in Arrangement mode (full timeline).
     */
    void runArrangementMode();
};