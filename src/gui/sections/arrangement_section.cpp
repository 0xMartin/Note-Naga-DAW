#include "arrangement_section.h"

#include "../dock_system/advanced_dock_widget.h"
#include "../editor/arrangement_layer_manager.h"
#include "../editor/arrangement_timeline_widget.h"
#include "../editor/arrangement_resource_panel.h"
#include "../editor/arrangement_timeline_ruler.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QDockWidget>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>

ArrangementSection::ArrangementSection(NoteNagaEngine *engine, QWidget *parent)
    : QMainWindow(parent), m_engine(engine)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Set background
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupDockLayout();
    connectSignals();
    
    // Timer for updating track stereo meters
    m_meterUpdateTimer = new QTimer(this);
    connect(m_meterUpdateTimer, &QTimer::timeout, this, [this]() {
        if (m_timeline) {
            m_timeline->updateTrackMeters();
        }
    });
    m_meterUpdateTimer->start(30); // ~33 Hz update rate
}

ArrangementSection::~ArrangementSection()
{
}

void ArrangementSection::onSectionActivated()
{
    // Refresh data when section becomes active
    if (m_layerManager) {
        m_layerManager->refreshFromArrangement();
    }
    if (m_resourcePanel) {
        m_resourcePanel->refreshFromProject();
    }
    if (m_timeline) {
        m_timeline->refreshFromArrangement();
    }
}

void ArrangementSection::onSectionDeactivated()
{
    // Cleanup if needed
}

void ArrangementSection::setupDockLayout()
{
    // === Layer Manager dock (left) - Now optional since headers are in timeline ===
    m_layerManager = new ArrangementLayerManager(m_engine, this);
    m_layerManager->setMinimumWidth(180);
    m_layerManager->setMaximumWidth(300);

    auto *layerDock = new AdvancedDockWidget(
        tr("Track Manager"),
        QIcon(":/icons/layers.svg"),
        nullptr,
        this
    );
    layerDock->setWidget(m_layerManager);
    layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, layerDock);
    layerDock->hide();  // Hidden by default - track headers are now in timeline
    m_docks["Layers"] = layerDock;

    // === Timeline dock (center) ===
    QWidget *timelineContainer = new QWidget();
    QVBoxLayout *timelineLayout = new QVBoxLayout(timelineContainer);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(0);

    // Timeline ruler at top
    m_timelineRuler = new ArrangementTimelineRuler(m_engine, this);
    m_timelineRuler->setFixedHeight(30);
    timelineLayout->addWidget(m_timelineRuler);

    // Main timeline widget
    m_timeline = new ArrangementTimelineWidget(m_engine, this);
    m_timeline->setMinimumWidth(300);
    m_timeline->setMinimumHeight(200);
    timelineLayout->addWidget(m_timeline, 1);

    // Horizontal scrollbar for timeline
    m_timelineScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_timelineScrollBar->setStyleSheet(R"(
        QScrollBar:horizontal {
            background-color: #1e1e24;
            height: 14px;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background-color: #4a4a55;
            min-width: 30px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #5a5a65;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }
    )");
    timelineLayout->addWidget(m_timelineScrollBar);

    QFrame *editorContainer = new QFrame();
    editorContainer->setObjectName("TimelineContainer");
    editorContainer->setStyleSheet("QFrame#TimelineContainer { border: 1px solid #19191f; }");
    QVBoxLayout *editorLayout = new QVBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(timelineContainer);
    editorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *timelineDock = new AdvancedDockWidget(
        tr("Timeline"),
        QIcon(":/icons/timeline.svg"),
        nullptr,
        this
    );
    timelineDock->setWidget(editorContainer);
    timelineDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    setCentralWidget(timelineDock);
    m_docks["Timeline"] = timelineDock;

    // === Resource Panel dock (right) ===
    m_resourcePanel = new ArrangementResourcePanel(m_engine, this);
    m_resourcePanel->setMinimumWidth(180);
    m_resourcePanel->setMaximumWidth(350);

    auto *resourceDock = new AdvancedDockWidget(
        tr("MIDI Sequences"),
        QIcon(":/icons/music-note.svg"),
        nullptr,
        this
    );
    resourceDock->setWidget(m_resourcePanel);
    resourceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, resourceDock);
    m_docks["Resources"] = resourceDock;
}

