#include "ai_command_executor.h"
#include "../editor/midi_editor_widget.h"

#include <note_naga_engine/core/types.h>
#include <QJsonObject>

namespace NoteNagaAI {

// ============================================================================
// AiCommandExecutor
// ============================================================================

AiCommandExecutor::AiCommandExecutor(MidiEditorWidget *editor, NoteNagaMidiSeq *sequence)
    : m_editor(editor), m_sequence(sequence)
{
}

ExecutionResult AiCommandExecutor::execute(const AiResponse &response) {
    ExecutionResult result;
    
    if (!m_editor || !m_sequence) {
        result.errorMessage = QObject::tr("Editor or sequence not available");
        return result;
    }
    
    if (!response.isValid()) {
        result.errorMessage = response.parseError;
        return result;
    }
    
    // Create compound command for undo
    auto *compound = new AiModificationCommand();
    compound->setRefreshCallback([this]() {
        if (m_editor) {
            m_editor->refreshAll();
        }
    });
    compound->setTrackListCallback(m_trackListChangedCallback);
    
    // Execute all commands
    for (const auto &cmd : response.commands) {
        QString error;
        if (executeCommand(cmd, compound, error)) {
            result.commandsExecuted++;
        } else {
            result.commandsFailed++;
            result.warnings.append(error);
        }
    }
    
    // Add compound command to undo stack if any operations were performed
    if (!compound->isEmpty()) {
        UndoManager *undoManager = m_editor->getUndoManager();
        if (undoManager) {
            undoManager->addCommandWithoutExecute(compound);
        } else {
            delete compound;
        }
        result.success = true;
    } else {
        delete compound;
        if (result.commandsFailed > 0 && result.commandsExecuted == 0) {
            result.errorMessage = QObject::tr("All commands failed to execute");
        }
    }
    
    // Refresh UI
    m_editor->refreshAll();
    if (m_trackListChangedCallback) {
        m_trackListChangedCallback();
    }
    
    // Recompute max tick
    m_sequence->computeMaxTick();
    
    return result;
}

bool AiCommandExecutor::executeCommand(const AiCommand &cmd, AiModificationCommand *compound, QString &error) {
    if (!cmd.isValid()) {
        error = cmd.error;
        return false;
    }
    
    switch (cmd.type) {
        case AiCommandType::AddNote:
            return executeAddNote(cmd.params, compound, error);
        case AiCommandType::RemoveNote:
            return executeRemoveNote(cmd.params, compound, error);
        case AiCommandType::ModifyNote:
            return executeModifyNote(cmd.params, compound, error);
        case AiCommandType::AddTrack:
            return executeAddTrack(cmd.params, compound, error);
        case AiCommandType::RemoveTrack:
            return executeRemoveTrack(cmd.params, compound, error);
        case AiCommandType::ModifyTrack:
            return executeModifyTrack(cmd.params, compound, error);
        case AiCommandType::ClearTrack:
            return executeClearTrack(cmd.params, compound, error);
        case AiCommandType::SetTempo:
            return executeSetTempo(cmd.params, compound, error);
        case AiCommandType::AddTempoEvent:
            return executeAddTempoEvent(cmd.params, compound, error);
        case AiCommandType::RemoveTempoEvent:
            return executeRemoveTempoEvent(cmd.params, compound, error);
        default:
            error = QObject::tr("Unknown command type");
            return false;
    }
}

bool AiCommandExecutor::executeAddNote(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    NN_Note_t note;
    note.note = params["note"].toInt(60);
    note.start = params["start"].toInt(0);
    note.length = params["length"].toInt(480);
    note.velocity = params["velocity"].toInt(100);
    if (params.contains("pan")) {
        note.pan = params["pan"].toInt();
    }
    note.parent = track;
    
    // Execute immediately
    track->addNote(note);
    
    // Record for undo
    
    if (compound) {
        compound->addAddedNote(track, note);
    }
    
    return true;
}

bool AiCommandExecutor::executeRemoveNote(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int noteNum = params["note"].toInt(-1);
    int start = params["start"].toInt(-1);
    
    NN_Note_t *foundNote = findNote(track, noteNum, start);
    if (!foundNote) {
        error = QObject::tr("Note %1 at tick %2 not found in track %3")
            .arg(noteNum).arg(start).arg(trackId);
        return false;
    }
    
    NN_Note_t noteCopy = *foundNote;
    
    // Execute immediately
    track->removeNote(noteCopy);
    
    // Record for undo
    
    if (compound) {
        compound->addRemovedNote(track, noteCopy);
    }
    
    return true;
}

bool AiCommandExecutor::executeModifyNote(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int originalNote = params["originalNote"].toInt(-1);
    int originalStart = params["originalStart"].toInt(-1);
    
    NN_Note_t *foundNote = findNote(track, originalNote, originalStart);
    if (!foundNote) {
        error = QObject::tr("Original note %1 at tick %2 not found").arg(originalNote).arg(originalStart);
        return false;
    }
    
    NN_Note_t oldNote = *foundNote;
    NN_Note_t newNote = oldNote;
    
    if (params.contains("newNote")) newNote.note = params["newNote"].toInt();
    if (params.contains("newStart")) newNote.start = params["newStart"].toInt();
    if (params.contains("newLength")) newNote.length = params["newLength"].toInt();
    if (params.contains("newVelocity")) newNote.velocity = params["newVelocity"].toInt();
    if (params.contains("newPan")) newNote.pan = params["newPan"].toInt();
    
    // Execute immediately (remove old, add new)
    track->removeNote(oldNote);
    track->addNote(newNote);
    
    // Record for undo
    
    if (compound) {
        compound->addModifiedNote(track, oldNote, newNote);
    }
    
    return true;
}

bool AiCommandExecutor::executeAddTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    QString name = params["name"].toString("AI Track");
    int instrument = params["instrument"].toInt(0);
    int channel = params["channel"].toInt(-1);
    
