#include "midi_editor_widget.h"

#include <QHBoxLayout>
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
    : QGraphicsView(parent), engine(engine), bg_color("#32353c"),
      fg_color("#e0e6ef"), line_color("#232731"), subline_color("#464a56"),
      grid_bar_color("#6177d1"), grid_row_color1("#35363b"),
      grid_row_color2("#292a2e"), grid_bar_label_color("#6fa5ff"),
      grid_subdiv_color("#44464b"), selection_color("#70a7ff") {
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

    this->content_width = 640;
    this->content_height = (127 - 0 + 1) * 16;

    // Inicializace RubberBand pro výběr více not
    rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

    this->title_widget = nullptr;
    initTitleUI();

    scene = new QGraphicsScene(this);
    setScene(scene);

    setBackgroundBrush(bg_color);
    setMouseTracking(true);

    setupConnections();

    // Init last_seq_ to current active sequence if available
    this->last_seq = engine->getProject()->getActiveSequence();
    refreshAll();
    
    // Povolení reakce na stisk klávesy
    setFocusPolicy(Qt::StrongFocus);
}

MidiEditorWidget::~MidiEditorWidget() {
    delete rubberBand;
}

void MidiEditorWidget::setupConnections() {
    auto project = engine->getProject();

    // project file loaded
    connect(project, &NoteNagaProject::projectFileLoaded, this, [this]() {
        this->last_seq = engine->getProject()->getActiveSequence();
        refreshAll();
    });

    // active sequence changed (switching, opening another file, etc.)
    connect(project, &NoteNagaProject::activeSequenceChanged, this,
            [this](NoteNagaMidiSeq *seq) {
                this->last_seq = seq;
                refreshAll();
            });

    // sequence metadata changed (refresh this sequence)
    connect(project, &NoteNagaProject::sequenceMetadataChanged, this,
            [this](NoteNagaMidiSeq *seq, const std::string &) {
                this->last_seq = seq;
                refreshAll();
            });

    // track metadata changed (refresh only the given track)
    connect(project, &NoteNagaProject::trackMetaChanged, this,
            [this](NoteNagaTrack *track, const std::string &) {
                refreshTrack(track);
            });

    // playback position changed
    connect(project, &NoteNagaProject::currentTickChanged, this,
            &MidiEditorWidget::currentTickChanged);

    // scrollbars value changed
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::horizontalScrollChanged);  // Emit signal for sync with other widgets
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

    // --- Note Duration Combo Box ---
    QLabel *lblNoteDur = new QLabel("Note:");
    lblNoteDur->setStyleSheet("color: #CCCCCC; font-size: 9pt;");
    combo_note_duration = new QComboBox();
    combo_note_duration->setFixedWidth(60);
    combo_note_duration->addItem("1/1", static_cast<int>(NoteDuration::Whole));
    combo_note_duration->addItem("1/2", static_cast<int>(NoteDuration::Half));
    combo_note_duration->addItem("1/4", static_cast<int>(NoteDuration::Quarter));
    combo_note_duration->addItem("1/8", static_cast<int>(NoteDuration::Eighth));
    combo_note_duration->addItem("1/16", static_cast<int>(NoteDuration::Sixteenth));
    combo_note_duration->addItem("1/32", static_cast<int>(NoteDuration::ThirtySecond));
    combo_note_duration->setCurrentIndex(2); // 1/4 default
   

    // --- Grid Resolution Combo Box ---
    QLabel *lblGridRes = new QLabel("Grid:");
    lblGridRes->setStyleSheet("color: #CCCCCC; font-size: 9pt;");
    combo_grid_resolution = new QComboBox();
    combo_grid_resolution->setFixedWidth(60);
    combo_grid_resolution->addItem("1/1", static_cast<int>(GridResolution::Whole));
    combo_grid_resolution->addItem("1/2", static_cast<int>(GridResolution::Half));
    combo_grid_resolution->addItem("1/4", static_cast<int>(GridResolution::Quarter));
    combo_grid_resolution->addItem("1/8", static_cast<int>(GridResolution::Eighth));
    combo_grid_resolution->addItem("1/16", static_cast<int>(GridResolution::Sixteenth));
    combo_grid_resolution->addItem("1/32", static_cast<int>(GridResolution::ThirtySecond));
    combo_grid_resolution->addItem("Off", static_cast<int>(GridResolution::Off));
    combo_grid_resolution->setCurrentIndex(2); // 1/4 default
    connect(combo_grid_resolution, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MidiEditorWidget::refreshAll);

    // follow mode buttons
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

    // zoom in/out for horizontal and vertical
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

    // looping button
    btn_looping =
        create_small_button(":/icons/loop.svg", "Toggle Looping", "Looping");
    btn_looping->setCheckable(true);
    connect(btn_looping, &QPushButton::clicked, this,
            &MidiEditorWidget::enableLooping);
    enableLooping(btn_looping->isChecked());
    // Step forward button
    QPushButton *btn_step = create_small_button(":/icons/step-forward.svg",
                                                "Step Forward", "StepForward");

    // Přidání widgetů do layoutu
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
// Public methods and slots
/*******************************************************************************************************/

