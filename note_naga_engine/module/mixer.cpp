#include <note_naga_engine/module/mixer.h>

#include <algorithm>
#include <note_naga_engine/logger.h>

#ifndef QT_DEACTIVATED
NoteNagaMixer::NoteNagaMixer(NoteNagaRuntimeData *project,
                             std::vector<NoteNagaSynthesizer *> *synthesizers)
    : QObject(nullptr), AsyncQueueComponent()
#else
NoteNagaMixer::NoteNagaMixer(NoteNagaRuntimeData *project, std::vector<NoteNagaSynthesizer *> *synthesizers)
    : AsyncQueueComponent()
#endif
{
    this->project = project;
    this->synthesizers = synthesizers;
    this->master_volume = 1.0f;
    this->master_pan = 0.0f;
    

#ifndef QT_DEACTIVATED
    connect(project, &NoteNagaRuntimeData::projectFileLoaded, this,
            &NoteNagaMixer::createDefaultRouting);
#endif

    // Detect available outputs and set default
    available_outputs = detectOutputs();
    if (available_outputs.empty()) {
        default_output = TRACK_ROUTING_ENTRY_ANY_DEVICE; // Default to "any device"
    } else {
        default_output = available_outputs.front();
    }
    NOTE_NAGA_LOG_INFO("Default output device set on: " + default_output);

    NOTE_NAGA_LOG_INFO("Initialized successfully");
}

NoteNagaMixer::~NoteNagaMixer() { close(); }

void NoteNagaMixer::setSynthVectorRef(std::vector<NoteNagaSynthesizer *> *synthesizers) {
    this->synthesizers = synthesizers;
    this->detectOutputs();
}

std::vector<std::string> NoteNagaMixer::detectOutputs() {
    std::vector<std::string> outputs;
    if (this->synthesizers) {
        for (auto *synth : *this->synthesizers) {
            outputs.push_back(synth->getName());
        }
    }
    return outputs;
}

void NoteNagaMixer::close() {
    NOTE_NAGA_LOG_INFO("Closing and cleaning up mixer resources...");
    // Syntetizéry čistí samy sebe (RAII)
    available_outputs.clear();
    routing_entries.clear();
    note_buffer_.clear();
    NOTE_NAGA_LOG_INFO("Closed and cleaned up resources successfully");
}

void NoteNagaMixer::createDefaultRouting() {
    routing_entries.clear();
    if (!project) return;

    // use active sequence
    NoteNagaMidiSeq *seq = project->getActiveSequence();
    if (!seq) return;

    std::vector<bool> used_channels(16, false);

    // Get and mark used channels for each track in sequence
    for (NoteNagaTrack *track : seq->getTracks()) {
        if (!track) continue;
        if (auto ch = track->getChannel(); ch.has_value()) {
            int channel = ch.value();
            if (channel >= 0 && channel < 16) used_channels[channel] = true;
        }
    }

    // Create default routing entries for each track
    for (NoteNagaTrack *track : seq->getTracks()) {
        if (!track) continue;
        int channel;
        if (auto ch = track->getChannel(); ch.has_value()) {
            channel = ch.value();
        } else {
            auto it = std::find(used_channels.begin(), used_channels.end(), false);
            if (it != used_channels.end()) {
                channel = std::distance(used_channels.begin(), it);
                used_channels[channel] = true;
            } else {
                channel = 15;
            }
        }
        routing_entries.push_back(NoteNagaRoutingEntry(track, default_output, channel));
    }

    NOTE_NAGA_LOG_INFO("Default routing created with " + std::to_string(routing_entries.size()) +
                       " entries");
    NN_QT_EMIT(routingEntryStackChanged());
}

void NoteNagaMixer::setRouting(const std::vector<NoteNagaRoutingEntry> &entries) {
    routing_entries = entries;
    NOTE_NAGA_LOG_INFO("Routing stack changed, now has " + std::to_string(routing_entries.size()) +
                       " entries");
    NN_QT_EMIT(routingEntryStackChanged());
}

bool NoteNagaMixer::addRoutingEntry(const std::optional<NoteNagaRoutingEntry> &entry) {
    if (entry.has_value()) {
        // add entry if it has a valid track
        if (!entry.value().track) return false;
        routing_entries.push_back(entry.value());
        NOTE_NAGA_LOG_INFO(
            "Added routing entry for track Id: " + std::to_string(entry.value().track->getId()) +
            " on device: " + entry.value().output);
        NN_QT_EMIT(routingEntryStackChanged());
    } else {
        // add default entry for active track (active track in active sequence)
        NoteNagaMidiSeq *seq = project->getActiveSequence();
        if (!seq) return false;
        NoteNagaTrack *track = seq->getActiveTrack();
        if (!track) track = seq->getTracks().empty() ? nullptr : seq->getTracks().front();
        if (!track) return false;
        routing_entries.push_back(NoteNagaRoutingEntry(track, default_output, 0));
        NOTE_NAGA_LOG_INFO("Added default routing entry for track Id: " +
                           std::to_string(track->getId()) + " on device: " + default_output);
        NN_QT_EMIT(routingEntryStackChanged());
    }
    return true;
}

bool NoteNagaMixer::removeRoutingEntry(int index) {
    if (index >= 0 && index < int(routing_entries.size())) {
        routing_entries.erase(routing_entries.begin() + index);
        NOTE_NAGA_LOG_INFO("Removed routing entry at index: " + std::to_string(index));
        NN_QT_EMIT(routingEntryStackChanged());
        return true;
    }
    NOTE_NAGA_LOG_WARNING("Failed to remove routing entry at index: " + std::to_string(index));
    return false;
}

