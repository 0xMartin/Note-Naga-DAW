#include "main_window.h"

#include <QMenuBar>
#include <QToolBar>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QColor>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFileDialog>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), auto_follow(true)
{
    setWindowTitle("Note Naga");
    resize(1200, 800);
    QRect qr = frameGeometry();
    QRect cp = QApplication::primaryScreen()->availableGeometry();
    qr.moveCenter(cp.center());
    move(qr.topLeft());

    ctx = AppContext::instance();
    mixer = new Mixer(ctx);
    mixer->detect_outputs();
    playback_worker = new PlaybackWorker(
        ctx, mixer, 30 
    );

    setup_actions();
    setup_menu_bar();
    setup_toolbar();
    setup_dock_layout();
    connect_signals();
}

void MainWindow::setup_actions() {
    action_open = new QAction(QIcon(":/icons/open.svg"), "Open MIDI", this);
    connect(action_open, &QAction::triggered, this, &MainWindow::open_midi);
    action_export = new QAction(QIcon(":/icons/save.svg"), "Save MIDI", this);
    connect(action_export, &QAction::triggered, this, &MainWindow::export_midi);
    action_quit = new QAction("Quit", this);
    connect(action_quit, &QAction::triggered, this, &MainWindow::close);

    action_zoom_in = new QAction(QIcon(":/icons/zoom-in.svg"), "Zoom In", this);
    connect(action_zoom_in, &QAction::triggered, this, &MainWindow::zoom_in_x);
    action_zoom_out = new QAction(QIcon(":/icons/zoom-out.svg"), "Zoom Out", this);
    connect(action_zoom_out, &QAction::triggered, this, &MainWindow::zoom_out_x);

    action_auto_follow = new QAction("Auto-Follow Playback", this);
    action_auto_follow->setCheckable(true);
    action_auto_follow->setChecked(auto_follow);
    connect(action_auto_follow, &QAction::toggled, this, &MainWindow::set_auto_follow);

    action_reset_colors = new QAction("Reset Track Colors", this);
    connect(action_reset_colors, &QAction::triggered, this, &MainWindow::reset_all_colors);
    action_randomize_colors = new QAction("Randomize Track Colors", this);
    connect(action_randomize_colors, &QAction::triggered, this, &MainWindow::randomize_all_colors);

    action_about = new QAction("About", this);
    connect(action_about, &QAction::triggered, this, &MainWindow::about_dialog);
    action_homepage = new QAction("Open Homepage", this);
    connect(action_homepage, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/0xMartin/MIDI-TC-Interrupter"));
    });
    action_toolbar_to_start = new QAction(QIcon(":/icons/media-backward-end.svg"), "Go to Start", this);
    connect(action_toolbar_to_start, &QAction::triggered, this, &MainWindow::goto_start);
    action_toolbar_play = new QAction(QIcon(":/icons/play.svg"), "Play/Pause", this);
    // TODO: SpaceAction
    connect(action_toolbar_play, &QAction::triggered, this, &MainWindow::toggle_play);
    action_toolbar_to_end = new QAction(QIcon(":/icons/media-forward-end.svg"), "Go to End", this);
    connect(action_toolbar_to_end, &QAction::triggered, this, &MainWindow::goto_end);

    action_toggle_editor = new QAction("Show/Hide MIDI Editor", this);
    action_toggle_editor->setCheckable(true);
    action_toggle_editor->setChecked(true);
    connect(action_toggle_editor, &QAction::toggled, this, [this](bool checked){ show_hide_dock("editor", checked); });
    action_toggle_tracklist = new QAction("Show/Hide Track List", this);
    action_toggle_tracklist->setCheckable(true);
    action_toggle_tracklist->setChecked(true);
    connect(action_toggle_tracklist, &QAction::toggled, this, [this](bool checked){ show_hide_dock("tracklist", checked); });
    action_toggle_mixer = new QAction("Show/Hide Track Mixer", this);
    action_toggle_mixer->setCheckable(true);
    action_toggle_mixer->setChecked(true);
    connect(action_toggle_mixer, &QAction::toggled, this, [this](bool checked){ show_hide_dock("mixer", checked); });
    action_reset_layout = new QAction("Reset Layout", this);
    connect(action_reset_layout, &QAction::triggered, this, &MainWindow::reset_layout);
}

