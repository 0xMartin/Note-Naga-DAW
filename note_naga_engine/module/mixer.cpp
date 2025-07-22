#include <note_naga_engine/module/mixer.h>

#include <algorithm>

#include <note_naga_engine/logger.h>

#ifndef QT_DEACTIVATED
NoteNagaMixer::NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path)
    : NoteNagaEngineComponent(), QObject(nullptr)
#else
NoteNagaMixer::NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path)
    : NoteNagaEngineComponent()
#endif
{
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
    connect(project, &NoteNagaProject::projectFileLoaded, this,
            &NoteNagaMixer::createDefaultRouting);
#endif

    ensureFluidsynth();
    available_outputs = detectOutputs();
    for (const std::string &output : available_outputs) {
        NOTE_NAGA_LOG_INFO("Detected output device: " + output);
    }

    auto it = std::find(available_outputs.begin(), available_outputs.end(), "fluidsynth");
    if (it != available_outputs.end()) {
        default_output = "fluidsynth";
    } else if (available_outputs.empty()) {
        default_output = std::string();
    } else {
        default_output = available_outputs.front();
    }
    NOTE_NAGA_LOG_INFO("Default output device set on: " + default_output);

    NOTE_NAGA_LOG_INFO("Initialized successfully");
}

NoteNagaMixer::~NoteNagaMixer() { close(); }

