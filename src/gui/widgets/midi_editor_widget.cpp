#include "midi_editor_widget.h"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QScrollBar>
#include <algorithm>
#include <cmath>

#include "../nn_gui_utils.h"

#define MIN_NOTE 0
#define MAX_NOTE 127

MidiEditorWidget::MidiEditorWidget(NoteNagaEngine *engine, QWidget *parent)
    : QGraphicsView(parent), engine(engine), bg_color("#32353c"), fg_color("#e0e6ef"),
      line_color("#232731"), subline_color("#464a56"), grid_bar_color("#6177d1"),
      grid_row_color1("#35363b"), grid_row_color2("#292a2e"),
      grid_bar_label_color("#6fa5ff"), grid_subdiv_color("#44464b") {
    setObjectName("MidiViewerWidget");
    setFrameStyle(QFrame::NoFrame);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    this->config.time_scale = 0.2; 
    this->config.key_height = 16;  
    this->config.tact_subdiv = 4;   
    
    this->content_width = 640;
    this->content_height = (127 - 0 + 1) * 16;

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
            [this](NoteNagaTrack *track, const std::string &) { refreshTrack(track); });

    // playback position changed
    connect(project, &NoteNagaProject::currentTickChanged, this, 
            &MidiEditorWidget::currentTickChanged);

    // scrollbars value changed'
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::verticalScrollChanged);
}

void MidiEditorWidget::initTitleUI() {
    if (this->title_widget) return;
    this->title_widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(title_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // follow mode buttons
    this->btn_follow_center = create_small_button(
        ":/icons/follow-from-center.svg", "Follow from Center", "FollowCenter");
    this->btn_follow_center->setCheckable(true);
    connect(btn_follow_center, &QPushButton::clicked, this, [this]() {
        selectFollowMode(MidiEditorFollowMode::CenterIsCurrent);
    });
    this->btn_follow_left = create_small_button(
        ":/icons/follow-from-left.svg", "Follow from Left", "FollowLeft");
    this->btn_follow_left->setCheckable(true);
    connect(btn_follow_left, &QPushButton::clicked, this, [this]() {
        selectFollowMode(MidiEditorFollowMode::LeftSideIsCurrent);
    });
    this->btn_follow_step = create_small_button(
        ":/icons/follow-step-by-step.svg", "Follow Step by Step", "FollowStep");
    this->btn_follow_step->setCheckable(true);
    connect(btn_follow_step, &QPushButton::clicked, this, [this]() {
        selectFollowMode(MidiEditorFollowMode::StepByStep);
    });
    this->btn_follow_none = create_small_button(
        ":/icons/follow-none.svg", "Don't Follow", "FollowNone");
    this->btn_follow_none->setCheckable(true);
    connect(btn_follow_none, &QPushButton::clicked, this, [this]() {
        selectFollowMode(MidiEditorFollowMode::None);
    });
    selectFollowMode(MidiEditorFollowMode::CenterIsCurrent);

    // zoom in/out for horizontal and vertical
    QPushButton *btn_h_zoom_in = create_small_button(
        ":/icons/zoom-in-horizontal.svg", "Horizontal Zoom In", "HZoomIn");
    connect(btn_h_zoom_in, &QPushButton::clicked, this, [this]() {
        setTimeScale(config.time_scale * 1.2);
    });
    QPushButton *btn_h_zoom_out = create_small_button(
        ":/icons/zoom-out-horizontal.svg", "Horizontal Zoom Out", "HZoomOut");
    connect(btn_h_zoom_out, &QPushButton::clicked, this, [this]() {
        setTimeScale(config.time_scale / 1.2);
    });
    QPushButton *btn_v_zoom_in = create_small_button(
        ":/icons/zoom-in-vertical.svg", "Vertical Zoom In", "VZoomIn");
    connect(btn_v_zoom_in, &QPushButton::clicked, this, [this]() {
        setKeyHeight(config.key_height * 1.2);
    });
    QPushButton *btn_v_zoom_out = create_small_button(
        ":/icons/zoom-out-vertical.svg", "Vertical Zoom Out", "VZoomOut");
    connect(btn_v_zoom_out, &QPushButton::clicked, this, [this]() {
        setKeyHeight(config.key_height / 1.2);
    });

    // looping and step buttons
    QPushButton *btn_looping = create_small_button(
        ":/icons/loop.svg", "Toggle Looping", "Looping");
    QPushButton *btn_step = create_small_button(
        ":/icons/step-forward.svg", "Step Forward", "StepForward");

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
    if (!last_seq || !track) return;
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

        switch(config.follow_mode) {
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
    if (config.follow_mode == mode) return; // No change

    config.follow_mode = mode;
    btn_follow_none->setChecked(false);
    btn_follow_center->setChecked(false);
    btn_follow_left->setChecked(false);
    btn_follow_step->setChecked(false);

    switch(config.follow_mode) {
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
    new_scroll = std::max(0, std::min(new_scroll, content_width - viewport_width));
    horizontalScrollBar()->setValue(new_scroll);
    emit horizontalScrollChanged(new_scroll);

    refreshAll();
}

void MidiEditorWidget::setKeyHeight(int h) {
    config.key_height = std::max(4, std::min(48, h));
    emit keyHeightChanged(config.key_height);
    refreshAll();
}

/*******************************************************************************************************/
// Qt GUI Event Handlers
/*******************************************************************************************************/

void MidiEditorWidget::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    refreshAll();
}

void MidiEditorWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && last_seq) {
        bool is_playing = engine->isPlaying();
        if (is_playing) {
            engine->stopPlayback();
        }

        int x = int(mapToScene(event->pos()).x());
        int tick = int(x / config.time_scale);
        engine->getProject()->setCurrentTick(tick);
        emit positionSelected(tick);
        refreshMarker();

        if (is_playing) {
            engine->startPlayback();
        }
    }
    QGraphicsView::mousePressEvent(event);
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
    while (px_per_bar * bar_skip < min_bar_dist_px)
        bar_skip *= 2;

    QFont label_font("Arial", 11, QFont::Bold);

    for (int bar = first_bar; bar < last_bar; bar += bar_skip) {
        int tick = bar * bar_length;
        int x = tick * config.time_scale;
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

        int subdiv_skip = 1;
        double px_per_div = px_per_bar / config.tact_subdiv;
        while (px_per_div * subdiv_skip < 18.0 && subdiv_skip < config.tact_subdiv)
            subdiv_skip *= 2;
        for (int sub = 1; sub < config.tact_subdiv; ++sub) {
            if (sub % subdiv_skip != 0) continue;
            int sub_tick = tick + (bar_length * sub) / config.tact_subdiv;
            int sub_x = sub_tick * config.time_scale;
            if (sub_x < visible_x0 - 200 || sub_x > visible_x1 + 200) continue;
            auto lsub = scene->addLine(sub_x, 0, sub_x, content_height,
                                       QPen(grid_subdiv_color, 1));
            lsub->setZValue(1);
        }
    }
}

