#include "ai_chat_dialog.h"
#include "ai_chat_manager.h"
#include "ai_prompt_generator.h"
#include "ai_command_parser.h"
#include "ai_command_executor.h"
#include "gemini_api_client.h"
#include "../editor/midi_editor_widget.h"
#include "../editor/ai_preview_state.h"
#include "../settings/settings_manager.h"

#include <note_naga_engine/core/types.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QDebug>

namespace NoteNagaAI {

// ============================================================================
// ChatMessageWidget
// ============================================================================

ChatMessageWidget::ChatMessageWidget(const ChatMessage &message, QWidget *parent)
    : QFrame(parent), m_textLabel(nullptr), m_copyBtn(nullptr)
{
    if (message.role == ChatMessage::Role::User) {
        setupUserMessage(message);
    } else {
        setupAssistantMessage(message);
    }
}

void ChatMessageWidget::setupUserMessage(const ChatMessage &message) {
    setObjectName("UserMessage");
    
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    
    // Header with "You" label
    auto *headerLayout = new QHBoxLayout();
    auto *roleLabel = new QLabel(tr("You"));
    roleLabel->setStyleSheet("font-weight: bold; color: #9090FF; font-size: 10pt;");
    headerLayout->addWidget(roleLabel);
    headerLayout->addStretch();
    
    // Copy button
    m_copyBtn = new QPushButton(tr("Copy Prompt"));
    m_copyBtn->setFixedHeight(24);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(80, 80, 100, 150);
            border: 1px solid rgba(120, 120, 150, 150);
            border-radius: 4px;
            padding: 2px 8px;
            color: #CCCCCC;
            font-size: 9pt;
        }
        QPushButton:hover {
            background-color: rgba(100, 100, 130, 180);
        }
    )");
    connect(m_copyBtn, &QPushButton::clicked, this, [this, fullPrompt = message.fullPrompt]() {
        emit copyRequested(fullPrompt);
    });
    headerLayout->addWidget(m_copyBtn);
    
    layout->addLayout(headerLayout);
    
    // Message text
    m_textLabel = new QLabel(message.displayText);
    m_textLabel->setWordWrap(true);
    m_textLabel->setStyleSheet("color: #E0E0E0; font-size: 10pt;");
    m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_textLabel);
    
    setStyleSheet(R"(
        #UserMessage {
            background-color: rgba(50, 50, 70, 180);
            border: 1px solid rgba(80, 80, 110, 150);
            border-radius: 8px;
            margin: 4px 8px 4px 40px;
        }
    )");
}

void ChatMessageWidget::setupAssistantMessage(const ChatMessage &message) {
    setObjectName("AssistantMessage");
    
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    
    // Header
    auto *headerLayout = new QHBoxLayout();
    auto *roleLabel = new QLabel(tr("AI Response"));
    roleLabel->setStyleSheet("font-weight: bold; color: #70C070; font-size: 10pt;");
    headerLayout->addWidget(roleLabel);
    
    // Execution status
    if (message.executed) {
        auto *statusLabel = new QLabel(tr("✓ Executed"));
        statusLabel->setStyleSheet("color: #70C070; font-size: 9pt;");
        headerLayout->addWidget(statusLabel);
    } else {
        auto *statusLabel = new QLabel(tr("✗ Failed"));
        statusLabel->setStyleSheet("color: #C07070; font-size: 9pt;");
        headerLayout->addWidget(statusLabel);
    }
    
    headerLayout->addStretch();
    layout->addLayout(headerLayout);
    
    // Message/summary text
    QString displayText = message.displayText;
    if (displayText.length() > 500) {
        displayText = displayText.left(500) + "...";
    }
    
    m_textLabel = new QLabel(displayText);
    m_textLabel->setWordWrap(true);
    m_textLabel->setStyleSheet("color: #D0D0D0; font-size: 10pt;");
    m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_textLabel);
    
    setStyleSheet(R"(
        #AssistantMessage {
            background-color: rgba(40, 60, 50, 180);
            border: 1px solid rgba(70, 100, 80, 150);
            border-radius: 8px;
            margin: 4px 40px 4px 8px;
        }
    )");
}

// ============================================================================
// AiChatDialog
// ============================================================================