std::vector<std::string> NoteNagaMixer::detectOutputs() {
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

void NoteNagaMixer::close() {
    NOTE_NAGA_LOG_INFO("Closing and cleaning up resources...");
    if (audio_driver) {
        delete_fluid_audio_driver(audio_driver);
        audio_driver = nullptr;
        NOTE_NAGA_LOG_INFO("Audio driver closed");
    }
    if (fluidsynth) {
        delete_fluid_synth(fluidsynth);
        fluidsynth = nullptr;
        NOTE_NAGA_LOG_INFO("FluidSynth closed");
    }
    if (synth_settings) {
        delete_fluid_settings(synth_settings);
        synth_settings = nullptr;
        NOTE_NAGA_LOG_INFO("FluidSynth settings deleted");
    }
    for (auto &pair : midi_outputs) {
        if (pair.second) {
            delete pair.second;
            NOTE_NAGA_LOG_INFO("MIDI output closed: " + pair.first);
        }
    }
    midi_outputs.clear();
    playing_notes.clear();
    channel_states.clear();
    NOTE_NAGA_LOG_INFO("Closed and cleaned up resources successfully");
}

void NoteNagaMixer::createDefaultRouting() {
    routing_entries.clear();
    if (!project) return;
    for (NoteNagaMidiSeq *seq : project->getSequences()) {
        if (!seq) continue;
        std::vector<bool> used_channels(16, false);
        for (NoteNagaTrack *track : seq->getTracks()) {
            if (!track) continue;
            if (auto ch = track->getChannel(); ch.has_value()) {
                int channel = ch.value();
                if (channel >= 0 && channel < 16) used_channels[channel] = true;
            }
        }
        for (NoteNagaTrack *track : seq->getTracks()) {
            if (!track) continue;
            int channel;
            if (auto ch = track->getChannel(); ch.has_value()) {
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
            routing_entries.push_back(
                NoteNagaRoutingEntry(track, default_output, channel));
        }
    }
    NOTE_NAGA_LOG_INFO("Default routing created with " +
                       std::to_string(routing_entries.size()) + " entries");
    NN_QT_EMIT(routingEntryStackChanged());
}

void NoteNagaMixer::setRouting(const std::vector<NoteNagaRoutingEntry> &entries) {
    routing_entries = entries;
    NOTE_NAGA_LOG_INFO("Rounting stack changed, now has " +
                       std::to_string(routing_entries.size()) + " entries");
    NN_QT_EMIT(routingEntryStackChanged());
}

bool NoteNagaMixer::addRoutingEntry(const std::optional<NoteNagaRoutingEntry> &entry) {
    if (entry.has_value()) {
        if (!entry.value().track) return false;
        routing_entries.push_back(entry.value());
        NOTE_NAGA_LOG_INFO("Added routing entry for track if Id: " +
                           std::to_string(entry.value().track->getId()) +
                           " on device: " + entry.value().output);
        NN_QT_EMIT(routingEntryStackChanged());
    } else {
        NoteNagaMidiSeq *seq = project->getActiveSequence();
        if (!seq) return false;
        NoteNagaTrack *track = seq->getActiveTrack();
        if (!track) track = seq->getTracks().empty() ? nullptr : seq->getTracks().front();
        if (!track) return false;
        routing_entries.push_back(NoteNagaRoutingEntry(track, default_output, 0));
        NOTE_NAGA_LOG_INFO("Added default routing entry for track Id: " +
                           std::to_string(track->getId()) +
                           " on device: " + default_output);
        NN_QT_EMIT(routingEntryStackChanged());
    }

    return true;
}

bool NoteNagaMixer::removeRoutingEntry(int index) {
    if (index >= 0 && index < int(routing_entries.size())) {
        routing_entries.erase(routing_entries.begin() + index);
        NOTE_NAGA_LOG_INFO("Removed routing entry at index: " + std::to_string(index));
        NN_QT_EMIT(routingEntryStackChanged());
        return true;
    }

    NOTE_NAGA_LOG_WARNING("Failed to remove routing entry at index: " +
                          std::to_string(index));
    return false;
}

void NoteNagaMixer::clearRoutingTable() {
    routing_entries.clear();
    NOTE_NAGA_LOG_INFO("Routing table cleared");
    NN_QT_EMIT(routingEntryStackChanged());
}

void NoteNagaMixer::playNote(const NN_Note_t &midi_note) {
    NoteNagaTrack *track = midi_note.parent;
    if (!track) {
        NOTE_NAGA_LOG_WARNING("Cannot play note, missing parent track");
        return;
    }
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq) {
        NOTE_NAGA_LOG_WARNING("Cannot play note, missing parent sequence");
        return;
    }

    int prog = track->getInstrument().value_or(0);
    NN_QT_EMIT(noteInSignal(midi_note));

    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track != track) continue;

        int note_num = midi_note.note + entry.note_offset + master_note_offset;
        if (note_num < 0 || note_num > 127) continue;
        if (note_num < master_min_note || note_num > master_max_note) continue;
        int velocity =
            int(std::min(127.0f, std::max(0.0f, float(midi_note.velocity.value_or(100)) *
                                                    entry.volume * master_volume)));
        if (velocity <= 0) continue;
        float pan = std::max(-1.0f, std::min(1.0f, entry.pan + master_pan));
        int pan_cc = int((pan + 1.0f) * 63.5f);
        int ch = entry.channel;

        if (entry.output == TRACK_ROUTING_ENTRY_ANY_DEVICE) {
            for (const std::string &out_dev : available_outputs) {
                playNoteOnOutputDevice(out_dev, ch, note_num, velocity, prog, pan_cc,
                                       midi_note, seq, track);
            }
        } else {
            playNoteOnOutputDevice(entry.output, ch, note_num, velocity, prog, pan_cc,
                                   midi_note, seq, track);
        }
    }
}

