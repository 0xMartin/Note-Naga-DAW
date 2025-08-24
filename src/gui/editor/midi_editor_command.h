#pragma once

#include <QUndoCommand>
#include <QString>

#include <note_naga_engine/core/types.h>

/**
 * @brief Základní třída pro příkazy MIDI editoru
 * 
 * Tato třída slouží jako základ pro všechny příkazy, které mohou být provedeny
 * v MIDI editoru a lze je vrátit zpět nebo znovu provést.
 */
class MidiEditorCommand : public QUndoCommand {
public:
    MidiEditorCommand(NoteNagaMidiSeq* seq, const QString& text);
    virtual ~MidiEditorCommand() = default;
    
    // Odkaz na sekvenci, kterou příkaz modifikuje
    NoteNagaMidiSeq* sequence() const { return seq; }
    
protected:
    NoteNagaMidiSeq* seq;
};

/**
 * @brief Příkaz pro přidání noty
 */
class AddNoteCommand : public MidiEditorCommand {
public:
    AddNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, const NN_Note_t& note);
    
    void undo() override;
    void redo() override;
    
private:
    NoteNagaTrack* track;
    NN_Note_t note;
    bool firstRun = true;
};

/**
 * @brief Příkaz pro odstranění noty
 */
class RemoveNoteCommand : public MidiEditorCommand {
public:
    RemoveNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, const NN_Note_t& note);
    
    void undo() override;
    void redo() override;
    
private:
    NoteNagaTrack* track;
    NN_Note_t note;
};

/**
 * @brief Příkaz pro přesunutí noty
 */
class MoveNoteCommand : public MidiEditorCommand {
public:
    MoveNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, 
                   const NN_Note_t& oldNote, const NN_Note_t& newNote);
    
    void undo() override;
    void redo() override;
    
private:
    NoteNagaTrack* track;
    NN_Note_t oldNote;
    NN_Note_t newNote;
};

/**
 * @brief Příkaz pro změnu délky noty
 */
class ResizeNoteCommand : public MidiEditorCommand {
public:
    ResizeNoteCommand(NoteNagaMidiSeq* seq, NoteNagaTrack* track, 
                     const NN_Note_t& oldNote, const NN_Note_t& newNote);
    
    void undo() override;
    void redo() override;
    
private:
    NoteNagaTrack* track;
    NN_Note_t oldNote;
    NN_Note_t newNote;
};

/**
 * @brief Příkaz pro hromadné operace s více notami najednou
 */
class CompoundNoteCommand : public MidiEditorCommand {
public:
    CompoundNoteCommand(NoteNagaMidiSeq* seq, const QString& text);
    
    void addCommand(std::unique_ptr<MidiEditorCommand> cmd);
    void undo() override;
    void redo() override;
    
private:
    std::vector<std::unique_ptr<MidiEditorCommand>> commands;
};