#pragma once

#include <fluidsynth.h>
#include <RtMidi.h>
#include <QObject>
#include <QVector>
#include <QString>
#include <QMap>
#include <memory>
#include "app_context.h"
#include "shared.h"

// TrackOutputEntry: routing config for one track
struct TrackOutputEntry {
    int track_id;
    QString device;
    int channel;
    float volume;
    int note_offset;
    float pan;
    TrackOutputEntry(int track_id, const QString& device, int channel, float volume = 1.0f, int note_offset = 0, float pan = 0.0f)
        : track_id(track_id), device(device), channel(channel), volume(volume), note_offset(note_offset), pan(pan) {}
};

class Mixer : public QObject {
    Q_OBJECT
public:
    explicit Mixer(AppContext* ctx, const QString& sf2_path = "./FluidR3_GM.sf2");
    ~Mixer();

    void create_default_routing();
    void set_routing(const QVector<TrackOutputEntry>& entries);
    void add_routing_entry(const TrackOutputEntry& entry = TrackOutputEntry(-1, "", 0));
    void remove_routing_entry(int index);
    void clear_routing_table();

    void note_play(const MidiNote& midi_note);
    void note_stop(const MidiNote& midi_note);
    void stop_all_notes(std::optional<int> track_id = std::nullopt);

    void close();

    QVector<QString> detect_outputs();
    QVector<QString> available_outputs;
    QString default_output;
    QVector<TrackOutputEntry> routing_entries;

    // Master parameters
    float master_volume;
    int master_min_note;
    int master_max_note;
    int master_note_offset;
    float master_pan;

signals:
    void routing_entry_stack_changed_signal();

private:
    AppContext* ctx;
    QString sf2_path;

    // --- MIDI OUT ---
    QMap<QString, RtMidiOut*> midi_outputs;

    // --- FluidSynth ---
    fluid_synth_t* fluidsynth;
    fluid_audio_driver_t* audio_driver;
    fluid_settings_t* synth_settings;

    // --- NOTE PLAYBACK TRACKING ---
    // (device, channel, note_num) -> note_id
    QMap<QString, QMap<int, QMap<int, int>>> playing_notes;

    // --- PER CHANNEL STATE TRACKING ---
    // (device, channel) -> program/pan
    QMap<QString, QMap<int, QPair<int, int>>> channel_states;

    void ensure_fluidsynth();
    RtMidiOut* ensure_midi_output(const QString& device);
};