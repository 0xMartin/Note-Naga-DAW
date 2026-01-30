#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QIcon>
#include <QString>
#include <QList>

/**
 * @brief AppSection enum defines available application sections
 */
enum class AppSection {
    MidiEditor = 0,
    DspEditor = 1,
    MediaExport = 2
};

/**
 * @brief SectionButton is a styled button for section switching (DaVinci Resolve style)
 */
class SectionButton : public QPushButton {
    Q_OBJECT
public:
    explicit SectionButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);
};

/**
 * @brief SectionSwitcher provides a bottom bar with section switching buttons
 *        similar to DaVinci Resolve's page navigation.
 */
class SectionSwitcher : public QWidget {
    Q_OBJECT
public:
    explicit SectionSwitcher(QWidget *parent = nullptr);

    /**
     * @brief Sets the currently active section
     * @param section The section to activate
     */
    void setCurrentSection(AppSection section);

    /**
     * @brief Gets the currently active section
     * @return The current section
     */
    AppSection currentSection() const { return m_currentSection; }

signals:
    /**
     * @brief Emitted when user clicks on a section button
     * @param section The newly selected section
     */
    void sectionChanged(AppSection section);

private:
    AppSection m_currentSection;
    QButtonGroup *m_buttonGroup;
    QList<SectionButton*> m_buttons;

    void setupUi();
};
