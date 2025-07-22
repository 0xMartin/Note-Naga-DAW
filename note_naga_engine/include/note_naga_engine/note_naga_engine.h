#pragma once

#include <note_naga_engine/core/mixer.h>
#include <note_naga_engine/core/project_data.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/worker/playback_worker.h>

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

#include <string>
#include <vector>

/**
 * @brief The main class for Note Naga engine. Engine allows loading MIDI files,
 * playback MIDI, and provides access to project data and offer methods to control
 * playback, mixer, and tracks. Engine have no GUI dependencies and can be used in both
 * GUI and non-GUI applications. For no GUI applications, specifi custom macro "cmake -S .
 * -B build -DQT_DEACTIVATED=ON".
 *
 * Engine how this main components:
 * - NoteNagaProject: Represents the current project and its MIDI data.
 * - NoteNagaMixer: Manages audio mixing and track control.
 * - PlaybackWorker: Handles playback in a separate thread, allowing for smooth audio
 * playback.
 *
 * Note data structure:
 * - NoteNagaNote <-(parent)- NoteNagaTrack <-(parent)- NoteNagaMidiSeq <-(parent)-
 * NoteNagaProject
 *
 * Note play workflow:
 * NoteNagaNote -> PlaybackWorker -> NoteNagaMixer -(Synthesizer)-> DSPEngine -> Audio
 * Output
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaEngine : public QObject {
    Q_OBJECT
#else
class NoteNagaEngine {
#endif

public:
    /**
     * @brief Default constructor for NoteNagaEngine.
     */
    explicit NoteNagaEngine();

    /**
     * @brief Destructor for NoteNagaEngine.
     */
    ~NoteNagaEngine();

    // --- Initialization ---

    /**
     * @brief Initializes the engine and its core components.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize();

    // --- Playback Control ---

    /**
     * @brief Starts MIDI/audio playback.
     */
    void startPlayback();

    /**
     * @brief Stops MIDI/audio playback.
     */
    void stopPlayback();

    /**
     * @brief Sets the playback position (tick).
     * @param tick Tick to set as new playback position.
     */
    void setPlaybackPosition(int tick);

    /**
     * @brief Returns whether playback is currently running.
     * @return True if playing, false otherwise.
     */
    bool isPlaying() const {
        return playback_worker ? playback_worker->isPlaying() : false;
    }

    // --- Project Control ---

    /**
     * @brief Loads a project from a MIDI file.
     * @param midi_file_path Path to the MIDI file to load.
     * @return True if the project is loaded successfully, false otherwise.
     */
    bool loadProject(const std::string &midi_file_path);

    /**
     * @brief Changes the current playback/project tempo.
     * @param new_tempo New tempo in BPM.
     */
    void changeTempo(int new_tempo);

    // --- Mixer Control ---

    /**
     * @brief Mutes or unmutes a track in the mixer.
     * @param track Pointer to the track to mute/unmute.
     * @param mute True to mute, false to unmute.
     */
    void muteTrack(NoteNagaTrack *track, bool mute = true);

    /**
     * @brief Solos or unsolos a track in the mixer.
     * @param track Pointer to the track to solo/unsolo.
     * @param solo True to solo, false to unsolo.
     */
    void soloTrack(NoteNagaTrack *track, bool solo = true);

    // --- Getters for main components ---

    /**
     * @brief Gets the current project.
     * @return Pointer to the NoteNagaProject.
     */
    NoteNagaProject *getProject() { return this->project; }

    /**
     * @brief Gets the mixer instance.
     * @return Pointer to the NoteNagaMixer.
     */
    NoteNagaMixer *getMixer() { return this->mixer; }

    /**
     * @brief Gets the playback worker instance.
     * @return Pointer to the PlaybackWorker.
     */
    PlaybackWorker *getPlaybackWorker() { return this->playback_worker; }

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when playback starts.
     */
    void playbackStarted(); 
    /**
     * @brief Signal emitted when playback stops.
     */
    void playbackStopped();
#endif

protected:
    NoteNagaProject *project;        ///< Pointer to the current project instance
    NoteNagaMixer *mixer;            ///< Pointer to the mixer instance
    PlaybackWorker *playback_worker; ///< Pointer to the playback worker instance
};