#include "mixer.h"

#include <QDebug>
#include <QVector>
#include <QString>
#include <algorithm>

NoteNagaMixer::NoteNagaMixer(std::shared_ptr<NoteNagaProjectData> projectData, const QString &sf2_path)
    : QObject(nullptr),
      projectData(projectData),
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
    connect(projectData.get(), &NoteNagaProjectData::project_file_loaded_signal, this, &NoteNagaMixer::create_default_routing);
    ensure_fluidsynth();
    available_outputs = detect_outputs();
    default_output = available_outputs.contains("fluidsynth")
                         ? "fluidsynth"
                         : (available_outputs.isEmpty() ? QString() : available_outputs.first());
}

NoteNagaMixer::~NoteNagaMixer()
{
    close();
}

QVector<QString> NoteNagaMixer::detect_outputs()
{
    QVector<QString> outputs;

    // Add FluidSynth if successfully initialized
    if (fluidsynth && audio_driver)
    {
        outputs.append("fluidsynth");
    }

    // Detect RtMidi outputs
    try
    {
        RtMidiOut midi;
        unsigned int nPorts = midi.getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i)
            outputs.append(QString::fromStdString(midi.getPortName(i)));
    }
    catch (...)
    {
    }

    return outputs;
}

void NoteNagaMixer::create_default_routing()
{
    routing_entries.clear();

    // get active sequence from project data
    std::shared_ptr<NoteNagaMIDISequence> active_sequence = this->projectData->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot create default routing.";
        return;
    }

    // Mark already assigned channels (if any)
    std::vector<bool> used_channels(16, false); // 16 MIDI channels
    for (const auto &track_ptr : active_sequence->get_tracks()){
        if (track_ptr->channel.has_value()) {
            int ch = track_ptr->channel.value();
            if (ch >= 0 && ch < 16)
                used_channels[ch] = true;
        }
    }

    for (const auto &track_ptr : active_sequence->get_tracks())
    {
        int channel;
        if (track_ptr->channel.has_value()) {
            channel = track_ptr->channel.value();
        } else {
            // Find first free channel
            auto it = std::find(used_channels.begin(), used_channels.end(), false);
            if (it != used_channels.end()) {
                channel = std::distance(used_channels.begin(), it);
                used_channels[channel] = true;
            } else {
                channel = 15; // fallback to max channel if all are used
            }
        }
        routing_entries.append(TrackRountingEntry(track_ptr->track_id, default_output, channel));
    }
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::set_routing(const QVector<TrackRountingEntry> &entries)
{
    routing_entries = entries;
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::add_routing_entry(const TrackRountingEntry &entry)
{
    // get active sequence from project data
    std::shared_ptr<NoteNagaMIDISequence> active_sequence = this->projectData->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot add routing entry.";
        return;
    }

    TrackRountingEntry new_entry = entry;
    if (new_entry.track_id < 0 && active_sequence->get_tracks().size() > 0)
    {
        new_entry.track_id = active_sequence->get_tracks().at(0)->track_id;
    }
    if (new_entry.output.isEmpty())
    {
        new_entry.output = this->default_output;
    }
    routing_entries.append(new_entry);
    qDebug() << "Added routing entry:" << new_entry.track_id << "->" << new_entry.output << "(ch" << new_entry.channel << ")";
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::remove_routing_entry(int index)
{
    if (index >= 0 && index < routing_entries.size())
    {
        const auto entry = routing_entries.takeAt(index);
        qDebug() << "Removed routing entry:" << entry.track_id << "->" << entry.output << "(ch" << entry.channel << ")";
        NN_QT_EMIT(routing_entry_stack_changed_signal());
    }
    else
    {
        qDebug() << "Invalid routing entry index:" << index;
    }
}

void NoteNagaMixer::clear_routing_table()
{
    routing_entries.clear();
    NN_QT_EMIT(routing_entry_stack_changed_signal());
}

void NoteNagaMixer::note_play(const MidiNote &midi_note, int track_id)
{
    // emit signal (note playing on track. note go to NoteNagaMixer)
    NN_QT_EMIT(note_in_signal(midi_note, track_id));

    // get active sequence from project data
    std::shared_ptr<NoteNagaMIDISequence> active_sequence = this->projectData->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot play note.";
        return;
    }
    
    // get track of note and retrieve its program (instrument)
    int prog = 0;
    std::shared_ptr<Track> track = active_sequence->get_active_track();
    if (track)
        prog = track->instrument.value_or(0);

    // process each routing entry for the track 
    for (const TrackRountingEntry &entry : this->routing_entries)
    {
        // skip if entry does not match the track of the note
        if (entry.track_id != track_id)
            continue;

        // calculate final note: input note + track offset + master note offset
        int note_num = midi_note.note + entry.note_offset + master_note_offset;
        if (note_num < 0 || note_num > 127)
            continue;
        // master note range cutoff
        if (note_num < master_min_note || note_num > master_max_note)
            continue;

        // calculate velocity and pan
        int velocity = int(std::min(127.0f, std::max(0.0f, float(midi_note.velocity.value_or(100)) * entry.volume * master_volume)));
        if (velocity <= 0)
            continue;
        float pan = std::max(-1.0f, std::min(1.0f, entry.pan + master_pan));
        int pan_cc = int((pan + 1.0f) * 63.5f);

        // get output channel from entry
        int ch = entry.channel;

        // play on all outputs if entry is set to "any device"
        if (entry.output == TRACK_ROUTING_ENTRY_ANY_DEVICE)
        {
            for (const QString &out_dev : available_outputs)
            {
                // get output device name
                QString output = out_dev;

                // deleguj přehrání na pomocnou metodu
                play_note_on_output(output, ch, note_num, velocity, prog, pan_cc, midi_note);
            }
            continue; // už jsme provedli pro všechny outputs, přeskoč zbytek pro tento entry
        }

        // --- původní logika pro konkrétní device, nyní delegováno ---
        QString output = entry.output;
        play_note_on_output(output, ch, note_num, velocity, prog, pan_cc, midi_note);
    }
}

