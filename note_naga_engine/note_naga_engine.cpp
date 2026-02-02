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
    this->project = nullptr;
    this->mixer = nullptr;
    this->playback_worker = nullptr;
    this->dsp_engine = nullptr;
    this->audio_worker = nullptr;
    NOTE_NAGA_LOG_INFO("Instance created. Version: " + std::string(NOTE_NAGA_VERSION_STR));
}

NoteNagaEngine::~NoteNagaEngine() {
    if (playback_worker) playback_worker->stop();

    if (dsp_engine) {
        delete dsp_engine;
        dsp_engine = nullptr;
    }

    if (audio_worker) {
        delete audio_worker;
        audio_worker = nullptr;
    }

    if (mixer) {
        delete mixer;
        mixer = nullptr;
    }

    if (playback_worker) {
        delete playback_worker;
        playback_worker = nullptr;
    }

    for (auto *synth : this->synthesizers) {
        delete synth;
    }

    if (project) {
        delete project;
        project = nullptr;
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

    NOTE_NAGA_LOG_INFO("Instance destroyed");
}

/*******************************************************************************************************/
// Initialization
/*******************************************************************************************************/

bool NoteNagaEngine::initialize() {
    // Initialize synthesizers
    if (this->synthesizers.empty()) {
        std::string soundfontPath = SoundFontFinder::findSoundFont();
        this->synthesizers.push_back(
            new NoteNagaSynthFluidSynth("FluidSynth 1", soundfontPath));
    }

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
        this->metronome->setProject(this->project);
    }

    // project
    if (!this->project) this->project = new NoteNagaRuntimeData();

    // mixer
    if (!this->mixer) { this->mixer = new NoteNagaMixer(this->project, &this->synthesizers); }

    // playback worker
    if (!this->playback_worker) {
        this->playback_worker = new NoteNagaPlaybackWorker(this->project, this->mixer, 30.0);

        this->playback_worker->addFinishedCallback([this]() {
            if (mixer) mixer->stopAllNotes();
            NN_QT_EMIT(this->playbackStopped());
        });
    }

    // dsp engine
    if (!this->dsp_engine) {
        this->dsp_engine = new NoteNagaDSPEngine(this->metronome, this->spectrum_analyzer, this->pan_analyzer);
        for (auto *synth : this->synthesizers) {
            if (auto *softSynth = dynamic_cast<INoteNagaSoftSynth *>(synth)) {
                this->dsp_engine->addSynth(softSynth);
            }
        }
    }

    // audio worker
    if (!this->audio_worker) { 
        this->audio_worker = new NoteNagaAudioWorker(this->dsp_engine);
        this->audio_worker->start(44100, 512);
    }

    bool status = this->project && this->mixer && this->playback_worker &&
                  !this->synthesizers.empty() && this->audio_worker && this->dsp_engine;
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
    if (this->project) {
        this->project->setTempo(new_tempo);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to change tempo: Project is not initialized");
    }
    if (playback_worker) {
        playback_worker->recalculateWorkerTempo();
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to change tempo: Playback worker is not initialized");
    }
}

bool NoteNagaEngine::startPlayback() {
    if (playback_worker) {
        if (playback_worker->play()) {
            NN_QT_EMIT(this->playbackStarted());
            return true;
        }
    }
    NOTE_NAGA_LOG_WARNING("Failed to start playback");
    return false;
}

bool NoteNagaEngine::stopPlayback() {
    if (playback_worker) {
        if (playback_worker->stop()) {
            // playbackStopped is emitted from thread finished callback
            return true;
        }
    }
    if (mixer) mixer->stopAllNotes();
    NOTE_NAGA_LOG_WARNING("Failed to stop playback");
    return false;
}

void NoteNagaEngine::playSingleNote(const NN_Note_t &midi_note) {
    if (mixer) {
        mixer->pushToQueue(NN_MixerMessage_t{midi_note, true, true});
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to play single note: Mixer is not initialized");
    }
}

void NoteNagaEngine::stopSingleNote(const NN_Note_t &midi_note) {
    if (mixer) {
        mixer->pushToQueue(NN_MixerMessage_t{midi_note, false, true});
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to play single note: Mixer is not initialized");
    }
}

void NoteNagaEngine::setPlaybackPosition(int tick) {
    if (playback_worker && playback_worker->isPlaying()) { playback_worker->stop(); }
    if (this->project) {
        this->project->setCurrentTick(tick);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to set playback position: Project is not initialized");
    }
}

/*******************************************************************************************************/
// Project Control
/*******************************************************************************************************/

bool NoteNagaEngine::loadProject(const std::string &midi_file_path) {
    if (!this->project) {
        NOTE_NAGA_LOG_ERROR("Project is not initialized");
        return false;
    }
    this->stopPlayback();
    return this->project->loadProject(midi_file_path);
}

/*******************************************************************************************************/
// Mixer Control
/*******************************************************************************************************/

void NoteNagaEngine::muteTrack(NoteNagaTrack *track, bool mute) {
    if (mixer) {
        mixer->muteTrack(track, mute);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to mute track: Mixer is not initialized");
    }
}

void NoteNagaEngine::soloTrack(NoteNagaTrack *track, bool solo) {
    if (mixer) {
        mixer->soloTrack(track, solo);
    } else {
        NOTE_NAGA_LOG_ERROR("Failed to solo track: Mixer is not initialized");
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
// Synthesizer Control
/*******************************************************************************************************/

void NoteNagaEngine::addSynthesizer(NoteNagaSynthesizer *synth) {
#ifndef QT_DEACTIVATED
    connect(synth, &NoteNagaSynthesizer::synthUpdated, this, &NoteNagaEngine::synthUpdated);
#endif
    this->synthesizers.push_back(synth);
    this->mixer->detectOutputs();
    if (auto *softSynth = dynamic_cast<INoteNagaSoftSynth *>(synth)) {
        this->dsp_engine->addSynth(softSynth);
    }
    NN_QT_EMIT(this->synthAdded(synth));
}

void NoteNagaEngine::removeSynthesizer(NoteNagaSynthesizer *synth) {
    auto it = std::find(this->synthesizers.begin(), this->synthesizers.end(), synth);
    if (it != this->synthesizers.end()) {
        this->synthesizers.erase(it);
        this->mixer->detectOutputs();
        if (auto *softSynth = dynamic_cast<INoteNagaSoftSynth *>(synth)) {
            this->dsp_engine->removeSynth(softSynth);
            NN_QT_EMIT(this->synthRemoved(synth));
        }
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