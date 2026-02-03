#include "midi_editor_section.h"

#include "../dock_system/advanced_dock_widget.h"
#include "../editor/midi_editor_widget.h"
#include "../editor/note_property_editor.h"
#include "../editor/tempo_track_editor.h"
#include "../widgets/midi_control_bar_widget.h"
#include "../widgets/midi_keyboard_ruler.h"
#include "../widgets/midi_tact_ruler.h"
#include "../widgets/timeline_overview_widget.h"
#include "../widgets/track_list_widget.h"
#include "../widgets/track_mixer_widget.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QDockWidget>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>

MidiEditorSection::MidiEditorSection(NoteNagaEngine *engine, QWidget *parent)
    : QMainWindow(parent), m_engine(engine)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Set background
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupDockLayout();
    connectSignals();
}

MidiEditorSection::~MidiEditorSection()
{
}

void MidiEditorSection::onSectionActivated()
{
    // MIDI editor section is the main editor - minimal optimization needed
    // Could enable auto-scroll, animations etc. here if needed
}

void MidiEditorSection::onSectionDeactivated()
{
    // MIDI editor section - could disable animations etc. to save resources
    // Currently no heavy background operations to stop
}

void MidiEditorSection::setupDockLayout()
{
    // === Editor dock (center) ===
    
    // Top part: MIDI Editor with rulers
    QWidget *editorMain = new QWidget();
    QGridLayout *grid = new QGridLayout(editorMain);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    m_midiEditor = new MidiEditorWidget(m_engine, this);
    m_midiEditor->setMouseTracking(true);
    m_midiEditor->setMinimumWidth(250);
    m_midiEditor->setMinimumHeight(150);

    m_midiKeyboardRuler = new MidiKeyboardRuler(m_engine, 16, this);
    m_midiKeyboardRuler->setFixedWidth(60);
    m_midiTactRuler = new MidiTactRuler(m_engine, this);
    m_midiTactRuler->setTimeScale(m_midiEditor->getConfig()->time_scale);

    // Timeline overview widget (minimap) - at bottom of MIDI editor column
    m_timelineOverview = new TimelineOverviewWidget(m_engine, this);

    grid->addWidget(new QWidget(), 0, 0);
    grid->addWidget(m_midiTactRuler, 0, 1);
    grid->addWidget(m_midiKeyboardRuler, 1, 0);
    grid->addWidget(m_midiEditor, 1, 1);
    grid->addWidget(new QWidget(), 2, 0);  // Empty cell below keyboard ruler
    grid->addWidget(m_timelineOverview, 2, 1);  // Timeline below MIDI editor
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(1, 1);

    // Note Property Editor (below MIDI editor)
    m_notePropertyEditor = new NotePropertyEditor(m_engine, m_midiEditor, this);
    m_notePropertyEditor->setMinimumHeight(80);
    
    // Tempo Track Editor (shown when tempo track is active)
    m_tempoTrackEditor = new TempoTrackEditor(m_engine, m_midiEditor, this);
    m_tempoTrackEditor->setMinimumHeight(100);
    m_tempoTrackEditor->hide();  // Hidden by default
    
    // Container for property editors (stacked-like behavior using visibility)
    m_propertyEditorContainer = new QWidget(this);
    m_propertyEditorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QVBoxLayout *propertyLayout = new QVBoxLayout(m_propertyEditorContainer);
    propertyLayout->setContentsMargins(0, 0, 0, 0);
    propertyLayout->setSpacing(0);
    propertyLayout->addWidget(m_notePropertyEditor);
    propertyLayout->addWidget(m_tempoTrackEditor);
    
    // Connect expandedChanged signals to update splitter sizes
    connect(m_notePropertyEditor, &NotePropertyEditor::expandedChanged, this, [this](bool expanded) {
        if (!expanded) {
            // When collapsed, adjust splitter to give minimal space to container
            int totalHeight = m_editorSplitter->height();
            m_editorSplitter->setSizes({totalHeight - 32, 32});
        }
    });
    connect(m_tempoTrackEditor, &TempoTrackEditor::expandedChanged, this, [this](bool expanded) {
        if (!expanded) {
            // When collapsed, adjust splitter to give minimal space to container
            int totalHeight = m_editorSplitter->height();
            m_editorSplitter->setSizes({totalHeight - 32, 32});
        }
    });
    
    // Splitter between MIDI editor and Property Editor
    m_editorSplitter = new QSplitter(Qt::Vertical);
    m_editorSplitter->setChildrenCollapsible(true);
    m_editorSplitter->setHandleWidth(5);
    m_editorSplitter->setStyleSheet(R"(
        QSplitter::handle {
            background: #2a2d35;
        }
        QSplitter::handle:hover {
            background: #3a5d75;
        }
    )");
    
    m_editorSplitter->addWidget(editorMain);
    m_editorSplitter->addWidget(m_propertyEditorContainer);
    
    // Set initial sizes (80% for editor, 20% for note property)
    m_editorSplitter->setSizes({600, 150});
    m_editorSplitter->setStretchFactor(0, 4);
    m_editorSplitter->setStretchFactor(1, 1);

    // Main editor layout with splitter and control bar
    QVBoxLayout *editorLayout = new QVBoxLayout();
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_editorSplitter, 1);
    
    m_controlBar = new MidiControlBarWidget(m_engine, this);
    editorLayout->addWidget(m_controlBar);
    
    QFrame *editorContainer = new QFrame();
    editorContainer->setObjectName("EditorContainer");
    editorContainer->setStyleSheet("QFrame#EditorContainer { border: 1px solid #19191f; }");
    editorContainer->setLayout(editorLayout);
    editorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *editorDock = new AdvancedDockWidget(
        tr("MIDI Editor"), 
        QIcon(":/icons/midi.svg"),
        m_midiEditor->getTitleWidget(), 
        this
    );
    editorDock->setWidget(editorContainer);
    editorDock->setObjectName("editor");
    editorDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    editorDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                            QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, editorDock);
    m_docks["editor"] = editorDock;

    // === Track list dock (left top) ===
    m_trackListWidget = new TrackListWidget(m_engine, this);
    m_trackListWidget->setMinimumWidth(250);
    m_trackListWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    
    auto *tracklistDock = new AdvancedDockWidget(
        tr("Tracks"), 
        QIcon(":/icons/track.svg"),
        m_trackListWidget->getTitleWidget(), 
        this
    );
    tracklistDock->setWidget(m_trackListWidget);
    tracklistDock->setObjectName("tracklist");
    tracklistDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    tracklistDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, tracklistDock);
    m_docks["tracklist"] = tracklistDock;

    // === Mixer dock (left bottom) ===
    m_mixerWidget = new TrackMixerWidget(m_engine, this);
    m_mixerWidget->setMinimumWidth(250);
    m_mixerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    
    auto *mixerDock = new AdvancedDockWidget(
        tr("Track Mixer"), 
        QIcon(":/icons/mixer.svg"),
        m_mixerWidget->getTitleWidget(), 
        this
    );
    mixerDock->setWidget(m_mixerWidget);
    mixerDock->setObjectName("mixer");
    mixerDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mixerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                           QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, mixerDock);
    m_docks["mixer"] = mixerDock;

    // === Configure layout ===
    // Left side: track list on top, mixer on bottom (vertical split)
    // Right side: MIDI editor (takes most space)
    m_docks["editor"]->setParent(this);
    m_docks["tracklist"]->setParent(this);
    m_docks["mixer"]->setParent(this);

    // First, split horizontally: left area (tracklist) and right area (editor)
    splitDockWidget(m_docks["tracklist"], m_docks["editor"], Qt::Horizontal);
    // Then split the left area vertically: tracklist on top, mixer below
    splitDockWidget(m_docks["tracklist"], m_docks["mixer"], Qt::Vertical);
    
    m_docks["editor"]->raise();
    m_docks["tracklist"]->setFloating(false);
    m_docks["mixer"]->setFloating(false);

    for (auto dock : m_docks) {
        dock->setVisible(true);
    }
}

