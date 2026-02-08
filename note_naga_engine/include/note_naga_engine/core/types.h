#pragma once

#include <note_naga_engine/io/midi_file.h>
#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/audio/audio_resource.h>

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

// Forward declarations for synth types (avoid circular include)
class NoteNagaSynthesizer;
class INoteNagaSoftSynth;

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
     * @brief Adds multiple MIDI notes to the track in bulk (optimized, no signal per note).
     * @param notes Vector of MIDI notes to add.
     */
    void addNotesBulk(const std::vector<NN_Note_t> &notes);

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

    // PER-TRACK SYNTH GETTERS
    // ///////////////////////////////////////////////////////////////////////////////
    
    /**
     * @brief Gets the track's synthesizer instance.
     * @return Pointer to the synthesizer, or nullptr if not set.
     */
    NoteNagaSynthesizer* getSynth() const { return synth_; }
    
    /**
     * @brief Gets the track's synthesizer as a soft synth interface.
     * @return Pointer to INoteNagaSoftSynth, or nullptr if synth doesn't support it.
     */
    INoteNagaSoftSynth* getSoftSynth() const;
    
    /**
     * @brief Gets the audio output volume in dB.
     * @return Volume in dB (-24.0 to +6.0).
     */
    float getAudioVolumeDb() const { return audio_volume_db_; }
    
    /**
     * @brief Gets the audio output volume as linear gain.
     * @return Linear gain multiplier.
     */
    float getAudioVolumeLinear() const;
    
    /**
     * @brief Gets the MIDI pan offset.
     * @return Pan offset (-64 to +64).
     */
    int getMidiPanOffset() const { return midi_pan_offset_; }
    
    /**
     * @brief Gets the pan as normalized float for audio processing.
     * @return Pan value (-1.0 = full left, 0.0 = center, +1.0 = full right).
     */
    float getPanNormalized() const { return midi_pan_offset_ / 64.0f; }

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

    // PER-TRACK SYNTH SETTERS
    // ///////////////////////////////////////////////////////////////////////////////
    
    /**
     * @brief Sets the track's synthesizer instance.
     *        The track takes ownership of the synth and will delete it on destruction.
     * @param synth Pointer to the synthesizer.
     */
    void setSynth(NoteNagaSynthesizer* synth);
    
    /**
     * @brief Initializes the track's default synthesizer using SoundFontFinder.
     *        Creates a FluidSynth with the first available soundfont.
     * @return True if synth was successfully initialized.
     */
    bool initDefaultSynth();
    
    /**
     * @brief Sets the audio output volume in dB.
     * @param db Volume in dB (-24.0 to +6.0).
     */
    void setAudioVolumeDb(float db);
    
    /**
     * @brief Sets the MIDI pan offset.
     * @param offset Pan offset (-64 to +64).
     */
    void setMidiPanOffset(int offset);
    
    /**
     * @brief Plays a note through this track's synth with pan offset applied.
     * @param note The note to play.
     */
    void playNote(const NN_Note_t& note);
    
    /**
     * @brief Stops a note on this track's synth.
     * @param note The note to stop.
     */
    void stopNote(const NN_Note_t& note);
    
    /**
     * @brief Stops all notes on this track's synth.
     */
    void stopAllNotes();
    
    /**
     * @brief Renders audio from this track's synth with volume applied.
     * @param left Left channel buffer.
     * @param right Right channel buffer.
     * @param num_frames Number of frames to render.
     */
    void renderAudio(float* left, float* right, size_t num_frames);

