#pragma once

#include <note_naga_engine/io/midi_file.h>
#include <note_naga_engine/note_naga_api.h>

#ifndef QT_DEACTIVATED
#include <QColor>
#include <QObject>
#endif

#include <complex>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

/*******************************************************************************************************/
// Macros for emitting signals depending on NN_QT_EMIT_ENABLED
/*******************************************************************************************************/
/**
 * @brief Macro for emitting Qt signals depending on NN_QT_EMIT_ENABLED.
 *        When QT_DEACTIVATED is defined, does nothing.
 *        Otherwise, emits the specified signal.
 */
// #define QT_DEACTIVATED

#ifdef QT_DEACTIVATED
#define NN_QT_EMIT(X)
#else
#define NN_QT_EMIT(X) emit X
#endif

/*******************************************************************************************************/
// Channel Colors
/*******************************************************************************************************/

/**
 * @brief Represents an RGB color for channel coloring.
 */
struct NOTE_NAGA_ENGINE_API NN_Color_t {
    uint8_t red, green, blue; ///< Red, green, and blue color channels

    /**
     * @brief Default constructor (black color).
     */
    NN_Color_t() : red(0), green(0), blue(0) {}

    /**
     * @brief Parameterized constructor.
     * @param rr Red value.
     * @param gg Green value.
     * @param bb Blue value.
     */
    NN_Color_t(uint8_t rr, uint8_t gg, uint8_t bb) : red(rr), green(gg), blue(bb) {}

    /**
     * @brief Equality operator for comparing two NNColor objects.
     */
    bool operator==(const NN_Color_t &other) const {
        return red == other.red && green == other.green && blue == other.blue;
    }

    /**
     * @brief Make the color lighter.
     * @param factor Brightness factor (default is 120).
     *        100 = původní barva, >100 zesvětlení, <100 ztmavení.
     * @return Lighter color.
     */
    NN_Color_t lighter(int factor = 120) const {
        // Qt: result = qMin(255, (component * factor) / 100)
        return NN_Color_t(std::min(255, (red * factor) / 100),
                          std::min(255, (green * factor) / 100),
                          std::min(255, (blue * factor) / 100));
    }

    /**
     * @brief Make the color darker.
     * @param factor Darkness factor (default is 120).
     *        100 = původní barva, >100 ztmavení, <100 zesvětlení.
     * @return Darker color.
     */
    NN_Color_t darker(int factor = 120) const {
        // Qt: result = qMin(255, (component * 100) / factor)
        // Ale logičtější (a běžnější) je: result = qMax(0, (component * 100) / factor)
        return NN_Color_t(std::max(0, (red * 100) / factor),
                          std::max(0, (green * 100) / factor),
                          std::max(0, (blue * 100) / factor));
    }

#ifndef QT_DEACTIVATED
    /**
     * @brief Converts NNColor to QColor (Qt).
     * @return QColor representation.
     */
    QColor toQColor() const { return QColor(red, green, blue); }

    /**
     * @brief Creates NNColor from QColor (Qt).
     * @param color QColor to convert.
     * @return NNColor representation.
     */
    static NN_Color_t fromQColor(const QColor &color) {
        return NN_Color_t(color.red(), color.green(), color.blue());
    }
#endif
};

/**
 * @brief Default channel colors used for tracks/channels.
 */
NOTE_NAGA_ENGINE_API extern const std::vector<NN_Color_t> DEFAULT_CHANNEL_COLORS;

/**
 * @brief Blends two colors with the given opacity for the foreground color.
 * @param fg Foreground color.
 * @param bg Background color.
 * @param opacity Opacity of the foreground color (0.0-1.0).
 * @return Blended NNColor.
 */
NOTE_NAGA_ENGINE_API extern NN_Color_t
nn_color_blend(const NN_Color_t &fg, const NN_Color_t &bg, double opacity);

/**
 * @brief Converts NNColor to a string representation in the format "R,G,B".
 * @param color NNColor to convert.
 * @return String representation.
 */
NOTE_NAGA_ENGINE_API extern double nn_yiq_luminance(const NN_Color_t &color);

/*******************************************************************************************************/
// Forwards declarations
/*******************************************************************************************************/

