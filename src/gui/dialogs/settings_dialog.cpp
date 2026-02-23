#include "settings_dialog.h"
#include "../settings/settings_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setMinimumWidth(500);
    setModal(true);
    
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // === AI Settings Group ===
    auto *aiGroup = new QGroupBox(tr("Gemini AI"));
    auto *aiLayout = new QVBoxLayout(aiGroup);
    
    // API Key section
    auto *apiKeyLayout = new QHBoxLayout();
    
    auto *apiKeyForm = new QFormLayout();
    m_apiKeyEdit = new QLineEdit();
    m_apiKeyEdit->setPlaceholderText(tr("Enter your Gemini API key..."));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setMinimumWidth(300);
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, &SettingsDialog::onApiKeyChanged);
    apiKeyForm->addRow(tr("API Key:"), m_apiKeyEdit);
    apiKeyLayout->addLayout(apiKeyForm, 1);
    
    // Show/Hide toggle button
    auto *toggleVisibility = new QPushButton(tr("Show"));
    toggleVisibility->setFixedWidth(60);
    toggleVisibility->setCheckable(true);
    connect(toggleVisibility, &QPushButton::toggled, this, [this, toggleVisibility](bool checked) {
        m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        toggleVisibility->setText(checked ? tr("Hide") : tr("Show"));
    });
    apiKeyLayout->addWidget(toggleVisibility);
    
    aiLayout->addLayout(apiKeyLayout);
    
    // Get API Key button
    auto *getKeyLayout = new QHBoxLayout();
    getKeyLayout->addStretch();
    
    m_getApiKeyBtn = new QPushButton(tr("Get API Key from Google AI Studio"));
    m_getApiKeyBtn->setIcon(QIcon(":/icons/external-link.svg"));
    connect(m_getApiKeyBtn, &QPushButton::clicked, this, &SettingsDialog::onGetApiKey);
    getKeyLayout->addWidget(m_getApiKeyBtn);
    
    aiLayout->addLayout(getKeyLayout);
    
    // Model selection with rate limit info
    auto *modelLayout = new QFormLayout();
    m_modelCombo = new QComboBox();
    
    // Add models with display text showing rate limits
    struct ModelInfo {
        QString id;
        QString display;
        QString tooltip;
    };
    QList<ModelInfo> models = {
        {"gemini-2.5-pro",        "Gemini 2.5 Pro (15 RPM)",        tr("Best quality, 15 requests/min")},
        {"gemini-2.0-flash",      "Gemini 2.0 Flash (15 RPM)",      tr("Fast and reliable, 15 requests/min")},
        {"gemini-2.0-flash-lite", "Gemini 2.0 Flash Lite (15 RPM)", tr("Fastest, 15 requests/min")},
        {"gemini-2.0-flash-exp",  "Gemini 2.0 Flash Exp (15 RPM)",  tr("Experimental features, 15 requests/min")},
        {"gemini-3.0-pro",        "Gemini 3.0 Pro (15 RPM)",        tr("Latest Pro model, 15 requests/min")},
        {"gemini-2.5-flash",      "Gemini 2.5 Flash (5 RPM)",       tr("Lower rate limit - 5 requests/min")},
        {"gemini-3.0-flash",      "Gemini 3.0 Flash (5 RPM)",       tr("Latest Flash model - 5 requests/min")},
    };
    
    for (const auto &model : models) {
        m_modelCombo->addItem(model.display, model.id);
        m_modelCombo->setItemData(m_modelCombo->count() - 1, model.tooltip, Qt::ToolTipRole);
    }
    
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onModelChanged);
    modelLayout->addRow(tr("Model:"), m_modelCombo);
    
    // Rate limit hint
    m_rateLimitLabel = new QLabel();
    m_rateLimitLabel->setStyleSheet("color: #888888; font-size: 9pt;");
    modelLayout->addRow("", m_rateLimitLabel);
    updateRateLimitLabel();
    
    aiLayout->addLayout(modelLayout);
    
    // Test button and status
    auto *testLayout = new QHBoxLayout();
    m_testBtn = new QPushButton(tr("Test Connection"));
    m_testBtn->setEnabled(false);
    connect(m_testBtn, &QPushButton::clicked, this, &SettingsDialog::onTestApiKey);
    testLayout->addWidget(m_testBtn);
    
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #888888;");
    testLayout->addWidget(m_statusLabel, 1);
    
    aiLayout->addLayout(testLayout);
    
    // Info text
    auto *infoLabel = new QLabel(tr(
        "The Gemini API key enables automatic AI-powered MIDI generation.\n"
        "Without a key, you can still use manual copy/paste workflow."
    ));
    infoLabel->setStyleSheet("color: #666666; font-size: 9pt;");
    infoLabel->setWordWrap(true);
    aiLayout->addWidget(infoLabel);
    
    mainLayout->addWidget(aiGroup);
    
    // Spacer
    mainLayout->addStretch();
    
    // === Button Row ===
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_cancelBtn = new QPushButton(tr("Cancel"));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelBtn);
    
    m_applyBtn = new QPushButton(tr("Apply"));
    m_applyBtn->setEnabled(false);
    connect(m_applyBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        m_applyBtn->setEnabled(false);
        m_hasUnsavedChanges = false;
    });
    buttonLayout->addWidget(m_applyBtn);
    
    m_okBtn = new QPushButton(tr("OK"));
    m_okBtn->setDefault(true);
    connect(m_okBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        accept();
    });
    buttonLayout->addWidget(m_okBtn);
    
    mainLayout->addLayout(buttonLayout);
}