void NoteNagaMixer::stopNote(const NN_Note_t &midi_note) {
    NoteNagaTrack *track = midi_note.parent;
    if (!track) {
        NOTE_NAGA_LOG_WARNING("Cannot stop note, missing parent track");
        return;
    }
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq) {
        NOTE_NAGA_LOG_WARNING("Cannot stop note, missing parent sequence");
        return;
    }

    auto &notes = playing_notes[seq][track];
    for (auto it = notes.begin(); it != notes.end();) {
        if (it->note_id == midi_note.id) {
            if (it->device == "fluidsynth" && fluidsynth) {
                fluid_synth_noteoff(fluidsynth, it->channel, it->note_num);
            } else {
                auto midi_out_it = midi_outputs.find(it->device);
                RtMidiOut *out =
                    (midi_out_it != midi_outputs.end()) ? midi_out_it->second : nullptr;
                if (out) {
                    std::vector<unsigned char> msg = {
                        static_cast<unsigned char>(0x80 | (it->channel & 0x0F)),
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

void NoteNagaMixer::stopAllNotes(NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
    if (!seq && !track) {
        // Stop all
        for (auto &seq_pair : playing_notes) {
            for (auto &trk_pair : seq_pair.second) {
                for (const PlayedNote &pn : trk_pair.second) {
                    if (pn.device == "fluidsynth" && fluidsynth) {
                        fluid_synth_noteoff(fluidsynth, pn.channel, pn.note_num);
                    } else {
                        auto midi_out_it = midi_outputs.find(pn.device);
                        RtMidiOut *out = (midi_out_it != midi_outputs.end())
                                             ? midi_out_it->second
                                             : nullptr;
                        if (out) {
                            std::vector<unsigned char> msg = {
                                static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
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
                    RtMidiOut *out = (midi_out_it != midi_outputs.end())
                                         ? midi_out_it->second
                                         : nullptr;
                    if (out) {
                        std::vector<unsigned char> msg = {
                            static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
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
                RtMidiOut *out =
                    (midi_out_it != midi_outputs.end()) ? midi_out_it->second : nullptr;
                if (out) {
                    std::vector<unsigned char> msg = {
                        static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
                        static_cast<unsigned char>(pn.note_num),
                        static_cast<unsigned char>(0)};
                    out->sendMessage(&msg);
                }
            }
        }
        notes.clear();
    }
}

void NoteNagaMixer::muteTrack(NoteNagaTrack *track, bool mute) {
    if (!track) return;
    track->setMuted(mute);
    stopAllNotes(track->getParent(), track);
}

void NoteNagaMixer::soloTrack(NoteNagaTrack *track, bool solo) {
    if (!track) return;
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq) return;
    track->setSolo(solo);

    if (solo) {
        seq->setSoloTrack(track);
        for (NoteNagaTrack *t : seq->getTracks()) {
            if (t && t != track) {
                t->setSolo(false);
                stopAllNotes(seq, t);
            }
        }
    } else {
        seq->setSoloTrack(nullptr);
    }
}

/********************************************************************************************************/
// Private methods
/********************************************************************************************************/

void NoteNagaMixer::onItem(const NN_MixerMessage_t &value) {
    // This method is called when a new item is dequeued from the component's queue.
    // It processes the MIDI note play/stop messages.
    if (value.play) {
        // Handle note play
        playNote(value.note);
    } else {
        // Handle note stop
        stopNote(value.note);
    }
}

void NoteNagaMixer::playNoteOnOutputDevice(const std::string &output, int ch,
                                           int note_num, int velocity, int prog,
                                           int pan_cc, const NN_Note_t &midi_note,
                                           NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
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
        RtMidiOut *out = ensureMidiOutput(output);
        if (out) {
            if (needs_pan) {
                std::vector<unsigned char> msg = {
                    static_cast<unsigned char>(0xB0 | (ch & 0x0F)),
                    static_cast<unsigned char>(10), static_cast<unsigned char>(pan_cc)};
                out->sendMessage(&msg);
                state.second = pan_cc;
            }
            if (needs_program) {
                std::vector<unsigned char> msg = {
                    static_cast<unsigned char>(0xC0 | (ch & 0x0F)),
                    static_cast<unsigned char>(prog)};
                out->sendMessage(&msg);
                state.first = prog;
            }
            std::vector<unsigned char> msg = {
                static_cast<unsigned char>(0x90 | (ch & 0x0F)),
                static_cast<unsigned char>(note_num),
                static_cast<unsigned char>(velocity)};
            out->sendMessage(&msg);
        }
    }
    notes.push_back(PlayedNote{note_num, midi_note.id, output, ch});
    NN_Note_t note_clone = midi_note;
    note_clone.velocity = velocity;
    NN_QT_EMIT(noteOutSignal(note_clone, output, ch));
}

void NoteNagaMixer::ensureFluidsynth() {
    if (!fluidsynth) {
        synth_settings = new_fluid_settings();
        fluidsynth = new_fluid_synth(synth_settings);
        int sfid = fluid_synth_sfload(fluidsynth, sf2_path.c_str(), 1);
        NOTE_NAGA_LOG_INFO("SoundFont loaded, sfid=" + std::to_string(sfid) + " from " +
                           sf2_path);

        audio_driver = new_fluid_audio_driver(synth_settings, fluidsynth);
        if (!audio_driver) {
            NOTE_NAGA_LOG_WARNING("FluidSynth audio driver could not be started!");
        }
    }
}

RtMidiOut *NoteNagaMixer::ensureMidiOutput(const std::string &output) {
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