void MidiEditorSection::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    if (!m_layoutInitialized) {
        m_layoutInitialized = true;
        // Use a timer to ensure window geometry is fully established
        QTimer::singleShot(50, this, [this]() {
            int totalWidth = width();
            int leftWidth = 280;  // Desired width for left panel
            int rightWidth = totalWidth - leftWidth - 10;  // Rest for editor
            if (rightWidth < 400) rightWidth = 400;
            
            resizeDocks({m_docks["tracklist"], m_docks["editor"]}, 
                        {leftWidth, rightWidth}, Qt::Horizontal);
        });
    }
}

void MidiEditorSection::connectSignals()
{
    // Tact ruler click -> set playback position
    connect(m_midiTactRuler, &MidiTactRuler::positionSelected, this, [this](int tick) {
        NoteNagaRuntimeData *project = m_engine->getRuntimeData();
        bool wasPlaying = m_engine->isPlaying();
        if (wasPlaying) {
            m_engine->stopPlayback();
        }
        project->setCurrentTick(tick);
        if (wasPlaying) {
            m_engine->startPlayback();
        }
    });
    
    // Editor scroll signals
    connect(m_midiEditor, &MidiEditorWidget::horizontalScrollChanged, 
            m_midiTactRuler, &MidiTactRuler::setHorizontalScroll);
    connect(m_midiEditor, &MidiEditorWidget::timeScaleChanged, 
            m_midiTactRuler, &MidiTactRuler::setTimeScale);
    connect(m_midiEditor, &MidiEditorWidget::verticalScrollChanged, 
            m_midiKeyboardRuler, &MidiKeyboardRuler::setVerticalScroll);
    connect(m_midiEditor, &MidiEditorWidget::keyHeightChanged, 
            m_midiKeyboardRuler, &MidiKeyboardRuler::setRowHeight);
    
    // Note property editor signals
    connect(m_midiEditor, &MidiEditorWidget::horizontalScrollChanged, 
            m_notePropertyEditor, &NotePropertyEditor::setHorizontalScroll);
    connect(m_midiEditor, &MidiEditorWidget::timeScaleChanged, 
            m_notePropertyEditor, &NotePropertyEditor::setTimeScale);
    
    // Tempo track editor signals
    connect(m_midiEditor, &MidiEditorWidget::horizontalScrollChanged, 
            m_tempoTrackEditor, &TempoTrackEditor::setHorizontalScroll);
    connect(m_midiEditor, &MidiEditorWidget::timeScaleChanged, 
            m_tempoTrackEditor, &TempoTrackEditor::setTimeScale);
    
    // Connect active track changes to switch between editors
    NoteNagaMidiSeq *seq = m_engine->getRuntimeData()->getActiveSequence();
    if (seq) {
        connect(seq, &NoteNagaMidiSeq::activeTrackChanged, 
                this, &MidiEditorSection::onActiveTrackChanged);
    }
    connect(m_engine->getRuntimeData(), &NoteNagaRuntimeData::activeSequenceChanged, this, [this](NoteNagaMidiSeq *newSeq) {
        if (newSeq) {
            connect(newSeq, &NoteNagaMidiSeq::activeTrackChanged, 
                    this, &MidiEditorSection::onActiveTrackChanged, Qt::UniqueConnection);
            // Update visibility for new sequence
            onActiveTrackChanged(newSeq->getActiveTrack());
        }
    });
    
    // Note selection and modification signals
    connect(m_midiEditor, &MidiEditorWidget::notesModified,
            m_notePropertyEditor, &NotePropertyEditor::onNotesChanged);
    connect(m_midiEditor, &MidiEditorWidget::selectionChanged,
            m_notePropertyEditor, &NotePropertyEditor::onSelectionChanged);
    
    // When note property editing is finished, refresh the MIDI editor
    // Using notePropertyEditFinished (emitted on mouse release) instead of
    // notePropertyChanged (emitted during drag) to avoid excessive refreshes
    connect(m_notePropertyEditor, &NotePropertyEditor::notePropertyEditFinished,
            m_midiEditor, &MidiEditorWidget::refreshTrack);
    
    // Timeline overview widget signals
    connect(m_midiEditor, &MidiEditorWidget::timeScaleChanged, 
            m_timelineOverview, &TimelineOverviewWidget::setTimeScale);
    connect(m_midiEditor, &MidiEditorWidget::notesModified,
            m_timelineOverview, &TimelineOverviewWidget::refresh);
    connect(m_midiEditor, &MidiEditorWidget::contentSizeChanged,
            m_timelineOverview, &TimelineOverviewWidget::setMaxTick);
    
    // Update timeline viewport when editor scrolls
    connect(m_midiEditor, &MidiEditorWidget::horizontalScrollChanged, this, [this](int scrollValue) {
        if (!m_midiEditor || !m_midiEditor->getSequence()) return;
        
        double timeScale = m_midiEditor->getConfig()->time_scale;
        int viewportWidth = m_midiEditor->viewport()->width();
        
        int startTick = static_cast<int>(scrollValue / timeScale);
        int endTick = static_cast<int>((scrollValue + viewportWidth) / timeScale);
        
        m_timelineOverview->setViewportRange(startTick, endTick);
    });
    
    // Update timeline overview when sequence changes (initial load)
    connect(m_engine->getRuntimeData(), &NoteNagaRuntimeData::activeSequenceChanged, this, [this](NoteNagaMidiSeq*) {
        // Delay update to allow MIDI editor to set up first
        QTimer::singleShot(50, this, [this]() {
            if (!m_midiEditor || !m_midiEditor->getSequence()) return;
            
            double timeScale = m_midiEditor->getConfig()->time_scale;
            int viewportWidth = m_midiEditor->viewport()->width();
            int scrollValue = m_midiEditor->horizontalScrollBar()->value();
            
            int startTick = static_cast<int>(scrollValue / timeScale);
            int endTick = static_cast<int>((scrollValue + viewportWidth) / timeScale);
            
            m_timelineOverview->setViewportRange(startTick, endTick);
            m_timelineOverview->refresh();
        });
    });
    
    // Navigate when clicking on timeline overview
    connect(m_timelineOverview, &TimelineOverviewWidget::viewportNavigationRequested, this, [this](int tick) {
        if (!m_midiEditor) return;
        
        double timeScale = m_midiEditor->getConfig()->time_scale;
        int viewportWidth = m_midiEditor->viewport()->width();
        
        // Center the view on the clicked tick
        int scrollValue = static_cast<int>(tick * timeScale - viewportWidth / 2);
        scrollValue = qMax(0, scrollValue);
        
        m_midiEditor->horizontalScrollBar()->setValue(scrollValue);
    });
}