protected:
    int track_id;                      ///< Unique track ID
    std::optional<int> instrument;     ///< Instrument index (optional)
    std::optional<int> channel;        ///< MIDI channel (optional)
    std::string name;                  ///< Track name
    NN_Color_t color;                  ///< Track color
    bool visible;                      ///< Track visibility
    bool muted;                        ///< Track muted state
    bool solo;                         ///< Track solo state
    float volume;                      ///< Track volume (0.0 - 1.0) - legacy, use audio_volume_db_
    std::vector<NN_Note_t> midi_notes; ///< MIDI notes in this track
    NoteNagaMidiSeq *parent;           ///< Pointer to parent MIDI sequence
    
    // Per-track synthesizer (new architecture)
    NoteNagaSynthesizer *synth_;           ///< Track's own synthesizer instance
    float audio_volume_db_;                ///< Audio output volume in dB (-24.0 to +6.0)
    int midi_pan_offset_;                  ///< MIDI pan offset (-64 to +64)
    
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

    /**
     * @brief Sets the file path (used for display name).
     * @param path New file path or display name.
     */
    void setFilePath(const std::string &path) { file_path = path; }
    
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

/*******************************************************************************************************/
// Arrangement Types (Timeline / Composition)
/*******************************************************************************************************/

/**
 * @brief MIDI channel used for drums (channel 10 in MIDI, 0-indexed = 9).
 */
constexpr int MIDI_DRUM_CHANNEL = 9;

/**
 * @brief Generates a unique identifier for a MidiClip.
 * @return Unique clip ID.
 */
NOTE_NAGA_ENGINE_API int nn_generate_unique_clip_id();

/**
 * @brief Generates a unique identifier for an ArrangementTrack.
 * @return Unique arrangement track ID.
 */
NOTE_NAGA_ENGINE_API int nn_generate_unique_arrangement_track_id();

/**
 * @brief Represents a clip on the arrangement timeline.
 *        A clip is a reference to a MIDI sequence placed at a specific position.
 *        The same sequence can be placed multiple times as different clips.
 */
struct NOTE_NAGA_ENGINE_API NN_MidiClip_t {
    int id;                  ///< Unique clip ID
    int sequenceId;          ///< Reference to NoteNagaMidiSeq ID
    int startTick;           ///< Position on the arrangement timeline (in ticks)
    int durationTicks;       ///< Duration of the clip (can be shorter than sequence)
    int offsetTicks;         ///< Offset within the sequence (for trimming start)
    int fadeInTicks = 0;     ///< Fade in duration in ticks
    int fadeOutTicks = 0;    ///< Fade out duration in ticks
    bool muted;              ///< Whether this clip is muted
    std::string name;        ///< Optional clip name (defaults to sequence name)
    NN_Color_t color;        ///< Clip color (defaults to sequence color)

    /**
     * @brief Default constructor.
     */
    NN_MidiClip_t()
        : id(nn_generate_unique_clip_id()), sequenceId(-1), startTick(0),
          durationTicks(0), offsetTicks(0), muted(false), name(""), color() {}

    /**
     * @brief Parameterized constructor.
     * @param seqId MIDI sequence ID to reference.
     * @param start Start position in ticks.
     * @param duration Duration in ticks.
     * @param offset Offset within sequence in ticks.
     */
    NN_MidiClip_t(int seqId, int start, int duration, int offset = 0)
        : id(nn_generate_unique_clip_id()), sequenceId(seqId), startTick(start),
          durationTicks(duration), offsetTicks(offset), muted(false), name(""), color() {}

    /**
     * @brief Gets the end tick of the clip (exclusive).
     * @return End tick position.
     */
    int getEndTick() const { return startTick + durationTicks; }

    /**
     * @brief Checks if this clip is active at a given tick.
     * @param tick The tick to check.
     * @return True if the clip contains this tick.
     */
    bool containsTick(int tick) const {
        return tick >= startTick && tick < getEndTick();
    }

    /**
     * @brief Converts arrangement tick to sequence-local tick.
     * @param arrangementTick The tick on the arrangement timeline.
     * @return The corresponding tick within the MIDI sequence.
     */
    int toSequenceTick(int arrangementTick) const {
        return (arrangementTick - startTick) + offsetTicks;
    }
    
