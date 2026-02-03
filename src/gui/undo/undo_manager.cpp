#include "undo_manager.h"

UndoManager::UndoManager(QObject *parent)
    : QObject(parent)
{
}

void UndoManager::executeCommand(std::unique_ptr<UndoCommand> command) {
    if (!command) return;
    
    QString desc = command->description();
    
    // Execute the command
    command->execute();
    
    // Add to undo stack
    m_undoStack.push_back(std::move(command));
    
    // Clear redo stack - new action invalidates redo history
    m_redoStack.clear();
    
    // Trim if necessary
    trimUndoStack();
    
    emit commandExecuted(desc);
    emit undoStateChanged();
}

void UndoManager::addCommandWithoutExecute(std::unique_ptr<UndoCommand> command) {
    if (!command) return;
    
    QString desc = command->description();
    
    // Add to undo stack WITHOUT executing (action already performed)
    m_undoStack.push_back(std::move(command));
    
    // Clear redo stack - new action invalidates redo history
    m_redoStack.clear();
    
    // Trim if necessary
    trimUndoStack();
    
    emit commandExecuted(desc);
    emit undoStateChanged();
}

bool UndoManager::undo() {
    if (m_undoStack.empty()) return false;
    
    // Pop from undo stack
    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    
    QString desc = command->description();
    
    // Undo the command
    command->undo();
    
    // Push to redo stack
    m_redoStack.push_back(std::move(command));
    
    emit undoPerformed(desc);
    emit undoStateChanged();
    return true;
}

bool UndoManager::redo() {
    if (m_redoStack.empty()) return false;
    
    // Pop from redo stack
    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    
    QString desc = command->description();
    
    // Re-execute the command
    command->execute();
    
    // Push back to undo stack
    m_undoStack.push_back(std::move(command));
    
    emit redoPerformed(desc);
    emit undoStateChanged();
    return true;
}

QString UndoManager::undoDescription() const {
    if (m_undoStack.empty()) return QString();
    return m_undoStack.back()->description();
}

QString UndoManager::redoDescription() const {
    if (m_redoStack.empty()) return QString();
    return m_redoStack.back()->description();
}

void UndoManager::clear() {
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoStateChanged();
}

void UndoManager::setMaxHistorySize(int size) {
    m_maxHistorySize = std::max(1, size);
    trimUndoStack();
}

void UndoManager::trimUndoStack() {
    while (static_cast<int>(m_undoStack.size()) > m_maxHistorySize) {
        m_undoStack.pop_front();
    }
}
