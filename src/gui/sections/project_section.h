#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QComboBox>
#include <QScrollArea>

#include <note_naga_engine/core/project_file_types.h>
#include "section_interface.h"

class NoteNagaEngine;
class RecentProjectsManager;
class NoteNagaProjectSerializer;

/**
 * @brief ProjectSection displays and allows editing of project metadata.
 * 
 * Redesigned as a single scrollable page with centered content (Bootstrap-like container).
 * Shows:
 * - Logo and title at the top
 * - Project name, author, description
 * - Creation and modification timestamps
 * - Project statistics (tracks, notes, duration)
 * - Synthesizer management
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
    
    /**
     * @brief Show save success dialog
     */
    void showSaveSuccess(const QString &filePath = QString());
    
    /**
     * @brief Show save error dialog
     */
    void showSaveError(const QString &error);
    
    /**
     * @brief Show export success dialog
     */
    void showExportSuccess(const QString &filePath);
    
    /**
     * @brief Show export error dialog
     */
    void showExportError(const QString &error);

signals:
    void saveRequested();
    void saveAsRequested();
    void exportMidiRequested();
    void metadataChanged();
    void unsavedChangesChanged(bool hasChanges);

private slots:
    void onMetadataEdited();
    void onSaveClicked();
    void onSaveAsClicked();
    void onExportMidiClicked();
    void onTempoChanged(double bpm);
    void onPPQChanged(int ppq);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    QWidget* createHeaderSection();
    QWidget* createActionsBar();
    QWidget* createMetadataSection();
    QWidget* createSequenceSettingsSection();
    QWidget* createStatisticsSection();
    QWidget* createFileInfoSection();
    QWidget* createCard(const QString &title, QWidget *content);
    void updateStatistics();
    void updateSequenceSettings();
    void refreshUI();
    void updateContainerWidth();

    NoteNagaEngine *m_engine;
    NoteNagaProjectSerializer *m_serializer;
    
    // Layout
    QScrollArea *m_scrollArea;
    QWidget *m_container;
    QVBoxLayout *m_containerLayout;
    QWidget *m_backgroundWidget;
    
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
    QLabel *m_fileFormatLabel;
    QLabel *m_fileVersionLabel;

    // UI Elements - Sequence Settings
    class QDoubleSpinBox *m_tempoSpinBox;
    class QSpinBox *m_ppqSpinBox;
    QLabel *m_timeSignatureLabel;

    // UI Elements - Statistics
    QLabel *m_trackCountLabel;
    QLabel *m_noteCountLabel;
    QLabel *m_durationLabel;

    // UI Elements - Actions
    QPushButton *m_saveBtn;
    QPushButton *m_saveAsBtn;
    QPushButton *m_exportMidiBtn;
};
