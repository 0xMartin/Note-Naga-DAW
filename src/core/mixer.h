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

// macros for send to anx devices
#define TRACK_ROUTING_ENTRY_ANY_DEVICE "any"

// TrackOutputEntry: routing config for one track
struct TrackRountingEntry {
    int track_id;
    QString output;
    int channel;
    float volume;
    int note_offset;
    float pan;
    TrackRountingEntry(int track_id, const QString& device, int channel, float volume = 1.0f, int note_offset = 0, float pan = 0.0f)
        : track_id(track_id), output(device), channel(channel), volume(volume), note_offset(note_offset), pan(pan) {}
};

class Mixer : public QObject {
    Q_OBJECT
public:
    explicit Mixer(AppContext* ctx, const QString& sf2_path = "./FluidR3_GM.sf2");
    ~Mixer();

    QVector<QString> detect_outputs();
    void close();

    void create_default_routing();
    void set_routing(const QVector<TrackRountingEntry>& entries);
    void add_routing_entry(const TrackRountingEntry& entry = TrackRountingEntry(-1, "", 0));
    void remove_routing_entry(int index);
    void clear_routing_table();

    void note_play(const MidiNote& midi_note, int track_id);
    void note_stop(const MidiNote& midi_note);
    void stop_all_notes(std::optional<int> track_id = std::nullopt);

    void mute_track(int track_id, bool mute);
    void solo_track(int track_id, bool solo);

    QVector<TrackRountingEntry>& get_routing_entries();
    QVector<QString> get_available_outputs();
    QString get_default_output();

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

    // --- Output devices and routing ---
    QVector<QString> available_outputs;
    QString default_output;
    QVector<TrackRountingEntry> routing_entries;

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

    // --- PRIVATE METHODS ---
    void play_note_on_output(
        const QString &output, 
        int ch, 
        int note_num, 
        int velocity, 
        int prog, 
        int pan_cc, 
        const MidiNote &midi_note);
    void ensure_fluidsynth();
    RtMidiOut* ensure_midi_output(const QString& device);
};