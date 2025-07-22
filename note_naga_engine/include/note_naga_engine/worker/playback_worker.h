#pragma once

#include <note_naga_engine/core/mixer.h>
#include <note_naga_engine/core/project_data.h>
#include <note_naga_engine/note_naga_api.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>

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

    /**
     * @brief Constructs the worker.
     * @param project Pointer to NoteNagaProject instance.
     * @param mixer Pointer to NoteNagaMixer instance.
     * @param timer_interval Timer interval in milliseconds.
     */
    PlaybackThreadWorker(NoteNagaProject *project, NoteNagaMixer *mixer,
                         double timer_interval);

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

    std::atomic<bool> should_stop{false}; ///< Flag to signal worker thread should stop

private:
    NoteNagaProject *project; ///< Pointer to project data (not owned)
    NoteNagaMixer *mixer;     ///< Pointer to mixer (not owned)
    double timer_interval;    ///< Timer interval in milliseconds
    double ms_per_tick;       ///< Milliseconds per tick, for timing
    std::chrono::high_resolution_clock::time_point
        start_time_point;    ///< Start time of playback
    int start_tick_at_start; ///< Tick at which playback started

    CallbackId last_id = 0; ///< Last assigned callback ID
    std::vector<std::pair<CallbackId, FinishedCallback>>
        finished_callbacks; ///< List of finished callbacks
    std::vector<std::pair<CallbackId, PositionChangedCallback>>
        position_changed_callbacks; ///< List of position changed callbacks

    /**
     * @brief Emits all registered finished callbacks.
     */
    void emitFinished();

    /**
     * @brief Emits all registered position changed callbacks with the given tick.
     * @param tick Current playback tick.
     */
    void emitPositionChanged(int tick);
};

/*******************************************************************************************************/
// Playback Worker
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED
/**
 * @brief Playback worker supporting Qt signals for GUI integration.
 *
 * This class manages a playback thread and provides signals/callbacks for playback state.
 */
class NOTE_NAGA_ENGINE_API PlaybackWorker : public QObject {
    Q_OBJECT
#else
/**
 * @brief Playback worker without Qt signals (non-GUI builds).
 *
 * This class manages a playback thread and provides callbacks for playback state.
 */
class NOTE_NAGA_ENGINE_API PlaybackWorker {
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
     * @param project Pointer to NoteNagaProject.
     * @param mixer Pointer to NoteNagaMixer.
     * @param timer_interval_ms Worker timer interval in milliseconds.
     */
    explicit PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer,
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
#endif

private:
    /**
     * @brief Main function for the worker thread.
     */
    void threadFunc();

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

    NoteNagaProject *project;              ///< Pointer to project data (not owned)
    NoteNagaMixer *mixer;                  ///< Pointer to mixer (not owned)
    double timer_interval;                 ///< Timer interval in milliseconds
    std::atomic<bool> playing{false};      ///< Whether playback is currently running
    std::atomic<bool> should_stop{false};  ///< Flag to signal playback should stop
    std::thread worker_thread;             ///< Thread running the playback logic
    PlaybackThreadWorker *worker{nullptr}; ///< Pointer to the thread worker

    CallbackId last_id = 0; ///< Last assigned callback ID
    std::vector<std::pair<CallbackId, FinishedCallback>>
        finished_callbacks; ///< List of finished callbacks
    std::vector<std::pair<CallbackId, PositionChangedCallback>>
        position_changed_callbacks; ///< List of position changed callbacks
    std::vector<std::pair<CallbackId, PlayingStateCallback>>
        playing_state_callbacks; ///< List of playing state change callbacks
};