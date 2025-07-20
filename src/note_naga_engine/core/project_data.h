#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <vector>
#include <memory>
#include <optional>
#include <QVariant>

#include "types.h"

// ---------- Note Naga Project file ----------
class NoteNagaProject : public QObject {
    Q_OBJECT

public:
    NoteNagaProject();

    bool load_project(const QString& project_path);

    void add_sequence(const std::shared_ptr<NoteNagaMIDISeq>& sequence);
    void remove_sequence(const std::shared_ptr<NoteNagaMIDISeq>& sequence);

    int compute_max_tick();

    int get_ppq() const { return ppq; }
    void set_ppq(int ppq) { this->ppq = ppq; }

    int get_tempo() const { return tempo; }
    void set_tempo(int tempo) { this->tempo = tempo; }

    std::optional<int> get_active_sequence_id() const { return active_sequence_id; }
    void set_active_sequence_id(std::optional<int> sequence_id);
    std::shared_ptr<NoteNagaMIDISeq> get_active_sequence() const;

    std::vector<std::shared_ptr<NoteNagaMIDISeq>> get_sequences() const { return sequences; }

Q_SIGNALS:
    void project_file_loaded_signal();

    void sequence_meta_changed_signal(int sequence_id, const QString& param);
    void active_sequence_changed_signal(int sequence_id);
    
protected:
    std::vector<std::shared_ptr<NoteNagaMIDISeq>> sequences;
    std::optional<int> active_sequence_id;

    int ppq;
    int tempo;
    int current_tick;
    int max_tick;
};