void MainWindow::setup_menu_bar() {
    QMenuBar* menubar = menuBar();
    QMenu* file_menu = menubar->addMenu("File");
    file_menu->addAction(action_open);
    file_menu->addAction(action_export);
    file_menu->addSeparator();
    file_menu->addAction(action_quit);

    QMenu* view_menu = menubar->addMenu("View");
    view_menu->addAction(action_zoom_in);
    view_menu->addAction(action_zoom_out);
    view_menu->addSeparator();
    view_menu->addAction(action_auto_follow);
    view_menu->addSeparator();
    view_menu->addAction(action_toggle_editor);
    view_menu->addAction(action_toggle_tracklist);
    view_menu->addAction(action_toggle_mixer);
    view_menu->addSeparator();
    view_menu->addAction(action_reset_layout);

    QMenu* tools_menu = menubar->addMenu("Tools");
    tools_menu->addAction(action_reset_colors);
    tools_menu->addAction(action_randomize_colors);

    QMenu* help_menu = menubar->addMenu("Help");
    help_menu->addAction(action_about);
    help_menu->addAction(action_homepage);
}

void MainWindow::setup_toolbar() {
    QToolBar* toolbar = new QToolBar("Playback");
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconSize(QSize(21, 21));
    toolbar->setMovable(false);
    addToolBar(Qt::TopToolBarArea, toolbar);

    toolbar->addAction(action_open);
    toolbar->addAction(action_export);
    toolbar->addSeparator();
    toolbar->addAction(action_zoom_in);
    toolbar->addAction(action_zoom_out);
}

void MainWindow::setup_dock_layout() {
    // Editor dock
    QWidget* editor_main = new QWidget();
    QGridLayout* grid = new QGridLayout(editor_main);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    midi_tact_ruler = new MidiTactRuler(ctx->ppq, 0.2, 10000, this);
    midi_keyboard_ruler = new MidiKeyboardRuler(ctx, mixer);
    midi_keyboard_ruler->setFixedWidth(80);
    midi_editor = new MidiEditorWidget(ctx);
    midi_editor->setMouseTracking(true);
    midi_editor->setMinimumWidth(250);
    midi_editor->setMinimumHeight(250);

    // -- GRID LAYOUT --
    grid->addWidget(new QWidget(), 0, 0);
    grid->addWidget(midi_tact_ruler, 0, 1);
    grid->addWidget(midi_keyboard_ruler, 1, 0);
    grid->addWidget(midi_editor, 1, 1);

    QVBoxLayout* editor_layout = new QVBoxLayout();
    editor_layout->setContentsMargins(0, 0, 0, 0);
    editor_layout->setSpacing(0);
    editor_layout->addWidget(editor_main, 1);
    control_bar = new MidiControlBarWidget(ctx, this);
    editor_layout->addWidget(control_bar);
    QWidget* editor_container = new QWidget();
    editor_container->setLayout(editor_layout);

    QDockWidget* editor_dock = new QDockWidget("MIDI Editor", this);
    editor_dock->setWidget(editor_container);
    editor_dock->setObjectName("editor");
    editor_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    editor_dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
    connect(editor_dock, &QDockWidget::visibilityChanged, this, [this](bool visible){ action_toggle_editor->setChecked(visible); });
    addDockWidget(Qt::RightDockWidgetArea, editor_dock);
    docks["editor"] = editor_dock;

    // Track list dock
    tracklist_widget = new TrackListWidget(ctx, mixer, this);
    QDockWidget* tracklist_dock = new QDockWidget("Track List", this);
    tracklist_dock->setWidget(tracklist_widget);
    tracklist_dock->setObjectName("tracklist");
    tracklist_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    tracklist_dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
    connect(tracklist_dock, &QDockWidget::visibilityChanged, this, [this](bool visible){ action_toggle_tracklist->setChecked(visible); });
    addDockWidget(Qt::LeftDockWidgetArea, tracklist_dock);
    docks["tracklist"] = tracklist_dock;

    // Mixer dock
    mixer_widget = new TrackMixerWidget(ctx, mixer, this);
    QDockWidget* mixer_dock = new QDockWidget("Track Mixer", this);
    mixer_dock->setWidget(mixer_widget);
    mixer_dock->setObjectName("mixer");
    mixer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mixer_dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
    connect(mixer_dock, &QDockWidget::visibilityChanged, this, [this](bool visible){ action_toggle_mixer->setChecked(visible); });
    addDockWidget(Qt::RightDockWidgetArea, mixer_dock);
    docks["mixer"] = mixer_dock;

    tabifyDockWidget(docks["editor"], docks["mixer"]);
    splitDockWidget(docks["tracklist"], docks["editor"], Qt::Horizontal);
    docks["editor"]->raise();
    docks["tracklist"]->setFloating(false);
    docks["mixer"]->setFloating(false);

    docks["editor"]->setVisible(true);
    docks["tracklist"]->setVisible(true);
    docks["mixer"]->setVisible(true);
    resizeDocks({docks["tracklist"], docks["editor"], docks["mixer"]}, {300, 800, 250}, Qt::Horizontal);
}