void SettingsDialog::loadSettings() {
    SettingsManager &settings = SettingsManager::instance();
    
    m_apiKeyEdit->setText(settings.geminiApiKey());
    
    // Find model by ID (stored in item data)
    int modelIndex = m_modelCombo->findData(settings.geminiModel());
    if (modelIndex >= 0) {
        m_modelCombo->setCurrentIndex(modelIndex);
    }
    
    updateApiKeyStatus();
    updateRateLimitLabel();
    m_hasUnsavedChanges = false;
    m_applyBtn->setEnabled(false);
}

void SettingsDialog::saveSettings() {
    SettingsManager &settings = SettingsManager::instance();
    
    settings.setGeminiApiKey(m_apiKeyEdit->text().trimmed());
    // Save model ID from item data, not display text
    settings.setGeminiModel(m_modelCombo->currentData().toString());
}

void SettingsDialog::onApiKeyChanged() {
    m_hasUnsavedChanges = true;
    m_applyBtn->setEnabled(true);
    updateApiKeyStatus();
}

void SettingsDialog::onModelChanged() {
    m_hasUnsavedChanges = true;
    m_applyBtn->setEnabled(true);
    updateRateLimitLabel();
}

void SettingsDialog::updateRateLimitLabel() {
    QString modelId = m_modelCombo->currentData().toString();
    bool isHighRpm = modelId.contains("2.0") || modelId.contains("2.5-pro") || modelId.contains("3.0-pro");
    
    if (modelId.contains("2.5-flash") || modelId.contains("3.0-flash")) {
        m_rateLimitLabel->setText(tr("⚠ Low rate limit (5/min) - may hit limits with frequent requests"));
        m_rateLimitLabel->setStyleSheet("color: #CC8800; font-size: 9pt;");
    } else {
        m_rateLimitLabel->setText(tr("✓ Good rate limit (15/min) - recommended for regular use"));
        m_rateLimitLabel->setStyleSheet("color: #00AA00; font-size: 9pt;");
    }
}

void SettingsDialog::updateApiKeyStatus() {
    bool hasKey = !m_apiKeyEdit->text().trimmed().isEmpty();
    m_testBtn->setEnabled(hasKey);
    
    if (!hasKey) {
        m_statusLabel->setText(tr("No API key configured"));
        m_statusLabel->setStyleSheet("color: #888888;");
    } else {
        m_statusLabel->setText(tr("API key set (click Test to verify)"));
        m_statusLabel->setStyleSheet("color: #888888;");
    }
}

void SettingsDialog::onTestApiKey() {
    QString apiKey = m_apiKeyEdit->text().trimmed();
    if (apiKey.isEmpty()) {
        return;
    }
    
    m_testBtn->setEnabled(false);
    m_statusLabel->setText(tr("Testing connection..."));
    m_statusLabel->setStyleSheet("color: #888888;");
    
    // Create network request to test API
    auto *manager = new QNetworkAccessManager(this);
    
    QString model = m_modelCombo->currentData().toString();
    QString url = QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2")
                      .arg(model)
                      .arg(apiKey);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // Simple test request
    QJsonObject contents;
    QJsonObject part;
    part["text"] = "Say 'Hello' in one word.";
    QJsonArray partsArray;
    partsArray.append(part);
    contents["parts"] = partsArray;
    QJsonArray contentsArray;
    contentsArray.append(contents);
    QJsonObject payload;
    payload["contents"] = contentsArray;
    
    QNetworkReply *reply = manager->post(request, QJsonDocument(payload).toJson());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
        m_testBtn->setEnabled(true);
        
        if (reply->error() == QNetworkReply::NoError) {
            m_statusLabel->setText(tr("✓ Connection successful!"));
            m_statusLabel->setStyleSheet("color: #00AA00; font-weight: bold;");
        } else {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString errorMsg;
            
            if (status == 400) {
                errorMsg = tr("Invalid API key");
            } else if (status == 403) {
                errorMsg = tr("API key not authorized");
            } else if (status == 404) {
                errorMsg = tr("Model not available - try a different model");
            } else if (status == 429) {
                errorMsg = tr("Rate limit exceeded - wait or try different model");
            } else {
                errorMsg = tr("Error: %1").arg(reply->errorString());
            }
            
            m_statusLabel->setText(tr("✗ %1").arg(errorMsg));
            m_statusLabel->setStyleSheet("color: #CC0000; font-weight: bold;");
        }
        
        reply->deleteLater();
        manager->deleteLater();
    });
}

void SettingsDialog::onGetApiKey() {
    QDesktopServices::openUrl(QUrl("https://aistudio.google.com/apikey"));
}
