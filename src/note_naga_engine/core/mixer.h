#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <RtMidi.h>
#include <fluidsynth.h>
#include <memory>
#include <vector>

#include "../note_naga_api.h"
#include "project_data.h"
#include "types.h"

#define TRACK_ROUTING_ENTRY_ANY_DEVICE "any"

struct NOTE_NAGA_ENGINE_API NoteNagaRoutingEntry;
struct NOTE_NAGA_ENGINE_API PlayedNote;

class NOTE_NAGA_ENGINE_API NoteNagaMixer : public QObject {
    Q_OBJECT
  public:
    explicit NoteNagaMixer(NoteNagaProject *project, const QString &sf2_path = "./FluidR3_GM.sf2");
    ~NoteNagaMixer();

    std::vector<QString> detect_outputs();
    void close();

    void create_default_routing();

    void set_routing(const std::vector<NoteNagaRoutingEntry> &entries);
    std::vector<NoteNagaRoutingEntry> &get_routing_entries() { return routing_entries; }
    bool add_routing_entry(const std::optional<NoteNagaRoutingEntry> &entry = std::nullopt);
    bool remove_routing_entry(int index);
    void clear_routing_table();

    std::vector<QString> get_available_outputs() { return available_outputs; }
    QString get_default_output() { return default_output; }

    void note_play(const NoteNagaNote &midi_note);
    void note_stop(const NoteNagaNote &midi_note);

    // Stop všechny noty buď na tracku, nebo v celé sekvenci, nebo vše
    void stop_all_notes(NoteNagaMIDISeq *seq = nullptr, NoteNagaTrack *track = nullptr);

    void mute_track(NoteNagaTrack *track, bool mute);
    void solo_track(NoteNagaTrack *track, bool solo);

    float master_volume;
    int master_min_note;
    int master_max_note;
    int master_note_offset;
    float master_pan;

  Q_SIGNALS:
    void routing_entry_stack_changed_signal();
    void note_in_signal(const NoteNagaNote &note);
    void note_out_signal(const NoteNagaNote &note, const QString &device_name, int channel);

  private:
    NoteNagaProject *project;
    QString sf2_path;

    std::vector<QString> available_outputs;
    QString default_output;
    std::vector<NoteNagaRoutingEntry> routing_entries;

    QMap<QString, RtMidiOut *> midi_outputs;
    fluid_synth_t *fluidsynth;
    fluid_audio_driver_t *audio_driver;
    fluid_settings_t *synth_settings;

    struct PlayedNote {
        int note_num;
        unsigned long note_id;
        QString device;
        int channel;
    };

    QMap<NoteNagaMIDISeq *, QMap<NoteNagaTrack *, QList<PlayedNote>>> playing_notes;

    QMap<QString, QMap<int, QPair<int, int>>> channel_states;

    void play_note_on_output(const QString &output, int ch, int note_num, int velocity, int prog, int pan_cc,
                             const NoteNagaNote &midi_note, NoteNagaMIDISeq *seq, NoteNagaTrack *track);

    void ensure_fluidsynth();
    RtMidiOut *ensure_midi_output(const QString &device);
};

struct NOTE_NAGA_ENGINE_API NoteNagaRoutingEntry {
    NoteNagaTrack *track;
    QString output;
    int channel;
    float volume;
    int note_offset;
    float pan;

    NoteNagaRoutingEntry(NoteNagaTrack *track, const QString &device, int channel, float volume = 1.0f,
                         int note_offset = 0, float pan = 0.0f)
        : track(track), output(device), channel(channel), volume(volume), note_offset(note_offset), pan(pan) {}
};