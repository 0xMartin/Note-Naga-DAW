#include "main_window.h"

#include <QApplication>
#include <QColor>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGridLayout>
#include <QIcon>
#include <QInputDialog> 
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QTimer>
#include <QShortcut>

#include <note_naga_engine/nn_utils.h>

// Section widget includes for signal connections
#include "widgets/global_transport_bar.h"
#include "widgets/midi_tact_ruler.h"
#include "editor/midi_editor_widget.h"
#include "widgets/track_list_widget.h"
#include "dialogs/project_wizard_dialog.h"
#include "undo/undo_manager.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), auto_follow(true), m_currentSection(AppSection::Project) {
    setWindowTitle("Note Naga");
    resize(1200, 800);
    QRect qr = frameGeometry();
    QRect cp = QApplication::primaryScreen()->availableGeometry();
    qr.moveCenter(cp.center());
    move(qr.topLeft());

    this->engine = new NoteNagaEngine();
    this->engine->initialize();

    // Initialize project management
    m_projectSerializer = new NoteNagaProjectSerializer(engine);
    m_recentProjectsManager = new RecentProjectsManager();
    
    // Setup autosave timer (every 2 minutes)
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(2 * 60 * 1000); // 2 minutes
    connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::onAutosave);

    setup_actions();
    setup_menu_bar();
    setup_toolbar();
    setup_sections();
    connect_signals();
    
    // Hide main window until project is loaded
    hide();
    
    // Show project wizard on startup
    QTimer::singleShot(100, this, [this]() {
        if (!showProjectWizard()) {
            // User cancelled wizard - close application
            QApplication::quit();
        } else {
            // Project loaded successfully - show main window maximized
            showMaximized();
        }
    });
}

MainWindow::~MainWindow() {
    if (m_autosaveTimer) {
        m_autosaveTimer->stop();
    }
    if (m_projectSerializer) {
        delete m_projectSerializer;
        m_projectSerializer = nullptr;
    }
    if (m_recentProjectsManager) {
        delete m_recentProjectsManager;
        m_recentProjectsManager = nullptr;
    }
    if (this->engine) {
        delete this->engine;
        this->engine = nullptr; 
    }
}

void MainWindow::setup_actions() {
    action_open = new QAction(QIcon(":/icons/open.svg"), "Open MIDI", this);
    connect(action_open, &QAction::triggered, this, &MainWindow::open_midi);
    action_export = new QAction(QIcon(":/icons/save.svg"), "Save MIDI", this);
    connect(action_export, &QAction::triggered, this, &MainWindow::export_midi);
    action_export_video = new QAction(QIcon(":/icons/video.svg"), "Export as Video...", this);
    connect(action_export_video, &QAction::triggered, this, &MainWindow::export_video);
    action_quit = new QAction("Quit", this);
    connect(action_quit, &QAction::triggered, this, &MainWindow::close);

    // Undo/Redo actions
    action_undo = new QAction(QIcon(":/icons/undo.svg"), tr("Undo"), this);
    action_undo->setShortcut(QKeySequence::Undo);
    action_undo->setEnabled(false);
    connect(action_undo, &QAction::triggered, this, &MainWindow::onUndo);
    
    action_redo = new QAction(QIcon(":/icons/redo.svg"), tr("Redo"), this);
    action_redo->setShortcut(QKeySequence::Redo);
    action_redo->setEnabled(false);
    connect(action_redo, &QAction::triggered, this, &MainWindow::onRedo);

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
    action_toolbar_to_start =
        new QAction(QIcon(":/icons/media-backward-end.svg"), "Go to Start", this);
    connect(action_toolbar_to_start, &QAction::triggered, this, &MainWindow::goto_start);
    action_toolbar_play = new QAction(QIcon(":/icons/play.svg"), "Play/Pause", this);
    action_toolbar_play->setShortcut(Qt::Key_Space);
    action_toolbar_play->setShortcutContext(Qt::ApplicationShortcut);
    connect(action_toolbar_play, &QAction::triggered, this, &MainWindow::toggle_play);
    action_toolbar_to_end = new QAction(QIcon(":/icons/media-forward-end.svg"), "Go to End", this);
    connect(action_toolbar_to_end, &QAction::triggered, this, &MainWindow::goto_end);

    action_toggle_editor = new QAction("Show/Hide MIDI Editor", this);
    action_toggle_editor->setCheckable(true);
    action_toggle_editor->setChecked(true);
    connect(action_toggle_editor, &QAction::toggled, this,
            [this](bool checked) { show_hide_dock("editor", checked); });
    action_toggle_tracklist = new QAction("Show/Hide Track List", this);
    action_toggle_tracklist->setCheckable(true);
    action_toggle_tracklist->setChecked(true);
    connect(action_toggle_tracklist, &QAction::toggled, this,
            [this](bool checked) { show_hide_dock("tracklist", checked); });
    action_toggle_mixer = new QAction("Show/Hide Track Mixer", this);
    action_toggle_mixer->setCheckable(true);
    action_toggle_mixer->setChecked(true);
    connect(action_toggle_mixer, &QAction::toggled, this,
            [this](bool checked) { show_hide_dock("mixer", checked); });
    action_reset_layout = new QAction("Reset Layout", this);
    connect(action_reset_layout, &QAction::triggered, this, &MainWindow::reset_layout);

    // === Vytvoření nových akcí pro MIDI utility ===
    action_quantize = new QAction("Quantize...", this);
    connect(action_quantize, &QAction::triggered, this, &MainWindow::util_quantize);

    action_humanize = new QAction("Humanize...", this);
    connect(action_humanize, &QAction::triggered, this, &MainWindow::util_humanize);

    action_transpose = new QAction("Transpose...", this);
    connect(action_transpose, &QAction::triggered, this, &MainWindow::util_transpose);

    action_set_velocity = new QAction("Set Fixed Velocity...", this);
    connect(action_set_velocity, &QAction::triggered, this, &MainWindow::util_set_velocity);

    action_scale_velocity = new QAction("Scale Velocity...", this);
    connect(action_scale_velocity, &QAction::triggered, this, &MainWindow::util_scale_velocity);

    action_set_duration = new QAction("Set Fixed Duration...", this);
    connect(action_set_duration, &QAction::triggered, this, &MainWindow::util_set_duration);
    
    action_scale_duration = new QAction("Scale Duration...", this);
    connect(action_scale_duration, &QAction::triggered, this, &MainWindow::util_scale_duration);

    action_legato = new QAction("Legato...", this);
    connect(action_legato, &QAction::triggered, this, &MainWindow::util_legato);
    
    action_staccato = new QAction("Staccato...", this);
    connect(action_staccato, &QAction::triggered, this, &MainWindow::util_staccato);

    action_invert = new QAction("Invert Selection...", this);
    connect(action_invert, &QAction::triggered, this, &MainWindow::util_invert);

    action_retrograde = new QAction("Retrograde (Reverse)", this);
    connect(action_retrograde, &QAction::triggered, this, &MainWindow::util_retrograde);

    action_delete_overlapping = new QAction("Delete Overlapping Notes", this);
    connect(action_delete_overlapping, &QAction::triggered, this, &MainWindow::util_delete_overlapping);
    
    action_scale_timing = new QAction("Scale Timing...", this);
    connect(action_scale_timing, &QAction::triggered, this, &MainWindow::util_scale_timing);
}