AiChatDialog::AiChatDialog(MidiEditorWidget *editor, AiChatManager *chatManager, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_chatManager(chatManager)
    , m_sequence(nullptr)
    , m_statusLabel(nullptr)
    , m_apiClient(new GeminiApiClient(this))
{
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setFocusPolicy(Qt::StrongFocus);
    
    setupUi();
    setupStyle();
    
    // Add shadow effect for better visibility
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 180));
    setGraphicsEffect(shadow);
    
    // Install event filter on parent for resize
    if (parent) {
        parent->installEventFilter(this);
    }
    
    // Connect to chat manager signals
    if (m_chatManager) {
        connect(m_chatManager, &AiChatManager::messageAdded, 
                this, &AiChatDialog::onMessageAdded);
    }
    
    // Connect to API client signals
    connect(m_apiClient, &GeminiApiClient::responseReceived,
            this, &AiChatDialog::onApiResponseReceived);
    connect(m_apiClient, &GeminiApiClient::requestStarted,
            this, &AiChatDialog::onApiRequestStarted);
    connect(m_apiClient, &GeminiApiClient::requestFinished,
            this, &AiChatDialog::onApiRequestFinished);
    
    hide();
}

AiChatDialog::~AiChatDialog() = default;

void AiChatDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    m_headerWidget = new QWidget();
    m_headerWidget->setFixedHeight(32);
    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(12, 0, 0, 0);
    headerLayout->setSpacing(0);
    
    m_titleLabel = new QLabel(tr("AI Assistant"));
    m_titleLabel->setStyleSheet("font-weight: bold; color: #FFFFFF; font-size: 10pt;");
    headerLayout->addWidget(m_titleLabel);
    
    headerLayout->addStretch();
    
    // Duration combo box
    m_durationCombo = new QComboBox();
    m_durationCombo->setFixedWidth(90);
    m_durationCombo->setToolTip(tr("Target duration for generated music"));
    m_durationCombo->addItem(tr("Auto"), 0);
    m_durationCombo->addItem(tr("~10 sec"), 10);
    m_durationCombo->addItem(tr("~30 sec"), 30);
    m_durationCombo->addItem(tr("~1 min"), 60);
    m_durationCombo->addItem(tr("~2 min"), 120);
    m_durationCombo->addItem(tr("~3 min"), 180);
    m_durationCombo->addItem(tr("~5 min"), 300);
    m_durationCombo->setCurrentIndex(2); // Default to 30 sec
    headerLayout->addWidget(m_durationCombo);
    
    headerLayout->addSpacing(8);
    
    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setObjectName("clearBtn");
    m_clearBtn->setFixedHeight(32);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearBtn, &QPushButton::clicked, this, &AiChatDialog::onClearClicked);
    headerLayout->addWidget(m_clearBtn);
    
    m_closeBtn = new QPushButton("×");
    m_closeBtn->setObjectName("closeBtn");
    m_closeBtn->setFixedSize(32, 32);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QPushButton::clicked, this, &AiChatDialog::hide);
    headerLayout->addWidget(m_closeBtn);
    
    mainLayout->addWidget(m_headerWidget);
    
    // Messages area
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_messagesContainer = new QWidget();
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(4, 4, 4, 4);
    m_messagesLayout->setSpacing(8);
    m_messagesLayout->addStretch();
    
    m_scrollArea->setWidget(m_messagesContainer);
    mainLayout->addWidget(m_scrollArea, 1);
    
    // Input area - taller to fit 3 lines of text
    auto *inputWidget = new QWidget();
    inputWidget->setFixedHeight(120);
    auto *inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(8, 8, 8, 8);
    inputLayout->setSpacing(8);
    
    m_inputEdit = new QTextEdit();
    m_inputEdit->setPlaceholderText(tr("Enter your prompt or paste AI response..."));
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setMinimumHeight(60);
    connect(m_inputEdit, &QTextEdit::textChanged, this, &AiChatDialog::onInputChanged);
    inputLayout->addWidget(m_inputEdit);
    
    // Button row with spinner and status on left
    auto *btnLayout = new QHBoxLayout();
    
    // Spinning indicator
    m_spinnerLabel = new QLabel();
    m_spinnerLabel->setFixedWidth(20);
    m_spinnerLabel->setStyleSheet("color: #AAAAFF; font-size: 16pt;");
    m_spinnerLabel->hide();
    btnLayout->addWidget(m_spinnerLabel);
    
    // Status text - "Generating..."
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #AAAAFF; font-size: 9pt;");
    m_statusLabel->hide();
    btnLayout->addWidget(m_statusLabel);
    
    // Cancel button (shown during generation)
    m_cancelBtn = new QPushButton(tr("Cancel"));
    m_cancelBtn->setFixedSize(60, 28);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->hide();
    connect(m_cancelBtn, &QPushButton::clicked, this, &AiChatDialog::onCancelClicked);
    btnLayout->addWidget(m_cancelBtn);
    
    // Retry button (shown after error)
    m_retryBtn = new QPushButton(tr("Retry"));
    m_retryBtn->setFixedSize(60, 28);
    m_retryBtn->setCursor(Qt::PointingHandCursor);
    m_retryBtn->hide();
    connect(m_retryBtn, &QPushButton::clicked, this, &AiChatDialog::onRetryClicked);
    btnLayout->addWidget(m_retryBtn);
    
    btnLayout->addStretch();
    
    m_sendBtn = new QPushButton(tr("Send"));
    m_sendBtn->setFixedSize(80, 28);
    m_sendBtn->setEnabled(false);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    connect(m_sendBtn, &QPushButton::clicked, this, &AiChatDialog::onSendClicked);
    btnLayout->addWidget(m_sendBtn);
    
    inputLayout->addLayout(btnLayout);
    mainLayout->addWidget(inputWidget);
    
    // Install event filter on input for Enter key handling
    m_inputEdit->installEventFilter(this);
    
    // Spinner animation timer - rotates through spinner characters
    m_spinnerTimer = new QTimer(this);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]() {
        // Unicode spinner frames: ◐ ◓ ◑ ◒
        static const QChar spinnerChars[] = {QChar(0x25D0), QChar(0x25D3), QChar(0x25D1), QChar(0x25D2)};
        m_spinnerFrame = (m_spinnerFrame + 1) % 4;
        m_spinnerLabel->setText(QString(spinnerChars[m_spinnerFrame]));
    });
}