/**
 * @brief Forward declaration of NoteNagaTrack.
 */
class NOTE_NAGA_ENGINE_API NoteNagaTrack;
/**
 * @brief Forward declaration of NoteNagaMIDISeq.
 */
class NOTE_NAGA_ENGINE_API NoteNagaMidiSeq;

/*******************************************************************************************************/
// Unique ID generation
/*******************************************************************************************************/

/**
 * @brief Generates a unique identifier for a note.
 * @return Unique note ID.
 */
NOTE_NAGA_ENGINE_API unsigned long nn_generate_unique_note_id();

/**
 * @brief Generates a unique identifier for a MIDI sequence.
 * @return Unique sequence ID.
 */
NOTE_NAGA_ENGINE_API int nn_generate_unique_seq_id();

/*******************************************************************************************************/
// Note Naga Note
/*******************************************************************************************************/

/**
 * @brief Structure representing a single MIDI note in Note Naga.
 */
struct NOTE_NAGA_ENGINE_API NN_Note_t {
    unsigned long id;            ///< Unique note ID (required for identification)
    int note;                    ///< MIDI note number (0-127)
    std::optional<int> start;    ///< Optional: note start tick
    std::optional<int> length;   ///< Optional: note length in ticks
    std::optional<int> velocity; ///< Optional: note velocity (0-127)
    std::optional<int> pan;      ///< Optional: note pan (0=left, 64=center, 127=right)
    NoteNagaTrack *parent;       ///< Pointer to parent track

    /**
     * @brief Default constructor. Initializes note with a unique ID and zero values.
     */
    NN_Note_t()
        : id(nn_generate_unique_note_id()), note(0), start(std::nullopt),
          length(std::nullopt), velocity(std::nullopt), pan(std::nullopt), parent(nullptr) {}

    /**
     * @brief Parameterized constructor for NoteNagaNote.
     * @param note_ MIDI note number.
     * @param parent_ Pointer to parent track.
     * @param start_ Optional start tick.
     * @param length_ Optional length.
     * @param velocity_ Optional velocity.
     * @param track_ (Unused) Optional track index.
     * @param pan_ Optional pan value (0=left, 64=center, 127=right).
     */
    NN_Note_t(unsigned long note_, NoteNagaTrack *parent_,
              const std::optional<int> &start_ = std::nullopt,
              const std::optional<int> &length_ = std::nullopt,
              const std::optional<int> &velocity_ = std::nullopt,
              const std::optional<int> &track_ = std::nullopt,
              const std::optional<int> &pan_ = std::nullopt)
        : id(nn_generate_unique_note_id()), note(note_), start(start_), length(length_),
          velocity(velocity_), pan(pan_), parent(parent_) {}
};

/**
 * @brief Calculates the time (in milliseconds) for a note given ppq and tempo.
 * @param note NoteNagaNote structure.
 * @param ppq Pulses per quarter note.
 * @param tempo Tempo in BPM.
 * @return Duration in milliseconds.
 */
NOTE_NAGA_ENGINE_API double note_time_ms(const NN_Note_t &note, int ppq, int tempo);

/*******************************************************************************************************/
// Note Naga Tempo Event
/*******************************************************************************************************/

/**
 * @brief Interpolation mode for tempo changes.
 */
enum class TempoInterpolation {
    Step,   ///< Instant tempo change (step function)
    Linear  ///< Linear interpolation to next tempo point
};

/**
 * @brief Structure representing a tempo change event at a specific tick.
 */
struct NOTE_NAGA_ENGINE_API NN_TempoEvent_t {
    int tick;                            ///< Tick position of the tempo event
    double bpm;                          ///< Tempo in beats per minute (20-300)
    TempoInterpolation interpolation;    ///< How to transition to next tempo point

    /**
     * @brief Default constructor. 120 BPM at tick 0 with step interpolation.
     */
    NN_TempoEvent_t()
        : tick(0), bpm(120.0), interpolation(TempoInterpolation::Step) {}