void MainWindow::setup_menu_bar() {
    QMenuBar *menubar = menuBar();
    QMenu *file_menu = menubar->addMenu("File");
    file_menu->addAction(action_open);
    file_menu->addAction(action_export);
    file_menu->addAction(action_export_video);
    file_menu->addSeparator();
    file_menu->addAction(action_quit);

    QMenu *view_menu = menubar->addMenu("View");
    view_menu->addAction(action_auto_follow);
    view_menu->addSeparator();
    view_menu->addAction(action_toggle_editor);
    view_menu->addAction(action_toggle_tracklist);
    view_menu->addAction(action_toggle_mixer);
    view_menu->addSeparator();
    view_menu->addAction(action_reset_layout);

    QMenu *tools_menu = menubar->addMenu("Tools");
    
    // === Vytvoření podmenu pro MIDI utility ===
    QMenu *midi_util_menu = tools_menu->addMenu("MIDI Utilities");
    midi_util_menu->addAction(action_quantize);
    midi_util_menu->addAction(action_humanize);
    midi_util_menu->addSeparator();
    midi_util_menu->addAction(action_transpose);
    midi_util_menu->addAction(action_set_velocity);
    midi_util_menu->addAction(action_scale_velocity);
    midi_util_menu->addAction(action_set_duration);
    midi_util_menu->addAction(action_scale_duration);
    midi_util_menu->addSeparator();
    midi_util_menu->addAction(action_legato);
    midi_util_menu->addAction(action_staccato);
    midi_util_menu->addSeparator();
    midi_util_menu->addAction(action_invert);
    midi_util_menu->addAction(action_retrograde);
    midi_util_menu->addAction(action_scale_timing);
    midi_util_menu->addSeparator();
    midi_util_menu->addAction(action_delete_overlapping);
    
    tools_menu->addSeparator();
    tools_menu->addAction(action_reset_colors);
    tools_menu->addAction(action_randomize_colors);

    QMenu *help_menu = menubar->addMenu("Help");
    help_menu->addAction(action_about);
    help_menu->addAction(action_homepage);
}

void MainWindow::setup_toolbar() {
    QToolBar *toolbar = new QToolBar("Playback");
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconSize(QSize(21, 21));
    toolbar->setMovable(true);
    addToolBar(Qt::LeftToolBarArea, toolbar);

    toolbar->addAction(action_open);
    toolbar->addAction(action_export);
    toolbar->addSeparator();
}

