#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <note_naga_engine/core/project_file_types.h>
#include <note_naga_engine/core/recent_projects_manager.h>

class NoteNagaEngine;

/**
 * @brief Wizard dialog shown at application startup for project selection/creation.
 * 
 * The wizard provides options to:
 * - Create a new empty project
 * - Open an existing .nnproj file
 * - Import a MIDI file as a new project
 * - Open a recent project from the list
 */
class ProjectWizardDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Result of the wizard
     */
    enum class WizardResult {
        None,
        NewProject,
        OpenProject,
        ImportMidi,
        OpenRecent
    };

    explicit ProjectWizardDialog(NoteNagaEngine *engine, 
                                  RecentProjectsManager *recentManager,
                                  QWidget *parent = nullptr);
    ~ProjectWizardDialog() override = default;

    /**
     * @brief Get the wizard result type
     */
    WizardResult getWizardResult() const { return m_wizardResult; }

    /**
     * @brief Get the project metadata (for new project)
     */
    NoteNagaProjectMetadata getProjectMetadata() const { return m_metadata; }

    /**
     * @brief Get the selected file path (for open/import)
     */
    QString getSelectedFilePath() const { return m_selectedFilePath; }

signals:
    /**
     * @brief Emitted when a project is ready to be loaded
     */
    void projectSelected(WizardResult result, const QString &filePath);

private slots:
    void onNewProjectClicked();
    void onOpenProjectClicked();
    void onImportMidiClicked();
    void onRecentProjectDoubleClicked(QListWidgetItem *item);
    void onRecentProjectRemove();
    void onCreateProject();
    void onBackToMain();

private:
    void setupMainPage();
    void setupNewProjectPage();
    void populateRecentProjects();
    void showMainPage();
    void showNewProjectPage();
    QString createStyledButton(const QString &baseStyle, bool primary = false);

    NoteNagaEngine *m_engine;
    RecentProjectsManager *m_recentManager;
    
    // Pages
    QStackedWidget *m_stackedWidget;
    QWidget *m_mainPage;
    QWidget *m_newProjectPage;
    
    // Main page widgets
    QListWidget *m_recentProjectsList;
    QPushButton *m_newProjectBtn;
    QPushButton *m_openProjectBtn;
    QPushButton *m_importMidiBtn;
    QPushButton *m_removeRecentBtn;
    
    // New project page widgets
    QLineEdit *m_projectNameEdit;
    QLineEdit *m_authorEdit;
    QPushButton *m_createProjectBtn;
    QPushButton *m_backBtn;
    
    // Result data
    WizardResult m_wizardResult = WizardResult::None;
    NoteNagaProjectMetadata m_metadata;
    QString m_selectedFilePath;
};
