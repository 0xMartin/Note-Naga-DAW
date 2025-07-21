#pragma once

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

#include <RtMidi.h>
#include <fluidsynth.h>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <string>
#include <utility>
#include <optional>

#include "../note_naga_api.h"
#include "project_data.h"
#include "types.h"

#define TRACK_ROUTING_ENTRY_ANY_DEVICE "any"

/*******************************************************************************************************/
// Note Naga Routing Entry
/*******************************************************************************************************/

struct NOTE_NAGA_ENGINE_API NoteNagaRoutingEntry {
    NoteNagaTrack *track;
    std::string output;
    int channel;
    float volume;
    int note_offset;
    float pan;

    NoteNagaRoutingEntry(NoteNagaTrack *track, const std::string &device, int channel, float volume = 1.0f,
                         int note_offset = 0, float pan = 0.0f)
        : track(track), output(device), channel(channel), volume(volume), note_offset(note_offset), pan(pan) {}
};

/*******************************************************************************************************/
// Typedefs & Structures for Clarity
/*******************************************************************************************************/

struct PlayedNote {
    int note_num;
    unsigned long note_id;
    std::string device;
    int channel;
};

// Map track pointer -> list of played notes
using TrackNotesMap = std::unordered_map<NoteNagaTrack *, std::vector<PlayedNote>>;

// Map sequence pointer -> map of track pointer -> list of played notes
using SequenceNotesMap = std::unordered_map<NoteNagaMIDISeq *, TrackNotesMap>;

// Typedef for program/pan state (pair of ints)
using ProgramPanState = std::pair<int, int>;

// Map channel (int) -> (program, pan)
using ChannelStateMap = std::unordered_map<int, ProgramPanState>;

// Map device name -> channel state map
using DeviceChannelStateMap = std::unordered_map<std::string, ChannelStateMap>;

// Map device name -> RtMidiOut pointer
using MidiOutputsMap = std::unordered_map<std::string, RtMidiOut *>;

/*******************************************************************************************************/
// Note Naga Mixer
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaMixer : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaMixer {
#endif

  public:
    explicit NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path = "./FluidR3_GM.sf2");
    virtual ~NoteNagaMixer();

    std::vector<std::string> detect_outputs();
    void close();

    void create_default_routing();

    void set_routing(const std::vector<NoteNagaRoutingEntry> &entries);
    std::vector<NoteNagaRoutingEntry> &get_routing_entries() { return routing_entries; }
    bool add_routing_entry(const std::optional<NoteNagaRoutingEntry> &entry = std::nullopt);
    bool remove_routing_entry(int index);
    void clear_routing_table();

    std::vector<std::string> get_available_outputs() { return available_outputs; }
    std::string get_default_output() { return default_output; }

    void note_play(const NoteNagaNote &midi_note);
    void note_stop(const NoteNagaNote &midi_note);

    void stop_all_notes(NoteNagaMIDISeq *seq = nullptr, NoteNagaTrack *track = nullptr);

    void mute_track(NoteNagaTrack *track, bool mute);
    void solo_track(NoteNagaTrack *track, bool solo);

    float master_volume;
    int master_min_note;
    int master_max_note;
    int master_note_offset;
    float master_pan;

#ifndef QT_DEACTIVATED
  Q_SIGNALS:
    void routing_entry_stack_changed_signal();
    void note_in_signal(const NoteNagaNote &note);
    void note_out_signal(const NoteNagaNote &note, const std::string &device_name, int channel);
#endif

  private:
    NoteNagaProject *project;
    std::string sf2_path;

    std::recursive_mutex mutex;

    std::vector<std::string> available_outputs;
    std::string default_output;
    std::vector<NoteNagaRoutingEntry> routing_entries;

    MidiOutputsMap midi_outputs;
    fluid_synth_t *fluidsynth;
    fluid_audio_driver_t *audio_driver;
    fluid_settings_t *synth_settings;

    SequenceNotesMap playing_notes;           
    DeviceChannelStateMap channel_states;  

    void play_note_on_output(const std::string &output, int ch, int note_num, int velocity, int prog, int pan_cc,
                             const NoteNagaNote &midi_note, NoteNagaMIDISeq *seq, NoteNagaTrack *track);

    void ensure_fluidsynth();
    RtMidiOut *ensure_midi_output(const std::string &device);
};