void MainWindow::setup_sections() {
    // Create central container with vertical layout
    m_centralContainer = new QWidget(this);
    QVBoxLayout *centralLayout = new QVBoxLayout(m_centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    
    // Create stacked widget for sections
    m_sectionStack = new QStackedWidget();
    
    // Create sections
    m_projectSection = new ProjectSection(engine, m_projectSerializer, this);
    m_midiEditorSection = new MidiEditorSection(engine, this);
    m_dspEditorSection = new DSPEditorSection(engine, this);
    m_notationSection = new NotationSection(engine, this);
    m_mediaExportSection = new MediaExportSection(engine, this);
    m_arrangementSection = new ArrangementSection(engine, this);
    
    // Add sections to stack (order must match AppSection enum)
    m_sectionStack->addWidget(m_projectSection);      // index 0 - Project
    m_sectionStack->addWidget(m_midiEditorSection);   // index 1 - MidiEditor
    m_sectionStack->addWidget(m_dspEditorSection);    // index 2 - DspEditor
    m_sectionStack->addWidget(m_arrangementSection);  // index 3 - Arrangement
    m_sectionStack->addWidget(m_mediaExportSection);  // index 4 - MediaExport
    m_sectionStack->addWidget(m_notationSection);     // index 5 - Notation
    
    // Create section switcher (bottom bar)
    m_sectionSwitcher = new SectionSwitcher(engine, this);
    connect(m_sectionSwitcher, &SectionSwitcher::sectionChanged, 
            this, &MainWindow::onSectionChanged);
    
    // Add to central layout
    centralLayout->addWidget(m_sectionStack, 1);
    centralLayout->addWidget(m_sectionSwitcher);
    
    setCentralWidget(m_centralContainer);
    
    // Set initial section
    m_sectionStack->setCurrentIndex(0);
    m_currentSection = AppSection::Project;
    m_projectSection->onSectionActivated();  // Activate the initial section
    
    // Connect project section signals
    connect(m_projectSection, &ProjectSection::saveRequested, 
            this, &MainWindow::onProjectSaveRequested);
    connect(m_projectSection, &ProjectSection::saveAsRequested, 
            this, &MainWindow::onProjectSaveAsRequested);
    connect(m_projectSection, &ProjectSection::exportMidiRequested, 
            this, &MainWindow::onProjectExportMidiRequested);
    connect(m_projectSection, &ProjectSection::unsavedChangesChanged, 
            this, &MainWindow::onProjectUnsavedChanged);
    connect(m_projectSection, &ProjectSection::metadataChanged,
            this, &MainWindow::onProjectMetadataChanged);
    
    // Connect MIDI editor undo manager to update global keyboard shortcut state
    MidiEditorWidget *midiEditor = m_midiEditorSection->getMidiEditor();
    if (midiEditor && midiEditor->getUndoManager()) {
        connect(midiEditor->getUndoManager(), &UndoManager::undoStateChanged,
                this, &MainWindow::updateUndoRedoState);
    }
    
    // Connect arrangement section edit sequence request
    connect(m_arrangementSection, &ArrangementSection::switchToMidiEditor,
            this, [this](int sequenceIndex) {
        // Set active sequence
        auto sequences = engine->getRuntimeData()->getSequences();
        if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(sequences.size())) {
            engine->getRuntimeData()->setActiveSequence(sequences[sequenceIndex]);
        }
        // Switch to MIDI editor
        m_sectionSwitcher->setCurrentSection(AppSection::MidiEditor);
        onSectionChanged(AppSection::MidiEditor);
    });
}

