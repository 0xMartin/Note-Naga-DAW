#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// MIDI Meta Event Types
#define MIDI_META_SEQUENCE_NUMBER 0x00
#define MIDI_META_TEXT_EVENT 0x01
#define MIDI_META_COPYRIGHT 0x02
#define MIDI_META_TRACK_NAME 0x03
#define MIDI_META_INSTRUMENT_NAME 0x04
#define MIDI_META_LYRIC 0x05
#define MIDI_META_MARKER 0x06
#define MIDI_META_CUE_POINT 0x07
#define MIDI_META_CHANNEL_PREFIX 0x20
#define MIDI_META_END_OF_TRACK 0x2F
#define MIDI_META_SET_TEMPO 0x51
#define MIDI_META_SMPTE_OFFSET 0x54
#define MIDI_META_TIME_SIGNATURE 0x58
#define MIDI_META_KEY_SIGNATURE 0x59
#define MIDI_META_SEQ_SPECIFIC 0x7F

// --- MIDI Event Types ---
/**
 * @brief Enum class representing all supported MIDI event types.
 */
enum class NOTE_NAGA_ENGINE_API MidiEventType {
    NoteOn,            ///< Note On event
    NoteOff,           ///< Note Off event
    PolyAftertouch,    ///< Polyphonic Aftertouch event
    ControlChange,     ///< Control Change event
    ProgramChange,     ///< Program Change event
    ChannelAftertouch, ///< Channel Aftertouch event
    PitchBend,         ///< Pitch Bend event
    Meta,              ///< Meta event
    SysEx,             ///< System Exclusive event
    Unknown            ///< Unknown event type
};

/**
 * @brief Structure representing a single MIDI event.
 */
struct NOTE_NAGA_ENGINE_API MidiEvent {
    uint32_t delta_time = 0; ///< Delta time in ticks since previous event
    MidiEventType type = MidiEventType::Unknown; ///< Type of MIDI event
    uint8_t channel = 0;       ///< Channel number (0-15 for channel events)
    std::vector<uint8_t> data; ///< Event-specific data

    // Meta-specific
    uint8_t meta_type = 0;          ///< Meta event type (for meta events only)
    std::vector<uint8_t> meta_data; ///< Data bytes for meta events

    // For SysEx
    std::vector<uint8_t> sysex_data; ///< Sysex event data
};

/**
 * @brief Structure representing a MIDI track (sequence of events).
 */
struct NOTE_NAGA_ENGINE_API MidiTrack {
    std::vector<MidiEvent> events; ///< List of events in the track
};

/**
 * @brief Structure representing the MIDI file header.
 */
struct NOTE_NAGA_ENGINE_API MidiFileHeader {
    uint16_t format = 1;     ///< MIDI file format (0, 1, or 2)
    uint16_t nTracks = 0;    ///< Number of tracks in the file
    uint16_t division = 480; ///< Ticks per quarter note
};

/**
 * @brief Class for representing and manipulating a MIDI file.
 */
class NOTE_NAGA_ENGINE_API MidiFile {
  public:
    /**
     * @brief Constructs a new, empty MidiFile object.
     */
    MidiFile();

    /**
     * @brief Loads a MIDI file from disk.
     * @param filename Path to the MIDI file.
     * @return True if loading was successful, false otherwise.
     */
    bool load(const std::string &filename);

    /**
     * @brief Saves the MIDI file to disk.
     * @param filename Path to save the MIDI file.
     * @return True if saving was successful, false otherwise.
     */
    bool save(const std::string &filename) const;

    /**
     * @brief Clears all tracks and resets the header.
     */
    void clear();

    /**
     * @brief Gets the number of tracks in the file.
     * @return Number of tracks.
     */
    int getNumTracks() const;

    /**
     * @brief Gets a const reference to the specified track.
     * @param idx Index of the track.
     * @return Reference to the MidiTrack object.
     */
    const MidiTrack &getTrack(int idx) const;

    /**
     * @brief Gets a reference to the specified track.
     * @param idx Index of the track.
     * @return Reference to the MidiTrack object.
     */
    MidiTrack &getTrack(int idx);

    MidiFileHeader header;         ///< MIDI file header
    std::vector<MidiTrack> tracks; ///< All tracks in the file

    /**
     * @brief Helper function to create a simple test MIDI file (single track, simple note
     * events).
     * @return A MidiFile object with test data.
     */
    static MidiFile createTestFile();

  private:
    /**
     * @brief Reads a variable-length value from the stream.
     * @param in Input stream.
     * @return The value read.
     */
    static uint32_t readVarLen(std::istream &in);

    /**
     * @brief Writes a variable-length value to the stream.
     * @param out Output stream.
     * @param value Value to write.
     */
    static void writeVarLen(std::ostream &out, uint32_t value);

    /**
     * @brief Reads a 16-bit big-endian value from the stream.
     * @param in Input stream.
     * @return The value read.
     */
    static uint16_t readBE16(std::istream &in);

    /**
     * @brief Reads a 32-bit big-endian value from the stream.
     * @param in Input stream.
     * @return The value read.
     */
    static uint32_t readBE32(std::istream &in);

    /**
     * @brief Writes a 16-bit big-endian value to the stream.
     * @param out Output stream.
     * @param value Value to write.
     */
    static void writeBE16(std::ostream &out, uint16_t value);

    /**
     * @brief Writes a 32-bit big-endian value to the stream.
     * @param out Output stream.
     * @param value Value to write.
     */
    static void writeBE32(std::ostream &out, uint32_t value);

    /**
     * @brief Parses the MIDI file header from stream.
     * @param in Input stream.
     * @return True if parsing succeeded, false otherwise.
     */
    bool parseHeader(std::istream &in);

    /**
     * @brief Parses a MIDI track from stream.
     * @param in Input stream.
     * @param track Track to fill.
     * @return True if parsing succeeded, false otherwise.
     */
    bool parseTrack(std::istream &in, MidiTrack &track);

    /**
     * @brief Writes the MIDI file header to stream.
     * @param out Output stream.
     * @return True if writing succeeded, false otherwise.
     */
    bool writeHeader(std::ostream &out) const;

    /**
     * @brief Writes a MIDI track to stream.
     * @param out Output stream.
     * @param track Track to write.
     * @return True if writing succeeded, false otherwise.
     */
    bool writeTrack(std::ostream &out, const MidiTrack &track) const;
};