    /**
     * @brief Parameterized constructor.
     * @param tick_ Tick position.
     * @param bpm_ Tempo in BPM.
     * @param interp_ Interpolation mode.
     */
    NN_TempoEvent_t(int tick_, double bpm_, TempoInterpolation interp_ = TempoInterpolation::Step)
        : tick(tick_), bpm(bpm_), interpolation(interp_) {}
    
    /**
     * @brief Comparison operator for sorting by tick.
     */
    bool operator<(const NN_TempoEvent_t &other) const {
        return tick < other.tick;
    }
};

/*******************************************************************************************************/
// Note Naga Track
/*******************************************************************************************************/

/**
 * @brief Represents a single MIDI track in Note Naga. Contains notes and track metadata.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaTrack : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaTrack {
#endif

public:
    /**
     * @brief Default constructor for NoteNagaTrack.
     */
    NoteNagaTrack();

    /**
     * @brief Parameterized constructor for NoteNagaTrack.
     * @param track_id Unique track ID.
     * @param parent Pointer to parent MIDI sequence.
     * @param name Track name.
     * @param instrument Optional instrument index.
     * @param channel Optional MIDI channel.
     */
    NoteNagaTrack(int track_id, NoteNagaMidiSeq *parent,
                  const std::string &name = "Track",
                  const std::optional<int> &instrument = std::nullopt,
                  const std::optional<int> &channel = std::nullopt);

    /**
     * @brief Destructor for NoteNagaTrack.
     */
    virtual ~NoteNagaTrack() = default;

    /**
     * @brief Adds a MIDI note to the track.
     * @param note The MIDI note to add.
     */
    void addNote(const NN_Note_t &note);

    /**
     * @brief Removes a MIDI note from the track.
     * @param note The MIDI note to remove.
     */
    void removeNote(const NN_Note_t &note);

    // GETTERS
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Gets the track's unique ID.
     * @return Track ID.
     */
    int getId() const { return track_id; }

    /**
     * @brief Gets the parent MIDI sequence.
     * @return Pointer to parent sequence.
     */
    NoteNagaMidiSeq *getParent() const { return parent; }

    /**
     * @brief Gets all MIDI notes in the track.
     * @return Vector of notes.
     */
    std::vector<NN_Note_t> getNotes() const { return midi_notes; }

    /**
     * @brief Gets the track's instrument index.
     * @return Optional instrument index.
     */
    std::optional<int> getInstrument() const { return instrument; }

    /**
     * @brief Gets the assigned MIDI channel.
     * @return Optional channel number.
     */
    std::optional<int> getChannel() const { return channel; }

    /**
     * @brief Gets the track name.
     * @return Reference to the name string.
     */
    const std::string &getName() const { return name; }

    /**
     * @brief Gets the assigned color for this track.
     * @return Reference to color.
     */
    const NN_Color_t &getColor() const { return color; }

    /**
     * @brief Returns whether the track is visible. Can be used to toggle visibility in
     * UI.
     * @return True if visible.
     */
    bool isVisible() const { return visible; }

    /**
     * @brief Returns whether the track is muted.
     * @return True if muted.
     */
    bool isMuted() const { return muted; }

    /**
     * @brief Returns whether the track is soloed.
     * @return True if solo.
     */
    bool isSolo() const { return solo; }

    /**
     * @brief Gets the volume for this track.
     * @return Volume (0.0 - 1.0).
     */
    float getVolume() const { return volume; }

    // SETTERS
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Sets the track's unique ID. Method not prevent setting the same ID for
     * multiple tracks.
     * @param new_id New track ID.
     */
    void setId(int new_id);

    /**
     * @brief Sets the parent MIDI sequence.
     * @param parent Pointer to parent MIDI sequence.
     */
    void setParent(NoteNagaMidiSeq *parent) { this->parent = parent; }

    /**
     * @brief Sets the notes for this track.
     * @param notes Vector of notes.
     */
    void setNotes(const std::vector<NN_Note_t> &notes) { this->midi_notes = notes; }

    /**
     * @brief Sets the instrument index.
     * @param instrument Optional instrument index.
     */
    void setInstrument(std::optional<int> instrument);

    /**
     * @brief Sets the MIDI channel.
     * @param channel Optional channel number.
     */
    void setChannel(std::optional<int> channel);

    /**
     * @brief Sets the track name.
     * @param new_name New track name.
     */
    void setName(const std::string &new_name);

    /**
     * @brief Sets the track color.
     * @param new_color New color.
     */
    void setColor(const NN_Color_t &new_color);

    /**
     * @brief Sets the track visibility. Can be used to toggle visibility in UI.
     * @param is_visible True to make visible.
     */
    void setVisible(bool is_visible);

    /**
     * @brief Sets the mute state.
     * @param is_muted True to mute.
     */
    void setMuted(bool is_muted);

    /**
     * @brief Sets the solo state.
     * @param is_solo True to solo.
     */
    void setSolo(bool is_solo);

    /**
     * @brief Sets the track's volume.
     * @param new_volume New volume value.
     */
    void setVolume(float new_volume);

