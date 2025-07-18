#pragma once

#include <QString>
#include <QColor>
#include <vector>
#include <optional>

// ---------- Channel colors ----------
extern const std::vector<QColor> DEFAULT_CHANNEL_COLORS;

QColor color_blend(const QColor& fg, const QColor& bg, double opacity);

// ---------- GM Instruments ----------
struct GMInstrument
{
    int index;
    QString name;
    QString icon;
};
extern const std::vector<GMInstrument> GM_INSTRUMENTS;

// ---------- Note names ----------
extern const std::vector<QString> NOTE_NAMES;

// ---------- MidiNote ----------
struct MidiNote
{
    // Required for unique identification
    int note_id;

    // Note id (0-127)
    int note;

    // Optional properties
    std::optional<int> start;
    std::optional<int> length;
    std::optional<int> velocity;
    
    MidiNote() : note(0), start(std::nullopt), length(std::nullopt),
                 velocity(std::nullopt), note_id(rand()) {}

    MidiNote(int note_,
             std::optional<int> start_ = std::nullopt,
             std::optional<int> length_ = std::nullopt,
             std::optional<int> velocity_ = std::nullopt,
             std::optional<int> track_ = std::nullopt)
        : note(note_), start(start_), length(length_),
          velocity(velocity_), note_id(rand()) {}
};

// ---------- TrackInfo ----------
struct Track
{
public:
    int track_id;
    std::vector<MidiNote> midi_notes;

    std::optional<int> instrument;
    std::optional<int> channel;

    QString name;
    QColor color;
    bool visible;
    bool muted;
    bool solo;
    float volume;

    Track();

    Track(int track_id,
          const QString &name,
          std::optional<int> instrument,
          std::optional<int> channel);
};

// ---------- Utility functions ----------
QString note_name(int n);
int index_in_octave(int n);
double note_time_ms(const MidiNote &note, int ppq, int tempo);
std::optional<GMInstrument> find_instrument_by_name(const QString &name);
std::optional<GMInstrument> find_instrument_by_index(int index);