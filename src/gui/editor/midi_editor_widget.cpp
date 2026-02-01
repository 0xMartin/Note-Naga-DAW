#include "midi_editor_widget.h"
#include "midi_editor_context_menu.h"
#include "midi_editor_note_handler.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollBar>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>
#include <QApplication>
#include <QCursor>

#include "../nn_gui_utils.h"

#define MIN_NOTE 0
#define MAX_NOTE 127

MidiEditorWidget::MidiEditorWidget(NoteNagaEngine *engine, QWidget *parent)
    : QGraphicsView(parent), engine(engine)
{
    setObjectName("MidiEditorWidget");
    setFrameStyle(QFrame::NoFrame);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    this->config.time_scale = 0.2;
    this->config.key_height = 16;
    this->config.tact_subdiv = 4;
    this->config.looping = false;
    this->config.color_mode = NoteColorMode::TrackColor;

    this->content_width = 640;
    this->content_height = (127 - 0 + 1) * 16;

    // Initialize rubber band
    rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

    // Initialize helper classes
    m_noteHandler = new MidiEditorNoteHandler(this, this);
    m_contextMenu = new MidiEditorContextMenu(this, this);
    
    // Connect helper signals
    connect(m_noteHandler, &MidiEditorNoteHandler::selectionChanged, this, &MidiEditorWidget::selectionChanged);
    connect(m_noteHandler, &MidiEditorNoteHandler::notesModified, this, &MidiEditorWidget::notesModified);
    
    // Connect context menu signals
    connect(m_contextMenu, &MidiEditorContextMenu::colorModeChanged, this, &MidiEditorWidget::onColorModeChanged);
    connect(m_contextMenu, &MidiEditorContextMenu::deleteNotesRequested, this, &MidiEditorWidget::onDeleteNotes);
    connect(m_contextMenu, &MidiEditorContextMenu::duplicateNotesRequested, this, &MidiEditorWidget::onDuplicateNotes);
    connect(m_contextMenu, &MidiEditorContextMenu::selectAllRequested, this, &MidiEditorWidget::onSelectAll);
    connect(m_contextMenu, &MidiEditorContextMenu::invertSelectionRequested, this, &MidiEditorWidget::onInvertSelection);
    connect(m_contextMenu, &MidiEditorContextMenu::quantizeRequested, this, &MidiEditorWidget::onQuantize);
    connect(m_contextMenu, &MidiEditorContextMenu::transposeUpRequested, this, &MidiEditorWidget::onTransposeUp);
    connect(m_contextMenu, &MidiEditorContextMenu::transposeDownRequested, this, &MidiEditorWidget::onTransposeDown);
    connect(m_contextMenu, &MidiEditorContextMenu::transposeOctaveUpRequested, this, &MidiEditorWidget::onTransposeOctaveUp);
    connect(m_contextMenu, &MidiEditorContextMenu::transposeOctaveDownRequested, this, &MidiEditorWidget::onTransposeOctaveDown);
    connect(m_contextMenu, &MidiEditorContextMenu::setVelocityRequested, this, &MidiEditorWidget::onSetVelocity);

    this->title_widget = nullptr;
    initTitleUI();

    scene = new QGraphicsScene(this);
    setScene(scene);

    setBackgroundBrush(m_colors.bg_color);
    setMouseTracking(true);

    // Create legend widget for velocity/pan color modes
    m_legendWidget = new QWidget(this);
    m_legendWidget->setFixedSize(120, 60);
    m_legendWidget->setStyleSheet("background: transparent;");
    m_legendWidget->hide();

    setupConnections();

    this->last_seq = engine->getProject()->getActiveSequence();
    refreshAll();
    
    setFocusPolicy(Qt::StrongFocus);
}

MidiEditorWidget::~MidiEditorWidget() {
    delete rubberBand;
}

void MidiEditorWidget::setupConnections() {
    auto project = engine->getProject();

    connect(project, &NoteNagaProject::projectFileLoaded, this, [this]() {
        this->last_seq = engine->getProject()->getActiveSequence();
        refreshAll();
    });

    connect(project, &NoteNagaProject::activeSequenceChanged, this,
            [this](NoteNagaMidiSeq *seq) {
                this->last_seq = seq;
                refreshAll();
            });

    connect(project, &NoteNagaProject::sequenceMetadataChanged, this,
            [this](NoteNagaMidiSeq *seq, const std::string &) {
                this->last_seq = seq;
                refreshAll();
            });

    connect(project, &NoteNagaProject::trackMetaChanged, this,
            [this](NoteNagaTrack *track, const std::string &) {
                refreshTrack(track);
            });

    connect(project, &NoteNagaProject::currentTickChanged, this,
            &MidiEditorWidget::currentTickChanged);

    // Connect playback stopped to clear row highlights
    connect(engine, &NoteNagaEngine::playbackStopped, this,
            &MidiEditorWidget::onPlaybackStopped);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::horizontalScrollChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::verticalScrollChanged);
}

