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

bool NoteNagaProject::load_project(const QString &project_path)
{
    // Check for empty path
    if (project_path.isEmpty()) 
    {
        return false;
    }

    // Create a new MIDI sequence
    std::shared_ptr<NoteNagaMIDISeq> sequence = std::make_shared<NoteNagaMIDISeq>();
    sequence->load_from_midi(project_path);
    add_sequence(sequence);

    // Set the active sequence to the first one
    if (!sequences.empty()) {
        active_sequence_id = sequences[0]->get_id();
    } else {
        active_sequence_id.reset();
    }

    // signals
    connect(sequence.get(), &NoteNagaMIDISeq::meta_changed_signal, this, &NoteNagaProject::sequence_meta_changed_signal);

    NN_QT_EMIT(this->project_file_loaded_signal());

    return true;
}

void NoteNagaProject::add_sequence(const std::shared_ptr<NoteNagaMIDISeq>& sequence) {
    if (sequence) {
        sequences.push_back(sequence);
        if (!active_sequence_id.has_value()) {
            active_sequence_id = sequence->get_track_by_id(0)->get_id(); 
        }
    }
}

void NoteNagaProject::remove_sequence(const std::shared_ptr<NoteNagaMIDISeq>& sequence) {
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
    // implement
    return 0.0;
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
    NN_QT_EMIT(active_sequence_changed_signal(sequence_id.value()));
}

std::shared_ptr<NoteNagaMIDISeq> NoteNagaProject::get_active_sequence() const
{
    if (active_sequence_id.has_value()) {
        auto it = std::find_if(sequences.begin(), sequences.end(),
            [this](const std::shared_ptr<NoteNagaMIDISeq>& seq) {
                return seq->get_id() == *active_sequence_id;
            });
        if (it != sequences.end()) {
            return *it;
        }
    }
    return nullptr;
}
