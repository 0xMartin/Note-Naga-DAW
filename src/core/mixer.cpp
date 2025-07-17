#include "mixer.h"

#include <QDebug>
#include <QVector>
#include <QString>
#include <algorithm>

Mixer::Mixer(AppContext* ctx, const QString& sf2_path)
    : QObject(nullptr),
      ctx(ctx),
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
    connect(ctx, &AppContext::midi_file_loaded_signal, this, &Mixer::create_default_routing);
    available_outputs = detect_outputs();
    default_output = available_outputs.contains("fluidsynth")
        ? "fluidsynth"
        : (available_outputs.isEmpty() ? QString() : available_outputs.first());
    ensure_fluidsynth();
}

Mixer::~Mixer() {
    close();
}

QVector<QString> Mixer::detect_outputs() {
    QVector<QString> outputs;
    // Detect RtMidi outputs
    try {
        RtMidiOut midi;
        unsigned int nPorts = midi.getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i)
            outputs.append(QString::fromStdString(midi.getPortName(i)));
    } catch (...) {}
    // Add FluidSynth if available
    outputs.append("fluidsynth");
    return outputs;
}

void Mixer::create_default_routing() {
    routing_entries.clear();
    int channel = 0;
    for (const auto& track_ptr : ctx->tracks) {
        routing_entries.append(TrackOutputEntry(track_ptr->track_id, default_output, channel));
        channel = std::min(channel + 1, 15); // Limit to 16 channels
    }
    emit routing_entry_stack_changed_signal();
}

void Mixer::set_routing(const QVector<TrackOutputEntry>& entries) {
    routing_entries = entries;
    emit routing_entry_stack_changed_signal();
}

void Mixer::add_routing_entry(const TrackOutputEntry& entry) {
    TrackOutputEntry new_entry = entry;
    if (new_entry.track_id < 0 && ctx->tracks.size() > 0) {
        new_entry.track_id = ctx->tracks.at(0)->track_id;
    }
    if (new_entry.device.isEmpty()) {
        new_entry.device = this->default_output;
    }
    routing_entries.append(new_entry);
    qDebug() << "Added routing entry:" << new_entry.track_id << "->" << new_entry.device << "(ch" << new_entry.channel << ")";
    emit routing_entry_stack_changed_signal();
}

void Mixer::remove_routing_entry(int index) {
    if (index >= 0 && index < routing_entries.size()) {
        const auto entry = routing_entries.takeAt(index);
        qDebug() << "Removed routing entry:" << entry.track_id << "->" << entry.device << "(ch" << entry.channel << ")";
        emit routing_entry_stack_changed_signal();
    } else {
        qDebug() << "Invalid routing entry index:" << index;
    }
}

void Mixer::clear_routing_table() {
    routing_entries.clear();
    emit routing_entry_stack_changed_signal();
}

// --- NOTE PLAY ---
void Mixer::note_play(const MidiNote& midi_note) {
    if (!midi_note.track.has_value())
        return;

    emit ctx->playing_note_signal(midi_note);

    int prog = 0;
    auto tr_cfg = ctx->get_track_by_id(midi_note.track.value());
    if (tr_cfg)
        prog = tr_cfg->instrument;

    for (const TrackOutputEntry& entry : this->routing_entries) {
        if (entry.track_id != midi_note.track.value()) continue;

        int note_num = midi_note.note + entry.note_offset + master_note_offset;
        if (note_num < 0 || note_num > 127) continue;
        if (note_num < master_min_note || note_num > master_max_note) continue;

        int velocity = int(std::min(127.0f, std::max(0.0f, float(midi_note.velocity.value_or(100)) * entry.volume * master_volume)));
        if (velocity <= 0) continue;

        float pan = std::max(-1.0f, std::min(1.0f, entry.pan + master_pan));
        int pan_cc = int((pan + 1.0f) * 63.5f);

        QString device = entry.device;
        int ch = entry.channel;

        // If this note is already playing, ignore
        if (playing_notes.value(device).value(ch).contains(note_num))
            continue;

        // --- TRACK & UPDATE PROGRAM AND PAN PER CHANNEL ONLY IF CHANGED ---
        auto& state = channel_states[device][ch];
        bool needs_program = (state.first != prog);
        bool needs_pan = (state.second != pan_cc);

        if (device == "fluidsynth" && fluidsynth) {
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
            // RtMidi output
            RtMidiOut* out = ensure_midi_output(device);
            if (out) {
                if (needs_pan) {
                    std::vector<unsigned char> msg = {
                        static_cast<unsigned char>(0xB0 | (ch & 0x0F)),
                        static_cast<unsigned char>(10),
                        static_cast<unsigned char>(pan_cc)
                    };
                    out->sendMessage(&msg);
                    state.second = pan_cc;
                }
                if (needs_program) {
                    std::vector<unsigned char> msg = {
                        static_cast<unsigned char>(0xC0 | (ch & 0x0F)),
                        static_cast<unsigned char>(prog)
                    };
                    out->sendMessage(&msg);
                    state.first = prog;
                }
                std::vector<unsigned char> msg = {
                    static_cast<unsigned char>(0x90 | (ch & 0x0F)),
                    static_cast<unsigned char>(note_num),
                    static_cast<unsigned char>(velocity)
                };
                out->sendMessage(&msg);
            }
        }

        // Register played note
        int note_id = midi_note.note_id;
        playing_notes[device][ch][note_num] = note_id;

        MidiNote note_clone = midi_note;
        note_clone.velocity = velocity;
        emit ctx->mixer_playing_note_signal(note_clone);
    }
}

