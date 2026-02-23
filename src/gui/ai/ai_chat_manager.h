#pragma once

#include "ai_types.h"
#include <QObject>
#include <QMap>
#include <memory>

class NoteNagaMidiSeq;
class MidiEditorWidget;

namespace NoteNagaAI {

/**
 * @brief Manages chat sessions for MIDI sequences.
 *        Each MIDI sequence has its own chat history that persists
 *        during the application session.
 */
class AiChatManager : public QObject {
    Q_OBJECT
    
public:
    explicit AiChatManager(QObject *parent = nullptr);
    ~AiChatManager() override;
    
    /**
     * @brief Gets or creates chat history for a sequence.
     * @param sequenceId MIDI sequence ID.
     * @return Reference to chat message list.
     */
    QList<ChatMessage>& getChatHistory(int sequenceId);
    
    /**
     * @brief Adds a user message to the chat history.
     * @param sequenceId MIDI sequence ID.
     * @param displayText User's prompt text (displayed).
     * @param fullPrompt Full prompt with instructions (for copy).
     * @return Reference to the added message.
     */
    ChatMessage& addUserMessage(int sequenceId, const QString &displayText, 
                                const QString &fullPrompt);
    
    /**
     * @brief Adds an AI response message to the chat history.
     * @param sequenceId MIDI sequence ID.
     * @param response The AI response text.
     * @param executed Whether the response was successfully executed.
     * @return Reference to the added message.
     */
    ChatMessage& addAssistantMessage(int sequenceId, const QString &response, 
                                      bool executed);
    
    /**
     * @brief Clears chat history for a sequence.
     * @param sequenceId MIDI sequence ID.
     */
    void clearHistory(int sequenceId);
    
    /**
     * @brief Clears all chat histories.
     */
    void clearAllHistories();
    
    /**
     * @brief Checks if a sequence has any chat history.
     * @param sequenceId MIDI sequence ID.
     * @return True if there's chat history.
     */
    bool hasHistory(int sequenceId) const;
    
    /**
     * @brief Gets the number of messages for a sequence.
     * @param sequenceId MIDI sequence ID.
     * @return Message count.
     */
    int messageCount(int sequenceId) const;

signals:
    /**
     * @brief Emitted when a new message is added.
     * @param sequenceId MIDI sequence ID.
     * @param message The new message.
     */
    void messageAdded(int sequenceId, const ChatMessage &message);
    
    /**
     * @brief Emitted when chat history is cleared.
     * @param sequenceId MIDI sequence ID.
     */
    void historyCleared(int sequenceId);

private:
    QMap<int, QList<ChatMessage>> m_chatHistories;
};

} // namespace NoteNagaAI