void MainWindow::onSectionChanged(AppSection section) {
    // Skip if switching to the same section
    if (m_currentSection == section) {
        return;
    }
    
    // Deactivate previous section to save resources
    switch (m_currentSection) {
        case AppSection::Project:
            m_projectSection->onSectionDeactivated();
            break;
        case AppSection::MidiEditor:
            m_midiEditorSection->onSectionDeactivated();
            break;
        case AppSection::DspEditor:
            m_dspEditorSection->onSectionDeactivated();
            break;
        case AppSection::Arrangement:
            m_arrangementSection->onSectionDeactivated();
            break;
        case AppSection::Notation:
            m_notationSection->onSectionDeactivated();
            break;
        case AppSection::MediaExport:
            m_mediaExportSection->onSectionDeactivated();
            break;
    }
    
    // Switch to new section
    m_currentSection = section;
    m_sectionStack->setCurrentIndex(static_cast<int>(section));
    
    // Activate new section
    switch (section) {
        case AppSection::Project:
            m_projectSection->onSectionActivated();
            // Project section - allow both modes
            if (m_sectionSwitcher && m_sectionSwitcher->getTransportBar()) {
                m_sectionSwitcher->getTransportBar()->setAllowedPlaybackModes(3); // Both
            }
            break;
        case AppSection::MidiEditor:
            m_midiEditorSection->onSectionActivated();
            // MIDI Editor - only Sequence mode allowed
            if (m_sectionSwitcher && m_sectionSwitcher->getTransportBar()) {
                m_sectionSwitcher->getTransportBar()->setAllowedPlaybackModes(1); // Sequence only
                m_sectionSwitcher->getTransportBar()->setPlaybackMode(PlaybackMode::Sequence);
            }
            break;
        case AppSection::DspEditor:
            m_dspEditorSection->onSectionActivated();
            // DSP Editor - allow both modes
            if (m_sectionSwitcher && m_sectionSwitcher->getTransportBar()) {
                m_sectionSwitcher->getTransportBar()->setAllowedPlaybackModes(3); // Both
            }
            break;
        case AppSection::Arrangement:
            m_arrangementSection->onSectionActivated();
            // Arrangement - only Arrangement mode allowed
            if (m_sectionSwitcher && m_sectionSwitcher->getTransportBar()) {
                m_sectionSwitcher->getTransportBar()->setAllowedPlaybackModes(2); // Arrangement only
                m_sectionSwitcher->getTransportBar()->setPlaybackMode(PlaybackMode::Arrangement);
            }
            break;
        case AppSection::Notation:
            m_notationSection->onSectionActivated();
            // Notation section - only Sequence mode (for playing a single sequence)
            if (m_sectionSwitcher && m_sectionSwitcher->getTransportBar()) {
                m_sectionSwitcher->getTransportBar()->setAllowedPlaybackModes(1); // Sequence only
            }
            break;
        case AppSection::MediaExport:
            m_mediaExportSection->onSectionActivated();
            // Media Export - allow both modes
            if (m_sectionSwitcher && m_sectionSwitcher->getTransportBar()) {
                m_sectionSwitcher->getTransportBar()->setAllowedPlaybackModes(3); // Both
            }
            break;
    }
}

void MainWindow::show_hide_dock(const QString &name, bool checked) {
    // Delegate to MIDI editor section
    if (m_midiEditorSection) {
        m_midiEditorSection->showHideDock(name, checked);
    }
}

void MainWindow::export_video() {
    // Get active sequence from engine
    NoteNagaMidiSeq *active_sequence = this->engine->getRuntimeData()->getActiveSequence();

    // Check if any sequence is loaded
    if (!active_sequence) {
        QMessageBox::warning(this, "No Sequence", "Please open a MIDI file first.");
        return;
    }

    // Switch to Media Export section
    m_sectionSwitcher->setCurrentSection(AppSection::MediaExport);
    onSectionChanged(AppSection::MediaExport);
}

void MainWindow::reset_layout() {
    // Reset layout of MIDI editor section
    if (m_midiEditorSection) {
        m_midiEditorSection->resetLayout();
    }

    // Update menu checkboxes
    action_toggle_editor->setChecked(true);
    action_toggle_tracklist->setChecked(true);
    action_toggle_mixer->setChecked(true);
}

void MainWindow::connect_signals() {
    // Connect engine signals
    connect(engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::playingStateChanged, this,
            &MainWindow::on_playing_state_changed);

    // Connect global transport bar signals from section switcher
    GlobalTransportBar *transportBar = m_sectionSwitcher->getTransportBar();
    connect(transportBar, &GlobalTransportBar::playToggled, this, &MainWindow::toggle_play);
    connect(transportBar, &GlobalTransportBar::goToStart, this, &MainWindow::goto_start);
    connect(transportBar, &GlobalTransportBar::goToEnd, this, &MainWindow::goto_end);
    connect(transportBar, &GlobalTransportBar::playPositionChanged, this,
            &MainWindow::onControlBarPositionClicked);
    
    // Connect playback mode changes to DSP editor (hide track preview in arrangement mode)
    connect(transportBar, &GlobalTransportBar::playbackModeChanged, 
            m_dspEditorSection, &DSPEditorSection::setPlaybackMode);
}

void MainWindow::set_auto_follow(bool checked) { auto_follow = checked; }

void MainWindow::toggle_play() {
    if (engine->isPlaying()) {
        engine->stopPlayback();
    } else {
        engine->startPlayback();
    }
}

void MainWindow::on_playing_state_changed(bool playing) {
    action_toolbar_play->setIcon(QIcon(playing ? ":/icons/stop.svg" : ":/icons/play.svg"));
}

void MainWindow::goto_start() {
    GlobalTransportBar *transportBar = m_sectionSwitcher->getTransportBar();
    PlaybackMode mode = transportBar ? transportBar->getPlaybackMode() : PlaybackMode::Sequence;
    
    if (mode == PlaybackMode::Arrangement) {
        // Set arrangement position to 0
        if (engine->getRuntimeData()) {
            engine->getRuntimeData()->setCurrentArrangementTick(0);
        }
        // Scroll timeline to start
        if (m_arrangementSection) {
            m_arrangementSection->scrollToTick(0);
        }
    } else {
        // Set sequence position to 0
        this->engine->setPlaybackPosition(0);
        MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
        if (midi_editor) {
            midi_editor->horizontalScrollBar()->setValue(0);
        }
    }
}