void MidiEditorWidget::refreshAll() {
    recalculateContentSize();
    updateScene();
}

void MidiEditorWidget::refreshMarker() {
    if (marker_line) {
        scene->removeItem(marker_line);
        delete marker_line;
        marker_line = nullptr;
    }

    int marker_x = engine->getProject()->getCurrentTick() * config.time_scale;
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    if (marker_x > 0 && marker_x < content_width) {
        marker_line = scene->addLine(marker_x, visible_y0, marker_x, visible_y1,
                                     QPen(QColor(255, 88, 88), 2));
        marker_line->setZValue(1000);
    }
}

void MidiEditorWidget::refreshTrack(NoteNagaTrack *track) {
    if (!last_seq || !track)
        return;
    clearTrackNotes(track->getId());
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

        // set value for horizontal scroll and emit signal
        value = std::max(0, value);
        value = std::min(value, content_width - width);
        this->horizontalScrollBar()->setValue(value);
        emit horizontalScrollChanged(value);
    }

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
    if (config.looping == enabled)
        return; // No change
    config.looping = enabled;
    this->engine->enableLooping(enabled);
    emit loopingChanged(config.looping);
}

void MidiEditorWidget::setTimeScale(double scale) {
    double old_scale = config.time_scale;
    int viewport_width = viewport()->width();
    int old_scroll = horizontalScrollBar()->value();

    // Časová pozice ve středu viewportu
    double center_tick = (old_scroll + viewport_width / 2.0) / old_scale;

    config.time_scale = std::max(0.02, scale);
    emit timeScaleChanged(config.time_scale);

    // Aktualizuj obsah (pro novou šířku)
    recalculateContentSize();

    // Nastav scroll tak, ať střed zůstane na stejném ticku
    int new_scroll = int(center_tick * config.time_scale - viewport_width / 2.0);
    new_scroll =
        std::max(0, std::min(new_scroll, content_width - viewport_width));
    horizontalScrollBar()->setValue(new_scroll);
    emit horizontalScrollChanged(new_scroll);

    refreshAll();
}

void MidiEditorWidget::setKeyHeight(int h) {
    int old_height = config.key_height;
    int viewport_height = viewport()->height();
    int old_scroll = verticalScrollBar()->value();

    // Pozice key (index) ve středu viewportu
    double center_key = (old_scroll + viewport_height / 2.0) / old_height;

    config.key_height = std::max(5, std::min(30, h));
    emit keyHeightChanged(config.key_height);

    // Aktualizuj obsah (pro novou výšku)
    recalculateContentSize();

    // Nastav scroll tak, ať střed zůstane na stejné klávese
    int new_scroll = int(center_key * config.key_height - viewport_height / 2.0);
    new_scroll =
        std::max(0, std::min(new_scroll, content_height - viewport_height));
    verticalScrollBar()->setValue(new_scroll);
    emit verticalScrollChanged(new_scroll);

    refreshAll();
}

/*******************************************************************************************************/
// Note Selection and Manipulation Methods
/*******************************************************************************************************/

std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> MidiEditorWidget::getSelectedNotes() const {
    std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> result;
    result.reserve(selectedNotes.size());
    for (const NoteGraphics *ng : selectedNotes) {
        if (ng && ng->track) {
            result.push_back({ng->track, ng->note});
        }
    }
    return result;
}

void MidiEditorWidget::selectNote(NoteGraphics *noteGraphics, bool clearPrevious) {
    if (!noteGraphics) return;
    
    if (clearPrevious) {
        // Clear without emitting - we'll emit once at the end
        for (NoteGraphics *ng : selectedNotes) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng->item);
            if (shapeItem) {
                bool is_selected = last_seq->getActiveTrack() &&
                                 last_seq->getActiveTrack()->getId() == ng->track->getId();
                NN_Color_t track_color = ng->track->getColor();
                float luminance = nn_yiq_luminance(track_color);
                QPen outline = is_selected
                    ? QPen(luminance < 128 ? Qt::white : Qt::black, 2)
                    : QPen((luminance < 128 ? track_color.lighter(150) : track_color.darker(150))
                               .toQColor());
                shapeItem->setPen(outline);
                shapeItem->setZValue(ng->track->getId() + 10);
            }
        }
        selectedNotes.clear();
    }
    
    // Přidej notu do výběru, pokud tam ještě není
    if (!selectedNotes.contains(noteGraphics)) {
        selectedNotes.append(noteGraphics);
        
        // Vizuálně označíme notu
        QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(noteGraphics->item);
        if (shapeItem) {
            // Změníme barvu podle toho, jestli je to buben nebo klasická nota
            shapeItem->setPen(QPen(selection_color, 2));
            shapeItem->setZValue(999); // Přesuneme vybranou notu nad ostatní
        }
        
        emit selectionChanged();
    }
}