    // Find available channel if not specified
    if (channel < 0) {
        std::set<int> usedChannels;
        for (auto *t : m_sequence->getTracks()) {
            if (t->getChannel().has_value()) {
                usedChannels.insert(t->getChannel().value());
            }
        }
        for (int ch = 0; ch < 16; ++ch) {
            if (ch != 9 && usedChannels.find(ch) == usedChannels.end()) {  // Skip drum channel
                channel = ch;
                break;
            }
        }
        if (channel < 0) channel = 0;
    }
    
    // Create new track using sequence method
    NoteNagaTrack *newTrack = m_sequence->addTrack(instrument);
    if (!newTrack) {
        error = QObject::tr("Failed to create new track");
        return false;
    }
    
    newTrack->setName(name.toStdString());
    newTrack->setChannel(channel);
    
    // Record for undo
    
    if (compound) {
        compound->addAddedTrack(m_sequence, newTrack);
    }
    
    return true;
}

bool AiCommandExecutor::executeRemoveTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    // Find track index
    auto tracks = m_sequence->getTracks();
    int trackIndex = -1;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i]->getId() == trackId) {
            trackIndex = static_cast<int>(i);
            break;
        }
    }
    
    if (trackIndex < 0) {
        error = QObject::tr("Could not find track index");
        return false;
    }
    
    // Record for undo BEFORE removing
    
    if (compound) {
        compound->addRemovedTrack(m_sequence, track, trackIndex);
    }
    
    // Execute immediately
    m_sequence->removeTrack(trackIndex);
    
    return true;
}

bool AiCommandExecutor::executeModifyTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    
    
    // Process each property change
    if (params.contains("name")) {
        QString oldName = QString::fromStdString(track->getName());
        QString newName = params["name"].toString();
        track->setName(newName.toStdString());
        if (compound) {
            compound->addTrackPropertyChange(track, "name", oldName, newName);
        }
    }
    
    if (params.contains("instrument")) {
        int oldInst = track->getInstrument().value_or(0);
        int newInst = params["instrument"].toInt();
        track->setInstrument(newInst);
        if (compound) {
            compound->addTrackPropertyChange(track, "instrument", oldInst, newInst);
        }
    }
    
    if (params.contains("channel")) {
        int oldCh = track->getChannel().value_or(0);
        int newCh = params["channel"].toInt();
        track->setChannel(newCh);
        if (compound) {
            compound->addTrackPropertyChange(track, "channel", oldCh, newCh);
        }
    }
    
    if (params.contains("mute")) {
        bool oldMute = track->isMuted();
        bool newMute = params["mute"].toBool();
        track->setMuted(newMute);
        if (compound) {
            compound->addTrackPropertyChange(track, "mute", oldMute, newMute);
        }
    }
    
    if (params.contains("solo")) {
        bool oldSolo = track->isSolo();
        bool newSolo = params["solo"].toBool();
        track->setSolo(newSolo);
        if (compound) {
            compound->addTrackPropertyChange(track, "solo", oldSolo, newSolo);
        }
    }
    
    if (params.contains("volume")) {
        float oldVol = track->getVolume();
        float newVol = params["volume"].toInt() / 100.0f;
        track->setVolume(newVol);
        if (compound) {
            compound->addTrackPropertyChange(track, "volume", oldVol, newVol);
        }
    }
    
    if (params.contains("pan")) {
        int oldPan = track->getMidiPanOffset();
        int newPan = params["pan"].toInt();
        track->setMidiPanOffset(newPan);
        if (compound) {
            compound->addTrackPropertyChange(track, "pan", oldPan, newPan);
        }
    }
    
    if (params.contains("color")) {
        QString colorStr = params["color"].toString();
        QStringList parts = colorStr.split(',');
        if (parts.size() == 3) {
            NN_Color_t oldColor = track->getColor();
            NN_Color_t newColor(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
            track->setColor(newColor);
            if (compound) {
                QString oldColorStr = QString("%1,%2,%3").arg(oldColor.red).arg(oldColor.green).arg(oldColor.blue);
                compound->addTrackPropertyChange(track, "color", oldColorStr, colorStr);
            }
        }
    }
    
    return true;
}