void MainWindow::show_hide_dock(const QString& name, bool checked) {
    QDockWidget* dock = docks.value(name, nullptr);
    if (dock) dock->setVisible(checked);
}

void MainWindow::reset_layout() {
    for (auto& dock : docks) dock->setVisible(true);
    action_toggle_editor->setChecked(true);
    action_toggle_tracklist->setChecked(true);
    action_toggle_mixer->setChecked(true);
    removeDockWidget(docks["editor"]);
    removeDockWidget(docks["tracklist"]);
    removeDockWidget(docks["mixer"]);
    addDockWidget(Qt::LeftDockWidgetArea, docks["tracklist"]);
    addDockWidget(Qt::RightDockWidgetArea, docks["editor"]);
    addDockWidget(Qt::RightDockWidgetArea, docks["mixer"]);
    tabifyDockWidget(docks["editor"], docks["mixer"]);
    docks["editor"]->raise();
    docks["tracklist"]->setFloating(false);
    docks["mixer"]->setFloating(false);
    resizeDocks({docks["tracklist"], docks["editor"], docks["mixer"]}, {300, 800, 250}, Qt::Horizontal);
}

void MainWindow::connect_signals() {
    connect(playback_worker, &PlaybackWorker::playing_state_changed_signal, this, &MainWindow::on_playing_state_changed);
    connect(playback_worker, &PlaybackWorker::on_position_changed_signal, this, &MainWindow::playback_worker_on_position_changed);
    connect(midi_editor, &MidiEditorWidget::set_play_position_signal, this, &MainWindow::set_play_position);
    connect(midi_keyboard_ruler, &MidiKeyboardRuler::play_note_signal, mixer, &Mixer::note_play);
    connect(midi_keyboard_ruler, &MidiKeyboardRuler::stop_note_signal, mixer, &Mixer::note_stop);
    connect(midi_tact_ruler, &MidiTactRuler::play_position_set_signal, this, &MainWindow::set_play_position);
    connect(control_bar, &MidiControlBarWidget::toggle_play_signal, this, &MainWindow::toggle_play);
    connect(control_bar, &MidiControlBarWidget::goto_start_signal, this, &MainWindow::goto_start);
    connect(control_bar, &MidiControlBarWidget::goto_end_signal, this, &MainWindow::goto_end);
    connect(control_bar, &MidiControlBarWidget::tempo_changed_signal, this, &MainWindow::on_tempo_changed);

    auto* hbar = midi_editor->horizontalScrollBar();
    auto* vbar = midi_editor->verticalScrollBar();
    connect(hbar, &QScrollBar::valueChanged, midi_tact_ruler, &MidiTactRuler::set_horizontal_scroll);
    connect(vbar, &QScrollBar::valueChanged,
            [this](int v){ midi_keyboard_ruler->set_vertical_scroll_slot(v, midi_editor->get_key_height()); });
}

void MainWindow::set_auto_follow(bool checked) { auto_follow = checked; }

void MainWindow::toggle_play() {
    if (playback_worker->is_playing()) {
        playback_worker->stop();
    } else {
        playback_worker->play();
    }
}

void MainWindow::zoom_in_x() {
    double scale = std::min(2.0, midi_editor->get_time_scale() * 1.3);
    midi_editor->set_time_scale_slot(scale);
    midi_tact_ruler->set_params(ctx->ppq, midi_editor->get_time_scale(), ctx->max_tick);
}

void MainWindow::zoom_out_x() {
    double scale = std::max(0.02, midi_editor->get_time_scale() / 1.3);
    midi_editor->set_time_scale_slot(scale);
    midi_tact_ruler->set_params(ctx->ppq, midi_editor->get_time_scale(), ctx->max_tick);
}

