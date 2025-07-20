#include "project_data.h"

#include <QFileInfo>
#include <QString>
#include <memory>
#include <iostream>
#include <algorithm>
#include <QDebug>


NoteNagaProject::NoteNagaProject() {
    // Initialize with empty sequences
    sequences.clear();
    active_sequence_id.reset();
    current_tick = 0;
    max_tick = 0;
}

NoteNagaProject::~NoteNagaProject() {
    for (NoteNagaMIDISeq* seq : sequences) {
        if (seq) delete seq;
    }
    sequences.clear();
}

bool NoteNagaProject::load_project(const QString &project_path)
{
    if (project_path.isEmpty()) {
        return false;
    }
    for (NoteNagaMIDISeq* seq : sequences) {
        if (seq) delete seq;
    }
    sequences.clear();
    active_sequence_id.reset();

    NoteNagaMIDISeq* sequence = new NoteNagaMIDISeq();
    sequence->load_from_midi(project_path);
    add_sequence(sequence);

    if (!sequences.empty()) {
        active_sequence_id = sequences[0]->get_id();
    } else {
        active_sequence_id.reset();
    }

    connect(sequence, &NoteNagaMIDISeq::meta_changed_signal, this, &NoteNagaProject::sequence_meta_changed_signal);
    NN_QT_EMIT(this->project_file_loaded_signal());
    return true;
}


void NoteNagaProject::add_sequence(NoteNagaMIDISeq* sequence) {
    if (sequence) {
        sequences.push_back(sequence);
        if (!active_sequence_id.has_value()) {
            active_sequence_id = sequence->get_track_by_id(0)->get_id(); 
        }
    }
}

void NoteNagaProject::remove_sequence(NoteNagaMIDISeq* sequence) {
    if (sequence) {
        auto it = std::remove(sequences.begin(), sequences.end(), sequence);
        if (it != sequences.end()) {
            sequences.erase(it, sequences.end());
            // Reset active sequence if it was removed
            if (active_sequence_id.has_value() && *active_sequence_id == sequence->get_track_by_id(0)->get_id()) {
                active_sequence_id.reset();
            }
        }
    }
}

int NoteNagaProject::compute_max_tick()
{
    NoteNagaMIDISeq* active_sequence = get_active_sequence();
    if (!active_sequence)
    {
        return 0;
    }
    return active_sequence->get_max_tick();
}

int NoteNagaProject::get_ppq() const
{
    NoteNagaMIDISeq* active_sequence = get_active_sequence();
    if (active_sequence)
    {
        return active_sequence->get_ppq();
    }
    return ppq;
}

int NoteNagaProject::get_tempo() const
{
    NoteNagaMIDISeq* active_sequence = get_active_sequence();
    if (active_sequence)
    {
        return active_sequence->get_tempo();
    }
    return tempo;
}

void NoteNagaProject::set_active_sequence_id(std::optional<int> sequence_id) 
{ 
    if (!sequence_id.has_value()) {
        active_sequence_id.reset();
        return;
    }
    if (sequence_id < 0 || sequence_id >= sequences.size()) {
        std::cerr << "Invalid sequence ID: " << sequence_id.value() << std::endl;
        return;
    }
    active_sequence_id = sequence_id;
    NN_QT_EMIT(active_sequence_changed_signal(get_sequence_by_id(sequence_id.value())));
}

void NoteNagaProject::set_current_tick(int tick)
{
    if (this->current_tick == tick)
        return;
    this->current_tick = tick;
    NN_QT_EMIT(current_tick_changed_signal(this->current_tick));
}

NoteNagaMIDISeq* NoteNagaProject::get_active_sequence() const
{
    if (active_sequence_id.has_value()) {
        auto it = std::find_if(sequences.begin(), sequences.end(),
            [this](const NoteNagaMIDISeq*& seq) {
                return seq->get_id() == *active_sequence_id;
            });
        if (it != sequences.end()) {
            return *it;
        }
    }
    return nullptr;
}

NoteNagaMIDISeq *NoteNagaProject::get_sequence_by_id(int sequence_id)
{
    for (NoteNagaMIDISeq* seq : this->sequences) {
        if (seq && seq->get_id() == sequence_id) {
            return seq;
        }
    }
    return nullptr;
}