void MidiEditorWidget::deselectNote(NoteGraphics *noteGraphics) {
    if (!noteGraphics) return;
    
    // Check if it's actually selected
    if (!selectedNotes.contains(noteGraphics)) return;
    
    // Odeber notu z výběru
    selectedNotes.removeOne(noteGraphics);
    
    // Vrátíme vizuální stav noty do původního stavu
    QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(noteGraphics->item);
    if (shapeItem) {
        // Resetujeme barvu a Z-index
        bool is_selected = last_seq->getActiveTrack() &&
                         last_seq->getActiveTrack()->getId() == noteGraphics->track->getId();
        NN_Color_t track_color = noteGraphics->track->getColor();
        float luminance = nn_yiq_luminance(track_color);
        QPen outline = is_selected
            ? QPen(luminance < 128 ? Qt::white : Qt::black, 2)
            : QPen((luminance < 128 ? track_color.lighter(150) : track_color.darker(150))
                       .toQColor());
                       
        shapeItem->setPen(outline);
        shapeItem->setZValue(noteGraphics->track->getId() + 10);
    }
    
    emit selectionChanged();
}

void MidiEditorWidget::clearSelection() {
    if (selectedNotes.isEmpty()) return;
    
    // Odznačíme všechny vybrané noty
    for (NoteGraphics *ng : selectedNotes) {
        QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng->item);
        if (shapeItem) {
            bool is_selected = last_seq->getActiveTrack() &&
                             last_seq->getActiveTrack()->getId() == ng->track->getId();
            NN_Color_t track_color = ng->track->getColor();
            float luminance = nn_yiq_luminance(track_color);
            QPen outline = is_selected
                ? QPen(luminance < 128 ? Qt::white : Qt::black, 2)
                : QPen((luminance < 128 ? track_color.lighter(150) : track_color.darker(150))
                           .toQColor());
                           
            shapeItem->setPen(outline);
            shapeItem->setZValue(ng->track->getId() + 10);
        }
    }
    selectedNotes.clear();
    
    emit selectionChanged();
}

void MidiEditorWidget::selectNotesInRect(const QRectF &rect) {
    if (!last_seq) return;
    
    int countBefore = selectedNotes.size();
    
    // Procházíme všechny viditelné noty a kontrolujeme, zda jsou v obdélníku
    for (auto &trackPair : note_items) {
        for (auto &ng : trackPair) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng.item);
            if (shapeItem) {
                QRectF noteRect = getRealNoteRect(&ng);
                if (rect.intersects(noteRect)) {
                    // Add without emitting - we'll emit once at the end
                    if (!selectedNotes.contains(&ng)) {
                        selectedNotes.append(&ng);
                        shapeItem->setPen(QPen(selection_color, 2));
                        shapeItem->setZValue(999);
                    }
                }
            }
        }
    }
    
    // Emit once if selection changed
    if (selectedNotes.size() != countBefore) {
        emit selectionChanged();
    }
}

MidiEditorWidget::NoteGraphics* MidiEditorWidget::findNoteUnderCursor(const QPointF &scenePos) {
    if (!last_seq) return nullptr;

    // Procházíme všechny viditelné noty a kontrolujeme, zda obsahují bod
    for (auto &trackPair : note_items) {
        for (auto &ng : trackPair) {
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng.item);
            if (shapeItem) {
                QRectF noteRect = getRealNoteRect(&ng);
                if (noteRect.contains(scenePos)) {
                    return &ng;
                }
            }
        }
    }
    return nullptr;
}

bool MidiEditorWidget::isNoteEdge(NoteGraphics *ng, const QPointF &scenePos) {
    if (!ng) return false;
    
    QRectF rect = getRealNoteRect(ng);
    
    // Kontrolujeme pravý okraj noty
    return (scenePos.x() >= rect.right() - resizeEdgeMargin && 
            scenePos.x() <= rect.right() + resizeEdgeMargin);
}

void MidiEditorWidget::addNewNote(const QPointF &scenePos) {
    if (!last_seq) return;
    
    NoteNagaTrack *activeTrack = last_seq->getActiveTrack();
    if (!activeTrack) return;
    
    int tick = sceneXToTick(scenePos.x());
    int noteValue = sceneYToNote(scenePos.y());
    noteValue = std::max(MIN_NOTE, std::min(MAX_NOTE, noteValue));
    
    NN_Note_t newNote;
    newNote.note = noteValue;
    newNote.start = this->snapTickToGrid(tick);
    newNote.velocity = 100; // Výchozí velocity pro nové noty
    newNote.parent = activeTrack; // Nastavíme parent track
    
    int ppq = last_seq->getPPQ();
    NoteDuration duration = static_cast<NoteDuration>(combo_note_duration->currentData().toInt());
    int noteLength = ppq; // Výchozí je 1/4

    switch (duration) {
        case NoteDuration::Whole:       noteLength = ppq * 4; break;
        case NoteDuration::Half:        noteLength = ppq * 2; break;
        case NoteDuration::Quarter:     noteLength = ppq; break;
        case NoteDuration::Eighth:      noteLength = ppq / 2; break;
        case NoteDuration::Sixteenth:   noteLength = ppq / 4; break;
        case NoteDuration::ThirtySecond:noteLength = ppq / 8; break;
    }
    newNote.length = std::max(1, noteLength); // Zajistíme minimální délku 1

    // Zkontroluj, zda na cílové pozici již neexistuje jiná nota
    for (const auto& existingNote : activeTrack->getNotes()) {
        // Kontrolujeme pouze noty se stejnou výškou tónu
        if (existingNote.note == newNote.note) {
            // Získání časových intervalů pro porovnání
            int newNoteStart = newNote.start.value_or(0);
            int newNoteEnd = newNoteStart + newNote.length.value_or(0);
            int existingNoteStart = existingNote.start.value_or(0);
            int existingNoteEnd = existingNoteStart + existingNote.length.value_or(0);

            // Podmínka pro detekci překrytí dvou intervalů
            if (newNoteStart < existingNoteEnd && existingNoteStart < newNoteEnd) {
                return; // Překryv nalezen, notu nepřidávej
            }
        }
    }

    // prida notu
    activeTrack->addNote(newNote);
    last_seq->computeMaxTick();
    
    emit notesModified();
    
    refreshTrack(activeTrack);
}

