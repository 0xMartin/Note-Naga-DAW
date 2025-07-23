#include "midi_editor_widget.h"
#include <QResizeEvent>
#include <QScrollBar>
#include <algorithm>
#include <cmath>

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
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    this->time_scale = 0.2;
    this->key_height = 16;
    this->content_width = 640;
    this->content_height = (127 - 0 + 1) * 16;
    this->tact_subdiv = 4;

    scene = new QGraphicsScene(this);
    setScene(scene);

    setBackgroundBrush(bg_color);
    setMouseTracking(true);

    setupConnections();

    // Init last_seq_ to current active sequence if available
    setSequence(engine->getProject()->getActiveSequence());
    refreshAll();
}

void MidiEditorWidget::setupConnections() {
    auto project = engine->getProject();

    // Projekt byl načten, nastavíme aktivní sekvenci a refresh
    connect(project, &NoteNagaProject::projectFileLoaded, this, [this]() {
        setSequence(engine->getProject()->getActiveSequence());
        refreshAll();
    });

    // Aktivní sekvence se změnila (přepnutí, otevření jiného souboru apod.)
    connect(project, &NoteNagaProject::activeSequenceChanged, this,
            [this](NoteNagaMidiSeq *seq) {
                setSequence(seq);
                refreshAll();
            });

    // Změna metadat sekvence (refresh této sekvence)
    connect(project, &NoteNagaProject::sequenceMetadataChanged, this,
            [this](NoteNagaMidiSeq *seq, const std::string &) {
                setSequence(seq);
                refreshAll();
            });

    // Změna metadat tracku (refresh pouze daného tracku)
    connect(project, &NoteNagaProject::trackMetaChanged, this,
            [this](NoteNagaTrack *track, const std::string &) { refreshTrack(track); });

    // Posunutí přehrávací pozice
    connect(project, &NoteNagaProject::currentTickChanged, this,
            [this](int) { refreshMarker(); });

    // Scrollování v editoru
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MidiEditorWidget::refreshAll);
}

void MidiEditorWidget::setSequence(NoteNagaMidiSeq *seq) { last_seq = seq; }

QSize MidiEditorWidget::sizeHint() const { return QSize(content_width, content_height); }
QSize MidiEditorWidget::minimumSizeHint() const { return QSize(320, 100); }

// --- Public slots ---

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

    int marker_x = engine->getProject()->getCurrentTick() * time_scale;
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
    setSequence(seq);
    refreshAll();
}

void MidiEditorWidget::setTimeScale(double scale) {
    time_scale = std::max(0.02, scale);
    refreshAll();
}

void MidiEditorWidget::setKeyHeight(int h) {
    key_height = std::max(4, std::min(48, h));
    refreshAll();
}

// --- UI update ---

void MidiEditorWidget::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    refreshAll();
}

void MidiEditorWidget::mousePressEvent(QMouseEvent *event) {
    if (engine->isPlaying()) return;
    if (event->button() == Qt::LeftButton && last_seq) {
        int x = int(mapToScene(event->pos()).x());
        int tick = int(x / time_scale);
        engine->getProject()->setCurrentTick(tick);
        emit positionSelected(tick);
        refreshMarker();
    }
    QGraphicsView::mousePressEvent(event);
}

void MidiEditorWidget::recalculateContentSize() {
    if (last_seq) {
        content_width = int((last_seq->getMaxTick() + 1) * time_scale) + 16;
        content_height = (MAX_NOTE - MIN_NOTE + 1) * key_height;
    } else {
        content_width = 640;
        content_height = (MAX_NOTE - MIN_NOTE + 1) * key_height;
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
        int y = content_height - (idx + 1) * key_height;
        if (y + key_height < visible_y0 || y > visible_y1) continue;
        QColor row_bg = (note_val % 2 == 0) ? grid_row_color1 : grid_row_color2;
        auto row_bg_rect = scene->addRect(0, y, content_width, key_height,
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

    double px_per_bar = time_scale * bar_length;
    int bar_skip = 1;
    double min_bar_dist_px = 58;
    while (px_per_bar * bar_skip < min_bar_dist_px)
        bar_skip *= 2;

    QFont label_font("Arial", 11, QFont::Bold);

    for (int bar = first_bar; bar < last_bar; bar += bar_skip) {
        int tick = bar * bar_length;
        int x = tick * time_scale;
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
        double px_per_div = px_per_bar / tact_subdiv;
        while (px_per_div * subdiv_skip < 18.0 && subdiv_skip < tact_subdiv)
            subdiv_skip *= 2;
        for (int sub = 1; sub < tact_subdiv; ++sub) {
            if (sub % subdiv_skip != 0) continue;
            int sub_tick = tick + (bar_length * sub) / tact_subdiv;
            int sub_x = sub_tick * time_scale;
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
                    : QPen((luminance < 128 ? t_color.lighter(150) : t_color.darker(150)).toQColor());

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
    if (!is_drum && w > 20 && h > 9 && time_scale > 0.04) {
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
            int y = content_height - (note.note - MIN_NOTE + 1) * key_height;
            int x = note.start.value() * time_scale;
            int w = std::max(1, int(note.length.value() * time_scale));
            int h = key_height;
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
        int y = content_height - (note.note - MIN_NOTE + 1) * key_height;
        int x = note.start.value() * time_scale;
        int w = std::max(1, int(note.length.value() * time_scale));
        int h = key_height;
        if (!((x + w > visible_x0 && x < visible_x1) &&
              (y + h > visible_y0 && y < visible_y1)))
            continue;
        drawNote(note, track, is_selected, is_drum, x, y, w, h);
    }
}

// --- Clear helpers ---

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