void MainWindow::goto_end() {
    GlobalTransportBar *transportBar = m_sectionSwitcher->getTransportBar();
    PlaybackMode mode = transportBar ? transportBar->getPlaybackMode() : PlaybackMode::Sequence;
    
    if (mode == PlaybackMode::Arrangement) {
        // Set arrangement position to end
        if (engine->getRuntimeData()) {
            NoteNagaArrangement *arrangement = engine->getRuntimeData()->getArrangement();
            if (arrangement) {
                int maxTick = arrangement->getMaxTick();
                engine->getRuntimeData()->setCurrentArrangementTick(maxTick);
                // Scroll timeline to end
                if (m_arrangementSection) {
                    m_arrangementSection->scrollToTick(maxTick);
                }
            }
        }
    } else {
        // Set sequence position to end
        this->engine->setPlaybackPosition(this->engine->getRuntimeData()->getMaxTick());
        MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
        if (midi_editor) {
            midi_editor->horizontalScrollBar()->setValue(midi_editor->horizontalScrollBar()->maximum());
        }
    }
}

void MainWindow::onControlBarPositionClicked(float seconds, int tick_position) {
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    MidiTactRuler *midi_tact_ruler = m_midiEditorSection->getTactRuler();
    if (midi_editor && midi_tact_ruler) {
        int marker_x = int(tick_position * midi_editor->getConfig()->time_scale);
        int width = midi_editor->viewport()->width();
        int margin = width / 2;
        int value = std::max(0, marker_x - margin);
        midi_editor->horizontalScrollBar()->setValue(value);
        midi_tact_ruler->setHorizontalScroll(value);
    }
}

void MainWindow::open_midi() {
    QString fname =
        QFileDialog::getOpenFileName(this, "Open MIDI file", "", "MIDI Files (*.mid *.midi)");
    if (fname.isEmpty()) return;

    if (!engine->loadProject(fname.toStdString())) {
        QMessageBox::critical(this, "Error", "Failed to load MIDI file.");
        return;
    }

    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    MidiTactRuler *midi_tact_ruler = m_midiEditorSection->getTactRuler();
    if (midi_editor && midi_tact_ruler) {
        QScrollBar *vertical_bar = midi_editor->verticalScrollBar();
        int center_pos = (vertical_bar->maximum() + vertical_bar->minimum()) / 2;
        vertical_bar->setSliderPosition(center_pos);
        midi_tact_ruler->setHorizontalScroll(0);
    }
}

void MainWindow::export_midi() {
    NoteNagaMidiSeq *active_sequence = this->engine->getRuntimeData()->getActiveSequence();
    if (!active_sequence) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to export.");
        return;
    }

    QString fname =
        QFileDialog::getSaveFileName(this, "Export as MIDI", "", "MIDI Files (*.mid *.midi)");
    
    if (fname.isEmpty()) {
        return; // User cancelled
    }

    // Ensure file has .mid extension
    if (!fname.endsWith(".mid", Qt::CaseInsensitive) && 
        !fname.endsWith(".midi", Qt::CaseInsensitive)) {
        fname += ".mid";
    }

    if (active_sequence->exportToMidi(fname.toStdString())) {
        QMessageBox::information(this, "Export Successful", 
            "MIDI file exported successfully to:\n" + fname);
    } else {
        QMessageBox::critical(this, "Export Failed", 
            "Failed to export MIDI file. Check the log for details.");
    }
}

void MainWindow::reset_all_colors() {
    NoteNagaMidiSeq *active_sequence = this->engine->getRuntimeData()->getActiveSequence();
    if (!active_sequence) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence found.");
        return;
    }

    for (NoteNagaTrack *tr : active_sequence->getTracks()) {
        tr->setColor(DEFAULT_CHANNEL_COLORS[tr->getId() % 16]);
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    if (midi_editor) midi_editor->update();
    QMessageBox::information(this, "Colors", "All track colors have been reset.");
}

void MainWindow::randomize_all_colors() {
    NoteNagaMidiSeq *active_sequence = this->engine->getRuntimeData()->getActiveSequence();
    if (!active_sequence) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence found.");
        return;
    }

    for (NoteNagaTrack *tr : active_sequence->getTracks()) {
        QColor c(rand() % 206 + 50, rand() % 206 + 50, rand() % 206 + 50, 200);
        tr->setColor(NN_Color_t::fromQColor(c));
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    if (midi_editor) midi_editor->update();
    QMessageBox::information(this, "Colors", "Track colors have been randomized.");
}