int NoteNagaMixer::removeRoutingEntryForTrack(NoteNagaTrack *track) {
    if (!track) return -1;

    // remove all entries for track
    int cnt = 0;
    for (auto it = routing_entries.begin(); it != routing_entries.end();) {
        if (it->track == track) {
            it = routing_entries.erase(it);
            NOTE_NAGA_LOG_INFO("Removed routing entry for track Id: " + std::to_string(track->getId()));
            ++cnt;
        } else {
            ++it;
        }
    }
    NN_QT_EMIT(routingEntryStackChanged());
    return cnt;
}

void NoteNagaMixer::clearRoutingTable() {
    routing_entries.clear();
    NOTE_NAGA_LOG_INFO("Routing table cleared");
    NN_QT_EMIT(routingEntryStackChanged());
}

bool NoteNagaMixer::isPercussion(NoteNagaTrack *track) const {
    if (!track) return false;
    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track == track && entry.channel == 9) { return true; }
    }
    return false;
}

/*******************************************************************************************************/
// Track control
/*******************************************************************************************************/

void NoteNagaMixer::muteTrack(NoteNagaTrack *track, bool mute) {
    if (!track) return;
    track->setMuted(mute);
    stopAllNotes(track->getParent(), track);
}

void NoteNagaMixer::soloTrack(NoteNagaTrack *track, bool solo) {
    if (!track) return;
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq) return;
    track->setSolo(solo);

    if (solo) {
        seq->setSoloTrack(track);
        for (NoteNagaTrack *t : seq->getTracks()) {
            if (t && t != track) {
                t->setSolo(false);
                stopAllNotes(seq, t);
            }
        }
    } else {
        seq->setSoloTrack(nullptr);
    }
}

/*******************************************************************************************************/
// Note play/stop methods
/*******************************************************************************************************/

void NoteNagaMixer::flushNotes() {
    // for each synth, push all buffered notes
    for (auto &pair : note_buffer_) {
        // get synth name and vector of messages
        const std::string &synth_name = pair.first;
        std::vector<NN_SynthMessage_t> &vec = pair.second;

        // push to synth queue
        for (auto *synth : *this->synthesizers) {
            if (synth->getName() == synth_name || synth_name == TRACK_ROUTING_ENTRY_ANY_DEVICE) {
                for (auto &msg : vec) {
                    synth->pushToQueue(msg);
                }
            }
        }
    }

    // signals
#ifndef QT_DEACTIVATED
    for (auto &pair : note_buffer_) {
        const std::string &synth_name = pair.first;
        for (const NN_SynthMessage_t &msg : pair.second) {
            NN_QT_EMIT(noteOutSignal(msg.note, synth_name, msg.channel));
        }
    }
#endif

    // Clear the buffer after flushing
    note_buffer_.clear();
}

void NoteNagaMixer::onItem(const NN_MixerMessage_t &value) {
    // This method is called when a new item is dequeued from the component's queue.
    // It processes the MIDI note play/stop messages.
    if (value.play) {
        // Handle note play
        playNote(value.note);
    } else {
        // Handle note stop
        stopNote(value.note);
    }
    // flush notes if requested
    if (value.flush) flushNotes();
}

void NoteNagaMixer::playNote(const NN_Note_t &midi_note) {
    NoteNagaTrack *track = midi_note.parent;
    if (!track) {
        NOTE_NAGA_LOG_WARNING("Cannot play note, missing parent track");
        return;
    }
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq) {
        NOTE_NAGA_LOG_WARNING("Cannot play note, missing parent sequence");
        return;
    }
    NN_QT_EMIT(noteInSignal(midi_note));

    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track != track) continue;
        int note_num = midi_note.note + entry.note_offset;
        if (note_num < 0 || note_num > 127) continue;
        int velocity = int(std::min(127.0f, std::max(0.0f, float(midi_note.velocity.value_or(100)) *
                                                               entry.volume * master_volume)));
        if (velocity <= 0) continue;

        NN_SynthMessage_t msg;
        msg.note = midi_note;
        msg.note.note = note_num;
        msg.note.velocity = velocity;
        // Calculate pan: note pan (0-127 to -1..1) + entry pan + master pan
        float note_pan = 0.0f;
        if (midi_note.pan.has_value()) {
            // Convert from 0-127 range to -1.0 to 1.0 range
            note_pan = (midi_note.pan.value() - 64) / 64.0f;
        }
        msg.pan = note_pan + entry.pan + master_pan; 
        msg.channel = entry.channel;
        msg.play = true;

        // store the message in the buffer for this output
        note_buffer_[entry.output].push_back(msg);
    }
}

void NoteNagaMixer::stopNote(const NN_Note_t &midi_note) {
    NoteNagaTrack *track = midi_note.parent;
    if (!track) {
        NOTE_NAGA_LOG_WARNING("Cannot stop note, missing parent track");
        return;
    }
    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track != track) continue;
        NN_SynthMessage_t msg;
        msg.note = midi_note;
        msg.play = false;
        note_buffer_[entry.output].push_back(msg);
    }
}

void NoteNagaMixer::stopAllNotes(NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
    // Předá požadavek všem syntetizérům
    for (auto *synth : *this->synthesizers) {
        synth->stopAllNotes(seq, track);
    }
}

void NoteNagaMixer::setMasterPan(float pan) {
    master_pan.store(pan);
    // Immediately update pan on all synthesizers for currently playing notes
    for (auto *synth : *this->synthesizers) {
        synth->setMasterPan(pan);
    }
}