#include "midi_note_commands.h"
#include "../editor/midi_editor_widget.h"
#include <note_naga_engine/note_naga_engine.h>

// ============================================================================
// MidiNoteCommandBase - Helper methods
// ============================================================================

void MidiNoteCommandBase::refreshTracks(const QSet<NoteNagaTrack*> &tracks) {
    if (!m_editor) return;
    for (NoteNagaTrack *track : tracks) {
        m_editor->refreshTrack(track);
    }
}

void MidiNoteCommandBase::refreshTracks(NoteNagaTrack *track) {
    if (!m_editor || !track) return;
    m_editor->refreshTrack(track);
}

void MidiNoteCommandBase::refreshAllTracks() {
    if (!m_editor) return;
    m_editor->refreshAll();
}

void MidiNoteCommandBase::computeMaxTick() {
    if (!m_editor) return;
    NoteNagaMidiSeq *seq = m_editor->getSequence();
    if (seq) {
        seq->computeMaxTick();
    }
}

// ============================================================================
// AddNoteCommand
// ============================================================================

AddNoteCommand::AddNoteCommand(MidiEditorWidget *editor, NoteNagaTrack *track, const NN_Note_t &note)
    : MidiNoteCommandBase(editor), m_track(track), m_note(note)
{
}

void AddNoteCommand::execute() {
    if (!m_track) return;
    m_track->addNote(m_note);
    computeMaxTick();
    refreshTracks(m_track);
}

void AddNoteCommand::undo() {
    if (!m_track) return;
    m_track->removeNote(m_note);
    computeMaxTick();
    refreshTracks(m_track);
}

// ============================================================================
// DeleteNotesCommand
// ============================================================================

DeleteNotesCommand::DeleteNotesCommand(MidiEditorWidget *editor,
                                       const QList<QPair<NoteNagaTrack*, NN_Note_t>> &notes)
    : MidiNoteCommandBase(editor), m_notes(notes)
{
}

void DeleteNotesCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &pair : m_notes) {
        if (pair.first) {
            pair.first->removeNote(pair.second);
            affectedTracks.insert(pair.first);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

void DeleteNotesCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &pair : m_notes) {
        if (pair.first) {
            pair.first->addNote(pair.second);
            affectedTracks.insert(pair.first);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

QString DeleteNotesCommand::description() const {
    if (m_notes.size() == 1) {
        return QObject::tr("Delete Note");
    }
    return QObject::tr("Delete %1 Notes").arg(m_notes.size());
}

// ============================================================================
// MoveNotesCommand
// ============================================================================

MoveNotesCommand::MoveNotesCommand(MidiEditorWidget *editor,
                                   const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges)
    : MidiNoteCommandBase(editor), m_noteChanges(noteChanges)
{
}

void MoveNotesCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<1>(change));  // Remove original
        track->addNote(std::get<2>(change));     // Add new
        affectedTracks.insert(track);
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

void MoveNotesCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<2>(change));  // Remove new
        track->addNote(std::get<1>(change));     // Restore original
        affectedTracks.insert(track);
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

// ============================================================================
// ResizeNotesCommand
// ============================================================================

ResizeNotesCommand::ResizeNotesCommand(MidiEditorWidget *editor,
                                       const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges)
    : MidiNoteCommandBase(editor), m_noteChanges(noteChanges)
{
}

void ResizeNotesCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<1>(change));
        track->addNote(std::get<2>(change));
        affectedTracks.insert(track);
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

void ResizeNotesCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<2>(change));
        track->addNote(std::get<1>(change));
        affectedTracks.insert(track);
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

// ============================================================================
// DuplicateNotesCommand
// ============================================================================

DuplicateNotesCommand::DuplicateNotesCommand(MidiEditorWidget *editor,
                                             const QList<QPair<NoteNagaTrack*, NN_Note_t>> &duplicatedNotes)
    : MidiNoteCommandBase(editor), m_duplicatedNotes(duplicatedNotes)
{
}

void DuplicateNotesCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &pair : m_duplicatedNotes) {
        if (pair.first) {
            pair.first->addNote(pair.second);
            affectedTracks.insert(pair.first);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

void DuplicateNotesCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &pair : m_duplicatedNotes) {
        if (pair.first) {
            pair.first->removeNote(pair.second);
            affectedTracks.insert(pair.first);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

// ============================================================================
// TransposeNotesCommand
// ============================================================================

TransposeNotesCommand::TransposeNotesCommand(MidiEditorWidget *editor,
                                             const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges,
                                             int semitones)
    : MidiNoteCommandBase(editor), m_noteChanges(noteChanges), m_semitones(semitones)
{
}

void TransposeNotesCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<1>(change));
        track->addNote(std::get<2>(change));
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

void TransposeNotesCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<2>(change));
        track->addNote(std::get<1>(change));
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

QString TransposeNotesCommand::description() const {
    if (m_semitones > 0) {
        return QObject::tr("Transpose +%1").arg(m_semitones);
    } else {
        return QObject::tr("Transpose %1").arg(m_semitones);
    }
}

// ============================================================================
// QuantizeNotesCommand
// ============================================================================

QuantizeNotesCommand::QuantizeNotesCommand(MidiEditorWidget *editor,
                                           const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges)
    : MidiNoteCommandBase(editor), m_noteChanges(noteChanges)
{
}

void QuantizeNotesCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<1>(change));
        track->addNote(std::get<2>(change));
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

void QuantizeNotesCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<2>(change));
        track->addNote(std::get<1>(change));
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

// ============================================================================
// ChangeVelocityCommand
// ============================================================================

ChangeVelocityCommand::ChangeVelocityCommand(MidiEditorWidget *editor,
                                             const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges,
                                             int newVelocity)
    : MidiNoteCommandBase(editor), m_noteChanges(noteChanges), m_newVelocity(newVelocity)
{
}

void ChangeVelocityCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<1>(change));
        track->addNote(std::get<2>(change));
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

void ChangeVelocityCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<2>(change));
        track->addNote(std::get<1>(change));
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

QString ChangeVelocityCommand::description() const {
    return QObject::tr("Set Velocity to %1").arg(m_newVelocity);
}

// ============================================================================
// PasteNotesCommand
// ============================================================================

PasteNotesCommand::PasteNotesCommand(MidiEditorWidget *editor,
                                     const QList<QPair<NoteNagaTrack*, NN_Note_t>> &pastedNotes)
    : MidiNoteCommandBase(editor), m_pastedNotes(pastedNotes)
{
}

void PasteNotesCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &pair : m_pastedNotes) {
        if (pair.first) {
            pair.first->addNote(pair.second);
            affectedTracks.insert(pair.first);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

void PasteNotesCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &pair : m_pastedNotes) {
        if (pair.first) {
            pair.first->removeNote(pair.second);
            affectedTracks.insert(pair.first);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

// ============================================================================
// MoveNotesToTrackCommand
// ============================================================================

MoveNotesToTrackCommand::MoveNotesToTrackCommand(MidiEditorWidget *editor,
                                                 const QList<std::tuple<NoteNagaTrack*, NoteNagaTrack*, NN_Note_t, NN_Note_t>> &moves)
    : MidiNoteCommandBase(editor), m_moves(moves)
{
}

void MoveNotesToTrackCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &move : m_moves) {
        NoteNagaTrack *sourceTrack = std::get<0>(move);
        NoteNagaTrack *targetTrack = std::get<1>(move);
        const NN_Note_t &originalNote = std::get<2>(move);
        const NN_Note_t &newNote = std::get<3>(move);
        
        if (sourceTrack) {
            sourceTrack->removeNote(originalNote);
            affectedTracks.insert(sourceTrack);
        }
        if (targetTrack) {
            targetTrack->addNote(newNote);
            affectedTracks.insert(targetTrack);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}

void MoveNotesToTrackCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &move : m_moves) {
        NoteNagaTrack *sourceTrack = std::get<0>(move);
        NoteNagaTrack *targetTrack = std::get<1>(move);
        const NN_Note_t &originalNote = std::get<2>(move);
        const NN_Note_t &newNote = std::get<3>(move);
        
        if (targetTrack) {
            targetTrack->removeNote(newNote);
            affectedTracks.insert(targetTrack);
        }
        if (sourceTrack) {
            sourceTrack->addNote(originalNote);
            affectedTracks.insert(sourceTrack);
        }
    }
    computeMaxTick();
    refreshTracks(affectedTracks);
}
// ============================================================================
// ChangeNotePropertyCommand
// ============================================================================

ChangeNotePropertyCommand::ChangeNotePropertyCommand(MidiEditorWidget *editor,
                                                     PropertyType type,
                                                     const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges)
    : MidiNoteCommandBase(editor), m_propertyType(type), m_noteChanges(noteChanges)
{
}

void ChangeNotePropertyCommand::execute() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<1>(change));  // Remove old note
        track->addNote(std::get<2>(change));     // Add new note
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

void ChangeNotePropertyCommand::undo() {
    QSet<NoteNagaTrack*> affectedTracks;
    for (const auto &change : m_noteChanges) {
        NoteNagaTrack *track = std::get<0>(change);
        if (!track) continue;
        
        track->removeNote(std::get<2>(change));  // Remove new note
        track->addNote(std::get<1>(change));     // Restore old note
        affectedTracks.insert(track);
    }
    refreshTracks(affectedTracks);
}

QString ChangeNotePropertyCommand::description() const {
    QString propName = (m_propertyType == PropertyType::Velocity) ? QObject::tr("Velocity") : QObject::tr("Pan");
    if (m_noteChanges.size() == 1) {
        return QObject::tr("Change %1").arg(propName);
    }
    return QObject::tr("Change %1 (%2 notes)").arg(propName).arg(m_noteChanges.size());
}