void MidiEditorWidget::moveSelectedNotes(const QPointF &delta) {
    if (selectedNotes.isEmpty() || (delta.x() == 0 && delta.y() == 0)) return;
    
    // Přepočteme delta na ticky a hodnoty not
    int deltaTicks = delta.x() / config.time_scale;
    int deltaNotes = -delta.y() / config.key_height; // záporné, protože y je invertované
    
    // Pokud není žádná změna, ukončíme
    //if (deltaTicks == 0 && deltaNotes == 0) return;
    
    // Upravíme pozice všech vybraných not
    for (NoteGraphics *ng : selectedNotes) {
        QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng->item);
        if (shapeItem) {
            // Fyzicky přesuneme grafický objekt noty
            shapeItem->moveBy(delta.x(), delta.y());
            
            // Přesuneme také textový label, pokud existuje
            if (ng->label) {
                ng->label->moveBy(delta.x(), delta.y());
            }
        }
    }
}

void MidiEditorWidget::resizeSelectedNotes(const QPointF &delta) {
    if (selectedNotes.isEmpty() || delta.x() == 0) return;
    
    // Přepočteme delta na ticky
    int deltaLength = delta.x() / config.time_scale;
    
    if (deltaLength == 0) return;
    
    // Změníme velikost všech vybraných not
    for (NoteGraphics *ng : selectedNotes) {
        QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(ng->item);
        if (shapeItem) {
            if (QGraphicsRectItem *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(ng->item)) {
                // Pro obdélníkové noty
                QRectF rect = rectItem->rect();
                qreal newWidth = std::max(1.0, rect.width() + delta.x());
                rectItem->setRect(rect.x(), rect.y(), newWidth, rect.height());
            }
            // Pro bubny (kruhové noty) není potřeba měnit velikost
        }
    }
}

void MidiEditorWidget::applyNoteChanges() {
    // 1. Ujistíme se, že máme co měnit a že máme uložený původní stav
    if (selectedNotes.isEmpty() || dragStartNoteStates.isEmpty()) {
        return;
    }

    QPointF totalDelta = lastDragPos - dragStartPos;
    QSet<NoteNagaTrack*> affectedTracks;
    QList<QPair<NoteGraphics*, NN_Note_t>> changesToApply;

    // --- Fáze 1: Výpočet změn ---
    // V této fázi se nic nemění, zůstává stejná
    for (NoteGraphics *ng : selectedNotes) {
        if (!dragStartNoteStates.contains(ng)) continue;

        NN_Note_t originalNote = dragStartNoteStates.value(ng);
        NN_Note_t newNote = originalNote;

        if (dragMode == DragMode::Move) {
            int deltaTicks = totalDelta.x() / config.time_scale;
            int deltaNotes = -round(totalDelta.y() / config.key_height);
            if (newNote.start.has_value()) {
                newNote.start = snapTickToGrid(originalNote.start.value() + deltaTicks);
            }
            newNote.note = originalNote.note + deltaNotes;
            newNote.note = std::max(MIN_NOTE, std::min(MAX_NOTE, newNote.note));
        } else if (dragMode == DragMode::Resize) {
            int deltaLength = totalDelta.x() / config.time_scale;
            if (newNote.length.has_value() && newNote.start.has_value()) {
                int originalEndTick = originalNote.start.value() + originalNote.length.value();
                int snappedEndTick = snapTickToGridNearest(originalEndTick + deltaLength);
                newNote.length = std::max(1, snappedEndTick - originalNote.start.value());
            }
        }
        changesToApply.append({ng, newNote});
    }

    // --- Fáze 2: Aplikace změn (s blokováním signálů) ---
    
    // Zablokujeme signály, aby se uprostřed cyklu nespustilo překreslení
    auto project = engine->getProject();
    bool signalsWereBlocked = project->signalsBlocked();
    project->blockSignals(true);

    for (const auto& change : changesToApply) {
        NoteGraphics* ng = change.first;
        NN_Note_t newNote = change.second;
        NN_Note_t originalNote = dragStartNoteStates.value(ng);
        
        ng->track->removeNote(originalNote);
        ng->track->addNote(newNote);
        
        affectedTracks.insert(ng->track);
    }
    
    // Aktualizujeme interní data grafických objektů (teď už je to bezpečné)
    for (const auto& change : changesToApply) {
        change.first->note = change.second;
    }

    // Odblokujeme signály
    project->blockSignals(signalsWereBlocked);
    
    dragStartNoteStates.clear();
    last_seq->computeMaxTick();

    // Signál o změně not emitujeme až teď, když je vše hotovo
    emit notesModified();

    // --- Fáze 3: Překreslení a vyčištění ---
    for (NoteNagaTrack* track : affectedTracks) {
        refreshTrack(track);
    }

    // Vyčistíme výběr, protože ukazatele v něm jsou po překreslení neplatné
    clearSelection();
}

