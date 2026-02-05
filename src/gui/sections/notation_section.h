#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QMap>
#include <QScrollArea>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/project_file_types.h>
#include "section_interface.h"

class AdvancedDockWidget;
class VerovioWidget;

/**
 * @brief NotationSection provides traditional music notation display.
 *        Uses Verovio C++ library for precise music engraving and synchronization.
 *        Features:
 *        - A4 page layout ready for PDF export
 *        - Piano grand staff with treble and bass clefs
 *        - Track visibility control
 *        - Zoom control via toolbar
 */
class NotationSection : public QMainWindow, public ISection
{
    Q_OBJECT

public:
    explicit NotationSection(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~NotationSection();

    // ISection interface
    void onSectionActivated() override;
    void onSectionDeactivated() override;
    
    /**
     * @brief Set project metadata for notation (used for composer/title)
     */
    void setProjectMetadata(const NoteNagaProjectMetadata &metadata);

private slots:
    void onPlaybackTickChanged(int tick);
    void onSequenceChanged();
    void applyNotationSettings();

private:
    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    bool m_sectionActive;
    
    // Project metadata
    NoteNagaProjectMetadata m_projectMetadata;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Main notation widget
    VerovioWidget *m_notationWidget;
    
    // Settings widgets
    QWidget *m_settingsWidget;
    QScrollArea *m_settingsScrollArea;
    
    // Track visibility
    QGroupBox *m_trackVisibilityGroup;
    QVBoxLayout *m_trackVisibilityLayout;
    QList<QCheckBox*> m_trackVisibilityCheckboxes;
    QList<int> m_checkboxToTrackIndex;  // Maps checkbox index to actual track index
    
    // Notation settings widgets
    QGroupBox *m_notationSettingsGroup;
    QComboBox *m_keySignatureCombo;
    QComboBox *m_timeSignatureCombo;
    QSpinBox *m_scaleSpinBox;
    QCheckBox *m_showTitleCheckbox;
    QCheckBox *m_showTempoCheckbox;
    QCheckBox *m_showInstrumentNamesCheckbox;
    QCheckBox *m_showComposerCheckbox;
    QComboBox *m_pageSizeCombo;
    QCheckBox *m_landscapeCheckbox;
    
    // No sequence placeholder
    QLabel *m_noSequenceLabel;
    
    // Flag to track if auto-render has been performed
    bool m_autoRenderDone;
    bool m_layoutInitialized = false;
    
    void setupUi();
    void setupDockLayout();
    void connectSignals();
    void refreshSequence();
    void updateTrackVisibilityCheckboxes();
    
protected:
    void showEvent(QShowEvent *event) override;
};
