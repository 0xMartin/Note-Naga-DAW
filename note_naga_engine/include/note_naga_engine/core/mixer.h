#pragma once

#include <note_naga_engine/core/project_data.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_api.h>

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

#include <RtMidi.h>
#include <fluidsynth.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define TRACK_ROUTING_ENTRY_ANY_DEVICE "any"

/*******************************************************************************************************/
// Note Naga Routing Entry
/*******************************************************************************************************/

/**
 * @brief Represents a routing entry for a track, specifying its output, channel, and
 * other parameters.
 */
struct NOTE_NAGA_ENGINE_API NoteNagaRoutingEntry {
    NoteNagaTrack *track; ///< Pointer to the routed track
    std::string output;   ///< Output device name
    int channel;          ///< MIDI channel
    float volume;         ///< Output volume multiplier
    int note_offset;      ///< Note number offset
    float pan;            ///< Stereo pan (-1.0 left, 1.0 right)

    /**
     * @brief Constructor for a routing entry.
     * @param track Pointer to the track.
     * @param device Output device name.
     * @param channel MIDI channel.
     * @param volume Output volume multiplier (default 1.0f).
     * @param note_offset Note number offset (default 0).
     * @param pan Stereo pan position (default 0.0f).
     */
    NoteNagaRoutingEntry(NoteNagaTrack *track, const std::string &device, int channel,
                         float volume = 1.0f, int note_offset = 0, float pan = 0.0f)
        : track(track), output(device), channel(channel), volume(volume),
          note_offset(note_offset), pan(pan) {}
};

/*******************************************************************************************************/
// Note Naga Mixer
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED
/**
 * @brief Mixer class responsible for managing MIDI routing, output devices, and playback
 * parameters.
 */
class NOTE_NAGA_ENGINE_API NoteNagaMixer : public QObject {
    Q_OBJECT
#else
/**
 * @brief Mixer class responsible for managing MIDI routing, output devices, and playback
 * parameters.
 */
class NOTE_NAGA_ENGINE_API NoteNagaMixer {
#endif

  public:
    /**
     * @brief Constructs the mixer and initializes the synthesizer and outputs.
     * @param project Pointer to the NoteNagaProject instance.
     * @param sf2_path Path to the SoundFont file (default: FluidR3_GM.sf2).
     */
    explicit NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path);

    /**
     * @brief Destroys the mixer, releasing all resources.
     */
    virtual ~NoteNagaMixer();

    /**
     * @brief Detects available MIDI output devices.
     * @return Vector of output device names.
     */
    std::vector<std::string> detectOutputs();

    /**
     * @brief Closes all audio and MIDI outputs and releases resources.
     */
    void close();

    /**
     * @brief Creates a default routing configuration for tracks.
     */
    void createDefaultRouting();

    /**
     * @brief Sets the routing entries for the mixer.
     * @param entries List of routing entries.
     */
    void setRouting(const std::vector<NoteNagaRoutingEntry> &entries);

    /**
     * @brief Gets all current routing entries.
     * @return Reference to the vector of routing entries.
     */
    std::vector<NoteNagaRoutingEntry> &getRoutingEntries() { return routing_entries; }

    /**
     * @brief Adds a routing entry to the routing table.
     * @param entry Optional routing entry to add.
     * @return True on success, false otherwise.
     */
    bool addRoutingEntry(const std::optional<NoteNagaRoutingEntry> &entry = std::nullopt);

    /**
     * @brief Removes a routing entry from the routing table by index.
     * @param index Index of the routing entry to remove.
     * @return True on success, false otherwise.
     */
    bool removeRoutingEntry(int index);

    /**
     * @brief Clears the routing table.
     */
    void clearRoutingTable();

    /**
     * @brief Gets the list of available output devices.
     * @return Vector of available output device names.
     */
    std::vector<std::string> getAvailableOutputs() { return available_outputs; }

    /**
     * @brief Gets the default output device name.
     * @return Default output device name.
     */
    std::string getDefaultOutput() { return default_output; }

    /**
     * @brief Starts playback of a MIDI note on the routed output.
     * @param midi_note MIDI note to play.
     */
    void playNote(const NoteNagaNote &midi_note);

    /**
     * @brief Stops playback of a MIDI note on the routed output.
     * @param midi_note MIDI note to stop.
     */
    void stopNote(const NoteNagaNote &midi_note);

    /**
     * @brief Stops all notes for the specified sequence and/or track.
     * @param seq Optional pointer to the MIDI sequence. If is nullptr, stops all notes in
     * all sequences.
     * @param track Optional pointer to the track. If is nullptr, stops all notes in the
     * sequence.
     */
    void stopAllNotes(NoteNagaMidiSeq *seq = nullptr, NoteNagaTrack *track = nullptr);

