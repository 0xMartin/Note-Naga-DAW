#include "settings_manager.h"

#include <QCoreApplication>

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, 
                 QCoreApplication::organizationName().isEmpty() ? "NoteNaga" : QCoreApplication::organizationName(),
                 QCoreApplication::applicationName().isEmpty() ? "NoteNaga" : QCoreApplication::applicationName())
{
}

QString SettingsManager::geminiApiKey() const {
    return m_settings.value(KEY_GEMINI_API_KEY, QString()).toString();
}

void SettingsManager::setGeminiApiKey(const QString &key) {
    if (geminiApiKey() != key) {
        m_settings.setValue(KEY_GEMINI_API_KEY, key);
        m_settings.sync();
        emit geminiApiKeyChanged();
        emit settingsChanged();
    }
}

QString SettingsManager::geminiModel() const {
    return m_settings.value(KEY_GEMINI_MODEL, defaultGeminiModel()).toString();
}

void SettingsManager::setGeminiModel(const QString &model) {
    if (geminiModel() != model) {
        m_settings.setValue(KEY_GEMINI_MODEL, model);
        m_settings.sync();
        emit geminiModelChanged();
        emit settingsChanged();
    }
}

bool SettingsManager::isGeminiConfigured() const {
    return !geminiApiKey().isEmpty();
}

QStringList SettingsManager::availableGeminiModels() {
    // Models sorted by recommended usage (higher RPM = better for frequent use)
    return {
        // 15 RPM models (recommended)
        "gemini-2.5-pro",         // 15 RPM, Unlimited TPM - best quality
        "gemini-2.0-flash",       // 15 RPM, Unlimited TPM - fast
        "gemini-2.0-flash-lite",  // 15 RPM, Unlimited TPM - fastest
        "gemini-2.0-flash-exp",   // 15 RPM, Unlimited TPM - experimental
        "gemini-3.0-pro",         // 15 RPM, Unlimited TPM
        // 5 RPM models (use sparingly)
        "gemini-2.5-flash",       // 5 RPM, 250K TPM
        "gemini-3.0-flash",       // 5 RPM, 250K TPM
    };
}

QString SettingsManager::defaultGeminiModel() {
    return "gemini-2.0-flash";  // 15 RPM - good balance of speed and rate limit
}