void MainWindow::about_dialog() {
    QMessageBox::about(
        this, "About",
        "Note Naga\n\nAuthor: 0xMartin\nGitHub: https://github.com/0xMartin/note-naga");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Check for unsaved changes
    if (m_hasUnsavedChanges || m_projectSection->hasUnsavedChanges()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Unsaved Changes",
            "You have unsaved changes. Do you want to save before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );
        
        if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        } else if (reply == QMessageBox::Save) {
            if (!saveProject()) {
                // Save failed or cancelled
                event->ignore();
                return;
            }
        }
    }
    
    event->accept();
}

// === MIDI utility implementation ===

void MainWindow::util_quantize() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int divisor = QInputDialog::getInt(this, "Quantize Notes", "Grid divisor (4=16th, 8=32nd, 3=8th triplet):", 4, 1, 64, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::quantize(selectedNotes, seq->getPPQ(), divisor);
        } else {
            NN_Utils::quantize(*seq, divisor);
        }
    }
}

void MainWindow::util_humanize() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok_time, ok_vel;
    int time_strength = QInputDialog::getInt(this, "Humanize Time", "Max time deviation (ticks):", 5, 0, 100, 1, &ok_time);
    if (!ok_time) return;
    int vel_strength = QInputDialog::getInt(this, "Humanize Velocity", "Max velocity deviation:", 5, 0, 127, 1, &ok_vel);
    if (ok_vel) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::humanize(selectedNotes, time_strength, vel_strength);
        } else {
            NN_Utils::humanize(*seq, time_strength, vel_strength);
        }
    }
}

void MainWindow::util_transpose() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int semitones = QInputDialog::getInt(this, "Transpose", "Semitones (+/-):", 12, -127, 127, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::transpose(selectedNotes, semitones);
        } else {
            NN_Utils::transpose(*seq, semitones);
        }
    }
}

void MainWindow::util_set_velocity() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int value = QInputDialog::getInt(this, "Set Fixed Velocity", "New velocity (0-127):", 100, 0, 127, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::changeVelocity(selectedNotes, value, false);
        } else {
            NN_Utils::changeVelocity(*seq, value, false);
        }
    }
}

void MainWindow::util_scale_velocity() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int percent = QInputDialog::getInt(this, "Scale Velocity", "Scale factor (%):", 120, 0, 500, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::changeVelocity(selectedNotes, percent, true);
        } else {
            NN_Utils::changeVelocity(*seq, percent, true);
        }
    }
}

void MainWindow::util_set_duration() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int ticks = QInputDialog::getInt(this, "Set Fixed Duration", "New duration (ticks):", seq->getPPQ() / 4, 1, 10000, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::changeDuration(selectedNotes, ticks, false);
        } else {
            NN_Utils::changeDuration(*seq, ticks, false);
        }
    }
}

void MainWindow::util_scale_duration() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int percent = QInputDialog::getInt(this, "Scale Duration", "Scale factor (%):", 90, 1, 500, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::changeDuration(selectedNotes, percent, true);
        } else {
            NN_Utils::changeDuration(*seq, percent, true);
        }
    }
}

void MainWindow::util_legato() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    bool ok;
    int strength = QInputDialog::getInt(this, "Legato", "Strength (%):", 100, 1, 200, 1, &ok);
    if (ok) {
        NN_Utils::legato(*seq, strength);
    }
}

void MainWindow::util_staccato() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int strength = QInputDialog::getInt(this, "Staccato", "New note length (% of original):", 50, 1, 99, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::staccato(selectedNotes, strength);
        } else {
            NN_Utils::staccato(*seq, strength);
        }
    }
}

void MainWindow::util_invert() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    int axis_note = QInputDialog::getInt(this, "Invert", "Axis MIDI Note (60 = C4):", 60, 0, 127, 1, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::invert(selectedNotes, axis_note);
        } else {
            NN_Utils::invert(*seq, axis_note);
        }
    }
}

void MainWindow::util_retrograde() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    NN_Utils::retrograde(*seq);
    QMessageBox::information(this, "Success", "Note order has been reversed.");
}

void MainWindow::util_delete_overlapping() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    NN_Utils::deleteOverlappingNotes(*seq);
    QMessageBox::information(this, "Success", "Overlapping notes have been removed.");
}

void MainWindow::util_scale_timing() {
    NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
    if (!seq) {
        QMessageBox::warning(this, "No Sequence", "No active MIDI sequence to process.");
        return;
    }
    MidiEditorWidget *midi_editor = m_midiEditorSection->getMidiEditor();
    bool ok;
    double factor = QInputDialog::getDouble(this, "Scale Timing", "Time factor (e.g., 2.0 = double tempo, 0.5 = half tempo):", 2.0, 0.1, 10.0, 2, &ok);
    if (ok) {
        if (midi_editor && midi_editor->hasSelection()) {
            auto selectedNotes = midi_editor->getSelectedNotes();
            NN_Utils::scaleTiming(selectedNotes, factor);
        } else {
            NN_Utils::scaleTiming(*seq, factor);
        }
    }
}

// === Project Management ===

