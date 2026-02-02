#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QComboBox>
#include <QMap>

#include <note_naga_engine/core/project_file_types.h>
#include "section_interface.h"

class NoteNagaEngine;
class RecentProjectsManager;
class NoteNagaProjectSerializer;
class AdvancedDockWidget;

/**
 * @brief ProjectSection displays and allows editing of project metadata.
 * 
 * This section shows:
 * - Project name, author, description
 * - Creation and modification timestamps
 * - Project statistics (tracks, notes, duration)
 * - Synthesizer management
 * - Quick actions (save, save as, export MIDI)
 * 
 * Uses AdvancedDockWidget for flexible layout.
 */
class ProjectSection : public QMainWindow, public ISection {
    Q_OBJECT

public:
    explicit ProjectSection(NoteNagaEngine *engine,
                            NoteNagaProjectSerializer *serializer,
                            QWidget *parent = nullptr);
    ~ProjectSection() override = default;

    // ISection interface
    void onSectionActivated() override;
    void onSectionDeactivated() override;

    /**
     * @brief Set project metadata for display
     */
    void setProjectMetadata(const NoteNagaProjectMetadata &metadata);

    /**
     * @brief Get current metadata (with any edits)
     */
    NoteNagaProjectMetadata getProjectMetadata() const;

    /**
     * @brief Set the current project file path
     */
    void setProjectFilePath(const QString &filePath);

    /**
     * @brief Get current project file path
     */
    QString getProjectFilePath() const { return m_projectFilePath; }

    /**
     * @brief Check if there are unsaved changes
     */
    bool hasUnsavedChanges() const { return m_hasUnsavedChanges; }

    /**
     * @brief Mark changes as saved
     */
    void markAsSaved();
    
    /**
     * @brief Refresh synthesizer list from engine
     */
    void refreshSynthesizerList();

signals:
    /**
     * @brief Emitted when save is requested
     */
    void saveRequested();

    /**
     * @brief Emitted when save-as is requested
     */
    void saveAsRequested();

    /**
     * @brief Emitted when MIDI export is requested
     */
    void exportMidiRequested();

    /**
     * @brief Emitted when metadata is changed
     */
    void metadataChanged();

    /**
     * @brief Emitted when project has unsaved changes
     */
    void unsavedChangesChanged(bool hasChanges);

private slots:
    void onMetadataEdited();
    void onSaveClicked();
    void onSaveAsClicked();
    void onExportMidiClicked();
    void onRenameSynthClicked();
    void onAddSynthClicked();
    void onRemoveSynthClicked();
    void onConfigureSynthClicked();
    void onSynthSelectionChanged();

private:
    void setupDockLayout();
    QWidget* createMetadataWidget();
    QWidget* createStatisticsWidget();
    QWidget* createSynthesizerWidget();
    QWidget* createActionsWidget();
    void updateStatistics();
    void refreshUI();

    NoteNagaEngine *m_engine;
    NoteNagaProjectSerializer *m_serializer;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Metadata
    NoteNagaProjectMetadata m_metadata;
    QString m_projectFilePath;
    bool m_hasUnsavedChanges = false;

    // UI Elements - Metadata
    QLineEdit *m_projectNameEdit;
    QLineEdit *m_authorEdit;
    QTextEdit *m_descriptionEdit;
    QLabel *m_createdAtLabel;
    QLabel *m_modifiedAtLabel;
    QLabel *m_filePathLabel;

    // UI Elements - Statistics
    QLabel *m_trackCountLabel;
    QLabel *m_noteCountLabel;
    QLabel *m_durationLabel;
    QLabel *m_tempoLabel;
    QLabel *m_ppqLabel;

    // UI Elements - Synthesizers
    QListWidget *m_synthList;
    QComboBox *m_synthTypeCombo;
    QPushButton *m_addSynthBtn;
    QPushButton *m_removeSynthBtn;
    QPushButton *m_renameSynthBtn;
    QPushButton *m_configureSynthBtn;

    // UI Elements - Actions
    QPushButton *m_saveBtn;
    QPushButton *m_saveAsBtn;
    QPushButton *m_exportMidiBtn;
};