    /**
     * @brief Converts arrangement tick to sequence-local tick with looping support.
     * @param arrangementTick The tick on the arrangement timeline.
     * @param sequenceLength The length of the source sequence in ticks.
     * @return The corresponding tick within the MIDI sequence (wrapped for looping).
     */
    int toSequenceTickLooped(int arrangementTick, int sequenceLength) const {
        if (sequenceLength <= 0) return 0;
        int localTick = (arrangementTick - startTick) + offsetTicks;
        return localTick % sequenceLength;
    }
    
    /**
     * @brief Gets the loop iteration number at a given arrangement tick.
     * @param arrangementTick The tick on the arrangement timeline.
     * @param sequenceLength The length of the source sequence in ticks.
     * @return The loop iteration (0 for first play, 1 for first repeat, etc.)
     */
    int getLoopIteration(int arrangementTick, int sequenceLength) const {
        if (sequenceLength <= 0) return 0;
        int localTick = (arrangementTick - startTick) + offsetTicks;
        return localTick / sequenceLength;
    }

    /**
     * @brief Comparison operator for sorting by start tick.
     */
    bool operator<(const NN_MidiClip_t &other) const {
        return startTick < other.startTick;
    }
};

/**
 * @brief Represents a track/layer in the arrangement timeline.
 *        Each arrangement track can contain multiple clips.
 *        Provides channel remapping to avoid MIDI channel conflicts.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaArrangementTrack : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaArrangementTrack {
#endif

public:
    /**
     * @brief Default constructor.
     */
    NoteNagaArrangementTrack();

    /**
     * @brief Constructor with ID and name.
     * @param id Unique track ID.
     * @param name Track name.
     */
    NoteNagaArrangementTrack(int id, const std::string &name);

    /**
     * @brief Destructor.
     */
    virtual ~NoteNagaArrangementTrack() = default;

    // CLIP MANAGEMENT
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Adds a clip to this track.
     * @param clip The clip to add.
     */
    void addClip(const NN_MidiClip_t &clip);

    /**
     * @brief Removes a clip by ID.
     * @param clipId The clip ID to remove.
     * @return True if clip was found and removed.
     */
    bool removeClip(int clipId);

    /**
     * @brief Gets a clip by ID.
     * @param clipId The clip ID.
     * @return Pointer to the clip, or nullptr if not found.
     */
    NN_MidiClip_t* getClipById(int clipId);
    const NN_MidiClip_t* getClipById(int clipId) const;

    /**
     * @brief Gets all clips that are active at a given tick.
     * @param tick The tick to check.
     * @return Vector of pointers to active clips.
     */
    std::vector<const NN_MidiClip_t*> getClipsAtTick(int tick) const;

    /**
     * @brief Gets all clips in this track.
     * @return Reference to the clips vector.
     */
    const std::vector<NN_MidiClip_t>& getClips() const { return clips_; }
    std::vector<NN_MidiClip_t>& getClips() { return clips_; }

    // AUDIO CLIPS
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Adds an audio clip to this track.
     * @param audioResourceId ID of the audio resource.
     * @param startTick Start position in ticks.
     * @param durationTicks Duration in ticks.
     * @param looping Whether the clip should loop.
     * @return Reference to the created clip.
     */
    NN_AudioClip_t& addAudioClip(int audioResourceId, int startTick, int durationTicks, bool looping = false);

    /**
     * @brief Adds an existing audio clip to this track.
     * @param clip The audio clip to add.
     */
    void addAudioClip(const NN_AudioClip_t& clip);

    /**
     * @brief Removes an audio clip by ID.
     * @param clipId The clip ID.
     * @return True if successful.
     */
    bool removeAudioClip(int clipId);

    /**
     * @brief Gets an audio clip by ID.
     * @param clipId The clip ID.
     * @return Pointer to the clip, or nullptr if not found.
     */
    NN_AudioClip_t* getAudioClipById(int clipId);
    const NN_AudioClip_t* getAudioClipById(int clipId) const;

    /**
     * @brief Gets all audio clips in this track.
     * @return Reference to the audio clips vector.
     */
    const std::vector<NN_AudioClip_t>& getAudioClips() const { return audioClips_; }
    std::vector<NN_AudioClip_t>& getAudioClips() { return audioClips_; }

    /**
     * @brief Moves an audio clip to a new position.
     * @param clipId The clip ID.
     * @param newStartTick New start position.
     * @return True if successful.
     */
    bool moveAudioClip(int clipId, int newStartTick);

    /**
     * @brief Resizes an audio clip.
     * @param clipId The clip ID.
     * @param newDuration New duration in ticks.
     * @return True if successful.
     */
    bool resizeAudioClip(int clipId, int newDuration);

    /**
     * @brief Moves a clip to a new position.
     * @param clipId The clip ID.
     * @param newStartTick New start position.
     * @return True if successful.
     */
    bool moveClip(int clipId, int newStartTick);

    /**
     * @brief Resizes a clip.
     * @param clipId The clip ID.
     * @param newDuration New duration in ticks.
     * @return True if successful.
     */
    bool resizeClip(int clipId, int newDuration);

    // CHANNEL REMAPPING
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Gets the remapped MIDI channel for a given original channel.
     *        Drum channel (9) is never remapped.
     * @param originalChannel The original MIDI channel from the sequence.
     * @param isDrumTrack Whether the source track is a drum track.
     * @return The remapped channel to use for output.
     */
    int getRemappedChannel(int originalChannel, bool isDrumTrack) const;

    /**
     * @brief Sets the channel offset for this arrangement track.
     *        Non-drum channels will be offset by this value (mod 16, skipping drum channel).
     * @param offset Channel offset (0-15).
     */
    void setChannelOffset(int offset);

    /**
     * @brief Gets the channel offset.
     * @return Channel offset value.
     */
    int getChannelOffset() const { return channelOffset_; }

    // GETTERS
    // ///////////////////////////////////////////////////////////////////////////////

    int getId() const { return id_; }
    const std::string& getName() const { return name_; }
    const NN_Color_t& getColor() const { return color_; }
    bool isMuted() const { return muted_; }
    bool isSolo() const { return solo_; }
    float getVolume() const { return volume_; }
    float getPan() const { return pan_; }

    // SETTERS
    // ///////////////////////////////////////////////////////////////////////////////

    void setId(int id) { id_ = id; }
    void setName(const std::string &name);
    void setColor(const NN_Color_t &color);
    void setMuted(bool muted);
    void setSolo(bool solo);
    void setVolume(float volume);
    void setPan(float pan);

