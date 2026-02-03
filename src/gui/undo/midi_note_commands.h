#pragma once

#include "undo_manager.h"
#include <note_naga_engine/core/types.h>
#include <QList>
#include <QPair>
#include <functional>

class MidiEditorWidget;
class NoteNagaTrack;
class NoteNagaMidiSeq;

/**
 * @brief Base class for MIDI note commands with common functionality.
 */
class MidiNoteCommandBase : public UndoCommand {
protected:
    MidiEditorWidget *m_editor;
    
    // Helper to refresh affected tracks after execute/undo
    void refreshTracks(const QSet<NoteNagaTrack*> &tracks);
    void refreshTracks(NoteNagaTrack *track);
    void refreshAllTracks();
    void computeMaxTick();
    
public:
    explicit MidiNoteCommandBase(MidiEditorWidget *editor) : m_editor(editor) {}
};

/**
 * @brief Command for adding a single note.
 */
class AddNoteCommand : public MidiNoteCommandBase {
public:
    AddNoteCommand(MidiEditorWidget *editor, NoteNagaTrack *track, const NN_Note_t &note);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Add Note"); }
    
private:
    NoteNagaTrack *m_track;
    NN_Note_t m_note;
};

/**
 * @brief Command for deleting multiple notes.
 */
class DeleteNotesCommand : public MidiNoteCommandBase {
public:
    DeleteNotesCommand(MidiEditorWidget *editor, 
                       const QList<QPair<NoteNagaTrack*, NN_Note_t>> &notes);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    QList<QPair<NoteNagaTrack*, NN_Note_t>> m_notes;
};

/**
 * @brief Command for moving notes (changing position and/or pitch).
 */
class MoveNotesCommand : public MidiNoteCommandBase {
public:
    /**
     * @param editor The MIDI editor widget
     * @param noteChanges List of (track, originalNote, newNote) tuples
     */
    MoveNotesCommand(MidiEditorWidget *editor,
                     const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Move Notes"); }
    
private:
    // Each entry: track, original note, new note
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_noteChanges;
};

/**
 * @brief Command for resizing notes (changing length).
 */
class ResizeNotesCommand : public MidiNoteCommandBase {
public:
    ResizeNotesCommand(MidiEditorWidget *editor,
                       const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Resize Notes"); }
    
private:
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_noteChanges;
};

/**
 * @brief Command for duplicating notes.
 */
class DuplicateNotesCommand : public MidiNoteCommandBase {
public:
    DuplicateNotesCommand(MidiEditorWidget *editor,
                          const QList<QPair<NoteNagaTrack*, NN_Note_t>> &duplicatedNotes);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Duplicate Notes"); }
    
private:
    QList<QPair<NoteNagaTrack*, NN_Note_t>> m_duplicatedNotes;
};

/**
 * @brief Command for transposing notes.
 */
class TransposeNotesCommand : public MidiNoteCommandBase {
public:
    TransposeNotesCommand(MidiEditorWidget *editor,
                          const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges,
                          int semitones);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_noteChanges;
    int m_semitones;
};

/**
 * @brief Command for quantizing notes.
 */
class QuantizeNotesCommand : public MidiNoteCommandBase {
public:
    QuantizeNotesCommand(MidiEditorWidget *editor,
                         const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Quantize Notes"); }
    
private:
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_noteChanges;
};

/**
 * @brief Command for changing note velocity.
 */
class ChangeVelocityCommand : public MidiNoteCommandBase {
public:
    ChangeVelocityCommand(MidiEditorWidget *editor,
                          const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges,
                          int newVelocity);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_noteChanges;
    int m_newVelocity;
};

/**
 * @brief Command for pasting notes.
 */
class PasteNotesCommand : public MidiNoteCommandBase {
public:
    PasteNotesCommand(MidiEditorWidget *editor,
                      const QList<QPair<NoteNagaTrack*, NN_Note_t>> &pastedNotes);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Paste Notes"); }
    
private:
    QList<QPair<NoteNagaTrack*, NN_Note_t>> m_pastedNotes;
};

/**
 * @brief Command for moving notes to a different track.
 */
class MoveNotesToTrackCommand : public MidiNoteCommandBase {
public:
    /**
     * @param editor The MIDI editor widget
     * @param moves List of (sourceTrack, targetTrack, originalNote, newNote) tuples
     */
    MoveNotesToTrackCommand(MidiEditorWidget *editor,
                            const QList<std::tuple<NoteNagaTrack*, NoteNagaTrack*, NN_Note_t, NN_Note_t>> &moves);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Move Notes to Track"); }
    
private:
    // sourceTrack, targetTrack, originalNote, newNote
    QList<std::tuple<NoteNagaTrack*, NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_moves;
};
/**
 * @brief Command for changing note properties (velocity, pan, etc.) from property editor.
 *        Stores old and new notes for proper undo/redo.
 */
class ChangeNotePropertyCommand : public MidiNoteCommandBase {
public:
    enum class PropertyType { Velocity, Pan };
    
    ChangeNotePropertyCommand(MidiEditorWidget *editor,
                              PropertyType type,
                              const QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> &noteChanges);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    PropertyType m_propertyType;
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_noteChanges;  // track, oldNote, newNote
};