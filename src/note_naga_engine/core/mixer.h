#pragma once

#include <fluidsynth.h>
#include <RtMidi.h>
#include <QObject>
#include <QVector>
#include <QString>
#include <QMap>
#include <memory>

#include "project_data.h"
#include "types.h"

// macros for send to anx devices
#define TRACK_ROUTING_ENTRY_ANY_DEVICE "any"

// TrackOutputEntry: routing config for one track
struct NoteNagaRountingEntry {
    int track_id;
    QString output;
    int channel;
    float volume;
    int note_offset;
    float pan;
    
    NoteNagaRountingEntry(
        int track_id, 
        const QString& device, 
        int channel, 
        float volume = 1.0f, 
        int note_offset = 0, 
        float pan = 0.0f);
};

class NoteNagaMixer : public QObject {
    Q_OBJECT
public:
    explicit NoteNagaMixer(std::shared_ptr<NoteNagaProject> projectData, const QString& sf2_path = "./FluidR3_GM.sf2");
    ~NoteNagaMixer();

    QVector<QString> detect_outputs();
    void close();

    void create_default_routing();
    void set_routing(const QVector<NoteNagaRountingEntry>& entries);
    void add_routing_entry(const NoteNagaRountingEntry& entry = NoteNagaRountingEntry(-1, "", 0));
    void remove_routing_entry(int index);
    void clear_routing_table();

    void note_play(const NoteNagaNote& midi_note, int track_id);
    void note_stop(const NoteNagaNote& midi_note);
    void stop_all_notes(std::optional<int> track_id = std::nullopt);

    void mute_track(int track_id, bool mute);
    void solo_track(int track_id, bool solo);

    QVector<NoteNagaRountingEntry>& get_routing_entries();
    QVector<QString> get_available_outputs();
    QString get_default_output();

    // Master parameters
    float master_volume;
    int master_min_note;
    int master_max_note;
    int master_note_offset;
    float master_pan;

Q_SIGNALS:
    void routing_entry_stack_changed_signal();
    void note_in_signal(const NoteNagaNote& note, const NoteNagaMIDISeq *sequence, const NoteNagaTrack *track);
    void note_out_signal(const NoteNagaNote& note, const QString& device_name, int channel);

private:
    std::shared_ptr<NoteNagaProject> projectData;
    QString sf2_path;

    // --- Output devices and routing ---
    QVector<QString> available_outputs;
    QString default_output;
    QVector<NoteNagaRountingEntry> routing_entries;

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
        const NoteNagaNote &midi_note);
    void ensure_fluidsynth();
    RtMidiOut* ensure_midi_output(const QString& device);
};