void MidiEditorWidget::initTitleUI() {
    if (this->title_widget)
        return;
    this->title_widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(title_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Note Duration Combo Box
    QLabel *lblNoteDur = new QLabel("Note:");
    lblNoteDur->setStyleSheet("color: #CCCCCC; font-size: 9pt;");
    combo_note_duration = new QComboBox();
    combo_note_duration->setFixedWidth(70);
    combo_note_duration->setStyleSheet("QComboBox QAbstractItemView { min-width: 70px; }");
    combo_note_duration->addItem("1/1", static_cast<int>(NoteDuration::Whole));
    combo_note_duration->addItem("1/2", static_cast<int>(NoteDuration::Half));
    combo_note_duration->addItem("1/4", static_cast<int>(NoteDuration::Quarter));
    combo_note_duration->addItem("1/8", static_cast<int>(NoteDuration::Eighth));
    combo_note_duration->addItem("1/16", static_cast<int>(NoteDuration::Sixteenth));
    combo_note_duration->addItem("1/32", static_cast<int>(NoteDuration::ThirtySecond));
    combo_note_duration->setCurrentIndex(2);

    // Grid Resolution Combo Box
    QLabel *lblGridRes = new QLabel("Grid:");
    lblGridRes->setStyleSheet("color: #CCCCCC; font-size: 9pt;");
    combo_grid_resolution = new QComboBox();
    combo_grid_resolution->setFixedWidth(70);
    combo_grid_resolution->setStyleSheet("QComboBox QAbstractItemView { min-width: 70px; }");
    combo_grid_resolution->addItem("1/1", static_cast<int>(GridResolution::Whole));
    combo_grid_resolution->addItem("1/2", static_cast<int>(GridResolution::Half));
    combo_grid_resolution->addItem("1/4", static_cast<int>(GridResolution::Quarter));
    combo_grid_resolution->addItem("1/8", static_cast<int>(GridResolution::Eighth));
    combo_grid_resolution->addItem("1/16", static_cast<int>(GridResolution::Sixteenth));
    combo_grid_resolution->addItem("1/32", static_cast<int>(GridResolution::ThirtySecond));
    combo_grid_resolution->addItem("Off", static_cast<int>(GridResolution::Off));
    combo_grid_resolution->setCurrentIndex(2);
    connect(combo_grid_resolution, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MidiEditorWidget::refreshAll);

    // Follow mode buttons
    this->btn_follow_center = create_small_button(
        ":/icons/follow-from-center.svg", "Follow from Center", "FollowCenter");
    this->btn_follow_center->setCheckable(true);
    connect(btn_follow_center, &QPushButton::clicked, this, [this]() {
        selectFollowMode(MidiEditorFollowMode::CenterIsCurrent);
    });
    
    this->btn_follow_left = create_small_button(":/icons/follow-from-left.svg",
                                                "Follow from Left", "FollowLeft");
    this->btn_follow_left->setCheckable(true);
    connect(btn_follow_left, &QPushButton::clicked, this, [this]() {
        selectFollowMode(MidiEditorFollowMode::LeftSideIsCurrent);
    });
    
    this->btn_follow_step = create_small_button(
        ":/icons/follow-step-by-step.svg", "Follow Step by Step", "FollowStep");
    this->btn_follow_step->setCheckable(true);
    connect(btn_follow_step, &QPushButton::clicked, this,
            [this]() { selectFollowMode(MidiEditorFollowMode::StepByStep); });
    
    this->btn_follow_none = create_small_button(":/icons/follow-none.svg",
                                                "Don't Follow", "FollowNone");
    this->btn_follow_none->setCheckable(true);
    connect(btn_follow_none, &QPushButton::clicked, this,
            [this]() { selectFollowMode(MidiEditorFollowMode::None); });
    selectFollowMode(MidiEditorFollowMode::CenterIsCurrent);

    // Zoom buttons
    QPushButton *btn_h_zoom_in = create_small_button(
        ":/icons/zoom-in-horizontal.svg", "Horizontal Zoom In", "HZoomIn");
    connect(btn_h_zoom_in, &QPushButton::clicked, this,
            [this]() { setTimeScale(config.time_scale * 1.2); });
    
    QPushButton *btn_h_zoom_out = create_small_button(
        ":/icons/zoom-out-horizontal.svg", "Horizontal Zoom Out", "HZoomOut");
    connect(btn_h_zoom_out, &QPushButton::clicked, this,
            [this]() { setTimeScale(config.time_scale / 1.2); });
    
    QPushButton *btn_v_zoom_in = create_small_button(
        ":/icons/zoom-in-vertical.svg", "Vertical Zoom In", "VZoomIn");
    connect(btn_v_zoom_in, &QPushButton::clicked, this,
            [this]() { setKeyHeight(ceil(config.key_height * 1.2)); });
    
    QPushButton *btn_v_zoom_out = create_small_button(
        ":/icons/zoom-out-vertical.svg", "Vertical Zoom Out", "VZoomOut");
    connect(btn_v_zoom_out, &QPushButton::clicked, this,
            [this]() { setKeyHeight(floor(config.key_height / 1.2)); });

    // Looping button
    btn_looping = create_small_button(":/icons/loop.svg", "Toggle Looping", "Looping");
    btn_looping->setCheckable(true);
    connect(btn_looping, &QPushButton::clicked, this, &MidiEditorWidget::enableLooping);
    enableLooping(btn_looping->isChecked());
    
    QPushButton *btn_step = create_small_button(":/icons/step-forward.svg",
                                                "Step Forward", "StepForward");

    // Add widgets to layout
    layout->addWidget(lblNoteDur, 0, Qt::AlignRight);
    layout->addWidget(combo_note_duration, 0, Qt::AlignRight);
    layout->addWidget(lblGridRes, 0, Qt::AlignRight);
    layout->addWidget(combo_grid_resolution, 0, Qt::AlignRight);
    layout->addWidget(create_separator());
    layout->addWidget(btn_step, 0, Qt::AlignRight);
    layout->addWidget(btn_looping, 0, Qt::AlignRight);
    layout->addWidget(create_separator());
    layout->addWidget(btn_v_zoom_out, 0, Qt::AlignRight);
    layout->addWidget(btn_v_zoom_in, 0, Qt::AlignRight);
    layout->addWidget(btn_h_zoom_out, 0, Qt::AlignRight);
    layout->addWidget(btn_h_zoom_in, 0, Qt::AlignRight);
    layout->addWidget(create_separator());
    layout->addWidget(btn_follow_center, 0, Qt::AlignRight);
    layout->addWidget(btn_follow_left, 0, Qt::AlignRight);
    layout->addWidget(btn_follow_step, 0, Qt::AlignRight);
    layout->addWidget(btn_follow_none, 0, Qt::AlignRight);
}

/*******************************************************************************************************/
// Public methods
/*******************************************************************************************************/

std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> MidiEditorWidget::getSelectedNotes() const {
    return m_noteHandler->getSelectedNotesData();
}

bool MidiEditorWidget::hasSelection() const {
    return m_noteHandler->hasSelection();
}

NoteDuration MidiEditorWidget::getNoteDuration() const {
    return static_cast<NoteDuration>(combo_note_duration->currentData().toInt());
}

/*******************************************************************************************************/
// Slots
/*******************************************************************************************************/

void MidiEditorWidget::refreshAll() {
    recalculateContentSize();
    updateScene();
}

void MidiEditorWidget::refreshMarker() {
    int marker_x = engine->getProject()->getCurrentTick() * config.time_scale;
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    bool visible = (marker_x > 0 && marker_x < content_width);
    
    if (visible) {
        if (!marker_line) {
            marker_line = scene->addLine(marker_x, visible_y0, marker_x, visible_y1,
                                         QPen(QColor(255, 88, 88), 2));
            marker_line->setZValue(1000);
        } else {
            marker_line->setLine(marker_x, visible_y0, marker_x, visible_y1);
            marker_line->setVisible(true);
        }
    } else if (marker_line) {
        marker_line->setVisible(false);
    }
}

void MidiEditorWidget::refreshTrack(NoteNagaTrack *track) {
    if (!last_seq || !track)
        return;
    m_noteHandler->clearTrackNoteItems(track->getId());
    updateTrackNotes(track);
}

void MidiEditorWidget::refreshSequence(NoteNagaMidiSeq *seq) {
    this->last_seq = seq;
    refreshAll();
}

void MidiEditorWidget::currentTickChanged(int tick) {
    if (engine->isPlaying()) {
        int marker_x = int(tick * config.time_scale);
        int width = viewport()->width();
        int current_scroll = this->horizontalScrollBar()->value();
        int value = current_scroll;

        switch (config.follow_mode) {
        case MidiEditorFollowMode::None:
            break;

        case MidiEditorFollowMode::LeftSideIsCurrent:
            value = marker_x;
            break;

        case MidiEditorFollowMode::CenterIsCurrent: {
            int margin = width / 2;
            int center = current_scroll + margin;
            if (marker_x > center) {
                value = marker_x - margin;
            } else if (marker_x < current_scroll) {
                value = marker_x - margin;
            }
            break;
        }

        case MidiEditorFollowMode::StepByStep: {
            int right = current_scroll + width;
            if (marker_x >= right) {
                value = current_scroll + width;
            } else if (marker_x < current_scroll) {
                value = marker_x;
            }
            break;
        }
        }

        value = std::max(0, value);
        value = std::min(value, content_width - width);
        this->horizontalScrollBar()->setValue(value);
        emit horizontalScrollChanged(value);
    }

    updateRowHighlights();
    refreshMarker();
}

void MidiEditorWidget::selectFollowMode(MidiEditorFollowMode mode) {
    config.follow_mode = mode;
    btn_follow_none->setChecked(false);
    btn_follow_center->setChecked(false);
    btn_follow_left->setChecked(false);
    btn_follow_step->setChecked(false);

    switch (config.follow_mode) {
    case MidiEditorFollowMode::None:
        btn_follow_none->setChecked(true);
        break;
    case MidiEditorFollowMode::LeftSideIsCurrent:
        btn_follow_left->setChecked(true);
        break;
    case MidiEditorFollowMode::CenterIsCurrent:
        btn_follow_center->setChecked(true);
        break;
    case MidiEditorFollowMode::StepByStep:
        btn_follow_step->setChecked(true);
        break;
    }

    emit followModeChanged(config.follow_mode);
}

void MidiEditorWidget::enableLooping(bool enabled) {
    if (config.looping == enabled) return;
    config.looping = enabled;
    this->engine->enableLooping(enabled);
    emit loopingChanged(config.looping);
}

void MidiEditorWidget::setTimeScale(double scale) {
    double old_scale = config.time_scale;
    int viewport_width = viewport()->width();
    int old_scroll = horizontalScrollBar()->value();

    double center_tick = (old_scroll + viewport_width / 2.0) / old_scale;

    config.time_scale = std::max(0.02, scale);
    emit timeScaleChanged(config.time_scale);

    recalculateContentSize();

    int new_scroll = int(center_tick * config.time_scale - viewport_width / 2.0);
    new_scroll = std::max(0, std::min(new_scroll, content_width - viewport_width));
    horizontalScrollBar()->setValue(new_scroll);
    emit horizontalScrollChanged(new_scroll);

    refreshAll();
}

void MidiEditorWidget::setKeyHeight(int h) {
    int old_height = config.key_height;
    int viewport_height = viewport()->height();
    int old_scroll = verticalScrollBar()->value();

    double center_key = (old_scroll + viewport_height / 2.0) / old_height;

    config.key_height = std::max(5, std::min(30, h));
    emit keyHeightChanged(config.key_height);

    recalculateContentSize();

    int new_scroll = int(center_key * config.key_height - viewport_height / 2.0);
    new_scroll = std::max(0, std::min(new_scroll, content_height - viewport_height));
    verticalScrollBar()->setValue(new_scroll);
    emit verticalScrollChanged(new_scroll);

    refreshAll();
}

void MidiEditorWidget::onPlaybackStopped() {
    // Clear active notes and update row highlights when playback stops
    m_activeNotes.clear();
    updateRowHighlights();
}

/*******************************************************************************************************/
// Context Menu Action Slots
/*******************************************************************************************************/

void MidiEditorWidget::onColorModeChanged(NoteColorMode mode) {
    config.color_mode = mode;
    updateLegendVisibility();
    refreshAll();
}

void MidiEditorWidget::onDeleteNotes() {
    m_noteHandler->deleteSelectedNotes();
}

void MidiEditorWidget::onDuplicateNotes() {
    m_noteHandler->duplicateSelectedNotes();
}

void MidiEditorWidget::onSelectAll() {
    m_noteHandler->selectAll();
}

void MidiEditorWidget::onInvertSelection() {
    m_noteHandler->invertSelection();
}

void MidiEditorWidget::onQuantize() {
    m_noteHandler->quantizeSelectedNotes();
}

void MidiEditorWidget::onTransposeUp() {
    m_noteHandler->transposeSelectedNotes(1);
}

void MidiEditorWidget::onTransposeDown() {
    m_noteHandler->transposeSelectedNotes(-1);
}

void MidiEditorWidget::onTransposeOctaveUp() {
    m_noteHandler->transposeSelectedNotes(12);
}

void MidiEditorWidget::onTransposeOctaveDown() {
    m_noteHandler->transposeSelectedNotes(-12);
}

void MidiEditorWidget::onSetVelocity(int velocity) {
    m_noteHandler->setSelectedNotesVelocity(velocity);
}

/*******************************************************************************************************/
// Mouse Event Handlers
/*******************************************************************************************************/

void MidiEditorWidget::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    
    // Reposition legend widget
    if (m_legendWidget && m_legendWidget->isVisible()) {
        int x = width() - m_legendWidget->width() - 10;
        int y = 10;
        m_legendWidget->move(x, y);
    }
    
    refreshAll();
}

