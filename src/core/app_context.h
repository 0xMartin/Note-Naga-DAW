#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <vector>
#include <memory>
#include <optional>
#include <QVariant>

#include "midi_file.h"
#include "shared.h"

// ---------- AppContext Singleton ----------
class AppContext : public QObject {
    Q_OBJECT

public:
    static AppContext* instance();

    // Signals
    Q_SIGNAL void midi_file_loaded_signal();
    Q_SIGNAL void track_meta_changed_signal(int track_id);
    Q_SIGNAL void selected_track_changed_signal(int track_id);
    Q_SIGNAL void playing_note_signal(const MidiNote& note, int track_id);
    Q_SIGNAL void mixer_playing_note_signal(const MidiNote& note, const QString& device_name, int channel);

    // State
    std::vector<std::shared_ptr<Track>> tracks;
    int ppq;
    int tempo;
    std::optional<int> active_track_id;
    std::shared_ptr<MidiFile> midi_file; 
    int current_tick;
    int max_tick;

    void clear();
    std::shared_ptr<Track> get_track_by_id(int track_id);
    void set_track_attribute(int track_id, const QString& attr, const QVariant& value);
    int compute_max_tick();

    void load_from_midi(const QString& midi_file_path);
    std::vector<std::shared_ptr<Track>> load_type0_tracks(const MidiFile& midiFile);
    std::vector<std::shared_ptr<Track>> load_type1_tracks(const MidiFile& midiFile);

private:
    explicit AppContext(QObject* parent = nullptr);
    Q_DISABLE_COPY(AppContext)
};