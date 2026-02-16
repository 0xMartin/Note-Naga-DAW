#include "ai_chat_manager.h"

namespace NoteNagaAI {

AiChatManager::AiChatManager(QObject *parent)
    : QObject(parent)
{
}

AiChatManager::~AiChatManager() = default;

QList<ChatMessage>& AiChatManager::getChatHistory(int sequenceId) {
    // Creates empty list if doesn't exist
    return m_chatHistories[sequenceId];
}

ChatMessage& AiChatManager::addUserMessage(int sequenceId, const QString &displayText, 
                                            const QString &fullPrompt) {
    ChatMessage msg(ChatMessage::Role::User, displayText, fullPrompt);
    auto &history = m_chatHistories[sequenceId];
    history.append(msg);
    
    emit messageAdded(sequenceId, history.last());
    return history.last();
}

ChatMessage& AiChatManager::addAssistantMessage(int sequenceId, const QString &response, 
                                                  bool executed) {
    ChatMessage msg(ChatMessage::Role::Assistant, response);
    msg.executed = executed;
    auto &history = m_chatHistories[sequenceId];
    history.append(msg);
    
    emit messageAdded(sequenceId, history.last());
    return history.last();
}

void AiChatManager::clearHistory(int sequenceId) {
    if (m_chatHistories.contains(sequenceId)) {
        m_chatHistories[sequenceId].clear();
        emit historyCleared(sequenceId);
    }
}

void AiChatManager::clearAllHistories() {
    for (auto it = m_chatHistories.begin(); it != m_chatHistories.end(); ++it) {
        it.value().clear();
        emit historyCleared(it.key());
    }
}

bool AiChatManager::hasHistory(int sequenceId) const {
    return m_chatHistories.contains(sequenceId) && !m_chatHistories[sequenceId].isEmpty();
}

int AiChatManager::messageCount(int sequenceId) const {
    if (m_chatHistories.contains(sequenceId)) {
        return m_chatHistories[sequenceId].size();
    }
    return 0;
}

} // namespace NoteNagaAI