void AiChatDialog::setupStyle() {
    setStyleSheet(R"(
        AiChatDialog {
            background-color: rgba(25, 25, 30, 250);
            border: 2px solid rgba(100, 100, 130, 220);
            border-radius: 10px;
        }
        
        QScrollArea {
            background-color: rgba(30, 30, 38, 240);
            border: none;
        }
        
        QScrollArea > QWidget > QWidget {
            background-color: rgba(30, 30, 38, 240);
        }
        
        QTextEdit {
            background-color: rgba(40, 40, 50, 230);
            border: 2px solid rgba(80, 80, 100, 180);
            border-radius: 6px;
            color: #E0E0E0;
            padding: 8px;
            font-size: 10pt;
        }
        
        QTextEdit:focus {
            border: 2px solid rgba(140, 140, 180, 230);
        }
        
        QPushButton {
            background-color: rgba(70, 70, 90, 220);
            border: 1px solid rgba(100, 100, 130, 180);
            border-radius: 4px;
            color: #E0E0E0;
            font-size: 10pt;
        }
        
        QPushButton:hover {
            background-color: rgba(90, 90, 120, 240);
        }
        
        QPushButton:pressed {
            background-color: rgba(60, 60, 80, 250);
        }
        
        QPushButton:disabled {
            background-color: rgba(50, 50, 60, 150);
            color: #808080;
        }
        
        #closeBtn {
            font-size: 16pt;
            font-weight: bold;
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
            border-radius: 0px;
            border-top-right-radius: 8px;
            padding-bottom: 3px;
        }
        
        #clearBtn {
            font-size: 9pt;
            min-height: 32px;
            max-height: 32px;
            padding: 0 8px;
            border-radius: 0px;
        }
        
        QComboBox {
            background-color: rgba(50, 50, 65, 220);
            border: 1px solid rgba(80, 80, 110, 180);
            border-radius: 4px;
            color: #E0E0E0;
            padding: 4px 8px;
            font-size: 9pt;
        }
        
        QComboBox:hover {
            border: 1px solid rgba(120, 120, 160, 220);
        }
        
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 6px solid #AAAAAA;
            margin-right: 6px;
        }
        
        QComboBox QAbstractItemView {
            background-color: rgba(40, 40, 55, 250);
            border: 1px solid rgba(80, 80, 110, 180);
            selection-background-color: rgba(80, 80, 120, 200);
            color: #E0E0E0;
        }
    )");
    
    m_headerWidget->setStyleSheet(R"(
        background-color: rgba(35, 35, 45, 250);
        border-bottom: 2px solid rgba(90, 90, 120, 200);
        border-top-left-radius: 10px;
        border-top-right-radius: 10px;
    )");
}

void AiChatDialog::setSequence(NoteNagaMidiSeq *sequence) {
    if (m_sequence == sequence) return;
    
    m_sequence = sequence;
    m_currentSequenceId = sequence ? sequence->getId() : -1;
    
    // Refresh chat history for new sequence
    refreshChatHistory();
}