bool MainWindow::showProjectWizard() {
    ProjectWizardDialog wizard(engine, m_recentProjectsManager, this);
    
    if (wizard.exec() != QDialog::Accepted) {
        return false;
    }
    
    switch (wizard.getWizardResult()) {
        case ProjectWizardDialog::WizardResult::NewProject: {
            createNewProject(wizard.getProjectMetadata());
            break;
        }
        case ProjectWizardDialog::WizardResult::OpenProject:
        case ProjectWizardDialog::WizardResult::OpenRecent: {
            if (!openProject(wizard.getSelectedFilePath())) {
                QMessageBox::critical(this, "Error", "Failed to open project.");
                return showProjectWizard(); // Try again
            }
            break;
        }
        case ProjectWizardDialog::WizardResult::ImportMidi: {
            // Import MIDI and create project metadata
            NoteNagaProjectMetadata meta;
            meta.name = QFileInfo(wizard.getSelectedFilePath()).baseName().toStdString();
            meta.createdAt = NoteNagaProjectMetadata::currentTimestamp();
            meta.modifiedAt = NoteNagaProjectMetadata::currentTimestamp();
            
            if (!m_projectSerializer->importMidiAsProject(wizard.getSelectedFilePath().toStdString(), meta)) {
                QMessageBox::critical(this, "Error", "Failed to import MIDI file.");
                return showProjectWizard(); // Try again
            }
            
            m_projectMetadata = meta;
            m_currentProjectPath.clear(); // Not saved yet
            m_projectSection->setProjectMetadata(m_projectMetadata);
            m_projectSection->setProjectFilePath(QString());
            m_hasUnsavedChanges = true;
            break;
        }
        default:
            return false;
    }
    
    updateWindowTitle();
    m_autosaveTimer->start();
    
    return true;
}

void MainWindow::createNewProject(const NoteNagaProjectMetadata &metadata) {
    m_projectMetadata = metadata;
    m_currentProjectPath.clear();
    
    if (!m_projectSerializer->createEmptyProject(metadata)) {
        QMessageBox::warning(this, "Warning", "Failed to create empty project. Using default.");
    }
    
    m_projectSection->setProjectMetadata(m_projectMetadata);
    m_projectSection->setProjectFilePath(QString());
    m_hasUnsavedChanges = true;
    
    // Update notation section with project metadata
    if (m_notationSection) {
        m_notationSection->setProjectMetadata(m_projectMetadata);
    }
    
    updateWindowTitle();
}

bool MainWindow::openProject(const QString &filePath) {
    NoteNagaProjectMetadata loadedMetadata;
    
    if (!m_projectSerializer->loadProject(filePath.toStdString(), loadedMetadata)) {
        return false;
    }
    
    m_projectMetadata = loadedMetadata;
    m_currentProjectPath = filePath;
    m_hasUnsavedChanges = false;
    
    m_projectSection->setProjectMetadata(m_projectMetadata);
    m_projectSection->setProjectFilePath(m_currentProjectPath);
    m_projectSection->markAsSaved();
    
    // Update notation section with project metadata
    if (m_notationSection) {
        m_notationSection->setProjectMetadata(m_projectMetadata);
    }
    
    // Refresh DSP widgets to reflect loaded DSP chain
    if (m_dspEditorSection) {
        m_dspEditorSection->refreshDSPWidgets();
    }
    
    // Add to recent projects
    m_recentProjectsManager->addRecentProject(filePath, QString::fromStdString(m_projectMetadata.name));
    
    updateWindowTitle();
    
    return true;
}

bool MainWindow::saveProject() {
    if (m_currentProjectPath.isEmpty()) {
        return saveProjectAs();
    }
    
    // Get latest metadata from section
    m_projectMetadata = m_projectSection->getProjectMetadata();
    
    if (!m_projectSerializer->saveProject(m_currentProjectPath.toStdString(), m_projectMetadata)) {
        m_projectSection->showSaveError(QString::fromStdString(m_projectSerializer->lastError()));
        return false;
    }
    
    m_hasUnsavedChanges = false;
    m_projectSection->markAsSaved();
    
    // Update recent projects
    m_recentProjectsManager->addRecentProject(m_currentProjectPath, QString::fromStdString(m_projectMetadata.name));
    
    updateWindowTitle();
    
    return true;
}

bool MainWindow::saveProjectAs() {
    QString startDir = m_recentProjectsManager->getLastProjectDirectory();
    QString suggestedName = m_projectMetadata.name.empty() ? "project" : QString::fromStdString(m_projectMetadata.name);
    suggestedName = suggestedName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save Project As",
        startDir + "/" + suggestedName + ".nnproj",
        "NoteNaga Projects (*.nnproj)"
    );
    
    if (filePath.isEmpty()) {
        return false;
    }
    
    // Ensure extension
    if (!filePath.endsWith(".nnproj", Qt::CaseInsensitive)) {
        filePath += ".nnproj";
    }
    
    m_currentProjectPath = filePath;
    m_projectSection->setProjectFilePath(m_currentProjectPath);
    
    // Update directory
    m_recentProjectsManager->setLastProjectDirectory(QFileInfo(filePath).absolutePath());
    
    return saveProject();
}