void ArrangementSection::connectSignals()
{
    if (!m_engine || !m_engine->getRuntimeData()) return;

    NoteNagaRuntimeData *runtime = m_engine->getRuntimeData();

    // Connect runtime position signal
    connect(runtime, &NoteNagaRuntimeData::currentArrangementTickChanged,
            this, &ArrangementSection::onPlaybackPositionChanged);

    // Connect layer manager to timeline
    if (m_layerManager && m_timeline) {
        connect(m_layerManager, &ArrangementLayerManager::trackModified,
                this, &ArrangementSection::onArrangementChanged);
        connect(m_layerManager, &ArrangementLayerManager::tracksReordered,
                this, &ArrangementSection::onArrangementChanged);
    }
    
    // Connect scrollbar to timeline
    if (m_timelineScrollBar && m_timeline) {
        connect(m_timelineScrollBar, &QScrollBar::valueChanged,
                m_timeline, &ArrangementTimelineWidget::setHorizontalOffset);
        connect(m_timeline, &ArrangementTimelineWidget::horizontalOffsetChanged,
                m_timelineScrollBar, &QScrollBar::setValue);
        
        // Update scrollbar range when arrangement changes
        connect(runtime, &NoteNagaRuntimeData::arrangementChanged, this, [this]() {
            updateScrollBarRange();
        });
        
        // Also connect to timeline resize
        connect(m_timeline, &ArrangementTimelineWidget::zoomChanged, this, [this](double) {
            updateScrollBarRange();
        });
    }
    
    // Also connect scrollbar to ruler
    if (m_timelineScrollBar && m_timelineRuler) {
        connect(m_timelineScrollBar, &QScrollBar::valueChanged,
                m_timelineRuler, &ArrangementTimelineRuler::setHorizontalOffset);
    }

    // Connect resource panel edit requests
    if (m_resourcePanel) {
        connect(m_resourcePanel, &ArrangementResourcePanel::editSequenceRequested,
                this, [this](int sequenceIndex) {
            // Switch to MIDI editor section with this sequence
            emit switchToMidiEditor(sequenceIndex);
        });
        
        // Handle new sequence creation
        connect(m_resourcePanel, &ArrangementResourcePanel::createSequenceRequested,
                this, [this]() {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            
            NoteNagaRuntimeData *runtime = m_engine->getRuntimeData();
            
            // Create new sequence
            NoteNagaMidiSeq *newSeq = new NoteNagaMidiSeq();
            newSeq->setTempo(500000);  // 120 BPM default
            newSeq->setPPQ(480);
            
            // Add a default track (instrument_index 0)
            newSeq->addTrack(0);
            
            // Add sequence to runtime
            runtime->addSequence(newSeq);
            
            // Set as active
            runtime->setActiveSequence(newSeq);
            
            // Refresh resource panel
            m_resourcePanel->refreshFromProject();
        });
    }

    // Connect timeline clip drop to arrangement
    if (m_timeline) {
        connect(m_timeline, &ArrangementTimelineWidget::clipDropped,
                this, [this](int trackIndex, int64_t tick, int midiSequenceIndex) {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (!arrangement) return;
            
            auto sequences = m_engine->getRuntimeData()->getSequences();
            if (midiSequenceIndex < 0 || midiSequenceIndex >= static_cast<int>(sequences.size())) return;
            
            NoteNagaMidiSeq *seq = sequences[midiSequenceIndex];
            if (!seq) return;
            
            // Auto-create tracks if needed (when dropping to a track that doesn't exist)
            while (trackIndex >= 0 && trackIndex >= static_cast<int>(arrangement->getTrackCount())) {
                QString trackName = tr("Track %1").arg(arrangement->getTrackCount() + 1);
                arrangement->addTrack(trackName.toStdString());
            }
            
            if (trackIndex < 0) trackIndex = 0;
            if (trackIndex >= static_cast<int>(arrangement->getTrackCount())) return;
            
            // Create clip from dropped sequence
            NN_MidiClip_t clip;
            // Leave name empty - will use sequence name for display
            clip.name = "";
            clip.sequenceId = seq->getId();
            clip.startTick = static_cast<int>(tick);
            clip.durationTicks = seq->getMaxTick() > 0 ? seq->getMaxTick() : 480 * 4;
            
            arrangement->getTracks()[trackIndex]->addClip(clip);
            onArrangementChanged();
            
            // Refresh layer manager to show new tracks
            if (m_layerManager) {
                m_layerManager->refreshFromArrangement();
            }
        });
        
        // Synchronize ruler with timeline
        m_timeline->setRuler(m_timelineRuler);
        
        connect(m_timeline, &ArrangementTimelineWidget::horizontalOffsetChanged,
                m_timelineRuler, &ArrangementTimelineRuler::setHorizontalOffset);
        connect(m_timeline, &ArrangementTimelineWidget::zoomChanged,
                m_timelineRuler, &ArrangementTimelineRuler::setPixelsPerTick);
    }
    
    // Connect ruler seek requests - handle with playback stop/restart
    if (m_timelineRuler) {
        connect(m_timelineRuler, &ArrangementTimelineRuler::seekRequested,
                this, [this](int64_t tick) {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            
            // Remember if playback was running
            bool wasPlaying = m_engine->isPlaying();
            if (wasPlaying) {
                m_engine->stopPlayback();
            }
            
            // Set the arrangement position
            m_engine->getRuntimeData()->setCurrentArrangementTick(static_cast<int>(tick));
            
            // Restart playback if it was running
            if (wasPlaying) {
                m_engine->startPlayback();
            }
        });
    }
}

