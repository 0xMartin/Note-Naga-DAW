#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

namespace NoteNagaAI {

/**
 * @brief Client for Google Gemini API.
 *        Handles sending prompts and receiving AI-generated responses.
 */
class GeminiApiClient : public QObject {
    Q_OBJECT
    
public:
    explicit GeminiApiClient(QObject *parent = nullptr);
    ~GeminiApiClient() override = default;
    
    /**
     * @brief Checks if API is configured with valid key.
     */
    bool isConfigured() const;
    
    /**
     * @brief Sends a prompt to the Gemini API.
     *        Response will be emitted via responseReceived signal.
     * @param prompt The full prompt text to send.
     */
    void sendPrompt(const QString &prompt);
    
    /**
     * @brief Cancels any ongoing request.
     */
    void cancelRequest();
    
    /**
     * @brief Returns true if a request is currently in progress.
     */
    bool isRequestInProgress() const { return m_pendingReply != nullptr; }
    
signals:
    /**
     * @brief Emitted when a response is received from the API.
     * @param response The raw text response from AI.
     * @param success True if request was successful.
     * @param errorMessage Error message if request failed.
     */
    void responseReceived(const QString &response, bool success, const QString &errorMessage);
    
    /**
     * @brief Emitted when request starts.
     */
    void requestStarted();
    
    /**
     * @brief Emitted when request finishes (success or failure).
     */
    void requestFinished();

private slots:
    void onReplyFinished();

private:
    QString extractTextFromResponse(const QByteArray &data);
    
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_pendingReply = nullptr;
};

} // namespace NoteNagaAI