protected:
    int track_id;                      ///< Unique track ID
    std::optional<int> instrument;     ///< Instrument index (optional)
    std::optional<int> channel;        ///< MIDI channel (optional)
    std::string name;                  ///< Track name
    NN_Color_t color;                  ///< Track color
    bool visible;                      ///< Track visibility
    bool muted;                        ///< Track muted state
    bool solo;                         ///< Track solo state
    float volume;                      ///< Track volume (0.0 - 1.0)
    std::vector<NN_Note_t> midi_notes; ///< MIDI notes in this track
    NoteNagaMidiSeq *parent;           ///< Pointer to parent MIDI sequence
    
    // Tempo track support
    bool is_tempo_track;                         ///< True if this is a tempo track
    bool tempo_track_active;                     ///< True if tempo track is active (overrides fixed BPM)
    std::vector<NN_TempoEvent_t> tempo_events;   ///< Tempo events (only used if is_tempo_track)

public:
    // TEMPO TRACK METHODS
    // ///////////////////////////////////////////////////////////////////////////////
    
    /**
     * @brief Returns whether this track is a tempo track.
     * @return True if tempo track.
     */
    bool isTempoTrack() const { return is_tempo_track; }
    
    /**
     * @brief Returns whether tempo track is active (overrides fixed BPM).
     * @return True if tempo track is active.
     */
    bool isTempoTrackActive() const { return is_tempo_track && tempo_track_active; }
    
    /**
     * @brief Sets whether tempo track is active.
     * @param active True to activate tempo track.
     */
    void setTempoTrackActive(bool active);
    
    /**
     * @brief Sets whether this track is a tempo track.
     * @param is_tempo True to make this a tempo track.
     */
    void setTempoTrack(bool is_tempo);
    
    /**
     * @brief Gets the tempo events for this track.
     * @return Vector of tempo events.
     */
    const std::vector<NN_TempoEvent_t>& getTempoEvents() const { return tempo_events; }
    
    /**
     * @brief Sets the tempo events for this track.
     * @param events Vector of tempo events.
     */
    void setTempoEvents(const std::vector<NN_TempoEvent_t>& events);
    
    /**
     * @brief Adds a tempo event to this track.
     * @param event Tempo event to add.
     */
    void addTempoEvent(const NN_TempoEvent_t& event);
    
    /**
     * @brief Removes a tempo event at the specified tick.
     * @param tick Tick position of the event to remove.
     * @return True if event was removed.
     */
    bool removeTempoEventAtTick(int tick);
    
    /**
     * @brief Gets the BPM at a specific tick, with interpolation if applicable.
     * @param tick The tick position.
     * @return BPM at that tick.
     */
    double getTempoAtTick(int tick) const;
    
    /**
     * @brief Clears all tempo events and adds a single event at tick 0.
     * @param bpm Initial tempo in BPM.
     */
    void resetTempoEvents(double bpm = 120.0);

    // SIGNALS
    // ////////////////////////////////////////////////////////////////////////////////

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when track metadata changes (name, id, volume, ...) not
     * including notes vector.
     * @param track Pointer to the track.
     * @param param Name of the changed parameter.
     */
    void metadataChanged(NoteNagaTrack *track, const std::string &param);
    
    /**
     * @brief Signal emitted when tempo events change.
     * @param track Pointer to the track.
     */
    void tempoEventsChanged(NoteNagaTrack *track);