void AiChatDialog::updateGeometry() {
    QWidget *p = parentWidget();
    if (!p) return;
    
    // Position: right side, full height, 1/3 width (min 300px, max 400px)
    int dialogWidth = qBound(300, p->width() / 3, 400);
    int dialogHeight = p->height() - 20;
    int x = p->width() - dialogWidth - 10;
    int y = 10;
    
    setGeometry(x, y, dialogWidth, dialogHeight);
}

void AiChatDialog::show() {
    updateGeometry();
    QWidget::show();
    raise();
    m_inputEdit->setFocus();
    emit visibilityChanged(true);
}

void AiChatDialog::hide() {
    QWidget::hide();
    emit visibilityChanged(false);
}

void AiChatDialog::toggle() {
    if (isVisible()) {
        hide();
    } else {
        show();
    }
}

bool AiChatDialog::eventFilter(QObject *watched, QEvent *event) {
    // Handle parent resize
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        if (isVisible()) {
            updateGeometry();
        }
    }
    
    // Handle Enter key in input edit (send on Enter, newline on Shift+Enter)
    if (watched == m_inputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
            !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            if (m_sendBtn->isEnabled()) {
                onSendClicked();
            }
            return true;  // Consume the event
        }
    }
    
    return QWidget::eventFilter(watched, event);
}

void AiChatDialog::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    scrollToBottom();
}

void AiChatDialog::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
}

void AiChatDialog::onSendClicked() {
    QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;
    
    m_inputEdit->clear();
    processInput(text);
}

void AiChatDialog::onCancelClicked() {
    if (m_apiClient) {
        m_apiClient->cancelRequest();
    }
    onApiRequestFinished();
    
    // Add cancelled message to chat
    if (m_chatManager) {
        m_chatManager->addAssistantMessage(m_currentSequenceId, tr("Request cancelled."), false);
    }
}

void AiChatDialog::onRetryClicked() {
    if (!m_lastUserPrompt.isEmpty() && m_lastRequestFailed) {
        m_retryBtn->hide();
        processInput(m_lastUserPrompt);
    }
}

void AiChatDialog::onInputChanged() {
    m_sendBtn->setEnabled(!m_inputEdit->toPlainText().trimmed().isEmpty());
}

void AiChatDialog::onMessageAdded(int sequenceId, const ChatMessage &message) {
    if (sequenceId == m_currentSequenceId) {
        addMessageWidget(message);
        scrollToBottom();
    }
}

void AiChatDialog::onCopyClicked(const QString &text) {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
    
    // Visual feedback could be added here
}

void AiChatDialog::onClearClicked() {
    if (m_chatManager && m_currentSequenceId >= 0) {
        m_chatManager->clearHistory(m_currentSequenceId);
        refreshChatHistory();
    }
}