    /**
     * @brief Mutes or unmutes the specified track.
     * @param track Pointer to the track.
     * @param mute True to mute, false to unmute.
     */
    void muteTrack(NoteNagaTrack *track, bool mute);

    /**
     * @brief Solos or unsolos the specified track.
     * @param track Pointer to the track.
     * @param solo True to solo, false to unsolo.
     */
    void soloTrack(NoteNagaTrack *track, bool solo);

    float master_volume;    ///< Master volume multiplier
    int master_min_note;    ///< Master minimum note value
    int master_max_note;    ///< Master maximum note value
    int master_note_offset; ///< Master note number offset
    float master_pan;       ///< Master stereo pan position

#ifndef QT_DEACTIVATED
  Q_SIGNALS:
    /**
     * @brief Signal emitted when the routing entry stack changes.
     */
    void routingEntryStackChanged();

    /**
     * @brief Signal emitted when a MIDI note is played (input).
     * @param note The note played.
     */
    void noteInSignal(const NoteNagaNote &note);

    /**
     * @brief Signal emitted when a MIDI note is sent to an output device.
     * @param note The note played.
     * @param device_name Name of the output device.
     * @param channel MIDI channel.
     */
    void noteOutSignal(const NoteNagaNote &note, const std::string &device_name,
                       int channel);
#endif

  private:
    /********************************************************************************************************/
    // Custom private types
    /********************************************************************************************************/

    /**
     * @brief Represents a currently played note, including its device and channel.
     */
    struct PlayedNote {
        int note_num;          ///< MIDI note number
        unsigned long note_id; ///< Unique note identifier
        std::string device;    ///< Output device name
        int channel;           ///< MIDI channel
    };

    // Map from track pointer to a list of played notes
    using TrackNotesMap = std::unordered_map<NoteNagaTrack *, std::vector<PlayedNote>>;

    // Map from sequence pointer to map of track pointer to list of played notes
    using SequenceNotesMap = std::unordered_map<NoteNagaMidiSeq *, TrackNotesMap>;

    // Typedef for program/pan state (pair of ints)
    using ProgramPanState = std::pair<int, int>;

    // Map from channel (int) to (program, pan) state
    using ChannelStateMap = std::unordered_map<int, ProgramPanState>;

    // Map from device name to channel state map
    using DeviceChannelStateMap = std::unordered_map<std::string, ChannelStateMap>;

    // Map from device name to RtMidiOut pointer
    using MidiOutputsMap = std::unordered_map<std::string, RtMidiOut *>;

    /********************************************************************************************************/
    // Private data members
    /********************************************************************************************************/

    NoteNagaProject *project; ///< Pointer to the associated project
    std::string sf2_path;     ///< Path to the SoundFont file

    std::recursive_mutex mutex; ///< Mutex for thread safety

    std::vector<std::string>
        available_outputs;      ///< Names of available MIDI output devices
    std::string default_output; ///< Default output device name
    std::vector<NoteNagaRoutingEntry> routing_entries; ///< Current routing entries

    MidiOutputsMap midi_outputs; ///< Map of output device names to RtMidiOut pointers
    fluid_synth_t *fluidsynth;   ///< FluidSynth synthesizer instance
    fluid_audio_driver_t *audio_driver; ///< FluidSynth audio driver
    fluid_settings_t *synth_settings;   ///< FluidSynth settings

    SequenceNotesMap playing_notes;       ///< Map of currently playing notes
    DeviceChannelStateMap channel_states; ///< Map of device/channel state (program, pan)

    /********************************************************************************************************/
    // Private methods
    /********************************************************************************************************/

    /**
     * @brief Helper to play a note on a specific output and channel.
     * @param output Output device name.
     * @param ch MIDI channel.
     * @param note_num MIDI note number.
     * @param velocity Note velocity.
     * @param prog Program (instrument).
     * @param pan_cc Pan control value.
     * @param midi_note MIDI note data.
     * @param seq Pointer to the MIDI sequence.
     * @param track Pointer to the track.
     */
    void playNoteOnOutputDevice(const std::string &output, int ch, int note_num,
                                int velocity, int prog, int pan_cc,
                                const NoteNagaNote &midi_note, NoteNagaMidiSeq *seq,
                                NoteNagaTrack *track);

    /**
     * @brief Ensures that FluidSynth is initialized.
     */
    void ensureFluidsynth();

    /**
     * @brief Ensures that a MIDI output device is available and returns its pointer.
     * @param device Output device name.
     * @return Pointer to RtMidiOut.
     */
    RtMidiOut *ensureMidiOutput(const std::string &device);
};