QRectF MidiEditorWidget::getRealNoteRect(const NoteGraphics *ng) const {
    QRectF rect;
    
    // Pro obdélníkové noty
    if (QGraphicsRectItem *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(ng->item)) {
        rect = rectItem->sceneBoundingRect();
    } 
    // Pro kruhové noty (bubny)
    else if (QGraphicsEllipseItem *ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(ng->item)) {
        rect = ellipseItem->sceneBoundingRect();
    }
    
    return rect;
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

/*******************************************************************************************************/
// Qt GUI Event Handlers
/*******************************************************************************************************/

void MidiEditorWidget::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    refreshAll();
}

void MidiEditorWidget::mousePressEvent(QMouseEvent *event) {
    if (!last_seq) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    
    setFocus(); // Zaměříme widget, aby reagoval na klávesové zkratky
    QPointF scenePos = mapToScene(event->pos());
    
    // Levé tlačítko myši - výběr, přesun, změna pozice kurzoru
    if (event->button() == Qt::LeftButton) {
        NoteGraphics *noteUnderCursor = findNoteUnderCursor(scenePos);
        
        // Pokud je pod kurzorem nota
        if (noteUnderCursor) {
            bool isSelected = selectedNotes.contains(noteUnderCursor);
            
            if (!isSelected) {
                selectNote(noteUnderCursor, !(event->modifiers() & Qt::ControlModifier));
                dragMode = DragMode::None;
            } else {
                // FIX: Uložíme si původní stav VŠECH vybraných not
                dragStartNoteStates.clear();
                for (NoteGraphics *note : selectedNotes) {
                    dragStartNoteStates[note] = note->note;
                }

                if (isNoteEdge(noteUnderCursor, scenePos)) {
                    dragMode = DragMode::Resize;
                    QApplication::setOverrideCursor(Qt::SizeHorCursor);
                } else {
                    dragMode = DragMode::Move;
                    QApplication::setOverrideCursor(Qt::SizeAllCursor);
                }
                
                dragStartPos = scenePos;
                lastDragPos = scenePos;
            }
        }
        // Jinak zahájíme výběr obdélníkem nebo přesuneme kurzor
        else { 
            // Zrušíme výběr, pokud uživatel nezačíná výběr s držením Ctrl
            if (!(event->modifiers() & Qt::ControlModifier)) {
                clearSelection();
            }

            // Nastavíme novou pozici kurzoru
            int tick = sceneXToTick(scenePos.x());
            engine->getProject()->setCurrentTick(tick);
            emit positionSelected(tick);
            refreshMarker();
            
            // Zahájíme výběr obdélníkem
            dragMode = DragMode::Select;
            rubberBandOrigin = event->pos();
            rubberBand->setGeometry(QRect(rubberBandOrigin, QSize()));
            rubberBand->show();
        }
    }
    // Pravé tlačítko - přidání nové noty
    else if (event->button() == Qt::RightButton) {
        addNewNote(scenePos);
    }
    
    QGraphicsView::mousePressEvent(event);
}

void MidiEditorWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!last_seq) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    
    QPointF scenePos = mapToScene(event->pos());
    
    // Přetahování obdélníku pro výběr
    if (dragMode == DragMode::Select && rubberBand->isVisible()) {
        rubberBand->setGeometry(QRect(rubberBandOrigin, event->pos()).normalized());
    }
    // Přesun vybraných not
    else if (dragMode == DragMode::Move && !selectedNotes.isEmpty()) {
        QPointF delta = scenePos - lastDragPos;
        moveSelectedNotes(delta);
        lastDragPos = scenePos;
    }
    // Změna délky vybraných not
    else if (dragMode == DragMode::Resize && !selectedNotes.isEmpty()) {
        QPointF delta = scenePos - lastDragPos;
        resizeSelectedNotes(delta);
        lastDragPos = scenePos;
    }
    // Detekce okraje noty pro změnu kurzoru
    else if (dragMode == DragMode::None) {
        NoteGraphics *noteUnderCursor = findNoteUnderCursor(scenePos);
        if (noteUnderCursor && isNoteEdge(noteUnderCursor, scenePos)) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
    
    QGraphicsView::mouseMoveEvent(event);
}

void MidiEditorWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Dokončení výběru pomocí rubber band
        if (dragMode == DragMode::Select && rubberBand->isVisible()) {
            QRect viewRect = rubberBand->geometry();
            QRectF sceneRect = mapToScene(viewRect).boundingRect();
            selectNotesInRect(sceneRect);
            rubberBand->hide();
        }
        
        // Aplikujeme změny not, pokud jsme je přesouvali nebo měnili velikost
        if (dragMode == DragMode::Move || dragMode == DragMode::Resize) {
            applyNoteChanges();
            QApplication::restoreOverrideCursor();
        }
        
        // Reset drag mode
        dragMode = DragMode::None;
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
        // Zoom (Ctrl nebo Command + kolečko)
        double zoom_factor = (event->angleDelta().y() > 0) ? 1.2 : 0.8;
        setTimeScale(config.time_scale * zoom_factor);
    } else if (std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y())) {
        // Skutečný horizontální pohyb kolečkem (nebo Shift + Wheel na Macu)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->angleDelta().x() / 8);
    } else {
        // Vertikální posun (default)
        verticalScrollBar()->setValue(verticalScrollBar()->value() - event->angleDelta().y() / 8);
    }

    event->accept();
}

void MidiEditorWidget::keyPressEvent(QKeyEvent *event) {
    if (!last_seq) {
        QGraphicsView::keyPressEvent(event);
        return;
    }
    
    // Mazání vybraných not (Delete nebo Backspace)
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && !selectedNotes.isEmpty()) {
        
        QSet<NoteNagaTrack*> affectedTracks;
        
        // Zablokujeme signály, aby se předešlo pádům při hromadných změnách
        auto project = engine->getProject();
        bool signalsWereBlocked = project->signalsBlocked();
        project->blockSignals(true);
        
        for (NoteGraphics *ng : selectedNotes) {
            ng->track->removeNote(ng->note);
            affectedTracks.insert(ng->track);
        }
        
        // Odblokujeme signály
        project->blockSignals(signalsWereBlocked);
        
        // Uložíme si, které stopy překreslit, a vyčistíme výběr
        // Důležité: clearSelection() musí být před refreshTrack()
        clearSelection();
        
        last_seq->computeMaxTick();
        emit notesModified();
        
        // Překreslíme jen ovlivněné stopy
        for (NoteNagaTrack *track : affectedTracks) {
            refreshTrack(track);
        }
    }
    // Zrušení výběru klávesou Escape
    else if (event->key() == Qt::Key_Escape) {
        clearSelection();
    } else {
        QGraphicsView::keyPressEvent(event);
    }
}

/*******************************************************************************************************/
// UI Update Methods
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
    scene->setBackgroundBrush(bg_color);

    if (!last_seq) {
        auto *txt = scene->addSimpleText("Open file");
        txt->setBrush(fg_color);
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

    for (int idx = 0, note_val = MIN_NOTE; note_val <= MAX_NOTE; ++idx, ++note_val) {
        int y = content_height - (idx + 1) * config.key_height;
        if (y + config.key_height < visible_y0 || y > visible_y1) continue;
        QColor row_bg = (note_val % 2 == 0) ? grid_row_color1 : grid_row_color2;
        auto row_bg_rect = scene->addRect(0, y, content_width, config.key_height,
                                          QPen(Qt::NoPen), QBrush(row_bg));
        row_bg_rect->setZValue(-100);

        auto l = scene->addLine(0, y, content_width, y, QPen(line_color, 1));
        grid_lines.push_back(l);
    }
}

void MidiEditorWidget::updateBarGrid() {
    if (!last_seq)
        return;

    int ppq = last_seq->getPPQ();
    int bar_length = ppq * 4; // Délka jednoho taktu v ticích
    int first_bar = 0;
    int last_bar = (last_seq->getMaxTick() / bar_length) + 2;

    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();

    double px_per_bar = config.time_scale * bar_length;

    // --- Vykreslení hlavních taktových čar (modré) ---
    int bar_skip = 1;
    double min_bar_dist_px = 58; // Minimální mezera mezi popisky taktů
    while (px_per_bar * bar_skip < min_bar_dist_px) {
        bar_skip *= 2;
    }

    QFont label_font("Arial", 11, QFont::Bold);
    for (int bar = first_bar; bar < last_bar; bar += bar_skip) {
        int x = tickToSceneX(bar * bar_length);
        if (x < visible_x0 - 200 || x > visible_x1 + 200) continue;

        auto l = scene->addLine(x, 0, x, content_height, QPen(grid_bar_color, 1.5));
        l->setZValue(2);
        bar_grid_lines.push_back(l);

        if (px_per_bar > 30) {
            auto label = scene->addSimpleText(QString::number(bar + 1));
            label->setFont(label_font);
            label->setBrush(grid_bar_label_color);
            label->setPos(x + 4, visible_y0 + 4);
            label->setZValue(9999);
            bar_grid_labels.push_back(label);
        }
    }

    // --- Vykreslení podružných čar (šedé) podle gridu a zoomu ---
    GridResolution resolution = static_cast<GridResolution>(combo_grid_resolution->currentData().toInt());
    if (resolution == GridResolution::Off) return; // Pokud je mřížka vypnutá, nic nekreslíme

    int grid_step_ticks = ppq; // Výchozí hodnota (1/4)
    switch (resolution) {
        case GridResolution::Whole:       grid_step_ticks = ppq * 4; break;
        case GridResolution::Half:        grid_step_ticks = ppq * 2; break;
        case GridResolution::Quarter:     grid_step_ticks = ppq; break;
        case GridResolution::Eighth:      grid_step_ticks = ppq / 2; break;
        case GridResolution::Sixteenth:   grid_step_ticks = ppq / 4; break;
        case GridResolution::ThirtySecond:grid_step_ticks = ppq / 8; break;
        default: break;
    }

    if (grid_step_ticks == 0) return;

    // Optimalizace, aby se čáry nevykreslovaly příliš hustě
    double px_per_grid_step = config.time_scale * grid_step_ticks;
    int grid_skip = 1;
    while (px_per_grid_step * grid_skip < 8.0) { // Minimální mezera mezi čarami 8 pixelů
        grid_skip *= 2;
    }

    int total_ticks = last_bar * bar_length;
    for (int tick = 0; tick < total_ticks; tick += grid_step_ticks * grid_skip) {
        // Přeskočíme hlavní taktové čáry, aby se nepřekrývaly
        if (tick % bar_length == 0) continue;

        int x = tickToSceneX(tick);
        if (x < visible_x0 - 200 || x > visible_x1 + 200) continue;
        
        auto lsub = scene->addLine(x, 0, x, content_height, QPen(grid_subdiv_color, 1));
        lsub->setZValue(1);
    }
}

