#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QGridLayout>

#include <note_naga_engine/core/project_file_types.h>
#include "section_interface.h"

class NoteNagaEngine;
class RecentProjectsManager;
class NoteNagaProjectSerializer;

/**
 * @brief ProjectSection displays and allows editing of project metadata.
 * 
 * This section shows:
 * - Project name, author, description
 * - Creation and modification timestamps
 * - Project statistics (tracks, notes, duration)
 * - Quick actions (save, save as, export MIDI)
 */
class ProjectSection : public QWidget, public ISection {
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

private:
    void setupUI();
    void updateStatistics();
    void refreshUI();

    NoteNagaEngine *m_engine;
    NoteNagaProjectSerializer *m_serializer;
    
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

    // UI Elements - Actions
    QPushButton *m_saveBtn;
    QPushButton *m_saveAsBtn;
    QPushButton *m_exportMidiBtn;
};
