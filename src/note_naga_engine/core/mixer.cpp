#include "mixer.h"
#include <QDebug>
#include <algorithm>

NoteNagaMixer::NoteNagaMixer(NoteNagaProject* project, const QString& sf2_path)
    : QObject(nullptr),
      project(project),
      sf2_path(sf2_path),
      fluidsynth(nullptr),
      audio_driver(nullptr),
      synth_settings(nullptr),
      master_volume(1.0f),
      master_min_note(0),
      master_max_note(127),
      master_note_offset(0),
      master_pan(0.0f)
{
    connect(project, &NoteNagaProject::project_file_loaded_signal, this, &NoteNagaMixer::create_default_routing);
    ensure_fluidsynth();
    available_outputs = detect_outputs();
    auto it = std::find(available_outputs.begin(), available_outputs.end(), "fluidsynth");
    if (it != available_outputs.end()) {
        default_output = "fluidsynth";
    } else if (available_outputs.empty()) {
        default_output = QString();
    } else {
        default_output = available_outputs.front();
    }
}

NoteNagaMixer::~NoteNagaMixer()
{
    close();
}

std::vector<QString> NoteNagaMixer::detect_outputs()
{
    std::vector<QString> outputs;
    if (fluidsynth && audio_driver)
        outputs.push_back("fluidsynth");
    try {
        RtMidiOut midi;
        unsigned int nPorts = midi.getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i)
            outputs.push_back(QString::fromStdString(midi.getPortName(i)));
    } catch (...) {}
    return outputs;
}

