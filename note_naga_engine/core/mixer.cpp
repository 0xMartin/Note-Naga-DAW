#include "mixer.h"
#include <algorithm>
#include <iostream> // for debug output

#ifndef QT_DEACTIVATED
NoteNagaMixer::NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path) : QObject(nullptr) {
#else
NoteNagaMixer::NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path) {
#endif
    this->project = project;
    this->sf2_path = sf2_path;
    this->fluidsynth = nullptr;
    this->audio_driver = nullptr;
    this->synth_settings = nullptr;
    this->master_volume = 1.0f;
    this->master_min_note = 0;
    this->master_max_note = 127;
    this->master_note_offset = 0;
    this->master_pan = 0.0f;

#ifndef QT_DEACTIVATED
    connect(project, &NoteNagaProject::project_file_loaded_signal, this, &NoteNagaMixer::create_default_routing);
#endif

    ensure_fluidsynth();
    available_outputs = detect_outputs();
    auto it = std::find(available_outputs.begin(), available_outputs.end(), "fluidsynth");
    if (it != available_outputs.end()) {
        default_output = "fluidsynth";
    } else if (available_outputs.empty()) {
        default_output = std::string();
    } else {
        default_output = available_outputs.front();
    }
}

NoteNagaMixer::~NoteNagaMixer() { close(); }

std::vector<std::string> NoteNagaMixer::detect_outputs() {
    std::vector<std::string> outputs;
    if (fluidsynth && audio_driver) outputs.push_back("fluidsynth");
    try {
        RtMidiOut midi;
        unsigned int nPorts = midi.getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i)
            outputs.push_back(midi.getPortName(i));
    } catch (...) {}
    return outputs;
}