bool AiCommandExecutor::executeClearTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    
    
    // Record all notes for undo
    for (const auto &note : track->getNotes()) {
        if (compound) {
            compound->addRemovedNote(track, note);
        }
    }
    
    // Clear all notes
    track->setNotes({});
    
    return true;
}

NoteNagaTrack* AiCommandExecutor::findTrackById(int trackId) {
    if (!m_sequence) return nullptr;
    return m_sequence->getTrackById(trackId);
}

NN_Note_t* AiCommandExecutor::findNote(NoteNagaTrack *track, int noteNum, int start) {
    if (!track) return nullptr;
    
    auto notes = track->getNotes();
    for (auto &note : notes) {
        if (note.note == noteNum && note.start.value_or(-1) == start) {
            // Return pointer to the note in track's vector
            for (auto &trackNote : track->getNotes()) {
                if (trackNote.id == note.id) {
                    return const_cast<NN_Note_t*>(&trackNote);
                }
            }
        }
    }
    return nullptr;
}

// ============================================================================
// AiModificationCommand
// ============================================================================

AiModificationCommand::AiModificationCommand() {}

AiModificationCommand::~AiModificationCommand() {}

void AiModificationCommand::addAddedNote(NoteNagaTrack *track, const NN_Note_t &note) {
    m_addedNotes.append({track, note});
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::addRemovedNote(NoteNagaTrack *track, const NN_Note_t &note) {
    m_removedNotes.append({track, note});
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::addModifiedNote(NoteNagaTrack *track, const NN_Note_t &oldNote, const NN_Note_t &newNote) {
    m_modifiedNotes.append({track, oldNote, newNote});
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::addAddedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track) {
    m_addedTracks.append({seq, track});
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::addRemovedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track, int index) {
    m_removedTracks.append({seq, track, index});
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::addTrackPropertyChange(NoteNagaTrack *track, const QString &property,
                                                   const QVariant &oldValue, const QVariant &newValue) {
    m_trackPropertyChanges.append({track, property, oldValue, newValue});
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::execute() {
    // Re-execute all operations (for redo)
    
    // Add notes
    for (const auto &pair : m_addedNotes) {
        if (pair.first) {
            pair.first->addNote(pair.second);
        }
    }
    
    // Remove notes
    for (const auto &pair : m_removedNotes) {
        if (pair.first) {
            pair.first->removeNote(pair.second);
        }
    }
    
    // Modify notes
    for (const auto &tuple : m_modifiedNotes) {
        NoteNagaTrack *track = std::get<0>(tuple);
        if (track) {
            track->removeNote(std::get<1>(tuple));  // Remove old
            track->addNote(std::get<2>(tuple));      // Add new
        }
    }
    
    // Add tracks
    for (const auto &pair : m_addedTracks) {
        // Track was already added, just needs to be visible
        // This is complex - need to re-add to sequence
    }
    
    // Apply property changes
    for (const auto &tuple : m_trackPropertyChanges) {
        NoteNagaTrack *track = std::get<0>(tuple);
        QString prop = std::get<1>(tuple);
        QVariant newVal = std::get<3>(tuple);
        
        if (!track) continue;
        
        if (prop == "name") track->setName(newVal.toString().toStdString());
        else if (prop == "instrument") track->setInstrument(newVal.toInt());
        else if (prop == "channel") track->setChannel(newVal.toInt());
        else if (prop == "mute") track->setMuted(newVal.toBool());
        else if (prop == "solo") track->setSolo(newVal.toBool());
        else if (prop == "volume") track->setVolume(newVal.toFloat());
        else if (prop == "pan") track->setMidiPanOffset(newVal.toInt());
        else if (prop == "color") {
            QStringList parts = newVal.toString().split(',');
            if (parts.size() == 3) {
                track->setColor(NN_Color_t(parts[0].toInt(), parts[1].toInt(), parts[2].toInt()));
            }
        }
    }
    
    // Apply tempo changes
    for (const auto &tuple : m_tempoChanges) {
        NoteNagaMidiSeq *seq = std::get<0>(tuple);
        int newTempo = std::get<2>(tuple);
        if (seq) {
            seq->setTempo(newTempo);
        }
    }
    
    // Add tempo events
    for (const auto &tuple : m_addedTempoEvents) {
        NoteNagaMidiSeq *seq = std::get<0>(tuple);
        if (seq) {
            NoteNagaTrack *tempoTrack = seq->getTempoTrack();
            if (tempoTrack) {
                int tick = std::get<1>(tuple);
                int bpm = std::get<2>(tuple);
                tempoTrack->addTempoEvent(NN_TempoEvent_t(tick, static_cast<double>(bpm)));
            }
        }
    }
    
    // Remove tempo events (for redo)
    for (const auto &tuple : m_removedTempoEvents) {
        NoteNagaMidiSeq *seq = std::get<0>(tuple);
        if (seq) {
            NoteNagaTrack *tempoTrack = seq->getTempoTrack();
            if (tempoTrack) {
                int tick = std::get<1>(tuple);
                tempoTrack->removeTempoEventAtTick(tick);
            }
        }
    }
    
    refreshAll();
}

void AiModificationCommand::undo() {
    // Undo in reverse order
    
    // Undo removed tempo events (re-add them)
    for (auto it = m_removedTempoEvents.rbegin(); it != m_removedTempoEvents.rend(); ++it) {
        NoteNagaMidiSeq *seq = std::get<0>(*it);
        if (seq) {
            NoteNagaTrack *tempoTrack = seq->getTempoTrack();
            if (tempoTrack) {
                int tick = std::get<1>(*it);
                int bpm = std::get<2>(*it);
                tempoTrack->addTempoEvent(NN_TempoEvent_t(tick, static_cast<double>(bpm)));
            }
        }
    }
    
    // Undo added tempo events (remove them)
    for (auto it = m_addedTempoEvents.rbegin(); it != m_addedTempoEvents.rend(); ++it) {
        NoteNagaMidiSeq *seq = std::get<0>(*it);
        if (seq) {
            NoteNagaTrack *tempoTrack = seq->getTempoTrack();
            if (tempoTrack) {
                int tick = std::get<1>(*it);
                tempoTrack->removeTempoEventAtTick(tick);
            }
        }
    }
    
    // Undo tempo changes (restore old tempo)
    for (auto it = m_tempoChanges.rbegin(); it != m_tempoChanges.rend(); ++it) {
        NoteNagaMidiSeq *seq = std::get<0>(*it);
        int oldTempo = std::get<1>(*it);
        if (seq) {
            seq->setTempo(oldTempo);
        }
    }
    
    // Revert property changes
    for (auto it = m_trackPropertyChanges.rbegin(); it != m_trackPropertyChanges.rend(); ++it) {
        NoteNagaTrack *track = std::get<0>(*it);
        QString prop = std::get<1>(*it);
        QVariant oldVal = std::get<2>(*it);
        
        if (!track) continue;
        
        if (prop == "name") track->setName(oldVal.toString().toStdString());
        else if (prop == "instrument") track->setInstrument(oldVal.toInt());
        else if (prop == "channel") track->setChannel(oldVal.toInt());
        else if (prop == "mute") track->setMuted(oldVal.toBool());
        else if (prop == "solo") track->setSolo(oldVal.toBool());
        else if (prop == "volume") track->setVolume(oldVal.toFloat());
        else if (prop == "pan") track->setMidiPanOffset(oldVal.toInt());
        else if (prop == "color") {
            QStringList parts = oldVal.toString().split(',');
            if (parts.size() == 3) {
                track->setColor(NN_Color_t(parts[0].toInt(), parts[1].toInt(), parts[2].toInt()));
            }
        }
    }
    
    // Undo modified notes (reverse: remove new, add old)
    for (auto it = m_modifiedNotes.rbegin(); it != m_modifiedNotes.rend(); ++it) {
        NoteNagaTrack *track = std::get<0>(*it);
        if (track) {
            track->removeNote(std::get<2>(*it));  // Remove new
            track->addNote(std::get<1>(*it));      // Add old
        }
    }
    
    // Undo removed notes (re-add them)
    for (auto it = m_removedNotes.rbegin(); it != m_removedNotes.rend(); ++it) {
        if (it->first) {
            it->first->addNote(it->second);
        }
    }
    
    // Undo added notes (remove them)
    for (auto it = m_addedNotes.rbegin(); it != m_addedNotes.rend(); ++it) {
        if (it->first) {
            it->first->removeNote(it->second);
        }
    }
    
    refreshAll();
}

QString AiModificationCommand::description() const {
    return QObject::tr("AI Modification (%1 operations)").arg(m_operationCount);
}

void AiModificationCommand::refreshAll() {
    if (m_refreshCallback) {
        m_refreshCallback();
    }
    if (m_trackListCallback) {
        m_trackListCallback();
    }
}

// Tempo change implementations
void AiModificationCommand::addTempoChange(NoteNagaMidiSeq *seq, int oldTempo, int newTempo) {
    m_tempoChanges.append(std::make_tuple(seq, oldTempo, newTempo));
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::addTempoEventAdd(NoteNagaMidiSeq *seq, int tick, int tempo) {
    m_addedTempoEvents.append(std::make_tuple(seq, tick, tempo));
    m_isEmpty = false;
    m_operationCount++;
}

void AiModificationCommand::addTempoEventRemove(NoteNagaMidiSeq *seq, int tick, int tempo) {
    m_removedTempoEvents.append(std::make_tuple(seq, tick, tempo));
    m_isEmpty = false;
    m_operationCount++;
}

bool AiCommandExecutor::executeSetTempo(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    if (!m_sequence) {
        error = QObject::tr("No sequence available");
        return false;
    }
    
    int bpm = params["bpm"].toInt(120);
    if (bpm < 20 || bpm > 300) {
        error = QObject::tr("Invalid BPM value: %1 (must be 20-300)").arg(bpm);
        return false;
    }
    
    // Convert BPM to microseconds per quarter note
    int newTempo = static_cast<int>(60000000.0 / bpm);
    int oldTempo = m_sequence->getTempo();
    
    // Execute immediately
    m_sequence->setTempo(newTempo);
    
    // Record for undo
    if (compound) {
        compound->addTempoChange(m_sequence, oldTempo, newTempo);
    }
    
    return true;
}

bool AiCommandExecutor::executeAddTempoEvent(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    if (!m_sequence) {
        error = QObject::tr("No sequence available");
        return false;
    }
    
    NoteNagaTrack *tempoTrack = m_sequence->getTempoTrack();
    if (!tempoTrack) {
        error = QObject::tr("No tempo track available");
        return false;
    }
    
    int tick = params["tick"].toInt(0);
    int bpm = params["bpm"].toInt(120);
    
    if (bpm < 20 || bpm > 300) {
        error = QObject::tr("Invalid BPM value: %1 (must be 20-300)").arg(bpm);
        return false;
    }
    
    // Create tempo event (BPM directly, not microseconds)
    NN_TempoEvent_t event(tick, static_cast<double>(bpm));
    
    // Execute immediately
    tempoTrack->addTempoEvent(event);
    
    // Record for undo (store BPM)
    if (compound) {
        compound->addTempoEventAdd(m_sequence, tick, bpm);
    }
    
    return true;
}

bool AiCommandExecutor::executeRemoveTempoEvent(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    if (!m_sequence) {
        error = QObject::tr("No sequence available");
        return false;
    }
    
    NoteNagaTrack *tempoTrack = m_sequence->getTempoTrack();
    if (!tempoTrack) {
        error = QObject::tr("No tempo track available");
        return false;
    }
    
    int tick = params["tick"].toInt(-1);
    if (tick < 0) {
        error = QObject::tr("Invalid tick position");
        return false;
    }
    
    // Find the existing tempo at this tick before removing
    int oldBpm = static_cast<int>(tempoTrack->getTempoAtTick(tick));
    
    // Execute immediately
    if (!tempoTrack->removeTempoEventAtTick(tick)) {
        error = QObject::tr("No tempo event at tick %1").arg(tick);
        return false;
    }
    
    // Record for undo
    if (compound) {
        compound->addTempoEventRemove(m_sequence, tick, oldBpm);
    }
    
    return true;
}

} // namespace NoteNagaAI
