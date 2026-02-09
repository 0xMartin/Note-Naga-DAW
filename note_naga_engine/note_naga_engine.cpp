#include <note_naga_engine/note_naga_engine.h>

#include <note_naga_engine/logger.h>
#include <note_naga_engine/note_naga_version.h>
#include <note_naga_engine/synth/synth_fluidsynth.h>
#include <note_naga_engine/core/soundfont_finder.h>

NoteNagaEngine::NoteNagaEngine()
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
    this->runtime_data = nullptr;
    this->playback_worker = nullptr;
    this->dsp_engine = nullptr;
    this->audio_worker = nullptr;
    this->spectrum_analyzer = nullptr;
    this->pan_analyzer = nullptr;
    this->metronome = nullptr;
    this->external_midi_router = nullptr;
    NOTE_NAGA_LOG_INFO("Instance created. Version: " + std::string(NOTE_NAGA_VERSION_STR));
}

NoteNagaEngine::~NoteNagaEngine() {
    if (playback_worker) playback_worker->stop();

    // Audio worker must be deleted BEFORE dsp_engine because it uses dsp_engine in callback
    if (audio_worker) {
        delete audio_worker;
        audio_worker = nullptr;
    }

    if (dsp_engine) {
        delete dsp_engine;
        dsp_engine = nullptr;
    }

    if (playback_worker) {
        delete playback_worker;
        playback_worker = nullptr;
    }

    if (runtime_data) {
        delete runtime_data;
        runtime_data = nullptr;
    }

    if (spectrum_analyzer) {
        delete spectrum_analyzer;
        spectrum_analyzer = nullptr;
    }

    if (pan_analyzer) {
        delete pan_analyzer;
        pan_analyzer = nullptr;
    }

    if (metronome) {
        delete metronome;
        metronome = nullptr;
    }

    if (external_midi_router) {
        delete external_midi_router;
        external_midi_router = nullptr;
    }

    NOTE_NAGA_LOG_INFO("Instance destroyed");
}

/*******************************************************************************************************/
// Initialization
/*******************************************************************************************************/

bool NoteNagaEngine::initialize() {
    // Initialize spectrum analyzer
    if (!this->spectrum_analyzer) {
        this->spectrum_analyzer = new NoteNagaSpectrumAnalyzer(2048);
    }

    // Initialize pan analyzer
    if (!this->pan_analyzer) {
        this->pan_analyzer = new NoteNagaPanAnalyzer(2048);
    }

    // Initialize metronome
    if (!this->metronome) {
        this->metronome = new NoteNagaMetronome();
        this->metronome->setSampleRate(44100);
        this->metronome->setProject(this->runtime_data);
    }

    // Initialize external MIDI router
    if (!this->external_midi_router) {
        this->external_midi_router = new ExternalMidiRouter();
    }

    // runtime data
    if (!this->runtime_data) this->runtime_data = new NoteNagaRuntimeData();

    // playback worker (no longer needs mixer)
    if (!this->playback_worker) {
        this->playback_worker = new NoteNagaPlaybackWorker(this->runtime_data, 30.0);

        this->playback_worker->addFinishedCallback([this]() {
            // Stop all notes on all tracks
            if (runtime_data && runtime_data->getActiveSequence()) {
                for (auto* track : runtime_data->getActiveSequence()->getTracks()) {
                    if (track && !track->isTempoTrack()) {
                        track->stopAllNotes();
                    }
                }
            }
            NN_QT_EMIT(this->playbackStopped());
        });
    }

    // dsp engine - now works with tracks instead of global synths
    if (!this->dsp_engine) {
        this->dsp_engine = new NoteNagaDSPEngine(this->metronome, this->spectrum_analyzer, this->pan_analyzer);
        // Set runtime data for track-based rendering
        this->dsp_engine->setRuntimeData(this->runtime_data);
        this->dsp_engine->setSampleRate(44100);
    }
    
    // Set DSP engine on playback worker for audio synchronization
    if (this->playback_worker) {
        this->playback_worker->setDSPEngine(this->dsp_engine);
        this->playback_worker->setExternalMidiRouter(this->external_midi_router);
    }

    // audio worker - start asynchronously to avoid blocking UI on slow devices (e.g. Bluetooth)
    if (!this->audio_worker) { 
        this->audio_worker = new NoteNagaAudioWorker(this->dsp_engine);
        this->audio_worker->startAsync(44100, 512);
    }
    
    // Set audio manager sample rate
    this->runtime_data->getAudioManager().setSampleRate(44100);

    bool status = this->runtime_data && this->playback_worker &&
                  this->audio_worker && this->dsp_engine;
    if (status) {
        NOTE_NAGA_LOG_INFO("Initialized successfully");
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to initialize Note Naga Engine components");
    }
    return status;
}