void MidiEditorSection::toggleNotePropertyEditor()
{
    // Called externally (e.g., from menu) to toggle the note property editor
    m_notePropertyEditor->setExpanded(!m_notePropertyEditor->isExpanded());
}

void MidiEditorSection::showHideDock(const QString &name, bool checked)
{
    QDockWidget *dock = m_docks.value(name, nullptr);
    if (!dock) return;

    if (checked) {
        if (!dock->parentWidget()) {
            if (name == "tracklist" || name == "mixer") {
                addDockWidget(Qt::LeftDockWidgetArea, dock);
            } else {
                addDockWidget(Qt::RightDockWidgetArea, dock);
            }
        }
        dock->show();
        dock->raise();
    } else {
        dock->hide();
    }
}

void MidiEditorSection::resetLayout()
{
    // Add back all docks
    if (!m_docks["tracklist"]->parentWidget()) {
        addDockWidget(Qt::LeftDockWidgetArea, m_docks["tracklist"]);
    }
    if (!m_docks["mixer"]->parentWidget()) {
        addDockWidget(Qt::LeftDockWidgetArea, m_docks["mixer"]);
    }
    if (!m_docks["editor"]->parentWidget()) {
        addDockWidget(Qt::RightDockWidgetArea, m_docks["editor"]);
    }

    // Show all
    m_docks["tracklist"]->setVisible(true);
    m_docks["editor"]->setVisible(true);
    m_docks["mixer"]->setVisible(true);

    // First, split horizontally: left area (tracklist) and right area (editor)
    splitDockWidget(m_docks["tracklist"], m_docks["editor"], Qt::Horizontal);
    // Then split the left area vertically: tracklist on top, mixer below
    splitDockWidget(m_docks["tracklist"], m_docks["mixer"], Qt::Vertical);
    m_docks["editor"]->raise();

    // Set sizes - editor takes most horizontal space
    // Use QTimer to ensure layout is computed before resizing
    QTimer::singleShot(0, this, [this]() {
        QList<QDockWidget*> hOrder = {m_docks["tracklist"], m_docks["editor"]};
        QList<int> hSizes = {280, 1000};
        resizeDocks(hOrder, hSizes, Qt::Horizontal);
        
        // Adjust vertical ratios for left side
        QList<QDockWidget*> vOrder = {m_docks["tracklist"], m_docks["mixer"]};
        QList<int> vSizes = {300, 400};
        resizeDocks(vOrder, vSizes, Qt::Vertical);
    });
}

void MidiEditorSection::onActiveTrackChanged(NoteNagaTrack *track)
{
    bool isTempoTrack = track && track->isTempoTrack();
    
    if (isTempoTrack) {
        // Show tempo track editor, hide note property editor
        m_notePropertyEditor->hide();
        m_tempoTrackEditor->setTempoTrack(track);
        m_tempoTrackEditor->show();
    } else {
        // Show note property editor, hide tempo track editor
        m_tempoTrackEditor->hide();
        m_tempoTrackEditor->setTempoTrack(nullptr);
        m_notePropertyEditor->show();
    }
}
