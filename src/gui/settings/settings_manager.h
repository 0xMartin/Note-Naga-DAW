#pragma once

#include <QObject>
#include <QString>
#include <QSettings>

/**
 * @brief Manages application settings with persistent storage.
 *        Uses QSettings for cross-platform storage in app data directory.
 */
class SettingsManager : public QObject {
    Q_OBJECT
    
public:
    static SettingsManager& instance();
    
    // Prevent copying
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    
    // Gemini AI settings
    QString geminiApiKey() const;
    void setGeminiApiKey(const QString &key);
    
    QString geminiModel() const;
    void setGeminiModel(const QString &model);
    
    bool isGeminiConfigured() const;
    
    // Available Gemini models
    static QStringList availableGeminiModels();
    static QString defaultGeminiModel();
    
signals:
    void geminiApiKeyChanged();
    void geminiModelChanged();
    void settingsChanged();

private:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() = default;
    
    QSettings m_settings;
    
    // Setting keys
    static constexpr const char* KEY_GEMINI_API_KEY = "ai/gemini_api_key";
    static constexpr const char* KEY_GEMINI_MODEL = "ai/gemini_model";
};