void MidiEditorWidget::mousePressEvent(QMouseEvent *event) {
    if (!last_seq) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    
    setFocus();
    QPointF scenePos = mapToScene(event->pos());
    
    if (event->button() == Qt::LeftButton) {
        m_clickStartPos = scenePos;
        m_isDragging = false;
        
        NoteGraphics *noteUnderCursor = m_noteHandler->findNoteUnderCursor(scenePos);
        
        if (noteUnderCursor) {
            bool isSelected = m_noteHandler->selectedNotes().contains(noteUnderCursor);
            
            if (!isSelected) {
                // Select the note
                m_noteHandler->selectNote(noteUnderCursor, !(event->modifiers() & Qt::ControlModifier));
            }
            
            // Start drag for move/resize
            if (m_noteHandler->isNoteEdge(noteUnderCursor, scenePos)) {
                m_noteHandler->startDrag(scenePos, NoteDragMode::Resize);
                QApplication::setOverrideCursor(Qt::SizeHorCursor);
            } else {
                m_noteHandler->startDrag(scenePos, NoteDragMode::Move);
                QApplication::setOverrideCursor(Qt::SizeAllCursor);
            }
            m_isDragging = true;
        } else {
            // Clear selection unless Ctrl is held
            if (!(event->modifiers() & Qt::ControlModifier)) {
                m_noteHandler->clearSelection();
            }
            
            // Start rubber band selection
            m_noteHandler->startDrag(scenePos, NoteDragMode::Select);
            rubberBandOrigin = event->pos();
            rubberBand->setGeometry(QRect(rubberBandOrigin, QSize()));
            rubberBand->show();
        }
    }
    else if (event->button() == Qt::RightButton) {
        // Show context menu
        m_contextMenu->show(event->globalPos(), m_noteHandler->hasSelection());
    }
    
    QGraphicsView::mousePressEvent(event);
}

void MidiEditorWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!last_seq) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    
    QPointF scenePos = mapToScene(event->pos());
    NoteDragMode dragMode = m_noteHandler->dragMode();
    
    if (dragMode == NoteDragMode::Select && rubberBand->isVisible()) {
        rubberBand->setGeometry(QRect(rubberBandOrigin, event->pos()).normalized());
    }
    else if (dragMode == NoteDragMode::Move && m_noteHandler->hasSelection()) {
        // Calculate delta from last position
        static QPointF lastPos = scenePos;
        QPointF actualDelta = scenePos - lastPos;
        lastPos = scenePos;
        m_noteHandler->moveSelectedNotes(actualDelta);
        m_noteHandler->updateDrag(scenePos);
        m_isDragging = true;
    }
    else if (dragMode == NoteDragMode::Resize && m_noteHandler->hasSelection()) {
        static QPointF lastPos = scenePos;
        QPointF delta = scenePos - lastPos;
        lastPos = scenePos;
        m_noteHandler->resizeSelectedNotes(delta);
        m_noteHandler->updateDrag(scenePos);
    }
    else if (dragMode == NoteDragMode::None) {
        NoteGraphics *noteUnderCursor = m_noteHandler->findNoteUnderCursor(scenePos);
        if (noteUnderCursor && m_noteHandler->isNoteEdge(noteUnderCursor, scenePos)) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
    
    QGraphicsView::mouseMoveEvent(event);
}

void MidiEditorWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        NoteDragMode dragMode = m_noteHandler->dragMode();
        QPointF scenePos = mapToScene(event->pos());
        
        // Check if it was a click (no significant movement)
        bool wasClick = (scenePos - m_clickStartPos).manhattanLength() < 3;
        
        if (dragMode == NoteDragMode::Select && rubberBand->isVisible()) {
            QRect viewRect = rubberBand->geometry();
            QRectF sceneRect = mapToScene(viewRect).boundingRect();
            
            // Only select if there was actual dragging
            if (!wasClick) {
                m_noteHandler->selectNotesInRect(sceneRect);
            } else {
                // It was a click on empty space - add new note
                m_noteHandler->addNewNote(m_clickStartPos);
                
                // Also set playback position
                int tick = sceneXToTick(m_clickStartPos.x());
                engine->getProject()->setCurrentTick(tick);
                emit positionSelected(tick);
                refreshMarker();
            }
            rubberBand->hide();
        }
        
        if (dragMode == NoteDragMode::Move || dragMode == NoteDragMode::Resize) {
            if (!wasClick) {
                m_noteHandler->applyNoteChanges();
            }
            QApplication::restoreOverrideCursor();
        }
        
        m_noteHandler->endDrag();
        m_isDragging = false;
    }
    
    QGraphicsView::mouseReleaseEvent(event);
}

void MidiEditorWidget::wheelEvent(QWheelEvent *event) {
    Qt::KeyboardModifiers mods = event->modifiers();

#ifdef Q_OS_MAC
    bool ctrlZoom = mods & (Qt::ControlModifier | Qt::MetaModifier);
#else
    bool ctrlZoom = mods & Qt::ControlModifier;
#endif

    if (ctrlZoom) {
        double zoom_factor = (event->angleDelta().y() > 0) ? 1.2 : 0.8;
        setTimeScale(config.time_scale * zoom_factor);
    } else if (std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y())) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->angleDelta().x() / 8);
    } else {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - event->angleDelta().y() / 8);
    }

    event->accept();
}

void MidiEditorWidget::keyPressEvent(QKeyEvent *event) {
    if (!last_seq) {
        QGraphicsView::keyPressEvent(event);
        return;
    }
    
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && m_noteHandler->hasSelection()) {
        m_noteHandler->deleteSelectedNotes();
    }
    else if (event->key() == Qt::Key_Escape) {
        m_noteHandler->clearSelection();
    }
    else if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        m_noteHandler->selectAll();
    }
    else if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        m_noteHandler->duplicateSelectedNotes();
    }
    else if (event->key() == Qt::Key_Q && (event->modifiers() & Qt::ControlModifier)) {
        m_noteHandler->quantizeSelectedNotes();
    }
    else if (event->key() == Qt::Key_Up && m_noteHandler->hasSelection()) {
        int semitones = (event->modifiers() & Qt::ShiftModifier) ? 12 : 1;
        m_noteHandler->transposeSelectedNotes(semitones);
    }
    else if (event->key() == Qt::Key_Down && m_noteHandler->hasSelection()) {
        int semitones = (event->modifiers() & Qt::ShiftModifier) ? -12 : -1;
        m_noteHandler->transposeSelectedNotes(semitones);
    }
    else {
        QGraphicsView::keyPressEvent(event);
    }
}

/*******************************************************************************************************/
// Scene Update Methods
/*******************************************************************************************************/

void MidiEditorWidget::recalculateContentSize() {
    if (last_seq) {
        content_width = int((last_seq->getMaxTick() + 1) * config.time_scale) + 16;
        content_height = (MAX_NOTE - MIN_NOTE + 1) * config.key_height;
    } else {
        content_width = 640;
        content_height = (MAX_NOTE - MIN_NOTE + 1) * config.key_height;
    }
    setSceneRect(0, 0, content_width, content_height);
}

void MidiEditorWidget::updateScene() {
    clearScene();
    scene->setBackgroundBrush(m_colors.bg_color);

    if (!last_seq) {
        auto *txt = scene->addSimpleText("Open file");
        txt->setBrush(m_colors.fg_color);
        txt->setFont(QFont("Arial", 22, QFont::Bold));
        txt->setPos(sceneRect().width() / 2 - 100, sceneRect().height() / 2 - 20);
        return;
    }

    updateGrid();
    updateBarGrid();
    updateAllNotes();
    refreshMarker();
}

void MidiEditorWidget::updateGrid() {
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    row_backgrounds.clear();

    for (int idx = 0, note_val = MIN_NOTE; note_val <= MAX_NOTE; ++idx, ++note_val) {
        int y = content_height - (idx + 1) * config.key_height;
        if (y + config.key_height < visible_y0 || y > visible_y1) continue;
        
        QColor row_bg = (note_val % 2 == 0) ? m_colors.grid_row_color1 : m_colors.grid_row_color2;
        
        auto row_bg_rect = scene->addRect(0, y, content_width, config.key_height,
                                          QPen(Qt::NoPen), QBrush(row_bg));
        row_bg_rect->setZValue(-100);
        row_bg_rect->setData(0, note_val);
        row_backgrounds.push_back(row_bg_rect);

        auto l = scene->addLine(0, y, content_width, y, QPen(m_colors.line_color, 1));
        grid_lines.push_back(l);
    }
    
    m_lastActiveNotes.clear();
    updateRowHighlights();
}

