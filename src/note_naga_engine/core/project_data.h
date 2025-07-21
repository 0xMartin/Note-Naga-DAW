#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <vector>
#include <memory>
#include <optional>
#include <QVariant>

#include "../note_naga_api.h"
#include "types.h"
 
// ---------- Note Naga Project file ----------
class NOTE_NAGA_ENGINE_API NoteNagaProject : public QObject {
    Q_OBJECT

public:
    NoteNagaProject();
    virtual ~NoteNagaProject();

    bool load_project(const QString& project_path);

    void add_sequence(NoteNagaMIDISeq* sequence);
    void remove_sequence(NoteNagaMIDISeq* sequence);

    int get_ppq() const;
    int get_tempo() const;
    int get_current_tick() const { return current_tick; }
    int get_max_tick() const;
    NoteNagaMIDISeq* get_active_sequence() const { return this->active_sequence; }
    NoteNagaMIDISeq* get_sequence_by_id(int sequence_id);
    std::vector<NoteNagaMIDISeq*> get_sequences() const { return sequences; }

    void set_ppq(int ppq) { this->ppq = ppq; }
    void set_tempo(int tempo) { this->tempo = tempo; }
    void set_current_tick(int tick);
    bool set_active_sequence(NoteNagaMIDISeq* sequence);

Q_SIGNALS:
    void project_file_loaded_signal();
    void current_tick_changed_signal(int tick);
    void sequence_meta_changed_signal(NoteNagaMIDISeq *seq, const QString& param);
    void track_meta_changed_signal(NoteNagaTrack *track, const QString& param);
    void active_sequence_changed_signal(NoteNagaMIDISeq *seq);
    
protected:
    std::vector<NoteNagaMIDISeq*> sequences;
    NoteNagaMIDISeq* active_sequence;

    int ppq;
    int tempo;
    int current_tick;
    int max_tick;
};