void NoteNagaMixer::create_default_routing()
{
    routing_entries.clear();
    if (!project) return;
    for (NoteNagaMIDISeq* seq : project->get_sequences()) {
        if (!seq) continue;
        std::vector<bool> used_channels(16, false);
        for (NoteNagaTrack* track : seq->get_tracks()) {
            if (!track) continue;
            if (auto ch = track->get_channel(); ch.has_value()) {
                int channel = ch.value();
                if (channel >= 0 && channel < 16) used_channels[channel] = true;
            }
        }
        for (NoteNagaTrack* track : seq->get_tracks()) {
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

void NoteNagaMixer::set_routing(const std::vector<NoteNagaRoutingEntry>& entries)
{
    routing_entries = entries;
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::add_routing_entry(const NoteNagaRoutingEntry& entry)
{
    if (!entry.track) return;
    routing_entries.push_back(entry);
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::remove_routing_entry(int index)
{
    if (index >= 0 && index < routing_entries.size()) {
        routing_entries.erase(routing_entries.begin() + index);
        NN_QT_EMIT(routing_entry_stack_changed_signal());
    }
}

void NoteNagaMixer::clear_routing_table()
{
    routing_entries.clear();
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::note_play(const NoteNagaNote& midi_note)
{
    NoteNagaTrack* track = midi_note.parent;
    if (!track) {
        qWarning() << "NoteNagaMixer: Cannot play note, missing parent track";
        return;
    }
    NoteNagaMIDISeq* seq = track->get_parent();
    if (!seq) {
        qWarning() << "NoteNagaMixer: Cannot play note, missing parent sequence";
        return;
    }

    int prog = track->get_instrument().value_or(0);
    NN_QT_EMIT(note_in_signal(midi_note, seq, track));

    for (const NoteNagaRoutingEntry& entry : routing_entries) {
        if (entry.track != track) continue;

        int note_num = midi_note.note + entry.note_offset + master_note_offset;
        if (note_num < 0 || note_num > 127) continue;
        if (note_num < master_min_note || note_num > master_max_note) continue;
        int velocity = int(std::min(127.0f, std::max(0.0f, float(midi_note.velocity.value_or(100)) * entry.volume * master_volume)));
        if (velocity <= 0) continue;
        float pan = std::max(-1.0f, std::min(1.0f, entry.pan + master_pan));
        int pan_cc = int((pan + 1.0f) * 63.5f);
        int ch = entry.channel;

        if (entry.output == TRACK_ROUTING_ENTRY_ANY_DEVICE) {
            for (const QString& out_dev : available_outputs) {
                play_note_on_output(out_dev, ch, note_num, velocity, prog, pan_cc, midi_note, seq, track);
            }
        } else {
            play_note_on_output(entry.output, ch, note_num, velocity, prog, pan_cc, midi_note, seq, track);
        }
    }
}

void NoteNagaMixer::note_stop(const NoteNagaNote& midi_note)
{
    NoteNagaTrack* track = midi_note.parent;
    if (!track) return;
    NoteNagaMIDISeq* seq = track->get_parent();
    if (!seq) return;

    auto& notes = playing_notes[seq][track];
    for (auto it = notes.begin(); it != notes.end(); ) {
        if (it->note_id == midi_note.note_id) {
            if (it->device == "fluidsynth" && fluidsynth) {
                fluid_synth_noteoff(fluidsynth, it->channel, it->note_num);
            } else {
                RtMidiOut* out = midi_outputs.value(it->device, nullptr);
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

void NoteNagaMixer::stop_all_notes(NoteNagaMIDISeq* seq, NoteNagaTrack* track)
{
    if (!seq && !track) {
        // Stop all
        for (auto seq_it = playing_notes.begin(); seq_it != playing_notes.end(); ++seq_it) {
            for (auto trk_it = seq_it.value().begin(); trk_it != seq_it.value().end(); ++trk_it) {
                for (const PlayedNote& pn : trk_it.value()) {
                    if (pn.device == "fluidsynth" && fluidsynth) {
                        fluid_synth_noteoff(fluidsynth, pn.channel, pn.note_num);
                    } else {
                        RtMidiOut* out = midi_outputs.value(pn.device, nullptr);
                        if (out) {
                            std::vector<unsigned char> msg = {
                                static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
                                static_cast<unsigned char>(pn.note_num),
                                static_cast<unsigned char>(0)};
                            out->sendMessage(&msg);
                        }
                    }
                }
                trk_it.value().clear();
            }
        }
        playing_notes.clear();
    } else if (seq && !track) {
        for (auto trk_it = playing_notes[seq].begin(); trk_it != playing_notes[seq].end(); ++trk_it) {
            for (const PlayedNote& pn : trk_it.value()) {
                if (pn.device == "fluidsynth" && fluidsynth) {
                    fluid_synth_noteoff(fluidsynth, pn.channel, pn.note_num);
                } else {
                    RtMidiOut* out = midi_outputs.value(pn.device, nullptr);
                    if (out) {
                        std::vector<unsigned char> msg = {
                            static_cast<unsigned char>(0x80 | (pn.channel & 0x0F)),
                            static_cast<unsigned char>(pn.note_num),
                            static_cast<unsigned char>(0)};
                        out->sendMessage(&msg);
                    }
                }
            }
            trk_it.value().clear();
        }
        playing_notes[seq].clear();
    } else if (seq && track) {
        auto& notes = playing_notes[seq][track];
        for (const PlayedNote& pn : notes) {
            if (pn.device == "fluidsynth" && fluidsynth) {
                fluid_synth_noteoff(fluidsynth, pn.channel, pn.note_num);
            } else {
                RtMidiOut* out = midi_outputs.value(pn.device, nullptr);
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

void NoteNagaMixer::mute_track(NoteNagaTrack* track, bool mute)
{
    if (!track) return;
    track->set_muted(mute);
    stop_all_notes(track->get_parent(), track);
}

void NoteNagaMixer::solo_track(NoteNagaTrack* track, bool solo)
{
    if (!track) return;
    NoteNagaMIDISeq* seq = track->get_parent();
    if (!seq) return;
    track->set_solo(solo);

    if (solo) {
        seq->set_solo_track_id(track->get_id());
        for (NoteNagaTrack* t : seq->get_tracks()) {
            if (t && t != track) {
                t->set_solo(false);
                stop_all_notes(seq, t);
            }
        }
    } else {
        seq->set_solo_track_id(std::nullopt);
    }
}

void NoteNagaMixer::play_note_on_output(
    const QString& output, int ch, int note_num, int velocity, int prog, int pan_cc,
    const NoteNagaNote& midi_note, NoteNagaMIDISeq* seq, NoteNagaTrack* track)
{
    if (!seq || !track) return;
    auto& notes = playing_notes[seq][track];
    for (const PlayedNote& pn : notes) {
        if (pn.device == output && pn.channel == ch && pn.note_num == note_num)
            return;
    }

    auto& state = channel_states[output][ch];
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
        RtMidiOut* out = ensure_midi_output(output);
        if (out) {
            if (needs_pan) {
                std::vector<unsigned char> msg = {
                    static_cast<unsigned char>(0xB0 | (ch & 0x0F)),
                    static_cast<unsigned char>(10),
                    static_cast<unsigned char>(pan_cc)};
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
    notes.append(PlayedNote{note_num, midi_note.note_id, output, ch});
    NoteNagaNote note_clone = midi_note;
    note_clone.velocity = velocity;
    NN_QT_EMIT(note_out_signal(note_clone, output, ch, seq, track));
}

void NoteNagaMixer::ensure_fluidsynth()
{
    if (!fluidsynth) {
        synth_settings = new_fluid_settings();
        fluidsynth = new_fluid_synth(synth_settings);
        int sfid = fluid_synth_sfload(fluidsynth, sf2_path.toStdString().c_str(), 1);
        qDebug() << "NoteNagaMixer: SoundFont loaded, sfid=" << sfid << "from" << sf2_path;
        audio_driver = new_fluid_audio_driver(synth_settings, fluidsynth);
        if (!audio_driver) {
            qWarning() << "NoteNagaMixer: FluidSynth audio driver could not be started!";
        }
    }
}

RtMidiOut* NoteNagaMixer::ensure_midi_output(const QString& output)
{
    if (midi_outputs.contains(output))
        return midi_outputs.value(output);
    try {
        RtMidiOut* out = new RtMidiOut();
        unsigned int nPorts = out->getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i) {
            if (QString::fromStdString(out->getPortName(i)) == output) {
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

void NoteNagaMixer::close()
{
    qDebug() << "NoteNagaMixer: Closing and cleaning up resources...";
    if (audio_driver) { delete_fluid_audio_driver(audio_driver); audio_driver = nullptr; }
    if (fluidsynth) { delete_fluid_synth(fluidsynth); fluidsynth = nullptr; }
    if (synth_settings) { delete_fluid_settings(synth_settings); synth_settings = nullptr; }
    for (auto it = midi_outputs.begin(); it != midi_outputs.end(); ++it) {
        if (it.value()) delete it.value();
    }
    midi_outputs.clear();
    playing_notes.clear();
    channel_states.clear();
    qDebug() << "NoteNagaMixer: All resources cleaned up.";
}