void MidiEditorWidget::drawNote(const NN_Note_t &note, const NoteNagaTrack *track,
                                bool is_selected, bool is_drum, int x, int y, int w,
                                int h) {
    QGraphicsItem *shape = nullptr;
    NN_Color_t t_color =
        (is_selected ? track->getColor()
                     : nn_color_blend(track->getColor(),
                                      NN_Color_t::fromQColor(bg_color), 0.3));
    float luminance = nn_yiq_luminance(t_color);
    QPen outline =
        is_selected
            ? QPen(luminance < 128 ? Qt::white : Qt::black, 2)
            : QPen((luminance < 128 ? t_color.lighter(150) : t_color.darker(150))
                       .toQColor());

    if (is_drum) {
        int sz = h * 0.6;
        int cx = x + w / 2;
        int cy = y + h / 2;
        int left = cx - sz / 2;
        int top = cy - sz / 2;
        shape = scene->addEllipse(left, top, sz, sz, outline,
                                QBrush(t_color.toQColor()));
    } else {
        shape = scene->addRect(x, y, w, h, outline, QBrush(t_color.toQColor()));
    }
    shape->setZValue(is_selected ? 999 : track->getId() + 10);

    QGraphicsSimpleTextItem *txt = nullptr;
    if (!is_drum && w > 20 && h > 9 && config.time_scale > 0.04) {
        QString note_str = QString::fromStdString(nn_note_name(note.note));
        txt = scene->addSimpleText(note_str);
        txt->setBrush(QBrush(luminance < 128 ? Qt::white : Qt::black));
        QFont f("Arial", std::max(6, h - 6));
        txt->setFont(f);
        txt->setPos(x + 2, y + 2);
        txt->setZValue(shape->zValue() + 1);
    }
    
    // Uložíme si informace o notě pro pozdější manipulaci
    NoteGraphics ng = {shape, txt, note, const_cast<NoteNagaTrack*>(track)};
    note_items[track->getId()].push_back(ng);
    
    // Pokud byla nota předtím označena, musíme ji označit
    bool wasSelected = false;
    for (int i = 0; i < selectedNotes.size(); i++) {
        if (selectedNotes[i]->track->getId() == track->getId() && 
            selectedNotes[i]->note.note == note.note && 
            selectedNotes[i]->note.start.value_or(-1) == note.start.value_or(-1) && 
            selectedNotes[i]->note.length.value_or(-1) == note.length.value_or(-1)) {
            // Nahradíme starou referenci novou a označíme notu
            selectedNotes[i] = &note_items[track->getId()].back();
            wasSelected = true;
            
            // Vizuálně označíme notu
            QAbstractGraphicsShapeItem *shapeItem = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(shape);
            if (shapeItem) {
                shapeItem->setPen(QPen(selection_color, 2));
                shapeItem->setZValue(999);
            }
            break;
        }
    }
}

void MidiEditorWidget::updateAllNotes() {
    clearNotes();
    if (!last_seq)
        return;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    for (const auto &track : last_seq->getTracks()) {
        if (!track || !track->isVisible())
            continue;
        bool is_drum = engine->getMixer()->isPercussion(track);
        bool is_selected = last_seq->getActiveTrack() &&
                         last_seq->getActiveTrack()->getId() == track->getId();

        for (const auto &note : track->getNotes()) {
            if (!note.start.has_value() || !note.length.has_value())
                continue;
            int y = content_height - (note.note - MIN_NOTE + 1) * config.key_height;
            int x = note.start.value() * config.time_scale;
            int w = std::max(1, int(note.length.value() * config.time_scale));
            int h = config.key_height;
            if (!((x + w > visible_x0 && x < visible_x1) &&
                (y + h > visible_y0 && y < visible_y1)))
                continue;
            drawNote(note, track, is_selected, is_drum, x, y, w, h);
        }
    }
}