void MainWindow::onAutosave() {
    if (m_currentProjectPath.isEmpty()) {
        return; // Can't autosave without a file path
    }
    
    if (!m_hasUnsavedChanges && !m_projectSection->hasUnsavedChanges()) {
        return; // Nothing to save
    }
    
    // Get latest metadata from section
    m_projectMetadata = m_projectSection->getProjectMetadata();
    
    if (m_projectSerializer->saveProject(m_currentProjectPath.toStdString(), m_projectMetadata)) {
        m_hasUnsavedChanges = false;
        m_projectSection->markAsSaved();
        updateWindowTitle();
    }
}

void MainWindow::updateWindowTitle() {
    QString title = "Note Naga";
    
    if (!m_projectMetadata.name.empty()) {
        title += " - " + QString::fromStdString(m_projectMetadata.name);
    }
    
    if (m_hasUnsavedChanges || m_projectSection->hasUnsavedChanges()) {
        title += " *";
    }
    
    setWindowTitle(title);
}

void MainWindow::onProjectUnsavedChanged(bool hasChanges) {
    Q_UNUSED(hasChanges);
    updateWindowTitle();
}

void MainWindow::onProjectMetadataChanged() {
    // Update central metadata from ProjectSection
    m_projectMetadata = m_projectSection->getProjectMetadata();
    
    // Propagate to NotationSection
    if (m_notationSection) {
        m_notationSection->setProjectMetadata(m_projectMetadata);
    }
    
    // Update window title (in case project name changed)
    updateWindowTitle();
}

void MainWindow::onProjectSaveRequested() {
    if (saveProject()) {
        m_projectSection->showSaveSuccess(m_currentProjectPath);
    }
}

void MainWindow::onProjectSaveAsRequested() {
    if (saveProjectAs()) {
        m_projectSection->showSaveSuccess(m_currentProjectPath);
    }
}

void MainWindow::onProjectExportMidiRequested() {
    NoteNagaMidiSeq *active_sequence = this->engine->getRuntimeData()->getActiveSequence();
    if (!active_sequence) {
        m_projectSection->showExportError(tr("No active MIDI sequence to export."));
        return;
    }

    QString fname =
        QFileDialog::getSaveFileName(this, "Export as MIDI", "", "MIDI Files (*.mid *.midi)");
    
    if (fname.isEmpty()) {
        return; // User cancelled
    }

    // Ensure file has .mid extension
    if (!fname.endsWith(".mid", Qt::CaseInsensitive) && 
        !fname.endsWith(".midi", Qt::CaseInsensitive)) {
        fname += ".mid";
    }

    if (active_sequence->exportToMidi(fname.toStdString())) {
        m_projectSection->showExportSuccess(fname);
    } else {
        m_projectSection->showExportError(tr("Failed to export MIDI file. Check the log for details."));
    }
}

void MainWindow::onUndo() {
    // Get active undo manager based on current section
    UndoManager *mgr = nullptr;
    
    switch (m_currentSection) {
        case AppSection::MidiEditor:
            if (m_midiEditorSection) {
                MidiEditorWidget *editor = m_midiEditorSection->getMidiEditor();
                if (editor) mgr = editor->getUndoManager();
            }
            break;
        // Other sections can add their undo managers here in the future
        default:
            break;
    }
    
    if (mgr) {
        mgr->undo();
    }
}

void MainWindow::onRedo() {
    // Get active undo manager based on current section
    UndoManager *mgr = nullptr;
    
    switch (m_currentSection) {
        case AppSection::MidiEditor:
            if (m_midiEditorSection) {
                MidiEditorWidget *editor = m_midiEditorSection->getMidiEditor();
                if (editor) mgr = editor->getUndoManager();
            }
            break;
        // Other sections can add their undo managers here in the future
        default:
            break;
    }
    
    if (mgr) {
        mgr->redo();
    }
}

void MainWindow::updateUndoRedoState() {
    // This function updates the global undo/redo actions for keyboard shortcuts
    // The actions are not visible in menu/toolbar, but shortcuts still work
    bool canUndo = false;
    bool canRedo = false;
    
    // Get undo manager based on current active section
    switch (m_currentSection) {
        case AppSection::MidiEditor:
            if (m_midiEditorSection) {
                MidiEditorWidget *editor = m_midiEditorSection->getMidiEditor();
                if (editor && editor->getUndoManager()) {
                    canUndo = editor->getUndoManager()->canUndo();
                    canRedo = editor->getUndoManager()->canRedo();
                }
            }
            break;
        default:
            break;
    }
    
    action_undo->setEnabled(canUndo);
    action_redo->setEnabled(canRedo);
}