void Mixer::note_stop(const MidiNote& midi_note) {
    int note_id = midi_note.note_id;
    for (auto dev_it = playing_notes.begin(); dev_it != playing_notes.end(); ++dev_it) {
        const QString& device = dev_it.key();
        auto& channel_map = dev_it.value();
        for (auto ch_it = channel_map.begin(); ch_it != channel_map.end(); ++ch_it) {
            int ch = ch_it.key();
            auto& notes_map = ch_it.value();
            for (auto note_it = notes_map.begin(); note_it != notes_map.end();) {
                if (note_it.value() == note_id) {
                    // Send note_off to FluidSynth or MIDI Out
                    if (device == "fluidsynth" && fluidsynth) {
                        fluid_synth_noteoff(fluidsynth, ch, note_it.key());
                    } else {
                        RtMidiOut* out = midi_outputs.value(device, nullptr);
                        if (out) {
                            std::vector<unsigned char> msg = {
                                static_cast<unsigned char>(0x80 | (ch & 0x0F)),
                                static_cast<unsigned char>(note_it.key()),
                                static_cast<unsigned char>(0)
                            };
                            out->sendMessage(&msg);
                        }
                    }
                    note_it = notes_map.erase(note_it);
                } else {
                    ++note_it;
                }
            }
        }
    }
}

void Mixer::stop_all_notes(std::optional<int> track_id) {
    for (auto dev_it = playing_notes.begin(); dev_it != playing_notes.end(); ++dev_it) {
        const QString& device = dev_it.key();
        auto& channel_map = dev_it.value();
        for (auto ch_it = channel_map.begin(); ch_it != channel_map.end(); ++ch_it) {
            int ch = ch_it.key();
            auto& notes_map = ch_it.value();
            for (auto note_it = notes_map.begin(); note_it != notes_map.end();) {
                bool should_stop = true;
                if (track_id.has_value()) {
                    should_stop = false;
                    for (const auto& entry : routing_entries) {
                        if (entry.device == device && entry.channel == ch && entry.track_id == track_id.value()) {
                            should_stop = true;
                            break;
                        }
                    }
                }
                if (should_stop) {
                    if (device == "fluidsynth" && fluidsynth) {
                        fluid_synth_noteoff(fluidsynth, ch, note_it.key());
                    } else {
                        RtMidiOut* out = midi_outputs.value(device, nullptr);
                        if (out) {
                            std::vector<unsigned char> msg = {
                                static_cast<unsigned char>(0x80 | (ch & 0x0F)),
                                static_cast<unsigned char>(note_it.key()),
                                static_cast<unsigned char>(0)
                            };
                            out->sendMessage(&msg);
                        }
                    }
                    note_it = notes_map.erase(note_it);
                } else {
                    ++note_it;
                }
            }
        }
    }
}

void Mixer::ensure_fluidsynth() {
    if (!fluidsynth) {
        synth_settings = new_fluid_settings();
        fluidsynth = new_fluid_synth(synth_settings);
        int sfid = fluid_synth_sfload(fluidsynth, sf2_path.toStdString().c_str(), 1);
        qDebug() << "Mixer: SoundFont loaded, sfid=" << sfid << "from" << sf2_path;

        this->audio_driver = new_fluid_audio_driver(synth_settings, fluidsynth);
        if (!audio_driver) {
            qWarning() << "Mixer: FluidSynth audio driver could not be started!";
        }
    }
}

RtMidiOut* Mixer::ensure_midi_output(const QString& device) {
    if (midi_outputs.contains(device))
        return midi_outputs.value(device);
    try {
        RtMidiOut* out = new RtMidiOut();
        unsigned int nPorts = out->getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i) {
            if (QString::fromStdString(out->getPortName(i)) == device) {
                out->openPort(i);
                midi_outputs[device] = out;
                return out;
            }
        }
        delete out;
    } catch (...) {}
    midi_outputs[device] = nullptr;
    return nullptr;
}

void Mixer::close() {
    qDebug() << "Mixer: Closing and cleaning up resources...";

    if (audio_driver) {
        delete_fluid_audio_driver(audio_driver);
        audio_driver = nullptr;
        qDebug() << "Mixer: FluidSynth audio driver deleted.";
    }
    if (fluidsynth) {
        delete_fluid_synth(fluidsynth);
        fluidsynth = nullptr;
        qDebug() << "Mixer: FluidSynth instance closed.";
    }
    if (synth_settings) {
        delete_fluid_settings(synth_settings);
        synth_settings = nullptr;
        qDebug() << "Mixer: FluidSynth settings deleted.";
    }

    for (auto it = midi_outputs.begin(); it != midi_outputs.end(); ++it) {
        if (it.value()) {
            delete it.value();
        } else {
            qDebug() << "Mixer: RtMidiOut for" << it.key() << "was null.";
        }
    }
    midi_outputs.clear();
    playing_notes.clear();
    channel_states.clear();
    qDebug() << "Mixer: All resources cleaned up.";
}