void NoteNagaMixer::create_default_routing() {
    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    routing_entries.clear();
    if (!project) return;
    for (NoteNagaMIDISeq *seq : project->get_sequences()) {
        if (!seq) continue;
        std::vector<bool> used_channels(16, false);
        for (NoteNagaTrack *track : seq->get_tracks()) {
            if (!track) continue;
            if (auto ch = track->get_channel(); ch.has_value()) {
                int channel = ch.value();
                if (channel >= 0 && channel < 16) used_channels[channel] = true;
            }
        }
        for (NoteNagaTrack *track : seq->get_tracks()) {
            if (!track) continue;
            int channel;
            if (auto ch = track->get_channel(); ch.has_value()) {
                channel = ch.value();
            } else {
                auto it = std::find(used_channels.begin(), used_channels.end(), false);
                if (it != used_channels.end()) {
                    channel = std::distance(used_channels.begin(), it);
                    used_channels[channel] = true;
                } else {
                    channel = 15;
                }
            }
            routing_entries.push_back(NoteNagaRoutingEntry(track, default_output, channel));
        }
    }
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::set_routing(const std::vector<NoteNagaRoutingEntry> &entries) {
    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    routing_entries = entries;
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

bool NoteNagaMixer::add_routing_entry(const std::optional<NoteNagaRoutingEntry> &entry) {
    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (entry.has_value()) {
        if (!entry.value().track) return false;
        routing_entries.push_back(entry.value());
        NN_QT_EMIT(routing_entry_stack_changed_signal());
    } else {
        NoteNagaMIDISeq *seq = project->get_active_sequence();
        if (!seq) return false;
        NoteNagaTrack *track = seq->get_active_track();
        if (!track) track = seq->get_tracks().empty() ? nullptr : seq->get_tracks().front();
        if (!track) return false;
        routing_entries.push_back(NoteNagaRoutingEntry(track, default_output, 0));
        NN_QT_EMIT(routing_entry_stack_changed_signal());
    }

    return true;
}

bool NoteNagaMixer::remove_routing_entry(int index) {
    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (index >= 0 && index < int(routing_entries.size())) {
        routing_entries.erase(routing_entries.begin() + index);
        NN_QT_EMIT(routing_entry_stack_changed_signal());
        return true;
    }

    return false;
}

void NoteNagaMixer::clear_routing_table() {
    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    routing_entries.clear();
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::note_play(const NoteNagaNote &midi_note) {
    NoteNagaTrack *track = midi_note.parent;
    if (!track) {
        std::cerr << "NoteNagaMixer: Cannot play note, missing parent track" << std::endl;
        return;
    }
    NoteNagaMIDISeq *seq = track->get_parent();
    if (!seq) {
        std::cerr << "NoteNagaMixer: Cannot play note, missing parent sequence" << std::endl;
        return;
    }

    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    int prog = track->get_instrument().value_or(0);
    NN_QT_EMIT(note_in_signal(midi_note));

    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track != track) continue;

        int note_num = midi_note.note + entry.note_offset + master_note_offset;
        if (note_num < 0 || note_num > 127) continue;
        if (note_num < master_min_note || note_num > master_max_note) continue;
        int velocity = int(
            std::min(127.0f, std::max(0.0f, float(midi_note.velocity.value_or(100)) * entry.volume * master_volume)));
        if (velocity <= 0) continue;
        float pan = std::max(-1.0f, std::min(1.0f, entry.pan + master_pan));
        int pan_cc = int((pan + 1.0f) * 63.5f);
        int ch = entry.channel;

        if (entry.output == TRACK_ROUTING_ENTRY_ANY_DEVICE) {
            for (const std::string &out_dev : available_outputs) {
                play_note_on_output(out_dev, ch, note_num, velocity, prog, pan_cc, midi_note, seq, track);
            }
        } else {
            play_note_on_output(entry.output, ch, note_num, velocity, prog, pan_cc, midi_note, seq, track);
        }
    }
}

void NoteNagaMixer::note_stop(const NoteNagaNote &midi_note) {
    NoteNagaTrack *track = midi_note.parent;
    if (!track) return;
    NoteNagaMIDISeq *seq = track->get_parent();
    if (!seq) return;

    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    auto &notes = playing_notes[seq][track];
    for (auto it = notes.begin(); it != notes.end();) {
        if (it->note_id == midi_note.id) {
            if (it->device == "fluidsynth" && fluidsynth) {
                fluid_synth_noteoff(fluidsynth, it->channel, it->note_num);
            } else {
                auto midi_out_it = midi_outputs.find(it->device);
                RtMidiOut *out = (midi_out_it != midi_outputs.end()) ? midi_out_it->second : nullptr;
                if (out) {
                    std::vector<unsigned char> msg = {static_cast<unsigned char>(0x80 | (it->channel & 0x0F)),
                                                      static_cast<unsigned char>(it->note_num),
                                                      static_cast<unsigned char>(0)};
                    out->sendMessage(&msg);
                }
            }
            it = notes.erase(it);
        } else {
            ++it;
        }
    }
}

void NoteNagaMixer::stop_all_notes(NoteNagaMIDISeq *seq, NoteNagaTrack *track) {
    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!seq && !track) {
        // Stop all
        for (auto &seq_pair : playing_notes) {
            for (auto &trk_pair : seq_pair.second) {
                for (const PlayedNote &pn : trk_pair.second) {
                    if (pn.device == "fluidsynth" && fluidsynth) {
                        fluid_synth_noteoff(fluidsynth, pn.channel, pn.note_num);
                    } else {
                        auto midi_out_it = midi_outputs.find(pn.device);
                        RtMidiOut *out = (midi_out_it != midi_outputs.end()) ? midi_out_it->second : nullptr;
                        if (out) {
                            std::vector<unsigned char> msg = {static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
                                                              static_cast<unsigned char>(pn.note_num),
                                                              static_cast<unsigned char>(0)};
                            out->sendMessage(&msg);
                        }
                    }
                }
                trk_pair.second.clear();
            }
        }
        playing_notes.clear();
    } else if (seq && !track) {
        for (auto &trk_pair : playing_notes[seq]) {
            for (const PlayedNote &pn : trk_pair.second) {
                if (pn.device == "fluidsynth" && fluidsynth) {
                    fluid_synth_noteoff(fluidsynth, pn.channel, pn.note_num);
                } else {
                    auto midi_out_it = midi_outputs.find(pn.device);
                    RtMidiOut *out = (midi_out_it != midi_outputs.end()) ? midi_out_it->second : nullptr;
                    if (out) {
                        std::vector<unsigned char> msg = {static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
                                                          static_cast<unsigned char>(pn.note_num),
                                                          static_cast<unsigned char>(0)};
                        out->sendMessage(&msg);
                    }
                }
            }
            trk_pair.second.clear();
        }
        playing_notes[seq].clear();
    } else if (seq && track) {
        auto &notes = playing_notes[seq][track];
        for (const PlayedNote &pn : notes) {
            if (pn.device == "fluidsynth" && fluidsynth) {
                fluid_synth_noteoff(fluidsynth, pn.channel, pn.note_num);
            } else {
                auto midi_out_it = midi_outputs.find(pn.device);
                RtMidiOut *out = (midi_out_it != midi_outputs.end()) ? midi_out_it->second : nullptr;
                if (out) {
                    std::vector<unsigned char> msg = {static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
                                                      static_cast<unsigned char>(pn.note_num),
                                                      static_cast<unsigned char>(0)};
                    out->sendMessage(&msg);
                }
            }
        }
        notes.clear();
    }
}

void NoteNagaMixer::mute_track(NoteNagaTrack *track, bool mute) {
    if (!track) return;

    // Ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(mutex);

    track->set_muted(mute);
    stop_all_notes(track->get_parent(), track);
}

