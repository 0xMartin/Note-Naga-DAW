#include <note_naga_engine/core/runtime_data.h>

#include <algorithm>
#include <note_naga_engine/logger.h>

NoteNagaRuntimeData::NoteNagaRuntimeData()
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
    // Initialize with empty sequences
    sequences.clear();
    active_sequence = nullptr;
    arrangement_ = new NoteNagaArrangement();
    current_tick = 0;
    current_arrangement_tick_ = 0;
    max_tick = 0;
    ppq = 480;           // Default PPQ
    tempo = 500000;      // Default 120 BPM = 500000 microseconds per quarter note
    
#ifndef QT_DEACTIVATED
    // Connect arrangement signals
    connect(arrangement_, &NoteNagaArrangement::tracksChanged, this,
            &NoteNagaRuntimeData::arrangementChanged);
    connect(arrangement_, &NoteNagaArrangement::clipsChanged, this,
            &NoteNagaRuntimeData::arrangementChanged);
#endif
    
    NOTE_NAGA_LOG_INFO("Runtime data initialized");
}

NoteNagaRuntimeData::~NoteNagaRuntimeData() {
    for (NoteNagaMidiSeq *seq : sequences) {
        if (seq) delete seq;
    }
    sequences.clear();
    
    if (arrangement_) {
        delete arrangement_;
        arrangement_ = nullptr;
    }
    
    NOTE_NAGA_LOG_INFO("Runtime data destroyed");
}

bool NoteNagaRuntimeData::loadProject(const std::string &project_path) {
    if (project_path.empty()) { 
        NOTE_NAGA_LOG_ERROR("Project path is empty, cannot load project");
        return false;
    }
    
    if (!sequences.empty()) {
        NOTE_NAGA_LOG_INFO("Cleaning existing project data before loading new project");
    }
    for (NoteNagaMidiSeq *seq : sequences) {
        if (seq) delete seq;
    }
    this->current_tick = 0;
    this->max_tick = 0;
    this->sequences.clear();
    this->active_sequence = nullptr;

    NoteNagaMidiSeq *sequence = new NoteNagaMidiSeq();
    sequence->loadFromMidi(project_path);
    addSequence(sequence);  // This now sets up all signal connections

    NN_QT_EMIT(this->projectFileLoaded());
    NOTE_NAGA_LOG_INFO("Project loaded from: " + project_path);
    return true;
}

void NoteNagaRuntimeData::addSequence(NoteNagaMidiSeq *sequence) {
    if (sequence) {
        sequences.push_back(sequence);
        
#ifndef QT_DEACTIVATED
        // Connect sequence signals to project signals
        connect(sequence, &NoteNagaMidiSeq::metadataChanged, this,
                &NoteNagaRuntimeData::sequenceMetadataChanged);
        connect(sequence, &NoteNagaMidiSeq::trackMetadataChanged, this,
                &NoteNagaRuntimeData::trackMetaChanged);
        // Only emit activeSequenceTrackListChanged if this is the active sequence
        connect(sequence, &NoteNagaMidiSeq::trackListChanged, this, [this, sequence](){
            if (this->active_sequence == sequence) {
                this->activeSequenceTrackListChanged(sequence);
            }
        });
#endif
        
        // Emit signal for sequence list change
        NN_QT_EMIT(sequenceListChanged());
        
        if (!this->active_sequence) {
            this->active_sequence = sequence;
            NN_QT_EMIT(activeSequenceChanged(sequence));
        }
        NOTE_NAGA_LOG_INFO("Added MIDI sequence with ID: " + std::to_string(sequence->getId()));
    }
}

void NoteNagaRuntimeData::removeSequence(NoteNagaMidiSeq *sequence) {
    if (sequence) {
        auto it = std::remove(sequences.begin(), sequences.end(), sequence);
        if (it != sequences.end()) {
            sequences.erase(it, sequences.end());
            
            // Emit signal for sequence list change
            NN_QT_EMIT(sequenceListChanged());
            
            // Reset active sequence if it was removed
            if (active_sequence == sequence) {
                active_sequence = nullptr;
                NN_QT_EMIT(activeSequenceChanged(nullptr));
            }
            NOTE_NAGA_LOG_INFO("Removed MIDI sequence with ID: " + std::to_string(sequence->getId()));
        } else {
            NOTE_NAGA_LOG_WARNING("Attempted to remove a sequence that does not exist in the project");
        }
    }
}

int NoteNagaRuntimeData::getPPQ() const {
    NoteNagaMidiSeq *active_sequence = getActiveSequence();
    if (active_sequence) { return active_sequence->getPPQ(); }
    return ppq;
}

int NoteNagaRuntimeData::getTempo() const {
    NoteNagaMidiSeq *active_sequence = getActiveSequence();
    if (active_sequence) { return active_sequence->getTempo(); }
    return tempo;
}

void NoteNagaRuntimeData::setCurrentTick(int tick) {
    if (this->current_tick == tick) return;
    this->current_tick = tick;
    NN_QT_EMIT(currentTickChanged(this->current_tick));
}

bool NoteNagaRuntimeData::setActiveSequence(NoteNagaMidiSeq *sequence) {
    if (sequence == this->active_sequence) {
        NOTE_NAGA_LOG_WARNING("Active sequence is already set to the requested sequence");
        return false;
    }

    if (!sequence) {
        this->active_sequence = nullptr;
        NOTE_NAGA_LOG_INFO("Active sequence cleared");
        NN_QT_EMIT(activeSequenceChanged(sequence));
        return true;
    }

    for (NoteNagaMidiSeq *seq : this->sequences) {
        if (seq->getId() == sequence->getId()) {
            this->active_sequence = seq;
            NOTE_NAGA_LOG_INFO("Active sequence set to ID: " + std::to_string(seq->getId()));
            NN_QT_EMIT(activeSequenceChanged(seq));
            return true;
        }
    }

    NOTE_NAGA_LOG_WARNING("Could not set active sequence, sequence not found in project");
    return false;
}

int NoteNagaRuntimeData::getMaxTick() const {
    NoteNagaMidiSeq *active_sequence = getActiveSequence();
    if (!active_sequence) { return 0; }
    return active_sequence->getMaxTick();
}

NoteNagaMidiSeq *NoteNagaRuntimeData::getSequenceById(int sequence_id) {
    for (NoteNagaMidiSeq *seq : this->sequences) {
        if (seq && seq->getId() == sequence_id) { return seq; }
    }
    return nullptr;
}

void NoteNagaRuntimeData::setCurrentArrangementTick(int tick) {
    if (this->current_arrangement_tick_ == tick) return;
    this->current_arrangement_tick_ = tick;
    NN_QT_EMIT(currentArrangementTickChanged(tick));
}

int NoteNagaRuntimeData::getArrangementMaxTick() const {
    if (arrangement_) {
        return arrangement_->getMaxTick();
    }
    return 0;
}