#endif
};

/*******************************************************************************************************/
// Note Naga MIDI Sequence
/*******************************************************************************************************/

/**
 * @brief Represents a MIDI sequence in Note Naga, containing tracks and related metadata.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaMidiSeq : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaMidiSeq {
#endif

public:
    /**
     * @brief Default constructor.
     */
    NoteNagaMidiSeq();

    /**
     * @brief Constructor with sequence ID.
     * @param sequence_id Unique sequence ID.
     */
    NoteNagaMidiSeq(int sequence_id);

    /**
     * @brief Constructor with sequence ID and tracks.
     * @param sequence_id Unique sequence ID.
     * @param tracks Vector of track pointers.
     */
    NoteNagaMidiSeq(int sequence_id, std::vector<NoteNagaTrack *> tracks);

    /**
     * @brief Destructor.
     */
    virtual ~NoteNagaMidiSeq();

    /**
     * @brief Clears all tracks and data from this sequence.
     */
    void clear();

    /**
     * @brief Computes the maximum tick value across all tracks.
     * @return Maximum tick.
     */
    int computeMaxTick();

        /**
     * @brief Adds a new track to the sequence.
     * @param instrument_index Index of the instrument to use for the new track.
     * @return Pointer to the newly created track, or nullptr on failure.
     */
    NoteNagaTrack* addTrack(int instrument_index);

    /**
     * @brief Removes a track from the sequence.
     * @param track_index Index of the track to remove.
     * @return True if the track was removed successfully, false otherwise.
     */
    bool removeTrack(int track_index);

    /**
     * @brief Moves a track from one position to another in the sequence.
     * @param from_index Current index of the track to move.
     * @param to_index Target index where the track should be moved.
     * @return True if the track was moved successfully, false otherwise.
     */
    bool moveTrack(int from_index, int to_index);

    /**
     * @brief Loads a MIDI file into the sequence from the specified path.
     * @param midi_file_path Path to the MIDI file.
     */
    void loadFromMidi(const std::string &midi_file_path);

    /**
     * @brief Exports the sequence to a standard MIDI file.
     * @param midi_file_path Path to save the MIDI file.
     * @param trackIds Optional set of track IDs to include. If empty, all tracks are exported.
     * @return True if export was successful, false otherwise.
     */
    bool exportToMidi(const std::string &midi_file_path, 
                      const std::set<int> &trackIds = {}) const;

    /**
     * @brief Loads tracks for type 0 MIDI files.
     * @param midiFile Pointer to the MIDI file.
     * @return Vector of track pointers.
     */
    std::vector<NoteNagaTrack *> loadType0Tracks(const MidiFile *midiFile);

    /**
     * @brief Loads tracks for type 1 MIDI files.
     * @param midiFile Pointer to the MIDI file.
     * @return Vector of track pointers.
     */
    std::vector<NoteNagaTrack *> loadType1Tracks(const MidiFile *midiFile);

    // GETTERS
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Gets the sequence's unique ID.
     * @return Sequence ID.
     */
    int getId() const { return sequence_id; }

    /**
     * @brief Gets the pulses per quarter note (PPQ) value.
     * @return PPQ.
     */
    int getPPQ() const { return ppq; }

    /**
     * @brief Gets the tempo (BPM). In microseconds per quarter note.
     * @return Tempo.
     * @note int bpm = 60'000'000.0 / seq->getTempo(); // Convert to BPM
     */
    int getTempo() const { return tempo; }

    /**
     * @brief Gets the maximum tick value for this sequence.
     * @return Maximum tick.
     */
    int getMaxTick() const { return max_tick; }

    /**
     * @brief Gets the currently active track.
     * @return Pointer to the active track.
     */
    NoteNagaTrack *getActiveTrack() const { return active_track; }

    /**
     * @brief Gets the currently soloed track.
     * @return Pointer to the soloed track.
     */
    NoteNagaTrack *getSoloTrack() const { return solo_track; }

    /**
     * @brief Gets all tracks in the sequence.
     * @return Vector of track pointers.
     */
    std::vector<NoteNagaTrack *> getTracks() const { return tracks; }

    /**
     * @brief Gets a track by its ID.
     * @param track_id Track ID.
     * @return Pointer to the track.
     */
    NoteNagaTrack *getTrackById(int track_id);

    /**
     * @brief Gets the MIDI file associated with this sequence.
     * @return Pointer to the MIDI file.
     */
    MidiFile *getMidiFile() const { return midi_file; }

    /**
     * @brief Gets the file path of the MIDI file.
     * @return File path.
     */
    std::string getFilePath() const { return file_path; }

    // SETTERS
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Sets the sequence's unique ID.
     * @param new_id New sequence ID.
     */
    void setId(int new_id);

    /**
     * @brief Sets the pulses per quarter note (PPQ) value.
     * @param ppq PPQ value.
     */
    void setPPQ(int ppq);

    /**
     * @brief Sets the tempo (BPM). In microseconds per quarter note.
     * @param tempo Tempo value.
     * @note seq->getTempo(60'000'000.0 / bpm); // Convert to BPM in seconds in seconds
     */
    void setTempo(int tempo);

    /**
     * @brief Sets the currently active track.
     * @param track Pointer to the active track.
     */
    void setActiveTrack(NoteNagaTrack *track);

    /**
     * @brief Sets the currently soloed track.
     * @param track Pointer to the soloed track.
     */
    void setSoloTrack(NoteNagaTrack *track);
    
    // TEMPO TRACK METHODS
    // ///////////////////////////////////////////////////////////////////////////////
    
    /**
     * @brief Checks if the sequence has a tempo track.
     * @return True if a tempo track exists.
     */
    bool hasTempoTrack() const;
    
    /**
     * @brief Gets the tempo track if it exists.
     * @return Pointer to the tempo track, or nullptr if none.
     */
    NoteNagaTrack* getTempoTrack() const;
    
    /**
     * @brief Creates a new tempo track with the current fixed tempo.
     *        Only one tempo track can exist per sequence.
     * @return Pointer to the new tempo track, or existing tempo track.
     */
    NoteNagaTrack* createTempoTrack();
    
    /**
     * @brief Sets a track as the tempo track.
     *        The previous tempo track (if any) will be converted to a normal track.
     * @param track Pointer to the track to set as tempo track.
     * @return True if successful.
     */
    bool setTempoTrack(NoteNagaTrack* track);
    
    /**
     * @brief Removes the tempo track designation (converts it to a normal track).
     * @return True if a tempo track was removed.
     */
    bool removeTempoTrack();
    
    /**
     * @brief Gets the effective tempo at a specific tick.
     *        If a tempo track exists, uses it; otherwise uses the fixed tempo.
     * @param tick The tick position.
     * @return Tempo in microseconds per quarter note.
     */
    int getEffectiveTempoAtTick(int tick) const;
    
    /**
     * @brief Gets the effective BPM at a specific tick.
     *        If a tempo track exists, uses it; otherwise uses the fixed tempo.
     * @param tick The tick position.
     * @return Tempo in BPM.
     */
    double getEffectiveBPMAtTick(int tick) const;
    
    /**
     * @brief Converts ticks to seconds using tempo track if available.
     * @param tick The tick position.
     * @return Time in seconds from the start.
     */
    double ticksToSeconds(int tick) const;
    
    /**
     * @brief Converts seconds to ticks using tempo track if available.
     * @param seconds Time in seconds from the start.
     * @return Tick position.
     */
    int secondsToTicks(double seconds) const;