void NoteNagaMixer::solo_track(NoteNagaTrack *track, bool solo) {
    if (!track) return;
    NoteNagaMIDISeq *seq = track->get_parent();
    if (!seq) return;
    track->set_solo(solo);

    if (solo) {
        // Ensure thread safety
        std::lock_guard<std::recursive_mutex> lock(mutex);

        seq->set_solo_track(track);
        for (NoteNagaTrack *t : seq->get_tracks()) {
            if (t && t != track) {
                t->set_solo(false);
                stop_all_notes(seq, t);
            }
        }
    } else {
        seq->set_solo_track(nullptr);
    }
}

void NoteNagaMixer::play_note_on_output(const std::string &output, int ch, int note_num, int velocity, int prog,
                                        int pan_cc, const NoteNagaNote &midi_note, NoteNagaMIDISeq *seq,
                                        NoteNagaTrack *track) {
    if (!seq || !track) return;
    auto &notes = playing_notes[seq][track];

    // prevent duplicate notes on same device/channel/note
    for (const PlayedNote &pn : notes) {
        if (pn.device == output && pn.channel == ch && pn.note_num == note_num) return;
    }

    auto &state = channel_states[output][ch];
    bool needs_program = (state.first != prog);
    bool needs_pan = (state.second != pan_cc);

    if (output == "fluidsynth" && fluidsynth) {
        if (needs_program) {
            fluid_synth_program_change(fluidsynth, ch, prog);
            state.first = prog;
        }
        if (needs_pan) {
            fluid_synth_cc(fluidsynth, ch, 10, pan_cc);
            state.second = pan_cc;
        }
        fluid_synth_noteon(fluidsynth, ch, note_num, velocity);
    } else {
        RtMidiOut *out = ensure_midi_output(output);
        if (out) {
            if (needs_pan) {
                std::vector<unsigned char> msg = {static_cast<unsigned char>(0xB0 | (ch & 0x0F)),
                                                  static_cast<unsigned char>(10), static_cast<unsigned char>(pan_cc)};
                out->sendMessage(&msg);
                state.second = pan_cc;
            }
            if (needs_program) {
                std::vector<unsigned char> msg = {static_cast<unsigned char>(0xC0 | (ch & 0x0F)),
                                                  static_cast<unsigned char>(prog)};
                out->sendMessage(&msg);
                state.first = prog;
            }
            std::vector<unsigned char> msg = {static_cast<unsigned char>(0x90 | (ch & 0x0F)),
                                              static_cast<unsigned char>(note_num),
                                              static_cast<unsigned char>(velocity)};
            out->sendMessage(&msg);
        }
    }
    notes.push_back(PlayedNote{note_num, midi_note.id, output, ch});
    NoteNagaNote note_clone = midi_note;
    note_clone.velocity = velocity;
    NN_QT_EMIT(note_out_signal(note_clone, output, ch));
}

void NoteNagaMixer::ensure_fluidsynth() {
    if (!fluidsynth) {
        synth_settings = new_fluid_settings();
        fluidsynth = new_fluid_synth(synth_settings);
        int sfid = fluid_synth_sfload(fluidsynth, sf2_path.c_str(), 1);
        std::cout << "NoteNagaMixer: SoundFont loaded, sfid=" << sfid << " from " << sf2_path << std::endl;
        audio_driver = new_fluid_audio_driver(synth_settings, fluidsynth);
        if (!audio_driver) { std::cerr << "NoteNagaMixer: FluidSynth audio driver could not be started!" << std::endl; }
    }
}

RtMidiOut *NoteNagaMixer::ensure_midi_output(const std::string &output) {
    auto it = midi_outputs.find(output);
    if (it != midi_outputs.end()) return it->second;
    try {
        RtMidiOut *out = new RtMidiOut();
        unsigned int nPorts = out->getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i) {
            if (out->getPortName(i) == output) {
                out->openPort(i);
                midi_outputs[output] = out;
                return out;
            }
        }
        delete out;
    } catch (...) {}
    midi_outputs[output] = nullptr;
    return nullptr;
}

void NoteNagaMixer::close() {
    std::cout << "NoteNagaMixer: Closing and cleaning up resources..." << std::endl;
    if (audio_driver) {
        delete_fluid_audio_driver(audio_driver);
        audio_driver = nullptr;
    }
    if (fluidsynth) {
        delete_fluid_synth(fluidsynth);
        fluidsynth = nullptr;
    }
    if (synth_settings) {
        delete_fluid_settings(synth_settings);
        synth_settings = nullptr;
    }
    for (auto &pair : midi_outputs) {
        if (pair.second) delete pair.second;
    }
    midi_outputs.clear();
    playing_notes.clear();
    channel_states.clear();
    std::cout << "NoteNagaMixer: All resources cleaned up." << std::endl;
}