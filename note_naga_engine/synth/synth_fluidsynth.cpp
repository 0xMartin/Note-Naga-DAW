#include <note_naga_engine/synth/synth_fluidsynth.h>

#include <note_naga_engine/logger.h>

NoteNagaSynthFluidSynth::NoteNagaSynthFluidSynth(const std::string &sf2_path)
    : synth_settings_(nullptr), fluidsynth_(nullptr), audio_driver_(nullptr) {
    // Inicializace fluidsynthu
    synth_settings_ = new_fluid_settings();
    fluidsynth_ = new_fluid_synth(synth_settings_);
    int sfid = fluid_synth_sfload(fluidsynth_, sf2_path.c_str(), 1);
    audio_driver_ = new_fluid_audio_driver(synth_settings_, fluidsynth_);
    NOTE_NAGA_LOG_INFO("FluidSynth loaded sfid=" + std::to_string(sfid));
}

NoteNagaSynthFluidSynth::~NoteNagaSynthFluidSynth() {
    if (audio_driver_) delete_fluid_audio_driver(audio_driver_);
    if (fluidsynth_) delete_fluid_synth(fluidsynth_);
    if (synth_settings_) delete_fluid_settings(synth_settings_);
}

void NoteNagaSynthFluidSynth::playNote(const NN_Note_t &note) {
    NoteNagaTrack *track = note.parent;
    if (!track) return;
    int channel = track->getChannel().value_or(0);
    int prog = track->getInstrument().value_or(0);
    fluid_synth_program_change(fluidsynth_, channel, prog);
    int velocity = note.velocity.value_or(100) * master_volume_;
    fluid_synth_noteon(fluidsynth_, channel, note.note, velocity);
    playing_notes_[track].push_back(note.note);
}

void NoteNagaSynthFluidSynth::stopNote(const NN_Note_t &note) {
    NoteNagaTrack *track = note.parent;
    if (!track) return;
    int channel = track->getChannel().value_or(0);
    fluid_synth_noteoff(fluidsynth_, channel, note.note);
    auto &vec = playing_notes_[track];
    vec.erase(std::remove(vec.begin(), vec.end(), note.note), vec.end());
}

void NoteNagaSynthFluidSynth::stopAllNotes(NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
    if (track) {
        int channel = track->getChannel().value_or(0);
        for (int note : playing_notes_[track]) {
            fluid_synth_noteoff(fluidsynth_, channel, note);
        }
        playing_notes_[track].clear();
    } else if (seq) {
        for (auto &tr : seq->getTracks()) {
            if (tr) stopAllNotes(nullptr, tr);
        }
    } else {
        for (auto &[t, notes] : playing_notes_) {
            int channel = t->getChannel().value_or(0);
            for (int note : notes) {
                fluid_synth_noteoff(fluidsynth_, channel, note);
            }
            notes.clear();
        }
    }
}

void NoteNagaSynthFluidSynth::setParam(const std::string &param, float value) {
    if (param == "master_volume") master_volume_ = value;
    // Další parametry...
}