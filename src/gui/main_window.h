#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>

#include "widgets/midi_control_bar_widget.h"
#include "widgets/midi_editor_widget.h"
#include "widgets/midi_keyboard_ruler.h"
#include "widgets/midi_tact_ruler.h"
#include "widgets/track_list_widget.h"
#include "widgets/track_mixer_widget.h"
#include <note_naga_engine/note_naga_engine.h>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void set_auto_follow(bool checked);
    void toggle_play();
    void zoom_in_x();
    void zoom_out_x();
    void on_playing_state_changed(bool playing);
    void goto_start();
    void goto_end();
    void open_midi();
    void export_midi();
    void current_tick_position_changed(int current_tick);
    void reset_all_colors();
    void randomize_all_colors();
    void about_dialog();
    void reset_layout();
    void show_hide_dock(const QString &name, bool checked);

private:
    NoteNagaEngine *engine;

    bool auto_follow;
    QMap<QString, QDockWidget *> docks;

    QAction *action_open;
    QAction *action_export;
    QAction *action_quit;
    QAction *action_zoom_in;
    QAction *action_zoom_out;
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

    MidiTactRuler *midi_tact_ruler;
    MidiKeyboardRuler *midi_keyboard_ruler;
    MidiEditorWidget *midi_editor;
    MidiControlBarWidget *control_bar;

    TrackListWidget *tracklist_widget;
    TrackMixerWidget *mixer_widget;

    void setup_actions();
    void setup_menu_bar();
    void setup_toolbar();
    void setup_dock_layout();
    void connect_signals();
};