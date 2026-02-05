#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFrame>

class NoteNagaEngine;
class NoteNagaMidiSeq;

/**
 * @brief MidiSequenceSelector is a custom widget for selecting the active MIDI sequence.
 * 
 * This widget provides:
 * - A styled combo box showing available MIDI sequences
 * - Metadata display (duration, note count)
 * - Integration with the engine's active sequence
 * 
 * Designed to be placed in the SectionSwitcher for global access.
 */
class MidiSequenceSelector : public QFrame {
    Q_OBJECT

public:
    /**
     * @brief Constructs the MIDI sequence selector widget.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param parent Parent widget.
     */
    explicit MidiSequenceSelector(NoteNagaEngine* engine, QWidget* parent = nullptr);

    /**
     * @brief Updates the list of available sequences from the engine.
     */
    void refreshSequenceList();

    /**
     * @brief Sets the currently selected sequence by ID.
     * @param sequenceId The sequence ID to select.
     */
    void setCurrentSequenceById(int sequenceId);

    /**
     * @brief Gets the currently selected sequence.
     * @return Pointer to the selected sequence, or nullptr.
     */
    NoteNagaMidiSeq* getCurrentSequence() const;

signals:
    /**
     * @brief Emitted when the user selects a different sequence.
     * @param sequence Pointer to the newly selected sequence.
     */
    void sequenceSelected(NoteNagaMidiSeq* sequence);

private slots:
    void onComboIndexChanged(int index);
    void onEngineSequenceChanged(NoteNagaMidiSeq* sequence);

private:
    NoteNagaEngine* m_engine;
    
    // UI Components
    QLabel* m_iconLabel;
    QComboBox* m_sequenceCombo;
    QLabel* m_metadataLabel;
    
    void initUI();
    void setupConnections();
    void updateMetadataDisplay();
    QString formatDuration(int ticks, int ppq, int tempo) const;
};