void NoteNagaMixer::note_stop(const MidiNote &midi_note)
{
    int note_id = midi_note.note_id;
    for (auto dev_it = playing_notes.begin(); dev_it != playing_notes.end(); ++dev_it)
    {
        const QString &output = dev_it.key();
        auto &channel_map = dev_it.value();
        for (auto ch_it = channel_map.begin(); ch_it != channel_map.end(); ++ch_it)
        {
            int ch = ch_it.key();
            auto &notes_map = ch_it.value();
            for (auto note_it = notes_map.begin(); note_it != notes_map.end();)
            {
                if (note_it.value() == note_id)
                {
                    // Send note_off to FluidSynth or MIDI Out
                    if (output == "fluidsynth" && fluidsynth)
                    {
                        fluid_synth_noteoff(fluidsynth, ch, note_it.key());
                    }
                    else
                    {
                        RtMidiOut *out = midi_outputs.value(output, nullptr);
                        if (out)
                        {
                            std::vector<unsigned char> msg = {
                                static_cast<unsigned char>(0x80 | (ch & 0x0F)),
                                static_cast<unsigned char>(note_it.key()),
                                static_cast<unsigned char>(0)};
                            out->sendMessage(&msg);
                        }
                    }
                    note_it = notes_map.erase(note_it);
                }
                else
                {
                    ++note_it;
                }
            }
        }
    }
}

void NoteNagaMixer::stop_all_notes(std::optional<int> track_id)
{
    for (auto dev_it = playing_notes.begin(); dev_it != playing_notes.end(); ++dev_it)
    {
        const QString &output = dev_it.key();
        auto &channel_map = dev_it.value();
        for (auto ch_it = channel_map.begin(); ch_it != channel_map.end(); ++ch_it)
        {
            int ch = ch_it.key();
            auto &notes_map = ch_it.value();
            for (auto note_it = notes_map.begin(); note_it != notes_map.end();)
            {
                bool should_stop = true;
                if (track_id.has_value())
                {
                    should_stop = false;
                    for (const auto &entry : routing_entries)
                    {
                        if (entry.output == output && entry.channel == ch && entry.track_id == track_id.value())
                        {
                            should_stop = true;
                            break;
                        }
                    }
                }
                if (should_stop)
                {
                    if (output == "fluidsynth" && fluidsynth)
                    {
                        fluid_synth_noteoff(fluidsynth, ch, note_it.key());
                    }
                    else
                    {
                        RtMidiOut *out = midi_outputs.value(output, nullptr);
                        if (out)
                        {
                            std::vector<unsigned char> msg = {
                                static_cast<unsigned char>(0x80 | (ch & 0x0F)),
                                static_cast<unsigned char>(note_it.key()),
                                static_cast<unsigned char>(0)};
                            out->sendMessage(&msg);
                        }
                    }
                    note_it = notes_map.erase(note_it);
                }
                else
                {
                    ++note_it;
                }
            }
        }
    }
}

void NoteNagaMixer::mute_track(int track_id, bool mute)
{
    // get active sequence from project data
    std::shared_ptr<NoteNagaMIDISequence> active_sequence = this->projectData->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot mute track.";
        return;
    }

    if (track_id < 0 || track_id >= active_sequence->get_tracks().size())
    {
        qWarning() << "Invalid track ID for mute operation:" << track_id;
        return;
    }
    active_sequence->get_tracks()[track_id]->muted = mute;
    NN_QT_EMIT(active_sequence->track_meta_changed_signal(track_id));
    this->stop_all_notes(track_id);
}