void MidiEditorWidget::updateRowHighlights() {
    if (!last_seq) return;
    
    updateActiveNotes();
    
    if (m_activeNotes == m_lastActiveNotes) return;
    
    QSet<int> changedNotes;
    for (auto it = m_activeNotes.begin(); it != m_activeNotes.end(); ++it) {
        if (!m_lastActiveNotes.contains(it.key()) || m_lastActiveNotes.value(it.key()) != it.value()) {
            changedNotes.insert(it.key());
        }
    }
    for (auto it = m_lastActiveNotes.begin(); it != m_lastActiveNotes.end(); ++it) {
        if (!m_activeNotes.contains(it.key())) {
            changedNotes.insert(it.key());
        }
    }
    
    const auto& tracks = last_seq->getTracks();
    for (auto* row_rect : row_backgrounds) {
        if (!row_rect) continue;
        
        int note_val = row_rect->data(0).toInt();
        if (!changedNotes.contains(note_val)) continue;
        
        QColor row_bg = (note_val % 2 == 0) ? m_colors.grid_row_color1 : m_colors.grid_row_color2;
        
        if (m_activeNotes.contains(note_val)) {
            int trackIdx = m_activeNotes.value(note_val, 0);
            if (trackIdx >= 0 && trackIdx < (int)tracks.size() && tracks[trackIdx]) {
                QColor trackColor = tracks[trackIdx]->getColor().toQColor();
                int r = (row_bg.red() * 85 + trackColor.red() * 15) / 100;
                int g = (row_bg.green() * 85 + trackColor.green() * 15) / 100;
                int b = (row_bg.blue() * 85 + trackColor.blue() * 15) / 100;
                row_bg = QColor(r, g, b);
            }
        }
        
        row_rect->setBrush(QBrush(row_bg));
    }
    
    m_lastActiveNotes = m_activeNotes;
}

void MidiEditorWidget::updateActiveNotes() {
    m_activeNotes.clear();
    
    if (!last_seq || !engine->isPlaying()) {
        return;
    }
    
    int currentTick = engine->getProject()->getCurrentTick();
    const auto& tracks = last_seq->getTracks();
    
    for (int trackIdx = 0; trackIdx < (int)tracks.size(); ++trackIdx) {
        const auto& track = tracks[trackIdx];
        if (!track || !track->isVisible()) continue;
        
        for (const auto& note : track->getNotes()) {
            if (!note.start.has_value() || !note.length.has_value()) continue;
            
            int noteStart = note.start.value();
            int noteEnd = noteStart + note.length.value();
            
            if (currentTick >= noteStart && currentTick < noteEnd) {
                if (!m_activeNotes.contains(note.note)) {
                    m_activeNotes.insert(note.note, trackIdx);
                }
            }
        }
    }
}

void MidiEditorWidget::updateBarGrid() {
    if (!last_seq) return;

    int ppq = last_seq->getPPQ();
    int bar_length = ppq * 4;
    int first_bar = 0;
    int last_bar = (last_seq->getMaxTick() / bar_length) + 2;

    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();

    double px_per_bar = config.time_scale * bar_length;

    int bar_skip = 1;
    double min_bar_dist_px = 58;
    while (px_per_bar * bar_skip < min_bar_dist_px) {
        bar_skip *= 2;
    }

    QFont label_font("Arial", 11, QFont::Bold);
    for (int bar = first_bar; bar < last_bar; bar += bar_skip) {
        int x = tickToSceneX(bar * bar_length);
        if (x < visible_x0 - 200 || x > visible_x1 + 200) continue;

        auto l = scene->addLine(x, 0, x, content_height, QPen(m_colors.grid_bar_color, 1.5));
        l->setZValue(2);
        bar_grid_lines.push_back(l);

        if (px_per_bar > 30) {
            auto label = scene->addSimpleText(QString::number(bar + 1));
            label->setFont(label_font);
            label->setBrush(m_colors.grid_bar_label_color);
            label->setPos(x + 4, visible_y0 + 4);
            label->setZValue(9999);
            bar_grid_labels.push_back(label);
        }
    }

    int grid_step_ticks = getGridStepTicks();
    if (grid_step_ticks == 0) return;

    double px_per_grid_step = config.time_scale * grid_step_ticks;
    int grid_skip = 1;
    while (px_per_grid_step * grid_skip < 8.0) {
        grid_skip *= 2;
    }

    int total_ticks = last_bar * bar_length;
    for (int tick = 0; tick < total_ticks; tick += grid_step_ticks * grid_skip) {
        if (tick % bar_length == 0) continue;

        int x = tickToSceneX(tick);
        if (x < visible_x0 - 200 || x > visible_x1 + 200) continue;
        
        auto lsub = scene->addLine(x, 0, x, content_height, QPen(m_colors.grid_subdiv_color, 1));
        lsub->setZValue(1);
    }
}