void AiChatDialog::refreshChatHistory() {
    // Clear existing message widgets
    QLayoutItem *item;
    while ((item = m_messagesLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    // Add stretch at top
    m_messagesLayout->addStretch();
    
    // Add existing messages
    if (m_chatManager && m_currentSequenceId >= 0) {
        const auto &history = m_chatManager->getChatHistory(m_currentSequenceId);
        for (const auto &msg : history) {
            addMessageWidget(msg);
        }
    }
    
    scrollToBottom();
}

void AiChatDialog::addMessageWidget(const ChatMessage &message) {
    auto *widget = new ChatMessageWidget(message, m_messagesContainer);
    connect(widget, &ChatMessageWidget::copyRequested, this, &AiChatDialog::onCopyClicked);
    
    // Insert before the stretch
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, widget);
}

void AiChatDialog::scrollToBottom() {
    QTimer::singleShot(50, this, [this]() {
        QScrollBar *vbar = m_scrollArea->verticalScrollBar();
        vbar->setValue(vbar->maximum());
    });
}

void AiChatDialog::processInput(const QString &text) {
    // Check if this looks like an AI response
    if (AiCommandParser::isAiResponse(text)) {
        processAiResponse(text);
    } else {
        processUserPrompt(text);
    }
}

void AiChatDialog::processUserPrompt(const QString &prompt) {
    if (!m_sequence || !m_chatManager) return;
    
    // Store for potential retry
    m_lastUserPrompt = prompt;
    m_lastRequestFailed = false;
    m_retryBtn->hide();
    
    // Get existing chat history for context
    const QList<ChatMessage> &history = m_chatManager->getChatHistory(m_currentSequenceId);
    
    // Get target duration from combo box
    int targetDuration = m_durationCombo->currentData().toInt();
    
    // Generate full prompt with instructions, data, and conversation history
    QString fullPrompt = AiPromptGenerator::generateFullPrompt(prompt, m_sequence, history, targetDuration);
    
    // Add to chat history
    m_chatManager->addUserMessage(m_currentSequenceId, prompt, fullPrompt);
    
    // If API is configured, automatically send request
    if (m_apiClient->isConfigured()) {
        m_pendingPrompt = fullPrompt;
        m_apiClient->sendPrompt(fullPrompt);
    }
}

void AiChatDialog::processAiResponse(const QString &response) {
    qDebug() << "=== processAiResponse called ===";
    qDebug() << "Response length:" << response.length();
    qDebug() << "Response preview (first 500 chars):" << response.left(500);
    
    if (!m_sequence || !m_chatManager || !m_editor) {
        qDebug() << "ERROR: Missing dependencies - sequence:" << m_sequence 
                 << "chatManager:" << m_chatManager << "editor:" << m_editor;
        return;
    }
    
    // Parse the response
    AiResponse parsed = AiCommandParser::parseResponse(response);
    
    qDebug() << "Parse result - isValid:" << parsed.isValid() 
             << "parseError:" << parsed.parseError
             << "commands count:" << parsed.commands.size();
    
    if (!parsed.isValid()) {
        // Show error in chat
        QString errorMsg = tr("Failed to parse AI response: %1").arg(parsed.parseError);
        qDebug() << "Showing error in chat:" << errorMsg;
        m_chatManager->addAssistantMessage(m_currentSequenceId, errorMsg, false);
        return;
    }
    
    qDebug() << "AI response parsed successfully, commands:" << parsed.commands.size();
    
    // Execute commands with preview mode
    AiCommandExecutor executor(m_editor, m_sequence);
    AiPreviewState *previewState = m_editor->getPreviewState();
    qDebug() << "PreviewState pointer:" << previewState;
    ExecutionResult result = executor.executeWithPreview(parsed, previewState);
    
    qDebug() << "Execution result - success:" << result.success 
             << "executed:" << result.commandsExecuted 
             << "failed:" << result.commandsFailed;
    
    // Create summary message
    QString summary;
    if (!parsed.message.isEmpty()) {
        summary = parsed.message + "\n\n";
    }
    summary += tr("Executed %1 of %2 commands")
        .arg(result.commandsExecuted)
        .arg(result.commandsExecuted + result.commandsFailed);
    
    if (!result.warnings.isEmpty()) {
        summary += "\n\n" + tr("Warnings:") + "\n" + result.warnings.join("\n");
    }
    
    m_chatManager->addAssistantMessage(m_currentSequenceId, summary, result.success);
    
    if (result.success && result.commandsExecuted > 0) {
        qDebug() << ">>> Calling enterPreviewMode()";
        // Enter preview mode to show visual diff
        m_editor->enterPreviewMode();
        emit commandsExecuted(result.commandsExecuted);
    } else {
        qDebug() << "NOT entering preview mode - success:" << result.success 
                 << "commandsExecuted:" << result.commandsExecuted;
    }
}

void AiChatDialog::onApiRequestStarted() {
    // Disable input while waiting for response
    m_inputEdit->setEnabled(false);
    m_sendBtn->setEnabled(false);
    
    // Show animated spinner and status
    m_spinnerFrame = 0;
    m_spinnerLabel->setText(QChar(0x25D0));  // Initial spinner frame
    m_spinnerLabel->show();
    m_statusLabel->setText(tr("Generating..."));
    m_statusLabel->show();
    m_spinnerTimer->start(150);  // Fast spin animation
    
    // Show cancel button, hide retry
    m_cancelBtn->show();
    m_retryBtn->hide();
}

void AiChatDialog::onApiRequestFinished() {
    // Stop spinner animation
    m_spinnerTimer->stop();
    
    // Re-enable input
    m_inputEdit->setEnabled(true);
    m_sendBtn->setEnabled(!m_inputEdit->toPlainText().trimmed().isEmpty());
    m_spinnerLabel->hide();
    m_statusLabel->hide();
    m_cancelBtn->hide();
}

void AiChatDialog::onApiResponseReceived(const QString &response, bool success, const QString &errorMessage) {
    m_pendingPrompt.clear();
    
    if (success) {
        m_lastRequestFailed = false;
        // Process the AI response automatically
        processAiResponse(response);
    } else {
        // Show error in chat and enable retry
        m_lastRequestFailed = true;
        m_retryBtn->show();
        if (m_chatManager) {
            QString errorMsg = tr("API Error: %1").arg(errorMessage);
            m_chatManager->addAssistantMessage(m_currentSequenceId, errorMsg, false);
        }
    }
}

void AiChatDialog::keyPressEvent(QKeyEvent *event) {
    // Escape closes the dialog
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace NoteNagaAI
