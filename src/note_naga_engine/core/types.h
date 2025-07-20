#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <vector>
#include <optional>
#include <cstdint>

#include "note_naga_api.h"
#include "../io/midi_file.h"

// --- Macro for emitting signals depending on NN_QT_EMIT_ENABLED ---
#ifdef QT_DEACTIVATED
#define NN_QT_EMIT(X)
#else
#define NN_QT_EMIT(X) emit X
#endif

// ---------- Forwards declarations ----------
class NOTE_NAGA_ENGINE_API NoteNagaTrack;
class NOTE_NAGA_ENGINE_API NoteNagaMIDISeq;

// ---------- NoteNagaNote ----------
struct NOTE_NAGA_ENGINE_API NoteNagaNote
{
    // Required for unique identification
    int note_id;

    // Note id (0-127)
    int note;

    // Optional properties
    std::optional<int> start;
    std::optional<int> length;
    std::optional<int> velocity;

    // parent
    NoteNagaTrack *parent;

    NoteNagaNote() : note(0), start(std::nullopt), length(std::nullopt),
                     velocity(std::nullopt), note_id(generate_random_id()), parent(nullptr) {}

    NoteNagaNote(int note_,
                 NoteNagaTrack *parent_,
                 const std::optional<int> &start_ = std::nullopt,
                 const std::optional<int> &length_ = std::nullopt,
                 const std::optional<int> &velocity_ = std::nullopt,
                 const std::optional<int> &track_ = std::nullopt)
        : note(note_), start(start_), length(length_),
          velocity(velocity_), note_id(generate_random_id()), parent(parent_) {}
};

NOTE_NAGA_ENGINE_API double note_time_ms(const NoteNagaNote &note, int ppq, int tempo);
NOTE_NAGA_ENGINE_API int generate_random_id();

// ---------- NoteNagaTrack ----------
class NOTE_NAGA_ENGINE_API NoteNagaTrack : public QObject
{
    Q_OBJECT
public:
    NoteNagaTrack();

    NoteNagaTrack(int track_id,
                  NoteNagaMIDISeq *parent,
                  const QString &name = "Track",
                  const std::optional<int> &instrument = std::nullopt,
                  const std::optional<int> &channel = std::nullopt);
    virtual ~NoteNagaTrack() = default;

    int get_id() const { return track_id; }
    NoteNagaMIDISeq *get_parent() const { return parent; }
    std::vector<NoteNagaNote> get_notes() const { return midi_notes; }
    std::optional<int> get_instrument() const { return instrument; }
    std::optional<int> get_channel() const { return channel; }
    const QString &get_name() const { return name; }
    const QColor &get_color() const { return color; }
    bool is_visible() const { return visible; }
    bool is_muted() const { return muted; }
    bool is_solo() const { return solo; }
    float get_volume() const { return volume; }

    void set_id(int new_id);
    void set_parent(NoteNagaMIDISeq *parent) { this->parent = parent; }
    void set_notes(const std::vector<NoteNagaNote> &notes);
    void set_instrument(std::optional<int> instrument);
    void set_channel(std::optional<int> channel);
    void set_name(const QString &new_name);
    void set_color(const QColor &new_color);
    void set_visible(bool is_visible);
    void set_muted(bool is_muted);
    void set_solo(bool is_solo);
    void set_volume(float new_volume);

Q_SIGNALS:
    void meta_changed_signal(NoteNagaTrack *track, const QString &param);

protected:
    // META data
    int track_id;
    std::optional<int> instrument;
    std::optional<int> channel;
    QString name;
    QColor color;
    bool visible;
    bool muted;
    bool solo;
    float volume;

    // DATA
    std::vector<NoteNagaNote> midi_notes;

    // parent
    NoteNagaMIDISeq *parent;
};

// ---------- Note Naga MIDI file Sequence ----------
class NOTE_NAGA_ENGINE_API NoteNagaMIDISeq : public QObject
{
    Q_OBJECT

public:
    NoteNagaMIDISeq();
    NoteNagaMIDISeq(int sequence_id);
    NoteNagaMIDISeq(int sequence_id, std::vector<NoteNagaTrack*> tracks);
    virtual ~NoteNagaMIDISeq();

    void clear();
    int compute_max_tick();

    void load_from_midi(const QString &midi_file_path);
    std::vector<NoteNagaTrack*> load_type0_tracks(const MidiFile *midiFile);
    std::vector<NoteNagaTrack*> load_type1_tracks(const MidiFile *midiFile);

    int get_id() const { return sequence_id; }
    int get_ppq() const { return ppq; }
    int get_tempo() const { return tempo; }
    int get_max_tick() const { return max_tick; }
    std::optional<int> get_active_track_id() const { return active_track_id; }
    NoteNagaTrack* get_active_track();
    std::optional<int> get_solo_track_id() const { return solo_track_id; }
    std::vector<NoteNagaTrack*> get_tracks() const { return tracks; }
    NoteNagaTrack* get_track_by_id(int track_id);
    MidiFile* get_midi_file() const { return midi_file; }

    void set_id(int new_id);
    void set_ppq(int ppq);
    void set_tempo(int tempo);
    void set_active_track_id(std::optional<int> track_id);
    void set_solo_track_id(std::optional<int> track_id);

Q_SIGNALS:
    void meta_changed_signal(NoteNagaMIDISeq *seq, const QString &param);
    void track_meta_changed_signal(NoteNagaTrack *track, const QString &param);
    void active_track_changed_signal(NoteNagaTrack *track);

protected:
    int sequence_id;

    std::vector<NoteNagaTrack*> tracks;
    std::optional<int> active_track_id;
    std::optional<int> solo_track_id;
    MidiFile* midi_file;

    int ppq;
    int tempo;
    int max_tick;
};

// ---------- Channel colors ----------
struct NOTE_NAGA_ENGINE_API Color {
    uint8_t r, g, b;
    Color(uint8_t rr, uint8_t gg, uint8_t bb)
        : r(rr), g(gg), b(bb) {}

    QColor to_qcolor() const {
        return QColor(r, g, b);
    }

    static Color from_qcolor(const QColor &color) {
        return Color(color.red(), color.green(), color.blue());
    }
};

NOTE_NAGA_ENGINE_API extern const std::vector<QColor> DEFAULT_CHANNEL_COLORS;

NOTE_NAGA_ENGINE_API QColor color_blend(const QColor &fg, const QColor &bg, double opacity);

// ---------- GM Instruments ----------
struct NOTE_NAGA_ENGINE_API GMInstrument
{
    int index;
    QString name;
    QString icon;
};
NOTE_NAGA_ENGINE_API extern const std::vector<GMInstrument> GM_INSTRUMENTS;

NOTE_NAGA_ENGINE_API std::optional<GMInstrument> find_instrument_by_name(const QString &name);
NOTE_NAGA_ENGINE_API std::optional<GMInstrument> find_instrument_by_index(int index);

// ---------- Note names ----------
NOTE_NAGA_ENGINE_API extern const std::vector<QString> NOTE_NAMES;

NOTE_NAGA_ENGINE_API QString note_name(int n);
NOTE_NAGA_ENGINE_API int index_in_octave(int n);