#include "midi_editor_command.h"

// Základní třída MidiEditorCommand
MidiEditorCommand::MidiEditorCommand(NoteNagaMidiSeq* seq, const QString& text) 
    : QUndoCommand(text), seq(seq) {
}

// AddNoteCommand
AddNoteCommand::AddNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, const NN_Note_t& note)
    : MidiEditorCommand(seq, QObject::tr("Add Note")), 
      track(track), note(note) {
}

void AddNoteCommand::undo() {
    track->removeNote(note);
}

void AddNoteCommand::redo() {
    if (!firstRun) {
        track->addNote(note);
    }
    firstRun = false;
}

// RemoveNoteCommand
RemoveNoteCommand::RemoveNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, const NN_Note_t& note)
    : MidiEditorCommand(seq, QObject::tr("Remove Note")), 
      track(track), note(note) {
}

void RemoveNoteCommand::undo() {
    track->addNote(note);
}

void RemoveNoteCommand::redo() {
    track->removeNote(note);
}

// MoveNoteCommand
MoveNoteCommand::MoveNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, 
                               const NN_Note_t& oldNote, const NN_Note_t& newNote)
    : MidiEditorCommand(seq, QObject::tr("Move Note")), 
      track(track), oldNote(oldNote), newNote(newNote) {
}

void MoveNoteCommand::undo() {
    track->removeNote(newNote);
    track->addNote(oldNote);
}

void MoveNoteCommand::redo() {
    track->removeNote(oldNote);
    track->addNote(newNote);
}

// ResizeNoteCommand
ResizeNoteCommand::ResizeNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, 
                                 const NN_Note_t& oldNote, const NN_Note_t& newNote)
    : MidiEditorCommand(seq, QObject::tr("Resize Note")), 
      track(track), oldNote(oldNote), newNote(newNote) {
}

void ResizeNoteCommand::undo() {
    track->removeNote(newNote);
    track->addNote(oldNote);
}

void ResizeNoteCommand::redo() {
    track->removeNote(oldNote);
    track->addNote(newNote);
}

// CompoundNoteCommand
CompoundNoteCommand::CompoundNoteCommand(NoteNagaMidiSeq* seq, const QString& text)
    : MidiEditorCommand(seq, text) {
}

void CompoundNoteCommand::addCommand(std::unique_ptr<MidiEditorCommand> cmd) {
    commands.push_back(std::move(cmd));
}

void CompoundNoteCommand::undo() {
    for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
        (*it)->undo();
    }
}

void CompoundNoteCommand::redo() {
    for (auto& cmd : commands) {
        cmd->redo();
    }
}