void ArrangementSection::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    if (!m_layoutInitialized) {
        m_layoutInitialized = true;
        // Trigger initial layout update
        QTimer::singleShot(100, this, [this]() {
            onArrangementChanged();
        });
    }
}

void ArrangementSection::resetLayout()
{
    // Reset dock positions to default
    for (auto *dock : m_docks) {
        removeDockWidget(dock);
    }
    
    addDockWidget(Qt::LeftDockWidgetArea, m_docks["Layers"]);
    addDockWidget(Qt::RightDockWidgetArea, m_docks["Resources"]);
    
    for (auto *dock : m_docks) {
        dock->show();
    }
}

void ArrangementSection::showHideDock(const QString &name, bool checked)
{
    if (m_docks.contains(name)) {
        m_docks[name]->setVisible(checked);
    }
}

void ArrangementSection::onArrangementChanged()
{
    if (m_layerManager) {
        m_layerManager->refreshFromArrangement();
    }
    if (m_timeline) {
        m_timeline->refreshFromArrangement();
    }
    if (m_timelineRuler) {
        m_timelineRuler->update();
    }
    updateScrollBarRange();
}

void ArrangementSection::updateScrollBarRange()
{
    if (!m_timelineScrollBar || !m_timeline || !m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    // Calculate content width
    int64_t maxTick = arrangement->getMaxTick();
    double ppTick = m_timeline->getPixelsPerTick();
    int contentWidth = static_cast<int>(maxTick * ppTick);
    
    // Add 2x viewport width beyond last clip
    int viewportWidth = m_timeline->contentRect().width();
    int scrollRange = contentWidth + (2 * viewportWidth);
    
    m_timelineScrollBar->setRange(0, std::max(0, scrollRange - viewportWidth));
    m_timelineScrollBar->setPageStep(viewportWidth);
    m_timelineScrollBar->setSingleStep(viewportWidth / 10);
}

void ArrangementSection::onPlaybackPositionChanged(int tick)
{
    if (m_timeline) {
        m_timeline->setPlayheadTick(tick);
    }
    if (m_timelineRuler) {
        m_timelineRuler->setPlayheadTick(tick);
    }
}
