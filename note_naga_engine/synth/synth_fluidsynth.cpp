#include <note_naga_engine/synth/synth_fluidsynth.h>

#include <note_naga_engine/logger.h>

NoteNagaSynthFluidSynth::NoteNagaSynthFluidSynth(const std::string &name,
                                                 const std::string &sf2_path)
    : NoteNagaSynthesizer(name), synth_settings_(nullptr), fluidsynth_(nullptr) {
    // Initialize FluidSynth settings and synth
    synth_settings_ = new_fluid_settings();
    fluidsynth_ = new_fluid_synth(synth_settings_);
    int sfid = fluid_synth_sfload(fluidsynth_, sf2_path.c_str(), 1);
    // fluid_synth_set_reverb_on(fluidsynth_, 1);
    // fluid_synth_set_reverb(fluidsynth_, 0.8f, 0.5f, 0.9f, 0.3f);

    // Initialize all channels with no program set
    for (int i = 0; i < 16; ++i) {
        channel_programs_[i] = -1;
    }
    // Initialize all channels with no pan set
    for (int i = 0; i < 16; ++i) {
        channel_pan_[i] = 0.0f;
    }

    NOTE_NAGA_LOG_INFO("FluidSynth loaded sfid=" + std::to_string(sfid));
}

NoteNagaSynthFluidSynth::~NoteNagaSynthFluidSynth() {
    if (fluidsynth_) delete_fluid_synth(fluidsynth_);
    if (synth_settings_) delete_fluid_settings(synth_settings_);
}

void NoteNagaSynthFluidSynth::renderAudio(float* left, float* right, size_t num_frames) {
    // render audio using FluidSynth
    fluid_synth_write_float(
        this->fluidsynth_, num_frames,
        left, 0, 1,
        right, 0, 1
    );
}

void NoteNagaSynthFluidSynth::playNote(const NN_Note_t &note, int channel, float pan) {
    if (!note.velocity.has_value() || note.velocity.value() <= 0) return;

    NoteNagaTrack *track = note.parent;
    if (!track) return;

    // get program for the track (parent of note)
    int prog = track->getInstrument().value_or(0);

    // Set program change if needed
    if (channel_programs_[channel] != prog) {
        fluid_synth_program_change(fluidsynth_, channel, prog);
        channel_programs_[channel] = prog;
    }

    // Set pan if needed
    if (std::abs(channel_pan_[channel] - pan) > 0.01f) {
        int midiPan = static_cast<int>(std::round(pan * 63.5 + 63.5));
        fluid_synth_cc(fluidsynth_, channel, 10, static_cast<int>(std::clamp(midiPan, 0, 127)));
        channel_pan_[channel] = pan;
    }

    // Check if note is already playing
    if (playing_notes_[track].find(note.id) != playing_notes_[track].end()) { return; }

    // play note
    fluid_synth_noteon(fluidsynth_, channel, note.note, note.velocity.value_or(100));

    // Store the note in playing_notes_ for later stop
    playing_notes_[track][note.id] = PlayedNote_t{note, channel};
}

void NoteNagaSynthFluidSynth::stopNote(const NN_Note_t &note) {
    NoteNagaTrack *track = note.parent;
    if (!track) return;

    // find note in playing notes by ID
    TrackNotesMap &playingTrackNotes = playing_notes_[track];
    auto it = playingTrackNotes.find(note.id);

    // retrieve note parameters and stop it
    if (it != playingTrackNotes.end()) {
        const PlayedNote_t &pn = it->second;
        fluid_synth_noteoff(fluidsynth_, pn.channel, pn.note.note);
        playingTrackNotes.erase(it);
    }
}

void NoteNagaSynthFluidSynth::stopAllNotes(NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
    if (track) {
        for (const auto &[id, pn] : playing_notes_[track]) {
            fluid_synth_noteoff(fluidsynth_, pn.channel, pn.note.note);
        }
        playing_notes_[track].clear();
    } else if (seq) {
        for (auto &tr : seq->getTracks()) {
            if (tr) stopAllNotes(nullptr, tr);
        }
    } else {
        for (auto &[track, notes] : playing_notes_) {
            for (const auto &[id, pn] : notes) {
                fluid_synth_noteoff(fluidsynth_, pn.channel, pn.note.note);
            }
            notes.clear();
        }
    }
}