protected:
    int sequence_id;                     ///< Unique sequence ID
    std::string file_path;               ///< Path to the MIDI file
    std::vector<NoteNagaTrack *> tracks; ///< All tracks in the sequence
    NoteNagaTrack *active_track;         ///< Pointer to the currently active track
    NoteNagaTrack *solo_track;           ///< Pointer to the currently soloed track
    MidiFile *midi_file;                 ///< Pointer to the MIDI file object
    int ppq;                             ///< Pulses per quarter note (PPQ)
    int tempo;                           ///< Tempo (BPM)
    int max_tick;                        ///< Maximum tick in the sequence

    // SIGNALS
    // ////////////////////////////////////////////////////////////////////////////////

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when sequence metadata changes.
     * @param seq Pointer to the sequence.
     * @param param Name of the changed parameter.
     */
    void metadataChanged(NoteNagaMidiSeq *seq, const std::string &param);

    /**
     * @brief Signal emitted when track metadata changes.
     * @param track Pointer to the track.
     * @param param Name of the changed parameter.
     */
    void trackMetadataChanged(NoteNagaTrack *track, const std::string &param);

    /**
     * @brief Signal emitted when the active track changes.
     * @param track Pointer to the active track.
     */
    void activeTrackChanged(NoteNagaTrack *track);

    /**
     * @brief Signal emitted when the track list changes.
     * @param track Pointer to the track.
     */
    void trackListChanged();
    
    /**
     * @brief Signal emitted when the tempo track changes or tempo events are modified.
     */
    void tempoTrackChanged();
