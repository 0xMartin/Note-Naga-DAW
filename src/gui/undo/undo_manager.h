#pragma once

#include <QObject>
#include <QString>
#include <deque>
#include <memory>

/**
 * @brief Abstract base class for all undoable commands.
 */
class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    
    /**
     * @brief Execute the command (do/redo).
     */
    virtual void execute() = 0;
    
    /**
     * @brief Undo the command.
     */
    virtual void undo() = 0;
    
    /**
     * @brief Get a human-readable description of the command.
     * @return Description string for display in UI.
     */
    virtual QString description() const = 0;
};

/**
 * @brief Compound command that groups multiple commands into one undo step.
 */
class CompoundCommand : public UndoCommand {
public:
    explicit CompoundCommand(const QString &desc) : m_description(desc) {}
    
    void addCommand(std::unique_ptr<UndoCommand> cmd) {
        m_commands.push_back(std::move(cmd));
    }
    
    void execute() override {
        for (auto &cmd : m_commands) {
            cmd->execute();
        }
    }
    
    void undo() override {
        // Undo in reverse order
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
            (*it)->undo();
        }
    }
    
    QString description() const override { return m_description; }
    
    bool isEmpty() const { return m_commands.empty(); }
    size_t commandCount() const { return m_commands.size(); }
    
private:
    QString m_description;
    std::vector<std::unique_ptr<UndoCommand>> m_commands;
};

/**
 * @brief Manages undo/redo history for a single context (e.g., MIDI editor).
 *        Each editor/widget can have its own UndoManager instance.
 */
class UndoManager : public QObject {
    Q_OBJECT
    
    Q_PROPERTY(int maxHistorySize READ maxHistorySize WRITE setMaxHistorySize)
    
public:
    explicit UndoManager(QObject *parent = nullptr);
    ~UndoManager() = default;
    
    /**
     * @brief Execute a command and add it to the undo stack.
     * @param command The command to execute. Ownership is transferred.
     */
    void executeCommand(std::unique_ptr<UndoCommand> command);
    
    /**
     * @brief Add a command to the undo stack WITHOUT executing it.
     *        Used when the action has already been performed (e.g., during drag).
     * @param command The command to add. Ownership is transferred.
     */
    void addCommandWithoutExecute(std::unique_ptr<UndoCommand> command);
    
    /**
     * @brief Undo the last command.
     * @return true if undo was performed, false if nothing to undo.
     */
    bool undo();
    
    /**
     * @brief Redo the last undone command.
     * @return true if redo was performed, false if nothing to redo.
     */
    bool redo();
    
    /**
     * @brief Check if undo is available.
     */
    bool canUndo() const { return !m_undoStack.empty(); }
    
    /**
     * @brief Check if redo is available.
     */
    bool canRedo() const { return !m_redoStack.empty(); }
    
    /**
     * @brief Get description of the next undo action.
     */
    QString undoDescription() const;
    
    /**
     * @brief Get description of the next redo action.
     */
    QString redoDescription() const;
    
    /**
     * @brief Clear all history.
     */
    void clear();
    
    /**
     * @brief Get the maximum history size.
     */
    int maxHistorySize() const { return m_maxHistorySize; }
    
    /**
     * @brief Set the maximum history size.
     * @param size Maximum number of undo steps to keep.
     */
    void setMaxHistorySize(int size);
    
    /**
     * @brief Get current undo stack size.
     */
    int undoStackSize() const { return static_cast<int>(m_undoStack.size()); }
    
    /**
     * @brief Get current redo stack size.
     */
    int redoStackSize() const { return static_cast<int>(m_redoStack.size()); }

signals:
    /**
     * @brief Emitted when undo/redo availability changes.
     */
    void undoStateChanged();
    
    /**
     * @brief Emitted when a command is executed.
     */
    void commandExecuted(const QString &description);
    
    /**
     * @brief Emitted when undo is performed.
     */
    void undoPerformed(const QString &description);
    
    /**
     * @brief Emitted when redo is performed.
     */
    void redoPerformed(const QString &description);

private:
    void trimUndoStack();
    
    std::deque<std::unique_ptr<UndoCommand>> m_undoStack;
    std::deque<std::unique_ptr<UndoCommand>> m_redoStack;
    int m_maxHistorySize = 100;
};
