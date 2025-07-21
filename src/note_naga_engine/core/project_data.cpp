#include "project_data.h"

#include <QDebug>
#include <QFileInfo>
#include <QString>
#include <algorithm>
#include <iostream>
#include <memory>

NoteNagaProject::NoteNagaProject() {
    // Initialize with empty sequences
    sequences.clear();
    active_sequence = nullptr;
    current_tick = 0;
    max_tick = 0;
}

NoteNagaProject::~NoteNagaProject() {
    for (NoteNagaMIDISeq *seq : sequences) {
        if (seq) delete seq;
    }
    sequences.clear();
}

bool NoteNagaProject::load_project(const QString &project_path) {
    if (project_path.isEmpty()) { return false; }
    for (NoteNagaMIDISeq *seq : sequences) {
        if (seq) delete seq;
    }
    this->current_tick = 0;
    this->max_tick = 0;
    this->sequences.clear();
    this->active_sequence = nullptr;

    NoteNagaMIDISeq *sequence = new NoteNagaMIDISeq();
    sequence->load_from_midi(project_path);
    add_sequence(sequence);

    connect(sequence, &NoteNagaMIDISeq::meta_changed_signal, this, &NoteNagaProject::sequence_meta_changed_signal);
    connect(sequence, &NoteNagaMIDISeq::track_meta_changed_signal, this, &NoteNagaProject::track_meta_changed_signal);
    NN_QT_EMIT(this->project_file_loaded_signal());
    return true;
}

void NoteNagaProject::add_sequence(NoteNagaMIDISeq *sequence) {
    if (sequence) {
        sequences.push_back(sequence);
        if (!this->active_sequence) {
            this->active_sequence = sequence;
            NN_QT_EMIT(active_sequence_changed_signal(sequence));
        }
    }
}

void NoteNagaProject::remove_sequence(NoteNagaMIDISeq *sequence) {
    if (sequence) {
        auto it = std::remove(sequences.begin(), sequences.end(), sequence);
        if (it != sequences.end()) {
            sequences.erase(it, sequences.end());
            // Reset active sequence if it was removed
            if (active_sequence == sequence) {
                active_sequence = nullptr;
                NN_QT_EMIT(active_sequence_changed_signal(nullptr));
            }
        }
    }
}

int NoteNagaProject::get_ppq() const {
    NoteNagaMIDISeq *active_sequence = get_active_sequence();
    if (active_sequence) { return active_sequence->get_ppq(); }
    return ppq;
}

int NoteNagaProject::get_tempo() const {
    NoteNagaMIDISeq *active_sequence = get_active_sequence();
    if (active_sequence) { return active_sequence->get_tempo(); }
    return tempo;
}

void NoteNagaProject::set_current_tick(int tick) {
    if (this->current_tick == tick) return;
    this->current_tick = tick;
    NN_QT_EMIT(current_tick_changed_signal(this->current_tick));
}

bool NoteNagaProject::set_active_sequence(NoteNagaMIDISeq *sequence) {
    if (!sequence) {
        this->active_sequence = nullptr;
        return false;
    }
    for (NoteNagaMIDISeq *seq : this->sequences) {
        if (seq->get_id() == sequence->get_id()) {
            this->active_sequence = seq;
            NN_QT_EMIT(active_sequence_changed_signal(seq));
            return true;
        }
    }
    NN_QT_EMIT(active_sequence_changed_signal(sequence));
    return true;
}

int NoteNagaProject::get_max_tick() const {
    NoteNagaMIDISeq *active_sequence = get_active_sequence();
    if (!active_sequence) { return 0; }
    return active_sequence->get_max_tick();
}

NoteNagaMIDISeq *NoteNagaProject::get_sequence_by_id(int sequence_id) {
    for (NoteNagaMIDISeq *seq : this->sequences) {
        if (seq && seq->get_id() == sequence_id) { return seq; }
    }
    return nullptr;
}
