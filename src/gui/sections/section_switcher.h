#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QIcon>
#include <QString>
#include <QList>
#include <QTimer>

class NoteNagaEngine;
class TrackStereoMeter;
class GlobalTransportBar;
class MidiSequenceSelector;

/**
 * @brief AppSection enum defines available application sections
 */
enum class AppSection {
    Project = 0,
    MidiEditor = 1,
    DspEditor = 2,
    Arrangement = 3,
    MediaExport = 4,
    Notation = 5
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
    explicit SectionSwitcher(NoteNagaEngine *engine = nullptr, QWidget *parent = nullptr);

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

    /**
     * @brief Gets the global transport bar for signal connections
     * @return Pointer to the GlobalTransportBar widget
     */
    GlobalTransportBar* getTransportBar() const { return m_transportBar; }

    /**
     * @brief Gets the MIDI sequence selector widget
     * @return Pointer to the MidiSequenceSelector widget
     */
    MidiSequenceSelector* getSequenceSelector() const { return m_sequenceSelector; }

signals:
    /**
     * @brief Emitted when user clicks on a section button
     * @param section The newly selected section
     */
    void sectionChanged(AppSection section);

private:
    NoteNagaEngine *m_engine;
    AppSection m_currentSection;
    QButtonGroup *m_buttonGroup;
    QList<SectionButton*> m_buttons;
    GlobalTransportBar *m_transportBar;
    MidiSequenceSelector *m_sequenceSelector;

    void setupUi();
};
