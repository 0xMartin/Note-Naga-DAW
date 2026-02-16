#pragma once

#include "ai_types.h"
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>

class NoteNagaMidiSeq;
class MidiEditorWidget;

namespace NoteNagaAI {

class AiChatManager;
class AiCommandExecutor;
class GeminiApiClient;

/**
 * @brief Single message widget in the chat display.
 */
class ChatMessageWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit ChatMessageWidget(const ChatMessage &message, QWidget *parent = nullptr);
    
signals:
    void copyRequested(const QString &text);
    
private:
    void setupUserMessage(const ChatMessage &message);
    void setupAssistantMessage(const ChatMessage &message);
    
    QLabel *m_textLabel;
    QPushButton *m_copyBtn;
};

/**
 * @brief Overlay chat dialog for AI assistance.
 *        Displays chat history and allows sending prompts.
 */
class AiChatDialog : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Constructs AI chat dialog.
     * @param editor MIDI editor widget for context.
     * @param chatManager Shared chat manager instance.
     * @param parent Parent widget.
     */
    explicit AiChatDialog(MidiEditorWidget *editor, AiChatManager *chatManager, 
                          QWidget *parent = nullptr);
    ~AiChatDialog() override;
    
    /**
     * @brief Updates dialog for a new/different MIDI sequence.
     * @param sequence New active sequence.
     */
    void setSequence(NoteNagaMidiSeq *sequence);
    
    /**
     * @brief Gets current sequence.
     */
    NoteNagaMidiSeq* sequence() const { return m_sequence; }
    
    /**
     * @brief Updates dialog position and size.
     */
    void updateGeometry();

public slots:
    void show();
    void hide();
    void toggle();

signals:
    void visibilityChanged(bool visible);
    void commandsExecuted(int count);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onSendClicked();
    void onInputChanged();
    void onMessageAdded(int sequenceId, const ChatMessage &message);
    void onCopyClicked(const QString &text);
    void onClearClicked();
    void onApiResponseReceived(const QString &response, bool success, const QString &errorMessage);
    void onApiRequestStarted();
    void onApiRequestFinished();

private:
    void setupUi();
    void setupStyle();
    void refreshChatHistory();
    void addMessageWidget(const ChatMessage &message);
    void scrollToBottom();
    void processInput(const QString &text);
    void processUserPrompt(const QString &prompt);
    void processAiResponse(const QString &response);
    
    MidiEditorWidget *m_editor;
    AiChatManager *m_chatManager;
    NoteNagaMidiSeq *m_sequence;
    
    // UI elements
    QWidget *m_headerWidget;
    QLabel *m_titleLabel;
    QPushButton *m_closeBtn;
    QPushButton *m_clearBtn;
    
    QScrollArea *m_scrollArea;
    QWidget *m_messagesContainer;
    QVBoxLayout *m_messagesLayout;
    
    QTextEdit *m_inputEdit;
    QPushButton *m_sendBtn;
    QLabel *m_statusLabel;
    QTimer *m_spinnerTimer;
    int m_spinnerDots = 0;
    
    // Gemini API
    GeminiApiClient *m_apiClient;
    QString m_pendingPrompt;  // Store prompt while waiting for API response
    
    // State
    int m_currentSequenceId = -1;
};

} // namespace NoteNagaAI