protected:
    int id_;                              ///< Unique arrangement track ID
    std::string name_;                    ///< Track name
    NN_Color_t color_;                    ///< Track color
    bool muted_;                          ///< Mute state
    bool solo_;                           ///< Solo state
    float volume_;                        ///< Volume (0.0 - 1.0)
    float pan_;                           ///< Pan (-1.0 to 1.0, 0.0 = center)
    int channelOffset_;                   ///< MIDI channel offset for remapping
    std::vector<NN_MidiClip_t> clips_;    ///< All MIDI clips on this track
    std::vector<NN_AudioClip_t> audioClips_;  ///< All audio clips on this track

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when track metadata changes.
     * @param track Pointer to this track.
     * @param param Name of the changed parameter.
     */
    void metadataChanged(NoteNagaArrangementTrack *track, const std::string &param);

    /**
     * @brief Signal emitted when clips change.
     */
    void clipsChanged();
    
    /**
     * @brief Signal emitted when audio clips change.
     */
    void audioClipsChanged();
#endif
};

/**
 * @brief Represents the full arrangement/composition timeline.
 *        Contains multiple arrangement tracks with clips.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaArrangement : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaArrangement {
#endif

public:
    /**
     * @brief Default constructor.
     */
    NoteNagaArrangement();

    /**
     * @brief Destructor.
     */
    virtual ~NoteNagaArrangement();

    /**
     * @brief Clears all tracks and clips.
     */
    void clear();

    // TRACK MANAGEMENT
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Adds a new arrangement track.
     * @param name Track name.
     * @return Pointer to the newly created track.
     */
    NoteNagaArrangementTrack* addTrack(const std::string &name = "Track");

    /**
     * @brief Inserts a new arrangement track at a specific index.
     * @param index The index at which to insert the track.
     * @param name Track name.
     * @return Pointer to the newly created track.
     */
    NoteNagaArrangementTrack* insertTrack(int index, const std::string &name = "Track");

    /**
     * @brief Removes an arrangement track by ID.
     * @param trackId The track ID to remove.
     * @return True if track was found and removed.
     */
    bool removeTrack(int trackId);

    /**
     * @brief Removes an arrangement track by index.
     * @param index The track index to remove.
     * @return True if track was found and removed.
     */
    bool removeTrackByIndex(int index);

    /**
     * @brief Gets a track by ID.
     * @param trackId The track ID.
     * @return Pointer to the track, or nullptr if not found.
     */
    NoteNagaArrangementTrack* getTrackById(int trackId);
    const NoteNagaArrangementTrack* getTrackById(int trackId) const;

    /**
     * @brief Gets all arrangement tracks.
     * @return Reference to the tracks vector.
     */
    const std::vector<NoteNagaArrangementTrack*>& getTracks() const { return tracks_; }
    std::vector<NoteNagaArrangementTrack*>& getTracks() { return tracks_; }

    /**
     * @brief Moves a track from one position to another.
     * @param fromIndex Current index.
     * @param toIndex Target index.
     * @return True if successful.
     */
    bool moveTrack(int fromIndex, int toIndex);

    /**
     * @brief Gets the number of tracks.
     * @return Track count.
     */
    size_t getTrackCount() const { return tracks_.size(); }

    // PLAYBACK QUERIES
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Gets all clips that are active at a given tick across all tracks.
     * @param tick The tick to check.
     * @return Vector of pairs (track, clip) for all active clips.
     */
    std::vector<std::pair<NoteNagaArrangementTrack*, const NN_MidiClip_t*>> 
        getActiveClipsAtTick(int tick) const;

    /**
     * @brief Computes the total length of the arrangement in ticks.
     * @return Maximum end tick of all clips.
     */
    int computeMaxTick() const;

    /**
     * @brief Gets the total length of the arrangement.
     * @return Max tick value.
     */
    int getMaxTick() const { return maxTick_; }

    /**
     * @brief Updates the max tick value by scanning all clips.
     */
    void updateMaxTick();

    // LOOP REGION
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Sets the loop region.
     * @param startTick Start tick of the loop.
     * @param endTick End tick of the loop.
     */
    void setLoopRegion(int64_t startTick, int64_t endTick);

    /**
     * @brief Gets the loop start tick.
     * @return Loop start tick.
     */
    int64_t getLoopStartTick() const { return loopStartTick_; }

    /**
     * @brief Gets the loop end tick.
     * @return Loop end tick.
     */
    int64_t getLoopEndTick() const { return loopEndTick_; }

    /**
     * @brief Enables or disables loop region playback.
     * @param enabled True to enable looping.
     */
    void setLoopEnabled(bool enabled);

    /**
     * @brief Checks if loop region is enabled.
     * @return True if looping is enabled.
     */
    bool isLoopEnabled() const { return loopEnabled_; }

    /**
     * @brief Checks if loop region is valid (has positive length).
     * @return True if valid.
     */
    bool hasValidLoopRegion() const { return loopEndTick_ > loopStartTick_; }

    // TEMPO TRACK
    // ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Checks if the arrangement has a tempo track.
     * @return True if tempo track exists.
     */
    bool hasTempoTrack() const { return tempoTrack_ != nullptr; }

    /**
     * @brief Gets the tempo track.
     * @return Pointer to the tempo track, or nullptr if not present.
     */
    NoteNagaTrack* getTempoTrack() const { return tempoTrack_; }

    /**
     * @brief Creates a tempo track for the arrangement.
     * @param defaultBpm Default BPM to initialize the tempo track with.
     * @return Pointer to the created tempo track.
     */
    NoteNagaTrack* createTempoTrack(double defaultBpm = 120.0);

    /**
     * @brief Removes the tempo track from the arrangement.
     */
    void removeTempoTrack();

    /**
     * @brief Gets the effective tempo at a given tick.
     * @param tick The tick to check.
     * @return Tempo in microseconds per quarter note.
     */
    int getEffectiveTempoAtTick(int tick) const;

    /**
     * @brief Gets the effective BPM at a given tick.
     * @param tick The tick to check.
     * @return BPM value.
     */
    double getEffectiveBPMAtTick(int tick) const;

    /**
     * @brief Checks if a new MIDI clip would overlap with any existing clip from the same sequence.
     * 
     * This is used to prevent placing clips from the same sequence on multiple tracks
     * where they would play simultaneously, which would cause audio routing issues.
     * 
     * @param sequenceId The sequence ID of the new clip.
     * @param startTick The start tick of the new clip.
     * @param durationTicks The duration of the new clip.
     * @param excludeTrackId Track ID to exclude from check (for moving within same track), or -1.
     * @param excludeClipId Clip ID to exclude from check (for the clip being moved), or -1.
     * @return True if there would be overlap, false if placement is safe.
     */
    bool wouldClipOverlapSameSequence(int sequenceId, int startTick, int durationTicks, 
                                       int excludeTrackId = -1, int excludeClipId = -1) const;
    
    /**
     * @brief Find the nearest safe position to place a clip without overlapping.
     * @param sequenceId The sequence ID of the clip to place.
     * @param preferredStartTick The preferred start tick.
     * @param durationTicks The duration of the clip.
     * @param excludeClipId Clip ID to exclude (for the clip being moved), or -1.
     * @return The nearest safe start tick, searching forward from preferredStartTick.
     */
    int64_t findNearestSafePosition(int sequenceId, int64_t preferredStartTick, int durationTicks,
                                     int excludeClipId = -1) const;
    
    /**
     * @brief Get all time intervals where clips with a given sequence ID exist.
     * @param sequenceId The sequence ID to check.
     * @param excludeClipId Clip ID to exclude from the list, or -1.
     * @return Vector of pairs (startTick, endTick) representing forbidden zones.
     */
    std::vector<std::pair<int64_t, int64_t>> getForbiddenZonesForSequence(int sequenceId, 
                                                                            int excludeClipId = -1) const;

protected:
    std::vector<NoteNagaArrangementTrack*> tracks_;  ///< All arrangement tracks
    int maxTick_;                                     ///< Cached max tick value
    NoteNagaTrack* tempoTrack_ = nullptr;            ///< Arrangement tempo track
    
    // Loop region
    int64_t loopStartTick_ = 0;    ///< Loop start tick
    int64_t loopEndTick_ = 0;      ///< Loop end tick
    bool loopEnabled_ = false;     ///< Is loop enabled

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    /**
     * @brief Signal emitted when tracks are added/removed/reordered.
     */
    void tracksChanged();

    /**
     * @brief Signal emitted when any clip changes.
     */
    void clipsChanged();

    /**
     * @brief Signal emitted when the arrangement length changes.
     * @param maxTick New max tick value.
     */
    void maxTickChanged(int maxTick);

    /**
     * @brief Signal emitted when loop region changes.
     * @param startTick Loop start tick.
     * @param endTick Loop end tick.
     * @param enabled Is loop enabled.
     */
    void loopRegionChanged(int64_t startTick, int64_t endTick, bool enabled);

    /**
     * @brief Signal emitted when tempo track changes.
     */
    void tempoTrackChanged();
#endif
};