void MidiEditorWidget::updateAllNotes() {
    m_noteHandler->clearNoteItems();
    if (!last_seq) return;
    
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    for (const auto &track : last_seq->getTracks()) {
        if (!track || !track->isVisible()) continue;
        
        bool is_drum = engine->getMixer()->isPercussion(track);
        bool is_selected = last_seq->getActiveTrack() &&
                         last_seq->getActiveTrack()->getId() == track->getId();

        for (const auto &note : track->getNotes()) {
            if (!note.start.has_value() || !note.length.has_value()) continue;
            
            int y = content_height - (note.note - MIN_NOTE + 1) * config.key_height;
            int x = note.start.value() * config.time_scale;
            int w = std::max(1, int(note.length.value() * config.time_scale));
            int h = config.key_height;
            
            if (!((x + w > visible_x0 && x < visible_x1) &&
                (y + h > visible_y0 && y < visible_y1))) continue;
                
            drawNote(note, track, is_selected, is_drum, x, y, w, h);
        }
    }
}

void MidiEditorWidget::updateTrackNotes(NoteNagaTrack *track) {
    if (!last_seq || !track) return;
    
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    bool is_drum = engine->getMixer()->isPercussion(track);
    bool is_selected = last_seq->getActiveTrack() &&
                     last_seq->getActiveTrack()->getId() == track->getId();

    for (const auto &note : track->getNotes()) {
        if (!note.start.has_value() || !note.length.has_value()) continue;
        
        int y = content_height - (note.note - MIN_NOTE + 1) * config.key_height;
        int x = note.start.value() * config.time_scale;
        int w = std::max(1, int(note.length.value() * config.time_scale));
        int h = config.key_height;
        
        if (!((x + w > visible_x0 && x < visible_x1) &&
              (y + h > visible_y0 && y < visible_y1))) continue;
              
        drawNote(note, track, is_selected, is_drum, x, y, w, h);
    }
}

QColor MidiEditorWidget::getNoteColor(const NN_Note_t &note, const NoteNagaTrack *track) const {
    switch (config.color_mode) {
    case NoteColorMode::Velocity: {
        int vel = note.velocity.value_or(100);
        // Blue (low) -> Green (mid) -> Red (high)
        if (vel < 64) {
            int t = vel * 4;
            return QColor(0, t, 255 - t);
        } else {
            int t = (vel - 64) * 4;
            return QColor(t, 255 - t, 0);
        }
    }
    case NoteColorMode::Pan: {
        // Pan: 0=left (blue), 64=center (green), 127=right (red)
        int pan = note.pan.value_or(64); // Default to center
        pan = std::clamp(pan, 0, 127);
        if (pan < 64) {
            // Left side: Blue -> Green
            int t = pan * 4; // 0-252
            return QColor(0, t, 255 - t);
        } else {
            // Right side: Green -> Red
            int t = (pan - 64) * 4; // 0-252
            return QColor(t, 255 - t, 0);
        }
    }
    case NoteColorMode::TrackColor:
    default:
        return track->getColor().toQColor();
    }
}

void MidiEditorWidget::drawNote(const NN_Note_t &note, const NoteNagaTrack *track,
                                bool is_selected, bool is_drum, int x, int y, int w, int h) {
    QGraphicsItem *shape = nullptr;
    
    QColor baseColor = getNoteColor(note, track);
    NN_Color_t t_color = is_selected ? NN_Color_t::fromQColor(baseColor)
                                     : nn_color_blend(NN_Color_t::fromQColor(baseColor),
                                                      NN_Color_t::fromQColor(m_colors.bg_color), 0.3);
    QPen outline = getNotePen(track, is_selected, false);

    if (is_drum) {
        int sz = h * 0.6;
        int cx = x + w / 2;
        int cy = y + h / 2;
        int left = cx - sz / 2;
        int top = cy - sz / 2;
        shape = scene->addEllipse(left, top, sz, sz, outline, QBrush(t_color.toQColor()));
    } else {
        shape = scene->addRect(x, y, w, h, outline, QBrush(t_color.toQColor()));
    }
    shape->setZValue(is_selected ? 999 : track->getId() + 10);

    QGraphicsSimpleTextItem *txt = nullptr;
    if (!is_drum && w > 20 && h > 9 && config.time_scale > 0.04) {
        float luminance = nn_yiq_luminance(t_color);
        QString note_str = QString::fromStdString(nn_note_name(note.note));
        txt = scene->addSimpleText(note_str);
        txt->setBrush(QBrush(luminance < 128 ? Qt::white : Qt::black));
        QFont f("Arial", std::max(6, h - 6));
        txt->setFont(f);
        txt->setPos(x + 2, y + 2);
        txt->setZValue(shape->zValue() + 1);
    }
    
    NoteGraphics ng = {shape, txt, note, const_cast<NoteNagaTrack*>(track)};
    m_noteHandler->noteItems()[track->getId()].push_back(ng);
}

void MidiEditorWidget::clearScene() {
    // Clear note items tracking BEFORE scene->clear() to avoid dangling pointers
    // scene->clear() will delete all items, so we just clear our tracking structures
    m_noteHandler->noteItems().clear();
    auto &selectedNotes = const_cast<QList<NoteGraphics*>&>(m_noteHandler->selectedNotes());
    selectedNotes.clear();
    
    // Now safe to clear the scene - all tracking structures are already cleared
    scene->clear();
    
    grid_lines.clear();
    bar_grid_lines.clear();
    bar_grid_labels.clear();
    row_backgrounds.clear();
    marker_line = nullptr;
    m_lastActiveNotes.clear();
}

/*******************************************************************************************************/
// Coordinate Conversion Helpers
/*******************************************************************************************************/

int MidiEditorWidget::sceneXToTick(qreal x) const {
    return std::max(0, (int)(x / config.time_scale));
}

int MidiEditorWidget::sceneYToNote(qreal y) const {
    int noteIdx = (content_height - y) / config.key_height;
    return std::max(MIN_NOTE, std::min(MAX_NOTE, MIN_NOTE + noteIdx));
}

