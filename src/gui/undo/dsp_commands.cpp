#include "dsp_commands.h"
#include "../widgets/dsp_engine_widget.h"
#include <note_naga_engine/core/dsp_block_base.h>

// === AddDSPBlockCommand ===

AddDSPBlockCommand::AddDSPBlockCommand(DSPEngineWidget *widget, NoteNagaEngine *engine,
                                       NoteNagaDSPBlockBase *block, INoteNagaSoftSynth *synth)
    : m_widget(widget), m_engine(engine), m_block(block), m_synth(synth), m_ownsBlock(true)
{
}

AddDSPBlockCommand::~AddDSPBlockCommand()
{
    // If we still own the block (not added to engine), delete it
    if (m_ownsBlock && m_block) {
        delete m_block;
    }
}

void AddDSPBlockCommand::execute()
{
    if (!m_engine || !m_block) return;
    
    if (m_synth == nullptr) {
        m_engine->getDSPEngine()->addDSPBlock(m_block);
    } else {
        m_engine->getDSPEngine()->addSynthDSPBlock(m_synth, m_block);
    }
    m_ownsBlock = false;  // Engine now owns the block
    
    if (m_widget) {
        m_widget->refresh();
    }
}

void AddDSPBlockCommand::undo()
{
    if (!m_engine || !m_block) return;
    
    if (m_synth == nullptr) {
        m_engine->getDSPEngine()->removeDSPBlock(m_block);
    } else {
        m_engine->getDSPEngine()->removeSynthDSPBlock(m_synth, m_block);
    }
    m_ownsBlock = true;  // We own the block again
    
    if (m_widget) {
        m_widget->refresh();
    }
}

QString AddDSPBlockCommand::description() const
{
    return QObject::tr("Add %1").arg(m_block ? QString::fromStdString(m_block->getBlockName()) : "DSP Block");
}

// === RemoveDSPBlockCommand ===

RemoveDSPBlockCommand::RemoveDSPBlockCommand(DSPEngineWidget *widget, NoteNagaEngine *engine,
                                             NoteNagaDSPBlockBase *block, INoteNagaSoftSynth *synth)
    : m_widget(widget), m_engine(engine), m_block(block), m_synth(synth), m_ownsBlock(false)
{
}

RemoveDSPBlockCommand::~RemoveDSPBlockCommand()
{
    if (m_ownsBlock && m_block) {
        delete m_block;
    }
}

void RemoveDSPBlockCommand::execute()
{
    if (!m_engine || !m_block) return;
    
    if (m_synth == nullptr) {
        m_engine->getDSPEngine()->removeDSPBlock(m_block);
    } else {
        m_engine->getDSPEngine()->removeSynthDSPBlock(m_synth, m_block);
    }
    m_ownsBlock = true;  // We now own the block
    
    if (m_widget) {
        m_widget->refresh();
    }
}

void RemoveDSPBlockCommand::undo()
{
    if (!m_engine || !m_block) return;
    
    if (m_synth == nullptr) {
        m_engine->getDSPEngine()->addDSPBlock(m_block);
    } else {
        m_engine->getDSPEngine()->addSynthDSPBlock(m_synth, m_block);
    }
    m_ownsBlock = false;  // Engine owns the block again
    
    if (m_widget) {
        m_widget->refresh();
    }
}

QString RemoveDSPBlockCommand::description() const
{
    return QObject::tr("Remove %1").arg(m_block ? QString::fromStdString(m_block->getBlockName()) : "DSP Block");
}

// === RemoveAllDSPBlocksCommand ===

RemoveAllDSPBlocksCommand::RemoveAllDSPBlocksCommand(DSPEngineWidget *widget, NoteNagaEngine *engine,
                                                     std::vector<NoteNagaDSPBlockBase*> blocks,
                                                     INoteNagaSoftSynth *synth)
    : m_widget(widget), m_engine(engine), m_blocks(std::move(blocks)), m_synth(synth), m_ownsBlocks(false)
{
}

RemoveAllDSPBlocksCommand::~RemoveAllDSPBlocksCommand()
{
    if (m_ownsBlocks) {
        for (auto *block : m_blocks) {
            delete block;
        }
    }
}

void RemoveAllDSPBlocksCommand::execute()
{
    if (!m_engine) return;
    
    for (auto *block : m_blocks) {
        if (m_synth == nullptr) {
            m_engine->getDSPEngine()->removeDSPBlock(block);
        } else {
            m_engine->getDSPEngine()->removeSynthDSPBlock(m_synth, block);
        }
    }
    m_ownsBlocks = true;
    
    if (m_widget) {
        m_widget->refresh();
    }
}

void RemoveAllDSPBlocksCommand::undo()
{
    if (!m_engine) return;
    
    for (auto *block : m_blocks) {
        if (m_synth == nullptr) {
            m_engine->getDSPEngine()->addDSPBlock(block);
        } else {
            m_engine->getDSPEngine()->addSynthDSPBlock(m_synth, block);
        }
    }
    m_ownsBlocks = false;
    
    if (m_widget) {
        m_widget->refresh();
    }
}

QString RemoveAllDSPBlocksCommand::description() const
{
    return QObject::tr("Remove All DSP Blocks (%1)").arg(m_blocks.size());
}