void MainWindow::on_playing_state_changed(bool playing) {
    action_toolbar_play->setIcon(QIcon(playing ? ":/icons/stop.svg" : ":/icons/play.svg"));
    if (!playing) {
        midi_keyboard_ruler->clear_highlights_slot();
        mixer->stop_all_notes();
    }
    control_bar->set_playing(playing);
}

void MainWindow::set_play_position(int tick) {
    playback_worker->stop();
    ctx->current_tick = tick;
    midi_editor->update_marker_slot(); // <-- přidej toto!
    midi_editor->update(); // volitelné, pokud chceš refresh celé scény, lze i vynechat
    midi_tact_ruler->set_horizontal_scroll_slot(midi_editor->horizontalScrollBar()->value());
    midi_keyboard_ruler->set_vertical_scroll_slot(midi_editor->verticalScrollBar()->value(), midi_editor->get_key_height());
    control_bar->update_times(ctx->current_tick, ctx->max_tick, ctx->tempo, ctx->ppq);
    update();
}

void MainWindow::goto_start() {
    set_play_position(0);
    midi_editor->horizontalScrollBar()->setValue(0);
}

void MainWindow::goto_end() {
    set_play_position(ctx->max_tick);
    midi_editor->horizontalScrollBar()->setValue(midi_editor->horizontalScrollBar()->maximum());
}

void MainWindow::on_tempo_changed(float new_tempo) {
    if (playback_worker) playback_worker->recalculate_worker_tempo();
}

void MainWindow::open_midi() {
    QString fname = QFileDialog::getOpenFileName(this, "Open MIDI file", "", "MIDI Files (*.mid *.midi)");
    if (fname.isEmpty()) return;
    playback_worker->stop();
    ctx->load_from_midi(fname);
    midi_editor->update();
    QScrollBar* vertical_bar = midi_editor->verticalScrollBar();
    int center_pos = (vertical_bar->maximum() + vertical_bar->minimum()) / 2;
    vertical_bar->setSliderPosition(center_pos);
    midi_tact_ruler->set_params(ctx->ppq, midi_editor->get_time_scale(), ctx->max_tick);
    midi_tact_ruler->set_horizontal_scroll(0);
    control_bar->update_times(ctx->current_tick, ctx->max_tick, ctx->tempo, ctx->ppq);
}

void MainWindow::export_midi() {
    QString fname = QFileDialog::getSaveFileName(this, "Export as MIDI", "", "MIDI Files (*.mid *.midi)");
    if (fname.isEmpty() || !ctx->midi_file) return;
}

void MainWindow::play() {
    if (playback_worker->is_playing()) return;
    playback_worker->play();
}

void MainWindow::playback_worker_on_position_changed(int current_tick) {
    ctx->current_tick = current_tick;
    control_bar->update_times(ctx->current_tick, ctx->max_tick, ctx->tempo, ctx->ppq);
    if (auto_follow) {
        int marker_x = int(ctx->current_tick * midi_editor->get_time_scale());
        int width = midi_editor->viewport()->width();
        int margin = width / 2;
        int value = std::max(0, marker_x - margin);
        midi_editor->horizontalScrollBar()->setValue(value);
    }
    midi_tact_ruler->set_horizontal_scroll_slot(midi_editor->horizontalScrollBar()->value());
    midi_keyboard_ruler->set_vertical_scroll_slot(midi_editor->verticalScrollBar()->value(), midi_editor->get_key_height());
    midi_editor->repaint_slot();
}

void MainWindow::reset_all_colors() {
    for (auto& tr : ctx->tracks) {
        tr->color = QColor(DEFAULT_CHANNEL_COLORS[tr->track_id % 16]);
    }
    midi_editor->update();
    QMessageBox::information(this, "Colors", "All track colors have been reset.");
}

void MainWindow::randomize_all_colors() {
    for (auto& tr : ctx->tracks) {
        QColor c(rand() % 206 + 50, rand() % 206 + 50, rand() % 206 + 50, 200);
        tr->color = c;
    }
    midi_editor->update();
    QMessageBox::information(this, "Colors", "Track colors have been randomized.");
}

void MainWindow::about_dialog() {
    QMessageBox::about(this, "About", "Note Naga\n\nAuthor: 0xMartin\nGitHub: https://github.com/0xMartin/note-naga");
}

void MainWindow::closeEvent(QCloseEvent* event) {
    playback_worker->stop();
    mixer->close();
    event->accept();
}