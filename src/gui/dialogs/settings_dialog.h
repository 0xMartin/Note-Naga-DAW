#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

/**
 * @brief Settings dialog for configuring application preferences.
 *        Currently supports Gemini AI API configuration.
 */
class SettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override = default;

private slots:
    void onApiKeyChanged();
    void onModelChanged();
    void onTestApiKey();
    void onGetApiKey();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    void updateApiKeyStatus();
    void updateRateLimitLabel();
    
    // AI Settings
    QLineEdit *m_apiKeyEdit;
    QComboBox *m_modelCombo;
    QPushButton *m_testBtn;
    QPushButton *m_getApiKeyBtn;
    QLabel *m_statusLabel;
    QLabel *m_rateLimitLabel;
    
    // Buttons
    QPushButton *m_okBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_applyBtn;
    
    bool m_hasUnsavedChanges = false;
};