qreal MidiEditorWidget::tickToSceneX(int tick) const {
    return tick * config.time_scale;
}

qreal MidiEditorWidget::noteToSceneY(int note) const {
    return content_height - (note - MIN_NOTE + 1) * config.key_height;
}

int MidiEditorWidget::getGridStepTicks() const {
    if (!last_seq) return 0;
    
    GridResolution resolution = static_cast<GridResolution>(combo_grid_resolution->currentData().toInt());
    if (resolution == GridResolution::Off) return 0;

    int ppq = last_seq->getPPQ();
    
    switch (resolution) {
        case GridResolution::Whole:       return ppq * 4;
        case GridResolution::Half:        return ppq * 2;
        case GridResolution::Quarter:     return ppq;
        case GridResolution::Eighth:      return ppq / 2;
        case GridResolution::Sixteenth:   return ppq / 4;
        case GridResolution::ThirtySecond:return ppq / 8;
        default: return ppq;
    }
}

QPen MidiEditorWidget::getNotePen(const NoteNagaTrack *track, bool is_active_track, bool is_selected_note) const {
    if (is_selected_note) {
        return QPen(m_colors.selection_color, 2);
    }
    
    NN_Color_t t_color = track->getColor();
    float luminance = nn_yiq_luminance(t_color);
    
    if (is_active_track) {
        return QPen(luminance < 128 ? Qt::white : Qt::black, 2);
    }
    
    return QPen((luminance < 128 ? t_color.lighter(150) : t_color.darker(150)).toQColor());
}

int MidiEditorWidget::snapTickToGrid(int tick) const {
    int grid_step = getGridStepTicks();
    if (grid_step == 0) return tick;
    return (tick / grid_step) * grid_step;
}

int MidiEditorWidget::snapTickToGridNearest(int tick) const {
    int grid_step = getGridStepTicks();
    if (grid_step == 0) return tick;
    return static_cast<int>(round(static_cast<double>(tick) / grid_step)) * grid_step;
}

void MidiEditorWidget::updateLegendVisibility() {
    if (!m_legendWidget) return;
    
    bool showLegend = (config.color_mode == NoteColorMode::Velocity || 
                       config.color_mode == NoteColorMode::Pan);
    
    if (showLegend) {
        // Update legend content
        QString labelText = (config.color_mode == NoteColorMode::Velocity) ? "Velocity" : "Pan";
        QString leftLabel = (config.color_mode == NoteColorMode::Velocity) ? "0" : "L";
        QString midLabel = (config.color_mode == NoteColorMode::Velocity) ? "64" : "C";
        QString rightLabel = (config.color_mode == NoteColorMode::Velocity) ? "127" : "R";
        
        // Create gradient bar with labels
        QString gradientStyle = 
            "QWidget#legendBar {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "    stop:0 rgb(0,0,255), stop:0.5 rgb(0,255,0), stop:1 rgb(255,0,0));"
            "  border: 1px solid #555;"
            "  border-radius: 2px;"
            "}";
        
        // Clear old layout completely
        if (m_legendWidget->layout()) {
            QLayout *oldLayout = m_legendWidget->layout();
            // Delete all child widgets and layouts recursively
            while (QLayoutItem *item = oldLayout->takeAt(0)) {
                if (QWidget *widget = item->widget()) {
                    widget->deleteLater();
                }
                if (QLayout *childLayout = item->layout()) {
                    while (QLayoutItem *childItem = childLayout->takeAt(0)) {
                        if (QWidget *childWidget = childItem->widget()) {
                            childWidget->deleteLater();
                        }
                        delete childItem;
                    }
                }
                delete item;
            }
            delete oldLayout;
        }
        
        QVBoxLayout *layout = new QVBoxLayout(m_legendWidget);
        layout->setContentsMargins(5, 5, 5, 5);
        layout->setSpacing(2);
        
        QLabel *titleLabel = new QLabel(labelText);
        titleLabel->setStyleSheet("color: white; font-size: 10px; font-weight: bold;");
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);
        
        QWidget *gradientBar = new QWidget();
        gradientBar->setObjectName("legendBar");
        gradientBar->setFixedHeight(12);
        gradientBar->setStyleSheet(gradientStyle);
        layout->addWidget(gradientBar);
        
        QHBoxLayout *labelsLayout = new QHBoxLayout();
        labelsLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel *lbl0 = new QLabel(leftLabel);
        lbl0->setStyleSheet("color: white; font-size: 9px;");
        lbl0->setAlignment(Qt::AlignLeft);
        
        QLabel *lbl64 = new QLabel(midLabel);
        lbl64->setStyleSheet("color: white; font-size: 9px;");
        lbl64->setAlignment(Qt::AlignCenter);
        
        QLabel *lbl127 = new QLabel(rightLabel);
        lbl127->setStyleSheet("color: white; font-size: 9px;");
        lbl127->setAlignment(Qt::AlignRight);
        
        labelsLayout->addWidget(lbl0);
        labelsLayout->addStretch();
        labelsLayout->addWidget(lbl64);
        labelsLayout->addStretch();
        labelsLayout->addWidget(lbl127);
        
        layout->addLayout(labelsLayout);
        
        // Position in top-right corner
        int x = width() - m_legendWidget->width() - 10;
        int y = 10;
        m_legendWidget->move(x, y);
        m_legendWidget->show();
        m_legendWidget->raise();
    } else {
        m_legendWidget->hide();
    }
}
