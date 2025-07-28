#pragma once

#include <note_naga_engine/core/note_naga_component.h>
#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/project_data.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_api.h>

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

#include <atomic>
#include <memory>
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

    NoteNagaRoutingEntry(NoteNagaTrack *track, const std::string &device, int channel,
                         float volume = 1.0f, int note_offset = 0, float pan = 0.0f)
        : track(track), output(device), channel(channel), volume(volume), note_offset(note_offset),
          pan(pan) {}
};

/*******************************************************************************************************/
// Note Naga Mixer
/*******************************************************************************************************/

/**
 * @brief Mixer class responsible for managing MIDI routing, output devices, and playback
 * parameters. This component is NOT THREAD SAFE and should be used in only one thread.
 * If you want to use it in multiple threads, you queue in Note Naga engine playback
 * worker.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaMixer : public QObject,
                                           public NoteNagaEngineComponent<NN_SynthMessage_t, 1024> {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaMixer : public NoteNagaEngineComponent<NN_SynthMessage_t, 1024> {
#endif

public:
    /**
     * @brief Constructs the mixer and initializes the synthesizer and outputs.
     * @param project Pointer to the NoteNagaProject instance.
     * @param synthesizers Optional pointer to a vector of synthesizers to use.
     */
    explicit NoteNagaMixer(NoteNagaProject *project,
                           std::vector<NoteNagaSynthesizer *> *synthesizers);

    /**
     * @brief Destroys the mixer, releasing all resources.
     */
    virtual ~NoteNagaMixer();

    /**
     * @brief Initializes the mixer and synthesizers.
     */
    void setSynthVectorRef(std::vector<NoteNagaSynthesizer *> *synthesizers);

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
    void playNote(const NN_Note_t &midi_note);

    /**
     * @brief Stops playback of a MIDI note on the routed output.
     * @param midi_note MIDI note to stop.
     */
    void stopNote(const NN_Note_t &midi_note);

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

    /**
     * @brief Checks if the track is a percussion track.
     * @param track Pointer to the track.
     * @return True if this is a percussion track.
     */
    bool isPercussion(NoteNagaTrack *track) const;

    /**
     * @brief Buffer incoming play/stop notes and send them all at once to synthesizers.
     */
    void flushNotes();

    // GETTERS AND SETTERS for master controls

    float getMasterVolume() const { return master_volume.load(); }
    int getMasterMinNote() const { return master_min_note.load(); }
    int getMasterMaxNote() const { return master_max_note.load(); }
    int getMasterNoteOffset() const { return master_note_offset.load(); }
    float getMasterPan() const { return master_pan.load(); }

    void setMasterVolume(float volume) { master_volume.store(volume); }
    void setMasterMinNote(int min_note) { master_min_note.store(min_note); }
    void setMasterMaxNote(int max_note) { master_max_note.store(max_note); }
    void setMasterNoteOffset(int note_offset) { master_note_offset.store(note_offset); }
    void setMasterPan(float pan) { master_pan.store(pan); }

protected:
    /**
     * @brief Thread-safe method to handle a dequeued item.
     * @param value The item to process.
     */
    void onItem(const NN_SynthMessage_t &value) override;

private:
    ///< Pointer to the associated project
    NoteNagaProject *project;
    ///< List of synthesizers used by the mixer
    std::vector<NoteNagaSynthesizer *> *synthesizers;

    std::vector<std::string> available_outputs;        ///< Names of available MIDI output devices
    std::string default_output;                        ///< Default output device name
    std::vector<NoteNagaRoutingEntry> routing_entries; ///< Current routing entries

    std::atomic<float> master_volume;    ///< Master volume multiplier
    std::atomic<int> master_min_note;    ///< Master minimum note value
    std::atomic<int> master_max_note;    ///< Master maximum note value
    std::atomic<int> master_note_offset; ///< Master note number offset
    std::atomic<float> master_pan;       ///< Master stereo pan position

    // Buffer for "flushNotes"
    std::unordered_map<std::string, std::vector<NN_SynthMessage_t>> note_buffer_;

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
    void noteInSignal(const NN_Note_t &note);

    /**
     * @brief Signal emitted when a MIDI note is sent to an output device.
     * @param note The note played.
     * @param device_name Name of the output device.
     * @param channel MIDI channel.
     */
    void noteOutSignal(const NN_Note_t &note, const std::string &device_name, int channel);
#endif
};