void NoteNagaMixer::solo_track(int track_id, bool solo)
{
    // get active sequence from project data
    std::shared_ptr<NoteNagaMIDISequence> active_sequence = this->projectData->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot solo track.";
        return;
    }

    if (track_id < 0 || track_id >= active_sequence->get_tracks().size())
    {
        qWarning() << "Invalid track ID for solo operation:" << track_id;
        return;
    }

    active_sequence->get_tracks()[track_id]->solo = solo;
    NN_QT_EMIT(active_sequence->track_meta_changed_signal(track_id));

    if (solo)
    {
        active_sequence->set_solo_track_id(track_id);
        for (const auto& track : active_sequence->get_tracks()) {
            if (track->track_id != track_id) {
                track->solo = false; 
                this->stop_all_notes(track->track_id);
                NN_QT_EMIT(active_sequence->track_meta_changed_signal(track->track_id));
            }
        }
    }
    else
    {
        active_sequence->set_solo_track_id(std::nullopt);
    }
}

void NoteNagaMixer::play_note_on_output(const QString &output, int ch, int note_num, int velocity, int prog, int pan_cc, const MidiNote &midi_note)
{
    // skip if note is already playing on this output device
    if (playing_notes.value(output).value(ch).contains(note_num))
        return;

    // get per-channel state (program / pan)
    auto &state = channel_states[output][ch];
    bool needs_program = (state.first != prog);
    bool needs_pan = (state.second != pan_cc);

    // play note on output device
    if (output == "fluidsynth" && fluidsynth)
    {
        // play on FluidSynth if available
        if (needs_program)
        {
            fluid_synth_program_change(fluidsynth, ch, prog);
            state.first = prog;
        }
        if (needs_pan)
        {
            fluid_synth_cc(fluidsynth, ch, 10, pan_cc);
            state.second = pan_cc;
        }
        fluid_synth_noteon(fluidsynth, ch, note_num, velocity);
    }
    else
    {
        // play on RtMidiOut if available
        RtMidiOut *out = ensure_midi_output(output);
        if (out)
        {
            if (needs_pan)
            {
                std::vector<unsigned char> msg = {
                    static_cast<unsigned char>(0xB0 | (ch & 0x0F)),
                    static_cast<unsigned char>(10),
                    static_cast<unsigned char>(pan_cc)};
                out->sendMessage(&msg);
                state.second = pan_cc;
            }
            if (needs_program)
            {
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

    // register played note
    int note_id = midi_note.note_id;
    playing_notes[output][ch][note_num] = note_id;

    // emit signal (NoteNagaMixer playing note)
    MidiNote note_clone = midi_note;
    note_clone.velocity = velocity;
    NN_QT_EMIT(note_out_signal(note_clone, output, ch));
}

void NoteNagaMixer::ensure_fluidsynth()
{
    if (!fluidsynth)
    {
        synth_settings = new_fluid_settings();
        fluidsynth = new_fluid_synth(synth_settings);
        int sfid = fluid_synth_sfload(fluidsynth, sf2_path.toStdString().c_str(), 1);
        qDebug() << "NoteNagaMixer: SoundFont loaded, sfid=" << sfid << "from" << sf2_path;

        this->audio_driver = new_fluid_audio_driver(synth_settings, fluidsynth);
        if (!audio_driver)
        {
            qWarning() << "NoteNagaMixer: FluidSynth audio driver could not be started!";
        }
    }
}

RtMidiOut *NoteNagaMixer::ensure_midi_output(const QString &output)
{
    if (midi_outputs.contains(output))
        return midi_outputs.value(output);
    try
    {
        RtMidiOut *out = new RtMidiOut();
        unsigned int nPorts = out->getPortCount();
        for (unsigned int i = 0; i < nPorts; ++i)
        {
            if (QString::fromStdString(out->getPortName(i)) == output)
            {
                out->openPort(i);
                midi_outputs[output] = out;
                return out;
            }
        }
        delete out;
    }
    catch (...)
    {
    }
    midi_outputs[output] = nullptr;
    return nullptr;
}

void NoteNagaMixer::close()
{
    qDebug() << "NoteNagaMixer: Closing and cleaning up resources...";

    if (audio_driver)
    {
        delete_fluid_audio_driver(audio_driver);
        audio_driver = nullptr;
        qDebug() << "NoteNagaMixer: FluidSynth audio driver deleted.";
    }
    if (fluidsynth)
    {
        delete_fluid_synth(fluidsynth);
        fluidsynth = nullptr;
        qDebug() << "NoteNagaMixer: FluidSynth instance closed.";
    }
    if (synth_settings)
    {
        delete_fluid_settings(synth_settings);
        synth_settings = nullptr;
        qDebug() << "NoteNagaMixer: FluidSynth settings deleted.";
    }

    for (auto it = midi_outputs.begin(); it != midi_outputs.end(); ++it)
    {
        if (it.value())
        {
            delete it.value();
        }
        else
        {
            qDebug() << "NoteNagaMixer: RtMidiOut for" << it.key() << "was null.";
        }
    }
    midi_outputs.clear();
    playing_notes.clear();
    channel_states.clear();
    qDebug() << "NoteNagaMixer: All resources cleaned up.";
}

QVector<TrackRountingEntry> &NoteNagaMixer::get_routing_entries()
{
    return routing_entries;
}
QVector<QString> NoteNagaMixer::get_available_outputs()
{
    return available_outputs;
}
QString NoteNagaMixer::get_default_output()
{
    return default_output;
}