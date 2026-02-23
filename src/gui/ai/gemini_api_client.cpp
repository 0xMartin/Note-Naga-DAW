#include "gemini_api_client.h"
#include "../settings/settings_manager.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace NoteNagaAI {

GeminiApiClient::GeminiApiClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

bool GeminiApiClient::isConfigured() const {
    return SettingsManager::instance().isGeminiConfigured();
}

void GeminiApiClient::sendPrompt(const QString &prompt) {
    if (!isConfigured()) {
        emit responseReceived(QString(), false, tr("Gemini API key not configured"));
        return;
    }
    
    // Cancel any existing request
    cancelRequest();
    
    SettingsManager &settings = SettingsManager::instance();
    QString apiKey = settings.geminiApiKey();
    QString model = settings.geminiModel();
    
    // Build API URL
    QString url = QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2")
                      .arg(model)
                      .arg(apiKey);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // Build request payload
    QJsonObject contents;
    QJsonObject part;
    part["text"] = prompt;
    QJsonArray partsArray;
    partsArray.append(part);
    contents["parts"] = partsArray;
    
    QJsonArray contentsArray;
    contentsArray.append(contents);
    
    QJsonObject payload;
    payload["contents"] = contentsArray;
    
    // Optional: Add generation config for better JSON responses
    QJsonObject generationConfig;
    generationConfig["temperature"] = 0.7;
    generationConfig["topP"] = 0.95;
    generationConfig["topK"] = 40;
    generationConfig["maxOutputTokens"] = 8192;
    payload["generationConfig"] = generationConfig;
    
    QByteArray requestData = QJsonDocument(payload).toJson();
    
    emit requestStarted();
    
    m_pendingReply = m_networkManager->post(request, requestData);
    connect(m_pendingReply, &QNetworkReply::finished, this, &GeminiApiClient::onReplyFinished);
}

void GeminiApiClient::cancelRequest() {
    if (m_pendingReply) {
        m_pendingReply->abort();
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
}

void GeminiApiClient::onReplyFinished() {
    QNetworkReply *reply = m_pendingReply;
    m_pendingReply = nullptr;
    
    if (!reply) {
        return;
    }
    
    emit requestFinished();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString extractedText = extractTextFromResponse(data);
        
        if (!extractedText.isEmpty()) {
            emit responseReceived(extractedText, true, QString());
        } else {
            emit responseReceived(QString(), false, tr("Failed to parse API response"));
        }
    } else {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString errorMsg;
        
        // Try to extract error from response body
        QByteArray errorBody = reply->readAll();
        QJsonDocument errorDoc = QJsonDocument::fromJson(errorBody);
        if (errorDoc.isObject()) {
            QJsonObject errorObj = errorDoc.object();
            if (errorObj.contains("error")) {
                QJsonObject error = errorObj["error"].toObject();
                errorMsg = error["message"].toString();
            }
        }
        
        if (errorMsg.isEmpty()) {
            switch (status) {
                case 400:
                    errorMsg = tr("Invalid request. Check API key format.");
                    break;
                case 401:
                case 403:
                    errorMsg = tr("Invalid or unauthorized API key.");
                    break;
                case 404:
                    errorMsg = tr("Model not found. Try a different model.");
                    break;
                case 429:
                    errorMsg = tr("Rate limit exceeded. Please wait and try again.");
                    break;
                case 500:
                case 503:
                    errorMsg = tr("Server error. Please try again later.");
                    break;
                default:
                    errorMsg = tr("Network error: %1").arg(reply->errorString());
            }
        }
        
        emit responseReceived(QString(), false, errorMsg);
    }
    
    reply->deleteLater();
}

QString GeminiApiClient::extractTextFromResponse(const QByteArray &data) {
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        return QString();
    }
    
    if (!doc.isObject()) {
        return QString();
    }
    
    QJsonObject root = doc.object();
    
    // Navigate: candidates[0].content.parts[0].text
    if (!root.contains("candidates")) {
        return QString();
    }
    
    QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) {
        return QString();
    }
    
    QJsonObject candidate = candidates[0].toObject();
    if (!candidate.contains("content")) {
        return QString();
    }
    
    QJsonObject content = candidate["content"].toObject();
    if (!content.contains("parts")) {
        return QString();
    }
    
    QJsonArray parts = content["parts"].toArray();
    if (parts.isEmpty()) {
        return QString();
    }
    
    QJsonObject part = parts[0].toObject();
    return part["text"].toString();
}

} // namespace NoteNagaAI