void MidiEditorWidget::updateTrackNotes(NoteNagaTrack *track) {
    if (!last_seq || !track)
        return;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    bool is_drum = engine->getMixer()->isPercussion(track);
    bool is_selected = last_seq->getActiveTrack() &&
                     last_seq->getActiveTrack()->getId() == track->getId();

    for (const auto &note : track->getNotes()) {
        if (!note.start.has_value() || !note.length.has_value())
            continue;
        int y = content_height - (note.note - MIN_NOTE + 1) * config.key_height;
        int x = note.start.value() * config.time_scale;
        int w = std::max(1, int(note.length.value() * config.time_scale));
        int h = config.key_height;
        if (!((x + w > visible_x0 && x < visible_x1) &&
              (y + h > visible_y0 && y < visible_y1)))
            continue;
        drawNote(note, track, is_selected, is_drum, x, y, w, h);
    }
}

/*******************************************************************************************************/
// Scene Management Methods
/*******************************************************************************************************/

void MidiEditorWidget::clearScene() {
    scene->clear();
    note_items.clear();
    grid_lines.clear();
    bar_grid_lines.clear();
    bar_grid_labels.clear();
    marker_line = nullptr;
    selectedNotes.clear();  // Musíme vyčistit také výběr, protože by obsahoval neplatné pointery
}

void MidiEditorWidget::clearNotes() {
    // Zapamatujeme si, které noty byly vybrané (podle dat, ne podle pointerů)
    QList<QPair<int, NN_Note_t>> selectedNoteData;
    for (NoteGraphics *ng : selectedNotes) {
        selectedNoteData.append(qMakePair(ng->track->getId(), ng->note));
    }
    
    // Odstraníme všechny grafické objekty not
    for (auto &arr : note_items) {
        for (auto &ng : arr) {
            if (ng.item)
                scene->removeItem(ng.item);
            if (ng.label)
                scene->removeItem(ng.label);
        }
    }
    
    note_items.clear();
    selectedNotes.clear(); // Vyčistíme výběr, protože pointery budou neplatné
}

void MidiEditorWidget::clearTrackNotes(int track_id) {
    // Před odstraněním not odebereme z výběru noty této stopy
    for (int i = 0; i < selectedNotes.size(); /* no increment */) {
        if (selectedNotes[i]->track->getId() == track_id) {
            selectedNotes.removeAt(i);
        } else {
            ++i;
        }
    }
    
    // Odstraníme grafické objekty not této stopy
    auto it = note_items.find(track_id);
    if (it != note_items.end()) {
        for (auto &ng : it.value()) {
            if (ng.item)
                scene->removeItem(ng.item);
            if (ng.label)
                scene->removeItem(ng.label);
        }
        note_items.erase(it);
    }
}

int MidiEditorWidget::snapTickToGrid(int tick) const {
    GridResolution resolution = static_cast<GridResolution>(combo_grid_resolution->currentData().toInt());
    if (resolution == GridResolution::Off || !last_seq) {
        return tick;
    }

    int ppq = last_seq->getPPQ();
    int grid_step = ppq; // Default Quarter

    switch (resolution) {
        case GridResolution::Whole:       grid_step = ppq * 4; break;
        case GridResolution::Half:        grid_step = ppq * 2; break;
        case GridResolution::Quarter:     grid_step = ppq; break;
        case GridResolution::Eighth:      grid_step = ppq / 2; break;
        case GridResolution::Sixteenth:   grid_step = ppq / 4; break;
        case GridResolution::ThirtySecond:grid_step = ppq / 8; break;
        default: break;
    }

    if (grid_step == 0) return tick; // Ochrana před dělením nulou

    return (tick / grid_step) * grid_step;
}

int MidiEditorWidget::snapTickToGridNearest(int tick) const {
    GridResolution resolution = static_cast<GridResolution>(combo_grid_resolution->currentData().toInt());
    if (resolution == GridResolution::Off || !last_seq) {
        return tick;
    }

    int ppq = last_seq->getPPQ();
    int grid_step = ppq; // Default Quarter

    switch (resolution) {
        case GridResolution::Whole:       grid_step = ppq * 4; break;
        case GridResolution::Half:        grid_step = ppq * 2; break;
        case GridResolution::Quarter:     grid_step = ppq; break;
        case GridResolution::Eighth:      grid_step = ppq / 2; break;
        case GridResolution::Sixteenth:   grid_step = ppq / 4; break;
        case GridResolution::ThirtySecond:grid_step = ppq / 8; break;
        default: break;
    }

    if (grid_step == 0) return tick; // Ochrana před dělením nulou

    // Použijeme zaokrouhlení, aby se konec noty přichytil k nejbližší čáře gridu
    return static_cast<int>(round(static_cast<double>(tick) / grid_step)) * grid_step;
}