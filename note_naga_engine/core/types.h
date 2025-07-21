#pragma once

#ifndef QT_DEACTIVATED
#include <QObject>
#include <QColor>
#endif

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

#include "../note_naga_api.h"
#include "../io/midi_file.h"

/*******************************************************************************************************/
// Macros for emitting signals depending on NN_QT_EMIT_ENABLED
/*******************************************************************************************************/
// #define QT_DEACTIVATED

#ifdef QT_DEACTIVATED
#define NN_QT_EMIT(X)
#else
#define NN_QT_EMIT(X) emit X
#endif

/*******************************************************************************************************/
// Channel Colors
/*******************************************************************************************************/

struct NOTE_NAGA_ENGINE_API NNColor {
    uint8_t red, green, blue;

    NNColor() : red(0), green(0), blue(0) {}
    NNColor(uint8_t rr, uint8_t gg, uint8_t bb)
        : red(rr), green(gg), blue(bb) {}

    bool operator==(const NNColor &other) const {
        return red == other.red && green == other.green && blue == other.blue;
    }

#ifndef QT_DEACTIVATED
    QColor toQColor() const {
        return QColor(red, green, blue);
    }

    static NNColor fromQColor(const QColor &color) {
        return NNColor(color.red(), color.green(), color.blue());
    }
#endif
};

NOTE_NAGA_ENGINE_API extern const std::vector<NNColor> DEFAULT_CHANNEL_COLORS;

NOTE_NAGA_ENGINE_API NNColor color_blend(const NNColor &fg, const NNColor &bg, double opacity);

/*******************************************************************************************************/
// Forwards declarations
/*******************************************************************************************************/
class NOTE_NAGA_ENGINE_API NoteNagaTrack;
class NOTE_NAGA_ENGINE_API NoteNagaMIDISeq;

/*******************************************************************************************************/
// Unique ID generation
/*******************************************************************************************************/

NOTE_NAGA_ENGINE_API unsigned long generate_unique_note_id();
NOTE_NAGA_ENGINE_API int generate_unique_seq_id();

/*******************************************************************************************************/
// Note Naga Note
/*******************************************************************************************************/

struct NOTE_NAGA_ENGINE_API NoteNagaNote
{
    // Required for unique identification
    unsigned long id;

    // Note id (0-127)
    int note;

    // Optional properties
    std::optional<int> start;
    std::optional<int> length;
    std::optional<int> velocity;

    // parent
    NoteNagaTrack *parent;

    NoteNagaNote() : id(generate_unique_note_id()), note(0), start(std::nullopt),
                     length(std::nullopt), velocity(std::nullopt), parent(nullptr) {}

    NoteNagaNote(unsigned long note_,
                 NoteNagaTrack *parent_,
                 const std::optional<int> &start_ = std::nullopt,
                 const std::optional<int> &length_ = std::nullopt,
                 const std::optional<int> &velocity_ = std::nullopt,
                 const std::optional<int> &track_ = std::nullopt)
        : id(generate_unique_note_id()), note(note_), start(start_),
          length(length_), velocity(velocity_), parent(parent_) {}
};

NOTE_NAGA_ENGINE_API double note_time_ms(const NoteNagaNote &note, int ppq, int tempo);

