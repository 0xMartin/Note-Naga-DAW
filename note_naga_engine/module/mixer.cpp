#include <note_naga_engine/module/mixer.h>

#include <note_naga_engine/logger.h>
#include <algorithm>

#ifndef QT_DEACTIVATED
NoteNagaMixer::NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path)
    : NoteNagaEngineComponent(), QObject(nullptr)
#else
NoteNagaMixer::NoteNagaMixer(NoteNagaProject *project, const std::string &sf2_path)
    : NoteNagaEngineComponent()
#endif
{
    this->project = project;
    this->sf2_path = sf2_path;
    this->master_volume = 1.0f;
    this->master_min_note = 0;
    this->master_max_note = 127;
    this->master_note_offset = 0;
    this->master_pan = 0.0f;
    this->synthesizers = nullptr;

#ifndef QT_DEACTIVATED
    connect(project, &NoteNagaProject::projectFileLoaded, this,
            &NoteNagaMixer::createDefaultRouting);
#endif

    // Detekce dostupných výstupů pouze z existujících syntetizérů (přes engine později)
    available_outputs = detectOutputs();
    if (!available_outputs.empty()) {
        auto it = std::find(available_outputs.begin(), available_outputs.end(), "fluidsynth");
        if (it != available_outputs.end()) {
            default_output = "fluidsynth";
        } else {
            default_output = available_outputs.front();
        }
    }
    NOTE_NAGA_LOG_INFO("Default output device set on: " + default_output);

    NOTE_NAGA_LOG_INFO("Initialized successfully");
}

NoteNagaMixer::~NoteNagaMixer() {
    close();
}

std::vector<std::string> NoteNagaMixer::detectOutputs() {
    // Zde lze případně zavolat přes engine všechny syntetizéry a získat stringy
    // Prozatím vrací "fluidsynth" jako ukázku
    std::vector<std::string> outputs;
    if (this->synthesizers) {
        for (auto* synth : *this->synthesizers) {
            outputs.push_back(synth->getName());
        }
    } else {
        outputs.push_back("fluidsynth"); // fallback
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
    for (NoteNagaMidiSeq *seq : project->getSequences()) {
        if (!seq) continue;
        std::vector<bool> used_channels(16, false);
        for (NoteNagaTrack *track : seq->getTracks()) {
            if (!track) continue;
            if (auto ch = track->getChannel(); ch.has_value()) {
                int channel = ch.value();
                if (channel >= 0 && channel < 16) used_channels[channel] = true;
            }
        }
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
            routing_entries.push_back(
                NoteNagaRoutingEntry(track, default_output, channel));
        }
    }
    NOTE_NAGA_LOG_INFO("Default routing created with " +
                       std::to_string(routing_entries.size()) + " entries");
    NN_QT_EMIT(routingEntryStackChanged());
}

void NoteNagaMixer::setRouting(const std::vector<NoteNagaRoutingEntry> &entries) {
    routing_entries = entries;
    NOTE_NAGA_LOG_INFO("Routing stack changed, now has " +
                       std::to_string(routing_entries.size()) + " entries");
    NN_QT_EMIT(routingEntryStackChanged());
}

bool NoteNagaMixer::addRoutingEntry(const std::optional<NoteNagaRoutingEntry> &entry) {
    if (entry.has_value()) {
        if (!entry.value().track) return false;
        routing_entries.push_back(entry.value());
        NOTE_NAGA_LOG_INFO("Added routing entry for track Id: " +
                           std::to_string(entry.value().track->getId()) +
                           " on device: " + entry.value().output);
        NN_QT_EMIT(routingEntryStackChanged());
    } else {
        NoteNagaMidiSeq *seq = project->getActiveSequence();
        if (!seq) return false;
        NoteNagaTrack *track = seq->getActiveTrack();
        if (!track) track = seq->getTracks().empty() ? nullptr : seq->getTracks().front();
        if (!track) return false;
        routing_entries.push_back(NoteNagaRoutingEntry(track, default_output, 0));
        NOTE_NAGA_LOG_INFO("Added default routing entry for track Id: " +
                           std::to_string(track->getId()) +
                           " on device: " + default_output);
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
    NOTE_NAGA_LOG_WARNING("Failed to remove routing entry at index: " +
                          std::to_string(index));
    return false;
}

void NoteNagaMixer::clearRoutingTable() {
    routing_entries.clear();
    NOTE_NAGA_LOG_INFO("Routing table cleared");
    NN_QT_EMIT(routingEntryStackChanged());
}

void NoteNagaMixer::playNote(const NN_Note_t &midi_note) {
    // Z důvodu zpětné kompatibility stále volá příslušné syntetizéry podle routingu
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

    // Pro každý routing nalezne správný syntetizér a pošle notu do jeho fronty
    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track != track) continue;
        int note_num = midi_note.note + entry.note_offset + master_note_offset;
        if (note_num < 0 || note_num > 127) continue;
        if (note_num < master_min_note || note_num > master_max_note) continue;
        int velocity = int(std::min(127.0f, std::max(0.0f,
            float(midi_note.velocity.value_or(100)) * entry.volume * master_volume)));
        if (velocity <= 0) continue;

        NN_SynthMessage_t msg;
        msg.note = midi_note;
        msg.note.note = note_num;
        msg.note.velocity = velocity;
        msg.play = true;

        // najdi správný syntetizér podle jména
        for (auto* synth : *this->synthesizers) {
            if (synth->getName() == entry.output) {
                synth->pushToQueue(msg);
            }
        }
    }
}

void NoteNagaMixer::stopNote(const NN_Note_t &midi_note) {
    NoteNagaTrack *track = midi_note.parent;
    if (!track) {
        NOTE_NAGA_LOG_WARNING("Cannot stop note, missing parent track");
        return;
    }
    // podobně jako playNote - předá zprávu správným syntetizérům
    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track != track) continue;
        NN_SynthMessage_t msg;
        msg.note = midi_note;
        msg.play = false;
        for (auto* synth : *this->synthesizers) {
            if (synth->getName() == entry.output) {
                synth->pushToQueue(msg);
            }
        }
    }
}

void NoteNagaMixer::stopAllNotes(NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
    // Předá požadavek všem syntetizérům
    for (auto* synth : *this->synthesizers) {
        synth->stopAllNotes(seq, track);
    }
}

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

bool NoteNagaMixer::isPercussion(NoteNagaTrack *track) const {
    if (!track) return false;
    for (const NoteNagaRoutingEntry &entry : routing_entries) {
        if (entry.track == track && entry.channel == 9) {
            return true;
        }
    }
    return false;
}

void NoteNagaMixer::flushNotes() {
    // Vloží všechny buffered noty do queue příslušných syntetizérů
    for (const auto& msg : note_buffer_) {
        for (auto* synth : *this->synthesizers) {
            synth->pushToQueue(msg);
        }
    }
    note_buffer_.clear();
}

void NoteNagaMixer::onItem(const NN_SynthMessage_t &value) {
    // Odeslat zprávu do všech syntetizérů
    for (auto* synth : *this->synthesizers) {
        synth->pushToQueue(value);
    }
}