#include "ai_command_executor.h"
#include "../editor/midi_editor_widget.h"
#include "../editor/ai_preview_state.h"

#include <note_naga_engine/core/types.h>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <algorithm>
#include <climits>

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
    
    // Block signals on sequence and all tracks to prevent excessive UI updates during batch operations
    m_sequence->blockSignals(true);
    for (auto *track : m_sequence->getTracks()) {
        track->blockSignals(true);
    }
    
    // Create compound command for undo
    auto *compound = new AiModificationCommand();
    
    // Use QPointer to safely check if editor still exists when undo is called
    // (the executor is a temporary stack object, so we must capture editor directly)
    QPointer<MidiEditorWidget> editorPtr = m_editor;
    compound->setRefreshCallback([editorPtr]() {
        if (editorPtr) {
            editorPtr->refreshAll();
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
    
    // Unblock signals on sequence and all tracks (including any newly created ones)
    for (auto *track : m_sequence->getTracks()) {
        track->blockSignals(false);
    }
    m_sequence->blockSignals(false);
    
    // Emit signals once to notify all listeners (track list, editors, etc.)
    emit m_sequence->trackListChanged();
    for (auto *track : m_sequence->getTracks()) {
        emit track->metadataChanged(track, "notes");
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

ExecutionResult AiCommandExecutor::executeWithPreview(const AiResponse &response, AiPreviewState *previewState) {
    ExecutionResult result;
    
    if (!m_editor || !m_sequence) {
        result.errorMessage = QObject::tr("Editor or sequence not available");
        return result;
    }
    
    if (!response.isValid()) {
        result.errorMessage = response.parseError;
        return result;
    }
    
    if (!previewState) {
        result.errorMessage = QObject::tr("Preview state not provided");
        return result;
    }
    
    // Begin preview mode
    previewState->beginPreview(m_sequence);
    
    // Block signals on sequence and all tracks to prevent excessive UI updates during batch operations
    m_sequence->blockSignals(true);
    for (auto *track : m_sequence->getTracks()) {
        track->blockSignals(true);
    }
    
    // Create compound command for undo (still use undo stack so we can discard later)
    auto *compound = new AiModificationCommand();
    
    QPointer<MidiEditorWidget> editorPtr = m_editor;
    compound->setRefreshCallback([editorPtr]() {
        if (editorPtr) {
            editorPtr->refreshAll();
        }
    });
    compound->setTrackListCallback(m_trackListChangedCallback);
    
    // Execute all commands and track for preview
    for (const auto &cmd : response.commands) {
        QString error;
        if (executeCommand(cmd, compound, error)) {
            result.commandsExecuted++;
        } else {
            result.commandsFailed++;
            result.warnings.append(error);
        }
    }
    
    // Unblock signals on sequence and all tracks (including any newly created ones)
    for (auto *track : m_sequence->getTracks()) {
        track->blockSignals(false);
    }
    m_sequence->blockSignals(false);
    
    // Emit signals once to notify all listeners (track list, editors, etc.)
    emit m_sequence->trackListChanged();
    for (auto *track : m_sequence->getTracks()) {
        emit track->metadataChanged(track, "notes");
    }
    
    // Copy tracked changes to preview state from the compound command
    // We need to access the compound's tracked data
    copyChangesToPreviewState(compound, previewState);
    
    // Add compound command to undo stack if any operations were performed
    if (!compound->isEmpty()) {
        UndoManager *undoManager = m_editor->getUndoManager();
        if (undoManager) {
            undoManager->addCommandWithoutExecute(compound);
            // Track how many undo operations we pushed (it's one compound command)
            previewState->setPendingUndoCount(1);
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
    
    // Refresh UI (will show preview colors)
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
        case AiCommandType::ClearAllTracks:
            return executeClearAllTracks(cmd.params, compound, error);
        case AiCommandType::SetTempo:
            return executeSetTempo(cmd.params, compound, error);
        case AiCommandType::AddTempoEvent:
            return executeAddTempoEvent(cmd.params, compound, error);
        case AiCommandType::RemoveTempoEvent:
            return executeRemoveTempoEvent(cmd.params, compound, error);
        case AiCommandType::AddChord:
            return executeAddChord(cmd.params, compound, error);
        case AiCommandType::AddArpeggio:
            return executeAddArpeggio(cmd.params, compound, error);
        case AiCommandType::AddScale:
            return executeAddScale(cmd.params, compound, error);
        case AiCommandType::AddPattern:
            return executeAddPattern(cmd.params, compound, error);
        case AiCommandType::DuplicateNotes:
            return executeDuplicateNotes(cmd.params, compound, error);
        case AiCommandType::TransposeNotes:
            return executeTransposeNotes(cmd.params, compound, error);
        case AiCommandType::QuantizeNotes:
            return executeQuantizeNotes(cmd.params, compound, error);
        case AiCommandType::AddDrumPattern:
            return executeAddDrumPattern(cmd.params, compound, error);
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
    
    // Extract track (don't delete - undo needs the pointer)
    m_sequence->extractTrack(trackIndex);
    
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

bool AiCommandExecutor::executeClearAllTracks(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    Q_UNUSED(params);
    
    if (!m_sequence) {
        error = QObject::tr("Sequence not available");
        return false;
    }
    
    auto tracks = m_sequence->getTracks();
    if (tracks.empty()) {
        // Nothing to clear, but that's not an error
        return true;
    }
    
    // Remove tracks in reverse order to avoid index shifting issues
    for (int i = static_cast<int>(tracks.size()) - 1; i >= 0; --i) {
        NoteNagaTrack *track = tracks[i];
        
        // Record for undo BEFORE removing
        if (compound) {
            compound->addRemovedTrack(m_sequence, track, i);
        }
        
        // Extract track (don't delete - undo needs the pointer)
        m_sequence->extractTrack(i);
    }
    
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

void AiCommandExecutor::copyChangesToPreviewState(AiModificationCommand *compound, AiPreviewState *previewState) {
    if (!compound || !previewState) return;
    
    // Copy added notes
    for (const auto &pair : compound->getAddedNotes()) {
        previewState->addAddedNote(pair.first, pair.second);
    }
    
    // Copy removed notes
    for (const auto &pair : compound->getRemovedNotes()) {
        previewState->addRemovedNote(pair.first, pair.second);
    }
    
    // Copy modified notes
    for (const auto &tuple : compound->getModifiedNotes()) {
        previewState->addModifiedNote(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple));
    }
    
    // Copy added tracks
    for (const auto &pair : compound->getAddedTracks()) {
        previewState->addAddedTrack(pair.first, pair.second);
    }
    
    // Copy removed tracks
    for (const auto &tuple : compound->getRemovedTracks()) {
        previewState->addRemovedTrack(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple));
    }
}

// ============================================================================
// AiModificationCommand
// ============================================================================

AiModificationCommand::AiModificationCommand() {}

AiModificationCommand::~AiModificationCommand() {
    // Clean up owned tracks to prevent memory leaks.
    // Track ownership depends on whether undo() was called:
    // - Initial state (m_undone=false): we own extracted tracks in m_removedTracks
    // - After undo (m_undone=true): we own extracted tracks in m_addedTracks
    if (!m_undone) {
        // We own the tracks that were removed from the sequence
        for (const auto &tuple : m_removedTracks) {
            delete std::get<1>(tuple);
        }
    } else {
        // We own the tracks that were added (then extracted during undo)
        for (const auto &pair : m_addedTracks) {
            delete pair.second;
        }
    }
}

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
    
    // Block signals during batch operations for performance
    QSet<NoteNagaMidiSeq*> sequences = collectSequences();
    blockAllSignals(sequences, true);
    
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
    
    // Re-add tracks that were removed during undo (for redo)
    for (auto &tuple : m_addedTracks) {
        NoteNagaMidiSeq *seq = tuple.first;
        NoteNagaTrack *track = tuple.second;
        if (seq && track) {
            seq->insertTrack(static_cast<int>(seq->getTracks().size()), track);
        }
    }
    
    // Re-remove tracks that were added back during undo (for redo)
    for (auto it = m_removedTracks.begin(); it != m_removedTracks.end(); ++it) {
        NoteNagaMidiSeq *seq = std::get<0>(*it);
        NoteNagaTrack *track = std::get<1>(*it);
        if (seq && track) {
            // Find and extract the track
            auto tracks = seq->getTracks();
            for (size_t i = 0; i < tracks.size(); ++i) {
                if (tracks[i] == track) {
                    seq->extractTrack(static_cast<int>(i));
                    break;
                }
            }
        }
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
    
    m_undone = false;  // After execute/redo, we own m_removedTracks
    
    // Unblock and emit signals once
    blockAllSignals(sequences, false);
    emitAllSignals(sequences);
    
    refreshAll();
}

void AiModificationCommand::undo() {
    // Undo in reverse order
    
    // Block signals during batch operations for performance
    QSet<NoteNagaMidiSeq*> sequences = collectSequences();
    blockAllSignals(sequences, true);
    
    // Undo removed tracks (re-add them at original position)
    for (auto it = m_removedTracks.rbegin(); it != m_removedTracks.rend(); ++it) {
        NoteNagaMidiSeq *seq = std::get<0>(*it);
        NoteNagaTrack *track = std::get<1>(*it);
        int originalIndex = std::get<2>(*it);
        if (seq && track) {
            seq->insertTrack(originalIndex, track);
        }
    }
    
    // Undo added tracks (extract them from sequence)
    for (auto it = m_addedTracks.rbegin(); it != m_addedTracks.rend(); ++it) {
        NoteNagaMidiSeq *seq = it->first;
        NoteNagaTrack *track = it->second;
        if (seq && track) {
            // Find and extract the track
            auto tracks = seq->getTracks();
            for (size_t i = 0; i < tracks.size(); ++i) {
                if (tracks[i] == track) {
                    seq->extractTrack(static_cast<int>(i));
                    break;
                }
            }
        }
    }
    
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
    
    m_undone = true;  // After undo, we own m_addedTracks
    
    // Unblock and emit signals once
    blockAllSignals(sequences, false);
    emitAllSignals(sequences);
    
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

QSet<NoteNagaMidiSeq*> AiModificationCommand::collectSequences() const {
    QSet<NoteNagaMidiSeq*> sequences;
    
    // Collect from added tracks
    for (const auto &pair : m_addedTracks) {
        if (pair.first) sequences.insert(pair.first);
    }
    
    // Collect from removed tracks
    for (const auto &tuple : m_removedTracks) {
        if (std::get<0>(tuple)) sequences.insert(std::get<0>(tuple));
    }
    
    // Collect from tempo changes
    for (const auto &tuple : m_tempoChanges) {
        if (std::get<0>(tuple)) sequences.insert(std::get<0>(tuple));
    }
    
    // Collect from tempo events
    for (const auto &tuple : m_addedTempoEvents) {
        if (std::get<0>(tuple)) sequences.insert(std::get<0>(tuple));
    }
    for (const auto &tuple : m_removedTempoEvents) {
        if (std::get<0>(tuple)) sequences.insert(std::get<0>(tuple));
    }
    
    // Collect from notes (get parent sequence from track)
    for (const auto &pair : m_addedNotes) {
        if (pair.first && pair.first->getParent()) {
            sequences.insert(pair.first->getParent());
        }
    }
    for (const auto &pair : m_removedNotes) {
        if (pair.first && pair.first->getParent()) {
            sequences.insert(pair.first->getParent());
        }
    }
    
    return sequences;
}

void AiModificationCommand::blockAllSignals(const QSet<NoteNagaMidiSeq*> &sequences, bool block) {
    for (NoteNagaMidiSeq *seq : sequences) {
        if (seq) {
            seq->blockSignals(block);
            for (auto *track : seq->getTracks()) {
                track->blockSignals(block);
            }
        }
    }
}

void AiModificationCommand::emitAllSignals(const QSet<NoteNagaMidiSeq*> &sequences) {
    for (NoteNagaMidiSeq *seq : sequences) {
        if (seq) {
            emit seq->trackListChanged();
            for (auto *track : seq->getTracks()) {
                emit track->metadataChanged(track, "notes");
            }
        }
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

// ============================================================================
// Advanced Note Commands Implementation
// ============================================================================

namespace {
    // Chord intervals from root note
    QVector<int> getChordIntervals(const QString &type) {
        QString t = type.toLower();
        if (t == "maj" || t == "major") return {0, 4, 7};
        if (t == "min" || t == "minor" || t == "m") return {0, 3, 7};
        if (t == "dim") return {0, 3, 6};
        if (t == "aug") return {0, 4, 8};
        if (t == "7" || t == "dom7") return {0, 4, 7, 10};
        if (t == "maj7") return {0, 4, 7, 11};
        if (t == "min7" || t == "m7") return {0, 3, 7, 10};
        if (t == "dim7") return {0, 3, 6, 9};
        if (t == "sus2") return {0, 2, 7};
        if (t == "sus4") return {0, 5, 7};
        if (t == "add9") return {0, 4, 7, 14};
        if (t == "6") return {0, 4, 7, 9};
        if (t == "min6" || t == "m6") return {0, 3, 7, 9};
        if (t == "9") return {0, 4, 7, 10, 14};
        if (t == "maj9") return {0, 4, 7, 11, 14};
        if (t == "min9" || t == "m9") return {0, 3, 7, 10, 14};
        if (t == "power" || t == "5") return {0, 7};
        return {0, 4, 7}; // default to major
    }
    
    // Scale intervals
    QVector<int> getScaleIntervals(const QString &type) {
        QString t = type.toLower();
        if (t == "major" || t == "ionian") return {0, 2, 4, 5, 7, 9, 11, 12};
        if (t == "minor" || t == "aeolian" || t == "natural") return {0, 2, 3, 5, 7, 8, 10, 12};
        if (t == "harmonic") return {0, 2, 3, 5, 7, 8, 11, 12};
        if (t == "melodic") return {0, 2, 3, 5, 7, 9, 11, 12};
        if (t == "penta" || t == "pentatonic") return {0, 2, 4, 7, 9, 12};
        if (t == "penta_minor" || t == "minpenta") return {0, 3, 5, 7, 10, 12};
        if (t == "blues") return {0, 3, 5, 6, 7, 10, 12};
        if (t == "dorian") return {0, 2, 3, 5, 7, 9, 10, 12};
        if (t == "phrygian") return {0, 1, 3, 5, 7, 8, 10, 12};
        if (t == "lydian") return {0, 2, 4, 6, 7, 9, 11, 12};
        if (t == "mixo" || t == "mixolydian") return {0, 2, 4, 5, 7, 9, 10, 12};
        if (t == "locrian") return {0, 1, 3, 5, 6, 8, 10, 12};
        if (t == "chromatic") return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        if (t == "wholetone") return {0, 2, 4, 6, 8, 10, 12};
        return {0, 2, 4, 5, 7, 9, 11, 12}; // default to major
    }
    
    // Drum pattern - returns list of (tick_offset, note, velocity)
    struct DrumHit { int tickOffset; int note; int velocity; };
    
    QVector<DrumHit> getDrumPattern(const QString &pattern, int ppq) {
        QVector<DrumHit> hits;
        QString p = pattern.toLower();
        
        // Standard note values
        int q = ppq;        // quarter
        int e = ppq / 2;    // eighth
        int s = ppq / 4;    // sixteenth
        
        // Drum notes (GM standard)
        int kick = 36, snare = 38, hihat = 42, hihatOpen = 46, ride = 51, crash = 49;
        int tomHi = 50, tomMid = 47, tomLow = 43;
        
        if (p == "rock" || p == "basic") {
            // Basic rock beat - 1 bar
            for (int beat = 0; beat < 4; beat++) {
                hits.append({beat * q, hihat, 80});
                hits.append({beat * q + e, hihat, 70});
            }
            hits.append({0, kick, 110});
            hits.append({2 * q, kick, 110});
            hits.append({1 * q, snare, 100});
            hits.append({3 * q, snare, 100});
        }
        else if (p == "pop") {
            // Pop beat with more hihat
            for (int i = 0; i < 8; i++) {
                hits.append({i * e, hihat, 75});
            }
            hits.append({0, kick, 110});
            hits.append({q + e, kick, 100});
            hits.append({2 * q, kick, 110});
            hits.append({1 * q, snare, 100});
            hits.append({3 * q, snare, 100});
        }
        else if (p == "hiphop") {
            // Hip-hop beat
            hits.append({0, kick, 120});
            hits.append({q + e, kick, 110});
            hits.append({2 * q + s, kick, 100});
            hits.append({1 * q, snare, 100});
            hits.append({3 * q, snare, 100});
            for (int i = 0; i < 8; i++) {
                hits.append({i * e, hihat, 60 + (i % 2) * 20});
            }
        }
        else if (p == "jazz") {
            // Jazz swing pattern on ride
            hits.append({0, ride, 90});
            hits.append({q * 2 / 3, ride, 70}); // swing triplet feel
            hits.append({q, ride, 90});
            hits.append({q + q * 2 / 3, ride, 70});
            hits.append({2 * q, ride, 90});
            hits.append({2 * q + q * 2 / 3, ride, 70});
            hits.append({3 * q, ride, 90});
            hits.append({3 * q + q * 2 / 3, ride, 70});
            hits.append({2 * q, kick, 80});
            hits.append({3 * q + e, kick, 70});
        }
        else if (p == "latin" || p == "bossa") {
            // Bossa nova pattern
            hits.append({0, kick, 100});
            hits.append({3 * e, kick, 90});
            hits.append({q + e, snare, 80}); // cross-stick
            hits.append({2 * q + e, snare, 80});
            for (int i = 0; i < 16; i++) {
                hits.append({i * s, hihat, 60});
            }
        }
        else if (p == "metal") {
            // Double bass metal
            for (int i = 0; i < 16; i++) {
                hits.append({i * s, kick, 120});
            }
            hits.append({1 * q, snare, 127});
            hits.append({3 * q, snare, 127});
            for (int i = 0; i < 8; i++) {
                hits.append({i * e, hihat, 100});
            }
        }
        else if (p == "halftime") {
            // Half-time feel
            hits.append({0, kick, 110});
            hits.append({2 * q, snare, 100});
            for (int i = 0; i < 8; i++) {
                hits.append({i * e, hihat, 70});
            }
        }
        else if (p == "shuffle") {
            // Shuffle beat
            int triplet = q / 3;
            for (int beat = 0; beat < 4; beat++) {
                hits.append({beat * q, hihat, 85});
                hits.append({beat * q + 2 * triplet, hihat, 70});
            }
            hits.append({0, kick, 110});
            hits.append({2 * q, kick, 110});
            hits.append({1 * q, snare, 100});
            hits.append({3 * q, snare, 100});
        }
        else if (p == "fill1") {
            // Simple fill
            hits.append({0, snare, 90});
            hits.append({e, snare, 85});
            hits.append({2 * e, tomHi, 95});
            hits.append({3 * e, tomHi, 90});
            hits.append({4 * e, tomMid, 100});
            hits.append({5 * e, tomMid, 95});
            hits.append({6 * e, tomLow, 105});
            hits.append({7 * e, crash, 120});
        }
        else if (p == "fill2") {
            // 16th note fill
            for (int i = 0; i < 12; i++) {
                int note = (i < 4) ? tomHi : (i < 8) ? tomMid : tomLow;
                hits.append({i * s, note, 90 + (i % 2) * 10});
            }
            hits.append({12 * s, kick, 120});
            hits.append({12 * s, crash, 127});
        }
        else {
            // Default to basic rock
            return getDrumPattern("rock", ppq);
        }
        
        return hits;
    }
}

bool AiCommandExecutor::executeAddChord(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int root = params["root"].toInt(60);
    QString chordType = params["chordType"].toString("maj");
    int start = params["start"].toInt(0);
    int length = params["length"].toInt(480);
    int velocity = params["velocity"].toInt(100);
    
    QVector<int> intervals = getChordIntervals(chordType);
    
    for (int interval : intervals) {
        NN_Note_t note;
        note.note = root + interval;
        note.start = start;
        note.length = length;
        note.velocity = velocity;
        note.parent = track;
        
        if (note.note >= 0 && note.note <= 127) {
            track->addNote(note);
            if (compound) {
                compound->addAddedNote(track, note);
            }
        }
    }
    
    return true;
}

bool AiCommandExecutor::executeAddArpeggio(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int root = params["root"].toInt(60);
    QString chordType = params["chordType"].toString("maj");
    int start = params["start"].toInt(0);
    int noteLength = params["noteLength"].toInt(120);
    int velocity = params["velocity"].toInt(100);
    QString direction = params["direction"].toString("up");
    
    QVector<int> intervals = getChordIntervals(chordType);
    
    // Handle direction
    if (direction.toLower() == "down") {
        std::reverse(intervals.begin(), intervals.end());
    } else if (direction.toLower() == "updown") {
        QVector<int> down = intervals;
        std::reverse(down.begin(), down.end());
        down.removeFirst(); // Don't repeat top note
        intervals.append(down);
    }
    
    int currentStart = start;
    for (int interval : intervals) {
        NN_Note_t note;
        note.note = root + interval;
        note.start = currentStart;
        note.length = noteLength;
        note.velocity = velocity;
        note.parent = track;
        
        if (note.note >= 0 && note.note <= 127) {
            track->addNote(note);
            if (compound) {
                compound->addAddedNote(track, note);
            }
        }
        currentStart += noteLength;
    }
    
    return true;
}

bool AiCommandExecutor::executeAddScale(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int root = params["root"].toInt(60);
    QString scaleType = params["scaleType"].toString("major");
    int start = params["start"].toInt(0);
    int noteLength = params["noteLength"].toInt(240);
    int velocity = params["velocity"].toInt(100);
    
    QVector<int> intervals = getScaleIntervals(scaleType);
    
    int currentStart = start;
    for (int interval : intervals) {
        NN_Note_t note;
        note.note = root + interval;
        note.start = currentStart;
        note.length = noteLength;
        note.velocity = velocity;
        note.parent = track;
        
        if (note.note >= 0 && note.note <= 127) {
            track->addNote(note);
            if (compound) {
                compound->addAddedNote(track, note);
            }
        }
        currentStart += noteLength;
    }
    
    return true;
}

bool AiCommandExecutor::executeAddPattern(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    QString pattern = params["pattern"].toString();
    int start = params["start"].toInt(0);
    int noteLength = params["noteLength"].toInt(240);
    int velocity = params["velocity"].toInt(100);
    
    // Parse pattern - can be "60-64-67" or "60,64,67"
    QStringList notes;
    if (pattern.contains('-')) {
        notes = pattern.split('-');
    } else {
        notes = pattern.split(',');
    }
    
    int currentStart = start;
    for (const QString &noteStr : notes) {
        int pitch = noteStr.trimmed().toInt();
        
        NN_Note_t note;
        note.note = pitch;
        note.start = currentStart;
        note.length = noteLength;
        note.velocity = velocity;
        note.parent = track;
        
        if (note.note >= 0 && note.note <= 127) {
            track->addNote(note);
            if (compound) {
                compound->addAddedNote(track, note);
            }
        }
        currentStart += noteLength;
    }
    
    return true;
}

bool AiCommandExecutor::executeDuplicateNotes(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int srcStart = params["srcStart"].toInt(0);
    int srcEnd = params["srcEnd"].toInt(0);
    int destStart = params["destStart"].toInt(0);
    
    int offset = destStart - srcStart;
    
    // Collect notes to duplicate
    std::vector<NN_Note_t> notesToDuplicate;
    for (const auto &note : track->getNotes()) {
        int noteStart = note.start.value_or(0);
        if (noteStart >= srcStart && noteStart < srcEnd) {
            notesToDuplicate.push_back(note);
        }
    }
    
    // Add duplicated notes
    for (const auto &origNote : notesToDuplicate) {
        NN_Note_t note = origNote;
        note.start = origNote.start.value_or(0) + offset;
        note.parent = track;
        
        track->addNote(note);
        if (compound) {
            compound->addAddedNote(track, note);
        }
    }
    
    return true;
}

bool AiCommandExecutor::executeTransposeNotes(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int start = params["start"].toInt(0);
    int end = params["end"].toInt(INT_MAX);
    int semitones = params["semitones"].toInt(0);
    
    // Find and modify notes in range
    std::vector<NN_Note_t> notesToModify;
    for (const auto &note : track->getNotes()) {
        int noteStart = note.start.value_or(0);
        if (noteStart >= start && noteStart < end) {
            notesToModify.push_back(note);
        }
    }
    
    for (const auto &oldNote : notesToModify) {
        NN_Note_t newNote = oldNote;
        newNote.note = std::clamp(oldNote.note + semitones, 0, 127);
        
        track->removeNote(oldNote);
        track->addNote(newNote);
        
        if (compound) {
            compound->addModifiedNote(track, oldNote, newNote);
        }
    }
    
    return true;
}

bool AiCommandExecutor::executeQuantizeNotes(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    int start = params["start"].toInt(0);
    int end = params["end"].toInt(INT_MAX);
    int grid = params["grid"].toInt(120); // Default to 16th notes
    
    if (grid <= 0) {
        error = QObject::tr("Invalid grid value");
        return false;
    }
    
    // Find and modify notes in range
    std::vector<NN_Note_t> notesToQuantize;
    for (const auto &note : track->getNotes()) {
        int noteStart = note.start.value_or(0);
        if (noteStart >= start && noteStart < end) {
            notesToQuantize.push_back(note);
        }
    }
    
    for (const auto &oldNote : notesToQuantize) {
        int oldStart = oldNote.start.value_or(0);
        int newStart = ((oldStart + grid / 2) / grid) * grid; // Round to nearest grid
        
        if (newStart != oldStart) {
            NN_Note_t newNote = oldNote;
            newNote.start = newStart;
            
            track->removeNote(oldNote);
            track->addNote(newNote);
            
            if (compound) {
                compound->addModifiedNote(track, oldNote, newNote);
            }
        }
    }
    
    return true;
}

bool AiCommandExecutor::executeAddDrumPattern(const QJsonObject &params, AiModificationCommand *compound, QString &error) {
    int trackId = params["trackId"].toInt(-1);
    NoteNagaTrack *track = findTrackById(trackId);
    if (!track) {
        error = QObject::tr("Track %1 not found").arg(trackId);
        return false;
    }
    
    QString pattern = params["pattern"].toString("rock");
    int start = params["start"].toInt(0);
    int bars = params["bars"].toInt(1);
    
    int ppq = m_sequence ? m_sequence->getPPQ() : 480;
    int barLength = ppq * 4; // 4/4 time signature
    
    QVector<DrumHit> hits = getDrumPattern(pattern, ppq);
    
    for (int bar = 0; bar < bars; bar++) {
        int barStart = start + bar * barLength;
        
        for (const DrumHit &hit : hits) {
            NN_Note_t note;
            note.note = hit.note;
            note.start = barStart + hit.tickOffset;
            note.length = ppq / 4; // 16th note length
            note.velocity = hit.velocity;
            note.parent = track;
            
            track->addNote(note);
            if (compound) {
                compound->addAddedNote(track, note);
            }
        }
    }
    
    return true;
}

} // namespace NoteNagaAI
