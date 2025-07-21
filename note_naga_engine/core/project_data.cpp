#include <note_naga_engine/core/project_data.h>

#include <algorithm>
#include <iostream>
#include <memory>

#ifndef QT_DEACTIVATED
NoteNagaProject::NoteNagaProject() : QObject(nullptr) {
#else
NoteNagaProject::NoteNagaProject() {
#endif
    // Initialize with empty sequences
    sequences.clear();
    active_sequence = nullptr;
    current_tick = 0;
    max_tick = 0;
}

NoteNagaProject::~NoteNagaProject() {
    for (NoteNagaMidiSeq *seq : sequences) {
        if (seq) delete seq;
    }
    sequences.clear();
}

bool NoteNagaProject::loadProject(const std::string &project_path) {
    if (project_path.empty()) { return false; }
    for (NoteNagaMidiSeq *seq : sequences) {
        if (seq) delete seq;
    }
    this->current_tick = 0;
    this->max_tick = 0;
    this->sequences.clear();
    this->active_sequence = nullptr;

    NoteNagaMidiSeq *sequence = new NoteNagaMidiSeq();
    sequence->loadFromMidi(project_path);
    addSequence(sequence);

#ifndef QT_DEACTIVATED
    connect(sequence, &NoteNagaMidiSeq::metadataChanged, this,
            &NoteNagaProject::sequenceMetadataChanged);
    connect(sequence, &NoteNagaMidiSeq::trackMetadataChanged, this,
            &NoteNagaProject::trackMetaChanged);
#endif
    NN_QT_EMIT(this->projectFileLoaded());
    return true;
}

void NoteNagaProject::addSequence(NoteNagaMidiSeq *sequence) {
    if (sequence) {
        sequences.push_back(sequence);
        if (!this->active_sequence) {
            this->active_sequence = sequence;
            NN_QT_EMIT(activeSequenceChanged(sequence));
        }
    }
}

void NoteNagaProject::removeSequence(NoteNagaMidiSeq *sequence) {
    if (sequence) {
        auto it = std::remove(sequences.begin(), sequences.end(), sequence);
        if (it != sequences.end()) {
            sequences.erase(it, sequences.end());
            // Reset active sequence if it was removed
            if (active_sequence == sequence) {
                active_sequence = nullptr;
                NN_QT_EMIT(activeSequenceChanged(nullptr));
            }
        }
    }
}

int NoteNagaProject::getPPQ() const {
    NoteNagaMidiSeq *active_sequence = getActiveSequence();
    if (active_sequence) { return active_sequence->getPPQ(); }
    return ppq;
}

int NoteNagaProject::getTempo() const {
    NoteNagaMidiSeq *active_sequence = getActiveSequence();
    if (active_sequence) { return active_sequence->getTempo(); }
    return tempo;
}

void NoteNagaProject::setCurrentTick(int tick) {
    if (this->current_tick == tick) return;
    this->current_tick = tick;
    NN_QT_EMIT(currentTickChanged(this->current_tick));
}

bool NoteNagaProject::setActiveSequence(NoteNagaMidiSeq *sequence) {
    if (!sequence) {
        this->active_sequence = nullptr;
        return false;
    }
    for (NoteNagaMidiSeq *seq : this->sequences) {
        if (seq->getId() == sequence->getId()) {
            this->active_sequence = seq;
            NN_QT_EMIT(activeSequenceChanged(seq));
            return true;
        }
    }
    NN_QT_EMIT(activeSequenceChanged(sequence));
    return true;
}

int NoteNagaProject::getMaxTick() const {
    NoteNagaMidiSeq *active_sequence = getActiveSequence();
    if (!active_sequence) { return 0; }
    return active_sequence->getMaxTick();
}

NoteNagaMidiSeq *NoteNagaProject::getSequenceById(int sequence_id) {
    for (NoteNagaMidiSeq *seq : this->sequences) {
        if (seq && seq->getId() == sequence_id) { return seq; }
    }
    return nullptr;
}
