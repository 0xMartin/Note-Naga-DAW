#include "arrangement_section.h"

#include "../dock_system/advanced_dock_widget.h"
#include "../editor/arrangement_layer_manager.h"
#include "../editor/arrangement_timeline_widget.h"
#include "../editor/arrangement_track_headers_widget.h"
#include "../editor/arrangement_resource_panel.h"
#include "../editor/arrangement_timeline_ruler.h"
#include "../editor/arrangement_minimap_widget.h"
#include "../editor/arrangement_tempo_track_editor.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QDockWidget>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QColorDialog>

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
    
    // Update minimap visible range after layout is calculated
    QTimer::singleShot(50, this, [this]() {
        refreshMinimap();
        updateScrollBarRange();
    });
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
    // Main container with vertical layout: top row (corner+ruler), middle (splitter), bottom (scrollbar row)
    QWidget *timelineContainer = new QWidget();
    QVBoxLayout *timelineLayout = new QVBoxLayout(timelineContainer);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(0);
    
    // === Top row: corner spacer + ruler ===
    QWidget *topRow = new QWidget(this);
    QHBoxLayout *topRowLayout = new QHBoxLayout(topRow);
    topRowLayout->setContentsMargins(0, 0, 0, 0);
    topRowLayout->setSpacing(0);
    
    // Header corner spacer (top-left, matches ruler height)
    // Initial width includes splitter handle (4px) to align with timeline content
    m_headerCorner = new QWidget(this);
    m_headerCorner->setFixedHeight(30);
    m_headerCorner->setMinimumWidth(TRACK_HEADER_WIDTH + 4);  // +4 for splitter handle
    m_headerCorner->setStyleSheet("background-color: #1e1e24;");
    topRowLayout->addWidget(m_headerCorner);
    
    // Timeline ruler at top (right of corner spacer)
    m_timelineRuler = new ArrangementTimelineRuler(m_engine, this);
    m_timelineRuler->setFixedHeight(30);
    topRowLayout->addWidget(m_timelineRuler, 1);
    
    timelineLayout->addWidget(topRow);
    
    // === Middle: splitter with headers and timeline ===
    m_headerTimelineSplitter = new QSplitter(Qt::Horizontal, this);
    m_headerTimelineSplitter->setHandleWidth(4);
    m_headerTimelineSplitter->setStyleSheet(R"(
        QSplitter::handle {
            background-color: #3a3a42;
        }
        QSplitter::handle:hover {
            background-color: #5a5a65;
        }
    )");
    
    // Create track headers widget (left side)
    m_trackHeaders = new ArrangementTrackHeadersWidget(m_engine, this);
    m_trackHeaders->setMinimumWidth(120);
    m_headerTimelineSplitter->addWidget(m_trackHeaders);
    
    // Main timeline widget (right side)
    m_timeline = new ArrangementTimelineWidget(m_engine, this);
    m_timeline->setMinimumWidth(300);
    m_timeline->setMinimumHeight(200);
    m_timeline->setTrackHeadersWidget(m_trackHeaders);  // Connect headers widget
    m_headerTimelineSplitter->addWidget(m_timeline);
    
    // Set initial splitter sizes (headers: 160, timeline: stretch)
    m_headerTimelineSplitter->setSizes({TRACK_HEADER_WIDTH, 600});
    m_headerTimelineSplitter->setStretchFactor(0, 0);  // Headers don't stretch
    m_headerTimelineSplitter->setStretchFactor(1, 1);  // Timeline stretches
    
    // Connect splitter to update corner width (account for splitter handle width)
    connect(m_headerTimelineSplitter, &QSplitter::splitterMoved, this, [this](int pos, int index) {
        Q_UNUSED(index);
        // Add splitter handle width to align ruler with timeline content
        int handleWidth = m_headerTimelineSplitter->handleWidth();
        m_headerCorner->setFixedWidth(pos + handleWidth);
        m_scrollbarSpacer->setFixedWidth(pos + handleWidth);
        m_minimapSpacer->setFixedWidth(pos + handleWidth);
        // Update tempo track editor header width
        if (m_tempoTrackEditor) {
            m_tempoTrackEditor->setHeaderWidth(pos + handleWidth);
        }
    });
    
    timelineLayout->addWidget(m_headerTimelineSplitter, 1);
    
    // === Bottom row: scrollbar spacer + scrollbar ===
    QWidget *bottomRow = new QWidget(this);
    QHBoxLayout *bottomRowLayout = new QHBoxLayout(bottomRow);
    bottomRowLayout->setContentsMargins(0, 0, 0, 0);
    bottomRowLayout->setSpacing(0);
    
    m_scrollbarSpacer = new QWidget(this);
    m_scrollbarSpacer->setFixedWidth(TRACK_HEADER_WIDTH + 4);  // +4 for splitter handle
    m_scrollbarSpacer->setFixedHeight(14);
    m_scrollbarSpacer->setStyleSheet("background-color: #1e1e24;");
    bottomRowLayout->addWidget(m_scrollbarSpacer);
    
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
    bottomRowLayout->addWidget(m_timelineScrollBar, 1);
    
    timelineLayout->addWidget(bottomRow);
    
    // === Minimap row: spacer + minimap ===
    QWidget *minimapRow = new QWidget(this);
    QHBoxLayout *minimapRowLayout = new QHBoxLayout(minimapRow);
    minimapRowLayout->setContentsMargins(0, 0, 0, 0);
    minimapRowLayout->setSpacing(0);
    
    m_minimapSpacer = new QWidget(this);
    m_minimapSpacer->setFixedWidth(TRACK_HEADER_WIDTH + 4);  // +4 for splitter handle
    m_minimapSpacer->setFixedHeight(40);
    m_minimapSpacer->setStyleSheet("background-color: #1e1e24;");
    minimapRowLayout->addWidget(m_minimapSpacer);
    
    // Minimap widget
    m_minimap = new ArrangementMinimapWidget(m_engine, this);
    m_minimap->setFixedHeight(40);
    minimapRowLayout->addWidget(m_minimap, 1);
    
    timelineLayout->addWidget(minimapRow);
    
    // === Tempo track editor (at bottom with splitter) ===
    m_tempoTrackEditor = new ArrangementTempoTrackEditor(m_engine, this);
    m_tempoTrackEditor->setMinimumHeight(24);
    m_tempoTrackEditor->setMaximumHeight(200);
    
    // === Vertical splitter for main content and tempo track ===
    m_mainVerticalSplitter = new QSplitter(Qt::Vertical, this);
    m_mainVerticalSplitter->setHandleWidth(4);
    m_mainVerticalSplitter->setStyleSheet(R"(
        QSplitter::handle {
            background-color: #3a3a42;
        }
        QSplitter::handle:hover {
            background-color: #5a5a65;
        }
    )");
    m_mainVerticalSplitter->addWidget(timelineContainer);
    m_mainVerticalSplitter->addWidget(m_tempoTrackEditor);
    m_mainVerticalSplitter->setStretchFactor(0, 1);  // Timeline takes most space
    m_mainVerticalSplitter->setStretchFactor(1, 0);  // Tempo track has fixed size
    m_mainVerticalSplitter->setSizes({600, 60});     // Initial sizes
    
    // Connect to hide/show tempo track editor based on tempo track existence
    connect(m_tempoTrackEditor, &ArrangementTempoTrackEditor::expandedChanged, this, [this](bool expanded) {
        if (!expanded) {
            m_mainVerticalSplitter->setSizes({m_mainVerticalSplitter->sizes()[0] + m_mainVerticalSplitter->sizes()[1] - 24, 24});
        }
    });

    QFrame *editorContainer = new QFrame();
    editorContainer->setObjectName("TimelineContainer");
    editorContainer->setStyleSheet("QFrame#TimelineContainer { border: 1px solid #19191f; }");
    QVBoxLayout *editorLayout = new QVBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_mainVerticalSplitter);
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
    
    // Connect track headers widget
    if (m_trackHeaders) {
        connect(m_trackHeaders, &ArrangementTrackHeadersWidget::addTrackRequested, this, [this]() {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (!arrangement) return;
            QString name = tr("Track %1").arg(arrangement->getTrackCount() + 1);
            arrangement->addTrack(name.toStdString());
            onArrangementChanged();
        });
        
        // Connect track selection
        connect(m_trackHeaders, &ArrangementTrackHeadersWidget::trackSelected, this, [this](int trackIndex) {
            m_trackHeaders->setSelectedTrack(trackIndex);
        });
        
        // Connect track deletion
        connect(m_trackHeaders, &ArrangementTrackHeadersWidget::deleteTrackRequested, this, [this](int trackIndex) {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (!arrangement) return;
            
            auto tracks = arrangement->getTracks();
            if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
            
            // Remove the track
            arrangement->removeTrack(trackIndex);
            m_trackHeaders->setSelectedTrack(-1);  // Clear selection
            onArrangementChanged();
        });
        
        // Connect color change request to show color dialog
        connect(m_trackHeaders, &ArrangementTrackHeadersWidget::trackColorChangeRequested, this, [this](int trackIndex) {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (!arrangement) return;
            auto tracks = arrangement->getTracks();
            if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) return;
            
            NoteNagaArrangementTrack *track = tracks[trackIndex];
            if (!track) return;
            
            QColor currentColor = track->getColor().toQColor();
            QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Track Color"));
            if (newColor.isValid()) {
                track->setColor(NN_Color_t::fromQColor(newColor));
                onArrangementChanged();
            }
        });
        
        // Connect track reordering
        connect(m_trackHeaders, &ArrangementTrackHeadersWidget::tracksReordered, this, [this](int, int) {
            onArrangementChanged();
        });
    }
    
    // Connect loop region between ruler and timeline
    if (m_timelineRuler && m_timeline) {
        connect(m_timelineRuler, &ArrangementTimelineRuler::loopRegionChanged,
                m_timeline, &ArrangementTimelineWidget::setLoopRegion);
        connect(m_timelineRuler, &ArrangementTimelineRuler::loopEnabledChanged,
                m_timeline, &ArrangementTimelineWidget::setLoopEnabled);
        
        // Sync loop from timeline back to ruler
        connect(m_timeline, &ArrangementTimelineWidget::loopRegionChanged,
                m_timelineRuler, &ArrangementTimelineRuler::setLoopRegion);
        connect(m_timeline, &ArrangementTimelineWidget::loopEnabledChanged,
                m_timelineRuler, &ArrangementTimelineRuler::setLoopEnabled);
        
        // Sync loop region to engine arrangement
        connect(m_timelineRuler, &ArrangementTimelineRuler::loopRegionChanged, this, [this](int64_t start, int64_t end) {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (arrangement) {
                arrangement->setLoopRegion(start, end);
            }
        });
        connect(m_timelineRuler, &ArrangementTimelineRuler::loopEnabledChanged, this, [this](bool enabled) {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
            if (arrangement) {
                arrangement->setLoopEnabled(enabled);
            }
        });
    }
    
    // Connect minimap
    if (m_minimap && m_timeline) {
        // Minimap seek -> timeline
        connect(m_minimap, &ArrangementMinimapWidget::seekRequested, this, [this](int64_t tick) {
            if (!m_engine || !m_engine->getRuntimeData()) return;
            m_engine->getRuntimeData()->setCurrentArrangementTick(static_cast<int>(tick));
        });
        
        // Minimap visible range change -> scrollbar
        connect(m_minimap, &ArrangementMinimapWidget::visibleRangeChangeRequested, this, [this](int64_t startTick) {
            if (!m_timeline) return;
            int offset = static_cast<int>(startTick * m_timeline->getPixelsPerTick());
            m_timelineScrollBar->setValue(offset);
        });
        
        // Update minimap when timeline scrolls/zooms
        connect(m_timeline, &ArrangementTimelineWidget::horizontalOffsetChanged, this, [this](int) {
            updateMinimapVisibleRange();
        });
        connect(m_timeline, &ArrangementTimelineWidget::zoomChanged, this, [this](double) {
            updateMinimapVisibleRange();
        });
        
        // Sync loop region to minimap
        if (m_timelineRuler) {
            connect(m_timelineRuler, &ArrangementTimelineRuler::loopRegionChanged, this, [this](int64_t start, int64_t end) {
                m_minimap->setLoopRegion(start, end, m_timelineRuler->isLoopEnabled());
            });
            connect(m_timelineRuler, &ArrangementTimelineRuler::loopEnabledChanged, this, [this](bool enabled) {
                m_minimap->setLoopRegion(m_timelineRuler->getLoopStartTick(), 
                                         m_timelineRuler->getLoopEndTick(), enabled);
            });
        }
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
    
    // Connect tempo track editor to timeline and scrollbar
    if (m_tempoTrackEditor && m_timeline) {
        m_tempoTrackEditor->setTimelineWidget(m_timeline);
        
        // Sync scroll
        connect(m_timelineScrollBar, &QScrollBar::valueChanged,
                m_tempoTrackEditor, &ArrangementTempoTrackEditor::setHorizontalOffset);
        
        // Sync zoom
        connect(m_timeline, &ArrangementTimelineWidget::zoomChanged,
                m_tempoTrackEditor, &ArrangementTempoTrackEditor::setPixelsPerTick);
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
    
    // Also refresh the minimap to show new/modified clips
    refreshMinimap();
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
    if (m_minimap) {
        m_minimap->setPlayheadTick(tick);
    }
    if (m_tempoTrackEditor) {
        m_tempoTrackEditor->setPlayheadTick(tick);
    }
    
    // Auto-scroll to follow playhead
    if (m_autoScrollEnabled && m_engine && m_engine->isPlaying()) {
        autoScrollToPlayhead(tick);
    }
}

void ArrangementSection::updateMinimapVisibleRange()
{
    if (!m_minimap || !m_timeline) return;
    
    int64_t startTick = m_timeline->getVisibleStartTick();
    int64_t endTick = m_timeline->getVisibleEndTick();
    m_minimap->setVisibleTickRange(startTick, endTick);
}

void ArrangementSection::autoScrollToPlayhead(int tick)
{
    if (!m_timeline || !m_timelineScrollBar) return;
    
    int playheadX = static_cast<int>(tick * m_timeline->getPixelsPerTick()) - m_timeline->getHorizontalOffset();
    int viewportWidth = m_timeline->contentRect().width();
    
    // Define scroll margins (20% from edges)
    int leftMargin = viewportWidth / 5;
    int rightMargin = viewportWidth - viewportWidth / 5;
    
    // Scroll if playhead is near the right edge
    if (playheadX > rightMargin) {
        // Scroll to put playhead at left margin
        int newOffset = static_cast<int>(tick * m_timeline->getPixelsPerTick()) - leftMargin;
        newOffset = qMax(0, newOffset);
        m_timelineScrollBar->setValue(newOffset);
    }
    // Scroll if playhead is before the left edge (e.g., after loop)
    else if (playheadX < 0) {
        int newOffset = static_cast<int>(tick * m_timeline->getPixelsPerTick()) - leftMargin;
        newOffset = qMax(0, newOffset);
        m_timelineScrollBar->setValue(newOffset);
    }
}
void ArrangementSection::scrollToTick(int64_t tick)
{
    if (!m_timeline || !m_timelineScrollBar) return;
    
    int viewportWidth = m_timeline->contentRect().width();
    int leftMargin = viewportWidth / 5;
    
    // Calculate new offset to show the tick with some margin
    int newOffset = static_cast<int>(tick * m_timeline->getPixelsPerTick()) - leftMargin;
    newOffset = qMax(0, newOffset);
    
    // For start, just go to 0
    if (tick <= 0) {
        newOffset = 0;
    }
    
    m_timelineScrollBar->setValue(newOffset);
    updateMinimapVisibleRange();
}

void ArrangementSection::refreshMinimap()
{
    if (m_minimap) {
        m_minimap->update();
    }
    updateMinimapVisibleRange();
}