/*******************************************************************************************************/
// Note Naga Track
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaTrack : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaTrack {
#endif

public:
    NoteNagaTrack();

    NoteNagaTrack(int track_id,
                  NoteNagaMIDISeq *parent,
                  const std::string &name = "Track",
                  const std::optional<int> &instrument = std::nullopt,
                  const std::optional<int> &channel = std::nullopt);
    virtual ~NoteNagaTrack() = default;

    int get_id() const { return track_id; }
    NoteNagaMIDISeq *get_parent() const { return parent; }
    std::vector<NoteNagaNote> get_notes() const { return midi_notes; }
    std::optional<int> get_instrument() const { return instrument; }
    std::optional<int> get_channel() const { return channel; }
    const std::string &get_name() const { return name; }
    const NNColor &get_color() const { return color; }
    bool is_visible() const { return visible; }
    bool is_muted() const { return muted; }
    bool is_solo() const { return solo; }
    float get_volume() const { return volume; }

    void set_id(int new_id);
    void set_parent(NoteNagaMIDISeq *parent) { this->parent = parent; }
    void set_notes(const std::vector<NoteNagaNote> &notes);
    void set_instrument(std::optional<int> instrument);
    void set_channel(std::optional<int> channel);
    void set_name(const std::string &new_name);
    void set_color(const NNColor &new_color);
    void set_visible(bool is_visible);
    void set_muted(bool is_muted);
    void set_solo(bool is_solo);
    void set_volume(float new_volume);

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    void meta_changed_signal(NoteNagaTrack *track, const std::string &param);
#endif

protected:
    // META data
    int track_id;
    std::optional<int> instrument;
    std::optional<int> channel;
    std::string name;
    NNColor color;
    bool visible;
    bool muted;
    bool solo;
    float volume;

    // DATA
    std::vector<NoteNagaNote> midi_notes;

    // parent
    NoteNagaMIDISeq *parent;
};

/*******************************************************************************************************/
// Note Naga MIDI Sequence
/*******************************************************************************************************/

#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaMIDISeq : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaMIDISeq {
#endif

public:
    NoteNagaMIDISeq();
    NoteNagaMIDISeq(int sequence_id);
    NoteNagaMIDISeq(int sequence_id, std::vector<NoteNagaTrack*> tracks);
    virtual ~NoteNagaMIDISeq();

    void clear();
    int compute_max_tick();

    void load_from_midi(const std::string &midi_file_path);
    std::vector<NoteNagaTrack*> load_type0_tracks(const MidiFile *midiFile);
    std::vector<NoteNagaTrack*> load_type1_tracks(const MidiFile *midiFile);

    int get_id() const { return sequence_id; }
    int get_ppq() const { return ppq; }
    int get_tempo() const { return tempo; }
    int get_max_tick() const { return max_tick; }
    NoteNagaTrack* get_active_track() const { return active_track; } 
    NoteNagaTrack* get_solo_track() const { return solo_track; }
    std::vector<NoteNagaTrack*> get_tracks() const { return tracks; }
    NoteNagaTrack* get_track_by_id(int track_id);
    MidiFile* get_midi_file() const { return midi_file; }

    void set_id(int new_id);
    void set_ppq(int ppq);
    void set_tempo(int tempo);
    void set_active_track(NoteNagaTrack *track);
    void set_solo_track(NoteNagaTrack *track);

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    void meta_changed_signal(NoteNagaMIDISeq *seq, const std::string &param);
    void track_meta_changed_signal(NoteNagaTrack *track, const std::string &param);
    void active_track_changed_signal(NoteNagaTrack *track);
#endif

protected:
    int sequence_id;

    std::vector<NoteNagaTrack*> tracks;
    NoteNagaTrack *active_track;
    NoteNagaTrack *solo_track;
    MidiFile* midi_file;

    int ppq;
    int tempo;
    int max_tick;
};

/*******************************************************************************************************/
// General MIDI Instruments Utils
/*******************************************************************************************************/

struct NOTE_NAGA_ENGINE_API GMInstrument
{
    int index;
    std::string name;
    std::string icon;
};
NOTE_NAGA_ENGINE_API extern const std::vector<GMInstrument> GM_INSTRUMENTS;

NOTE_NAGA_ENGINE_API std::optional<GMInstrument> find_instrument_by_name(const std::string &name);
NOTE_NAGA_ENGINE_API std::optional<GMInstrument> find_instrument_by_index(int index);

/*******************************************************************************************************/
// Note Names Utils
/*******************************************************************************************************/

NOTE_NAGA_ENGINE_API extern const std::vector<std::string> NOTE_NAMES;

NOTE_NAGA_ENGINE_API std::string note_name(int n);
NOTE_NAGA_ENGINE_API int index_in_octave(int n);