#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QMainWindow>
#include <QMenu>
#include <QStackedWidget>
#include <QTimer>

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/project_file_types.h>
#include <note_naga_engine/core/project_serializer.h>
#include <note_naga_engine/core/recent_projects_manager.h>

#include "sections/section_switcher.h"
#include "sections/midi_editor_section.h"
#include "sections/dsp_editor_section.h"
#include "sections/notation_section.h"
#include "sections/media_export_section.h"
#include "sections/project_section.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void set_auto_follow(bool checked);
    void toggle_play();
    void on_playing_state_changed(bool playing);
    void goto_start();
    void goto_end();
    void onControlBarPositionClicked(float seconds, int tick_position);
    void open_midi();
    void export_midi();
    void reset_all_colors();
    void randomize_all_colors();
    void about_dialog();
    void reset_layout();
    void show_hide_dock(const QString &name, bool checked);
    void export_video();
    
    // Section switching
    void onSectionChanged(AppSection section);
    
    // Undo/Redo
    void onUndo();
    void onRedo();
    void updateUndoRedoState();

    //Midi utility slots
    void util_quantize();
    void util_humanize();
    void util_transpose();
    void util_set_velocity();
    void util_scale_velocity();
    void util_set_duration();
    void util_scale_duration();
    void util_legato();
    void util_staccato();
    void util_invert();
    void util_retrograde();
    void util_delete_overlapping();
    void util_scale_timing();


private:
    NoteNagaEngine *engine;

    bool auto_follow;
    AppSection m_currentSection;

    // Project management
    NoteNagaProjectSerializer *m_projectSerializer;
    RecentProjectsManager *m_recentProjectsManager;
    NoteNagaProjectMetadata m_projectMetadata;
    QString m_currentProjectPath;
    bool m_hasUnsavedChanges = false;
    QTimer *m_autosaveTimer;

    // Section system
    QWidget *m_centralContainer;
    QStackedWidget *m_sectionStack;
    SectionSwitcher *m_sectionSwitcher;
    
    // Sections
    ProjectSection *m_projectSection;
    MidiEditorSection *m_midiEditorSection;
    DSPEditorSection *m_dspEditorSection;
    NotationSection *m_notationSection;
    MediaExportSection *m_mediaExportSection;

    // Actions
    QAction *action_open;
    QAction *action_export;
    QAction *action_export_video;
    QAction *action_quit;
    QAction *action_undo;
    QAction *action_redo;
    QAction *action_auto_follow;
    QAction *action_reset_colors;
    QAction *action_randomize_colors;
    QAction *action_about;
    QAction *action_homepage;
    QAction *action_toolbar_to_start;
    QAction *action_toolbar_play;
    QAction *action_toolbar_to_end;
    QAction *action_toggle_editor;
    QAction *action_toggle_tracklist;
    QAction *action_toggle_mixer;
    QAction *action_reset_layout;

    // === Nové akce pro MIDI utility ===
    QAction *action_quantize;
    QAction *action_humanize;
    QAction *action_transpose;
    QAction *action_set_velocity;
    QAction *action_scale_velocity;
    QAction *action_set_duration;
    QAction *action_scale_duration;
    QAction *action_legato;
    QAction *action_staccato;
    QAction *action_invert;
    QAction *action_retrograde;
    QAction *action_delete_overlapping;
    QAction *action_scale_timing;

    // Setup functions
    void setup_actions();
    void setup_menu_bar();
    void setup_toolbar();
    void setup_sections();
    void connect_signals();
    
    // Project management
    bool showProjectWizard();
    void createNewProject(const NoteNagaProjectMetadata &metadata);
    bool openProject(const QString &filePath);
    bool saveProject();
    bool saveProjectAs();
    void onAutosave();
    void updateWindowTitle();
    void onProjectUnsavedChanged(bool hasChanges);
    void onProjectMetadataChanged();
    void onProjectSaveRequested();
    void onProjectSaveAsRequested();
    void onProjectExportMidiRequested();
};