#endif
};

/*******************************************************************************************************/
// General MIDI Instruments Utils
/*******************************************************************************************************/

/**
 * @brief Structure describing a General MIDI instrument.
 */
struct NOTE_NAGA_ENGINE_API NN_GMInstrument_t {
    int index;        ///< Instrument index
    std::string name; ///< Human-readable name
    std::string icon; ///< Icon filename or resource ID
};

/**
 * @brief List of all General MIDI instruments.
 */
NOTE_NAGA_ENGINE_API extern const std::vector<NN_GMInstrument_t> GM_INSTRUMENTS;

/**
 * @brief Finds a General MIDI instrument by name.
 * @param name Instrument name.
 * @return Optional GMInstrument if found.
 */
NOTE_NAGA_ENGINE_API extern std::optional<NN_GMInstrument_t>
nn_find_instrument_by_name(const std::string &name);

/**
 * @brief Finds a General MIDI instrument by index.
 * @param index Instrument index.
 * @return Optional GMInstrument if found.
 */
NOTE_NAGA_ENGINE_API std::optional<NN_GMInstrument_t>
nn_find_instrument_by_index(int index);

/*******************************************************************************************************/
// Note Names Utils
/*******************************************************************************************************/

/**
 * @brief List of note names (C, C#, D, ...).
 */
NOTE_NAGA_ENGINE_API extern const std::vector<std::string> NOTE_NAMES;

/**
 * @brief Gets the note name (e.g., "C4") for a given MIDI note number.
 * @param n MIDI note number.
 * @return Note name as string.
 */
NOTE_NAGA_ENGINE_API extern std::string nn_note_name(int n);

/**
 * @brief Gets the index inside the octave (0 = C, 1 = C#, ..., 11 = B) for a MIDI note
 * number.
 * @param n MIDI note number.
 * @return Index in octave (0-11).
 */
NOTE_NAGA_ENGINE_API extern int nn_index_in_octave(int n);

/*******************************************************************************************************/
// Time / Tick Utils
/*******************************************************************************************************/

/**
 * @brief Converts seconds to ticks based on PPQ and tempo.
 * @param seconds Time in seconds.
 * @param ppq Pulses per quarter note (pulses per quarter note).
 * @param tempo Tempo in BPM (In microseconds per quarter note).
 * @return Number of ticks.
 */
NOTE_NAGA_ENGINE_API extern double nn_seconds_to_ticks(double seconds, int ppq, int tempo);

/**
 * @brief Converts ticks to seconds based on PPQ and tempo.
 * @param ticks Number of ticks.
 * @param ppq Pulses per quarter note (pulses per quarter note).
 * @param tempo Tempo in BPM (In microseconds per quarter note).
 * @return Time in seconds.
 */
NOTE_NAGA_ENGINE_API extern double nn_ticks_to_seconds(int ticks, int ppq, int tempo);

/*******************************************************************************************************/
// Audio Analysis Utils
/*******************************************************************************************************/

/**
 * @brief Computes the root mean square (RMS) of an audio buffer.
 * @param buffer Pointer to the audio buffer.
 * @param num_frames Number of frames in the buffer.
 * @return RMS value.
 */
NOTE_NAGA_ENGINE_API extern void nn_fft(std::vector<std::complex<float>>& a);