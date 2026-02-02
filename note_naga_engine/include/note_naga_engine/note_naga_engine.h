#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <note_naga_engine/module/audio_worker.h>
#include <note_naga_engine/module/dsp_engine.h>
#include <note_naga_engine/module/mixer.h>
#include <note_naga_engine/module/playback_worker.h>
#include <note_naga_engine/module/spectrum_analyzer.h>
#include <note_naga_engine/module/pan_analyzer.h>
#include <note_naga_engine/module/metronome.h>

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
 * - NoteNagaRuntimeData: Represents the current runtime data and its MIDI data.
 * - NoteNagaMixer: Manages audio mixing and track control.
 * - PlaybackWorker: Handles playback in a separate thread, allowing for smooth audio
 * playback.
 *
 * Note data structure:
 * - NoteNagaNote <-(parent)- NoteNagaTrack <-(parent)- NoteNagaMidiSeq <-(parent)-
 * NoteNagaRuntimeData
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

    /*******************************************************************************************************/
    // Initialization
    /*******************************************************************************************************/

    /**
     * @brief Initializes the engine and its core components.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize();

    /*******************************************************************************************************/
    // Playback Control
    /*******************************************************************************************************/

    /**
     * @brief Changes the current playback/project tempo.
     * @param new_tempo New tempo in BPM.
     */
    void changeTempo(int new_tempo);

    /**
     * @brief Starts MIDI/audio playback.
     * @return True if playback started successfully, false if already playing or
     * failed to start.
     */
    bool startPlayback();

    /**
     * @brief Stops MIDI/audio playback.
     * @return True if playback stopped successfully, false if not playing.
     */
    bool stopPlayback();

    /**
     * @brief Plays a single MIDI note immediately. Send note on.
     * @param midi_note MIDI note to play.
     */
    void playSingleNote(const NN_Note_t &midi_note);

    /**
     * @brief Plays a single MIDI note immediately. Send note off.
     * @param midi_note MIDI note to play.
     */
    void stopSingleNote(const NN_Note_t &midi_note);

    /**
     * @brief Sets the playback position (tick).
     * @param tick Tick to set as new playback position.
     */
    void setPlaybackPosition(int tick);

    /**
     * @brief Returns whether playback is currently running.
     * @return True if playing, false otherwise.
     */
    bool isPlaying() const { return playback_worker ? playback_worker->isPlaying() : false; }

    /*******************************************************************************************************/
    // Project Control
    /*******************************************************************************************************/

    /**
     * @brief Loads a project from a MIDI file.
     * @param midi_file_path Path to the MIDI file to load.
     * @return True if the project is loaded successfully, false otherwise.
     */
    bool loadProject(const std::string &midi_file_path);

    /*******************************************************************************************************/
    // Mixer Control
    /*******************************************************************************************************/

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

    /**
     * @brief Enables or disables looping for the current sequence.
     * @param enabled True to enable looping, false to disable.
     */
    void enableLooping(bool enabled);

    /*******************************************************************************************************/
    // Synthesizer Control
    /*******************************************************************************************************/

    /**
     * @brief Gets the list of available synthesizers.
     * @return Vector of pointers to NoteNagaSynthesizer instances.
     */
    std::vector<NoteNagaSynthesizer *> getSynthesizers() { return this->synthesizers; }

    /**
     * @brief Adds a synthesizer to the engine.
     * @param synth Pointer to the NoteNagaSynthesizer to add.
     */
    void addSynthesizer(NoteNagaSynthesizer *synth);

    /**
     * @brief Removes a synthesizer from the engine.
     * @param synth Pointer to the NoteNagaSynthesizer to remove.
     */
    void removeSynthesizer(NoteNagaSynthesizer *synth);

    /*******************************************************************************************************/
    // DSP Engine Control
    /*******************************************************************************************************/

    /**
     * @brief Gets the DSP engine instance.
     * @return Pointer to the NoteNagaDSPEngine.
     */
    void enableMetronome(bool enabled);

    /**
     * @brief Returns whether the metronome is enabled.
     * @return True if metronome is enabled, false otherwise.
     */
    bool isMetronomeEnabled() const;

    /**
     * @brief Get the current volume in dB.
     * @return A pair containing the left and right channel volume in dB.
     */
    std::pair<float, float> getCurrentVolumeDb();

    /*******************************************************************************************************/
    // Getters for main components
    /*******************************************************************************************************/

    /**
     * @brief Gets the runtime data.
     * @return Pointer to the NoteNagaRuntimeData.
     */
    NoteNagaRuntimeData *getRuntimeData() { return this->runtime_data; }

    /**
     * @brief Gets the mixer instance.
     * @return Pointer to the NoteNagaMixer.
     */
    NoteNagaMixer *getMixer() { return this->mixer; }

    /**
     * @brief Gets the playback worker instance.
     * @return Pointer to the PlaybackWorker.
     */
    NoteNagaPlaybackWorker *getPlaybackWorker() { return this->playback_worker; }

    /**
     * @brief Gets the DSP engine instance.
     * @return Pointer to the NoteNagaDSPEngine.
     */
    NoteNagaDSPEngine *getDSPEngine() { return this->dsp_engine; }

    /**
     * @brief Gets the audio worker instance.
     * @return Pointer to the NoteNagaAudioWorker.
     */
    NoteNagaAudioWorker *getAudioWorker() { return this->audio_worker; }

    /**
     * @brief Gets the metronome instance.
     * @return Pointer to the NoteNagaMetronome.
     */
    NoteNagaMetronome *getMetronome() { return this->metronome; }

    /**
     * @brief Gets the spectrum analyzer instance.
     * @return Pointer to the NoteNagaSpectrumAnalyzer.
     */
    NoteNagaSpectrumAnalyzer *getSpectrumAnalyzer() { return this->spectrum_analyzer; }

    /**
     * @brief Gets the pan analyzer instance.
     * @return Pointer to the NoteNagaPanAnalyzer.
     */
    NoteNagaPanAnalyzer *getPanAnalyzer() { return this->pan_analyzer; }

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

    /**
     * @brief Signal emitted when a synthesizer is added.
     */
    void synthAdded(NoteNagaSynthesizer *synth);

    /**
     * @brief Signal emitted when a synthesizer is removed.
     */
    void synthRemoved(NoteNagaSynthesizer *synth);

    /**
     * @brief Signal emitted when a synthesizer is updated.
     */
    void synthUpdated(NoteNagaSynthesizer *synth);
#endif

protected:
    NoteNagaRuntimeData *runtime_data;                   ///< Pointer to the runtime data instance
    NoteNagaPlaybackWorker *playback_worker;         ///< Pointer to the playback worker instance
    NoteNagaMixer *mixer;                            ///< Pointer to the mixer instance
    NoteNagaDSPEngine *dsp_engine;                   ///< Pointer to the DSP engine instance
    NoteNagaAudioWorker *audio_worker;               ///< Pointer to the audio worker instance
    NoteNagaSpectrumAnalyzer *spectrum_analyzer;     ///< Pointer to the spectrum analyzer instance
    NoteNagaPanAnalyzer *pan_analyzer;               ///< Pointer to the pan analyzer instance
    NoteNagaMetronome *metronome;                    ///< Pointer to the metronome instance
    std::vector<NoteNagaSynthesizer *> synthesizers; ///< List of synthesizers used by the engine
};