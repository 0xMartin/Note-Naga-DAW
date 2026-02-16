#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

/**
 * @brief Banner displayed at top of MIDI editor during AI preview mode.
 *        Shows Keep/Undo buttons and instructions for the user.
 */
class AiPreviewBanner : public QWidget {
    Q_OBJECT
public:
    explicit AiPreviewBanner(QWidget *parent = nullptr);
    
    /**
     * @brief Set the message displayed in the banner.
     * @param message The message text (e.g., description of changes).
     */
    void setMessage(const QString &message);
    
    /**
     * @brief Set statistics about the preview changes.
     * @param added Number of notes added.
     * @param removed Number of notes removed.
     * @param modified Number of notes modified.
     */
    void setStats(int added, int removed, int modified);

signals:
    /**
     * @brief Emitted when user clicks Keep - apply changes.
     */
    void keepClicked();
    
    /**
     * @brief Emitted when user clicks Undo - discard changes.
     */
    void discardClicked();

private:
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_statsLabel;
    QPushButton *m_keepBtn;
    QPushButton *m_discardBtn;
};