/*******************************************************************************************************/
// Playback Control
/*******************************************************************************************************/

void NoteNagaEngine::changeTempo(int new_tempo) {
    if (this->runtime_data) {
        this->runtime_data->setTempo(new_tempo);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to change tempo: Runtime data is not initialized");
    }
    if (playback_worker) {
        playback_worker->recalculateWorkerTempo();
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to change tempo: Playback worker is not initialized");
    }
}

bool NoteNagaEngine::startPlayback() {
    if (playback_worker) {
        // Reset DSP blocks to prevent state bleed from previous playback
        if (dsp_engine) {
            dsp_engine->resetAllBlocks();
        }
        
        if (playback_worker->play()) {
            NN_QT_EMIT(this->playbackStarted());
            return true;
        }
    }
    NOTE_NAGA_LOG_WARNING("Failed to start playback");
    return false;
}

bool NoteNagaEngine::stopPlayback() {
    bool stopped = false;
    if (playback_worker) {
        stopped = playback_worker->stop();
    }
    
    // Stop all notes on all track synthesizers
    // This ensures no hanging notes even in arrangement mode with multiple clips
    if (runtime_data) {
        for (NoteNagaMidiSeq *seq : runtime_data->getSequences()) {
            if (seq) {
                for (NoteNagaTrack *track : seq->getTracks()) {
                    if (track) {
                        track->stopAllNotes();
                    }
                }
            }
        }
    }
    
    if (!stopped) {
        NOTE_NAGA_LOG_WARNING("Playback worker was not playing");
    }
    // playbackStopped is emitted from thread finished callback
    return stopped;
}

void NoteNagaEngine::playSingleNote(const NN_Note_t &midi_note) {
    // Play note directly through the track's synth
    if (midi_note.parent) {
        midi_note.parent->playNote(midi_note);
#ifndef QT_DEACTIVATED
        emit notePlayed(midi_note);
#endif
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to play single note: Note has no parent track");
    }
}

void NoteNagaEngine::stopSingleNote(const NN_Note_t &midi_note) {
    // Stop note directly through the track's synth
    if (midi_note.parent) {
        midi_note.parent->stopNote(midi_note);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to stop single note: Note has no parent track");
    }
}

void NoteNagaEngine::setPlaybackPosition(int tick) {
    if (playback_worker && playback_worker->isPlaying()) { playback_worker->stop(); }
    if (this->runtime_data) {
        this->runtime_data->setCurrentTick(tick);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to set playback position: Runtime data is not initialized");
    }
}

/*******************************************************************************************************/
// Project Control
/*******************************************************************************************************/

bool NoteNagaEngine::loadProject(const std::string &midi_file_path) {
    if (!this->runtime_data) {
        NOTE_NAGA_LOG_ERROR("Runtime data is not initialized");
        return false;
    }
    this->stopPlayback();
    return this->runtime_data->loadProject(midi_file_path);
}

/*******************************************************************************************************/
// Track Control
/*******************************************************************************************************/

void NoteNagaEngine::muteTrack(NoteNagaTrack *track, bool mute) {
    if (track) {
        track->setMuted(mute);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to mute track: Track is nullptr");
    }
}

void NoteNagaEngine::soloTrack(NoteNagaTrack *track, bool solo) {
    if (track) {
        track->setSolo(solo);
        // Update solo track in sequence
        if (runtime_data && runtime_data->getActiveSequence()) {
            NoteNagaMidiSeq* seq = runtime_data->getActiveSequence();
            if (solo) {
                seq->setSoloTrack(track);
                // Stop all notes on other tracks (they should not play during solo)
                for (NoteNagaTrack* otherTrack : seq->getTracks()) {
                    if (otherTrack && otherTrack != track) {
                        otherTrack->stopAllNotes();
                    }
                }
            } else if (seq->getSoloTrack() == track) {
                seq->setSoloTrack(nullptr);
            }
        }
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to solo track: Track is nullptr");
    }
}

void NoteNagaEngine::enableLooping(bool enabled) {
    if (playback_worker) {
        playback_worker->enableLooping(enabled);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to enable looping: Playback worker is not initialized");
    }
}

/*******************************************************************************************************/
// DSP Engine Control
/*******************************************************************************************************/

void NoteNagaEngine::enableMetronome(bool enabled) {
    if (this->metronome) {
        this->metronome->setEnabled(enabled);
    }
}

bool NoteNagaEngine::isMetronomeEnabled() const {
    return this->metronome ? this->metronome->isEnabled() : false;
}

std::pair<float, float> NoteNagaEngine::getCurrentVolumeDb() {
    if (this->dsp_engine) {
        return dsp_engine->getCurrentVolumeDb();
    }
    return {-100.0f, -100.0f};
}