void MidiEditorWidget::drawNote(const NN_Note_t &note, const NoteNagaTrack *track,
                                bool is_selected, bool is_drum, int x, int y, int w,
                                int h) {
    QGraphicsItem *shape = nullptr;
    NN_Color_t t_color =
        (is_selected
             ? track->getColor()
             : nn_color_blend(track->getColor(), NN_Color_t::fromQColor(bg_color), 0.3));
    float luminance = nn_yiq_luminance(t_color);
    QPen outline =
        is_selected ? QPen(luminance < 128 ? Qt::white : Qt::black, 2)
                    : QPen((luminance < 128 ? t_color.lighter(150) : t_color.darker(150))
                               .toQColor());

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
        QString note_str = QString::fromStdString(nn_note_name(note.note));
        txt = scene->addSimpleText(note_str);
        txt->setBrush(QBrush(luminance < 128 ? Qt::white : Qt::black));
        QFont f("Arial", std::max(6, h - 6));
        txt->setFont(f);
        txt->setPos(x + 2, y + 2);
        txt->setZValue(shape->zValue() + 1);
    }
    note_items[track->getId()].push_back({shape, txt});
}

void MidiEditorWidget::updateAllNotes() {
    clearNotes();
    if (!last_seq) return;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    for (const auto &track : last_seq->getTracks()) {
        if (!track || !track->isVisible()) continue;
        bool is_drum =
            track->getChannel().has_value() && track->getChannel().value() == 9;
        bool is_selected = last_seq->getActiveTrack() &&
                           last_seq->getActiveTrack()->getId() == track->getId();

        for (const auto &note : track->getNotes()) {
            if (!note.start.has_value() || !note.length.has_value()) continue;
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
    if (!last_seq || !track) return;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    bool is_drum = track->getChannel().has_value() && track->getChannel().value() == 9;
    bool is_selected = last_seq->getActiveTrack() &&
                       last_seq->getActiveTrack()->getId() == track->getId();

    for (const auto &note : track->getNotes()) {
        if (!note.start.has_value() || !note.length.has_value()) continue;
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
}
void MidiEditorWidget::clearNotes() {
    for (auto &arr : note_items) {
        for (auto &ng : arr) {
            if (ng.item) scene->removeItem(ng.item);
            if (ng.label) scene->removeItem(ng.label);
        }
    }
    note_items.clear();
}
void MidiEditorWidget::clearTrackNotes(int track_id) {
    auto it = note_items.find(track_id);
    if (it != note_items.end()) {
        for (auto &ng : it.value()) {
            if (ng.item) scene->removeItem(ng.item);
            if (ng.label) scene->removeItem(ng.label);
        }
        note_items.erase(it);
    }
}