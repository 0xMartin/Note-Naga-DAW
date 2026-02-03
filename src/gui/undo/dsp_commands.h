#pragma once

#include "undo_manager.h"
#include <note_naga_engine/note_naga_engine.h>

#include <functional>

class DSPEngineWidget;
class NoteNagaDSPBlockBase;
class INoteNagaSoftSynth;

/**
 * @brief Command to add a DSP block.
 */
class AddDSPBlockCommand : public UndoCommand {
public:
    AddDSPBlockCommand(DSPEngineWidget *widget, NoteNagaEngine *engine,
                       NoteNagaDSPBlockBase *block, INoteNagaSoftSynth *synth = nullptr);
    ~AddDSPBlockCommand() override;
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    DSPEngineWidget *m_widget;
    NoteNagaEngine *m_engine;
    NoteNagaDSPBlockBase *m_block;
    INoteNagaSoftSynth *m_synth;  // nullptr for master
    bool m_ownsBlock;  // True if we need to delete the block on destruction
};

/**
 * @brief Command to remove a DSP block.
 */
class RemoveDSPBlockCommand : public UndoCommand {
public:
    RemoveDSPBlockCommand(DSPEngineWidget *widget, NoteNagaEngine *engine,
                          NoteNagaDSPBlockBase *block, INoteNagaSoftSynth *synth = nullptr);
    ~RemoveDSPBlockCommand() override;
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    DSPEngineWidget *m_widget;
    NoteNagaEngine *m_engine;
    NoteNagaDSPBlockBase *m_block;
    INoteNagaSoftSynth *m_synth;
    bool m_ownsBlock;
};

/**
 * @brief Command to remove all DSP blocks.
 */
class RemoveAllDSPBlocksCommand : public UndoCommand {
public:
    RemoveAllDSPBlocksCommand(DSPEngineWidget *widget, NoteNagaEngine *engine,
                              std::vector<NoteNagaDSPBlockBase*> blocks,
                              INoteNagaSoftSynth *synth = nullptr);
    ~RemoveAllDSPBlocksCommand() override;
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
private:
    DSPEngineWidget *m_widget;
    NoteNagaEngine *m_engine;
    std::vector<NoteNagaDSPBlockBase*> m_blocks;
    INoteNagaSoftSynth *m_synth;
    bool m_ownsBlocks;
};
