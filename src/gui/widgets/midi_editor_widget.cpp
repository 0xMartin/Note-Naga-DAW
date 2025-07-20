#include "midi_editor_widget.h"
#include <QResizeEvent>
#include <QScrollBar>
#include <algorithm>
#include <cmath>

#define MIN_NOTE 0
#define MAX_NOTE 127

MidiEditorWidget::MidiEditorWidget(NoteNagaEngine* engine_, QWidget* parent)
    : QGraphicsView(parent),
      engine(engine_),
      time_scale(0.2),
      key_height(16),
      content_width(100),
      content_height(100),
      tact_subdiv(4),
      bg_color("#32353c"),
      fg_color("#e0e6ef"),
      line_color("#232731"),
      subline_color("#464a56"),
      grid_bar_color("#5e7fff"),
      grid_row_color1("#35363b"),
      grid_row_color2("#292a2e"),
      grid_bar_label_color("#6fa5ff"),
      grid_subdiv_color("#44464b")
{
    setObjectName("MidiViewerWidget");
    setFrameStyle(QFrame::NoFrame);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    scene = new QGraphicsScene(this);
    setScene(scene);

    // Signály z projektu: vždy předáváme aktuální sekvenci
    connect(engine->get_project(), &NoteNagaProject::active_sequence_changed_signal,
            this, [this](NoteNagaMIDISeq* seq) { update_scene_items(seq); });

    connect(engine->get_project(), &NoteNagaProject::track_meta_changed_signal,
            this, [this](NoteNagaTrack*, const QString&) { update_scene_items(engine->get_project()->get_active_sequence()); });

    connect(engine->get_project(), &NoteNagaProject::current_tick_changed_signal,
            this, &MidiEditorWidget::update_marker_slot);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &MidiEditorWidget::on_viewport_changed);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &MidiEditorWidget::on_viewport_changed);

    setBackgroundBrush(bg_color);
    setMouseTracking(true);

    recalculate_content_size();
}

QSize MidiEditorWidget::sizeHint() const {
    return QSize(content_width, content_height);
}
QSize MidiEditorWidget::minimumSizeHint() const {
    return QSize(10, 10);
}

void MidiEditorWidget::recalculate_content_size() {
    auto seq = engine->get_project()->get_active_sequence();
    if (!seq) return;
    content_width = int((seq->get_max_tick() + 1) * time_scale) + 16;
    content_height = (MAX_NOTE - MIN_NOTE + 1) * key_height;
    setSceneRect(0, 0, content_width, content_height);
    update_scene_items(seq);
}

void MidiEditorWidget::repaint_slot() {
    update_scene_items(engine->get_project()->get_active_sequence());
}

void MidiEditorWidget::update_marker_slot() {
    if (marker_line) {
        scene->removeItem(marker_line);
        delete marker_line;
        marker_line = nullptr;
    }
    update_marker(engine->get_project()->get_active_sequence());
}

void MidiEditorWidget::on_viewport_changed() {
    update_scene_items(engine->get_project()->get_active_sequence());
}

void MidiEditorWidget::set_time_scale(double scale) {
    time_scale = std::max(0.02, scale);
    recalculate_content_size();
}
void MidiEditorWidget::set_key_height(int h) {
    key_height = std::max(4, std::min(48, h));
    recalculate_content_size();
}
void MidiEditorWidget::set_time_scale_slot(double scale) { set_time_scale(scale); }
void MidiEditorWidget::set_key_height_slot(int h) { set_key_height(h); }

void MidiEditorWidget::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    update_scene_items(engine->get_project()->get_active_sequence());
}

void MidiEditorWidget::clear_scene() {
    scene->clear();
    note_items.clear();
    grid_lines.clear();
    bar_grid_lines.clear();
    bar_grid_labels.clear();
    marker_line = nullptr;
}

void MidiEditorWidget::update_scene_items(NoteNagaMIDISeq *seq) {
    clear_scene();
    scene->setBackgroundBrush(bg_color);

    if (!seq) {
        auto *txt = scene->addSimpleText("Open file");
        txt->setBrush(fg_color);
        txt->setFont(QFont("Arial", 22, QFont::Bold));
        txt->setPos(width()/2 - 100, height()/2 - 20);
        return;
    }

    update_grid(seq);
    update_bar_grid(seq);
    update_notes(seq);
    update_marker_slot();
}

void MidiEditorWidget::update_grid(const NoteNagaMIDISeq *seq) {
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    for (int idx = 0, note_val = MIN_NOTE; note_val <= MAX_NOTE; ++idx, ++note_val) {
        int y = content_height - (idx + 1) * key_height;
        if (y + key_height < visible_y0 || y > visible_y1) continue;
        QColor row_bg = (note_val % 2 == 0) ? grid_row_color1 : grid_row_color2;
        auto row_bg_rect = scene->addRect(0, y, content_width, key_height, QPen(Qt::NoPen), QBrush(row_bg));
        row_bg_rect->setZValue(-100);

        auto l = scene->addLine(0, y, content_width, y, QPen(line_color, 1));
        grid_lines.push_back(l);
    }
}

void MidiEditorWidget::update_bar_grid(const NoteNagaMIDISeq *seq) {
    int ppq = seq->get_ppq();
    int bar_length = ppq * 4;
    int first_bar = 0;
    int last_bar = (seq->get_max_tick() / bar_length) + 2;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();

    double px_per_bar = time_scale * bar_length;
    int bar_skip = 1;
    double min_bar_dist_px = 58;
    while (px_per_bar * bar_skip < min_bar_dist_px) bar_skip *= 2;

    QFont label_font("Arial", 11, QFont::Bold);

    for (int bar = first_bar; bar < last_bar; bar += bar_skip) {
        int tick = bar * bar_length;
        int x = tick * time_scale;
        if (x < visible_x0 - 200 || x > visible_x1 + 200) continue;
        auto l = scene->addLine(x, 0, x, content_height, QPen(grid_bar_color, 2));
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
        while (px_per_div * subdiv_skip < 18.0 && subdiv_skip < tact_subdiv) subdiv_skip *= 2;
        for (int sub = 1; sub < tact_subdiv; ++sub) {
            if (sub % subdiv_skip != 0) continue;
            int sub_tick = tick + (bar_length * sub) / tact_subdiv;
            int sub_x = sub_tick * time_scale;
            if (sub_x < visible_x0 - 200 || sub_x > visible_x1 + 200) continue;
            auto lsub = scene->addLine(sub_x, 0, sub_x, content_height, QPen(grid_subdiv_color, 1));
            lsub->setZValue(1);
        }
    }
}

void MidiEditorWidget::draw_note(const NoteNagaNote& note, const NoteNagaTrack* track, bool is_selected, bool is_drum, int x, int y, int w, int h)
{
    QGraphicsItem* shape = nullptr;
    QColor t_color = is_selected ? track->get_color()
                                 : color_blend(track->get_color(), bg_color, 0.3);
    QPen outline = is_selected ? QPen(t_color.lightness() < 128 ? Qt::white : Qt::black, 2)
                               : QPen(t_color.lightness() < 128 ? t_color.lighter(150) : t_color.darker(150));

    if (is_drum) {
        int sz = h * 0.6;
        int cx = x + w / 2;
        int cy = y + h / 2;
        int left = cx - sz / 2;
        int top = cy - sz / 2;
        shape = scene->addEllipse(left, top, sz, sz, outline, QBrush(t_color));
    } else {
        shape = scene->addRect(x, y, w, h, outline, QBrush(t_color));
    }
    shape->setZValue(is_selected ? 999 : track->get_id());

    QGraphicsSimpleTextItem* txt = nullptr;
    if (!is_drum && w > 15 && h > 9 && time_scale > 0.04) {
        QString note_str = note_name(note.note);
        txt = scene->addSimpleText(note_str);
        txt->setBrush(QBrush(t_color.lightness() < 128 ? Qt::white : Qt::black));
        QFont f("Arial", std::max(6, h - 6));
        txt->setFont(f);
        txt->setPos(x + 2, y + 2);
        txt->setZValue(shape->zValue() + 1);
    }
    note_items[track->get_id()].push_back({shape, txt});
}

void MidiEditorWidget::update_notes(const NoteNagaMIDISeq *seq) {
    if (!seq) return;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    for (const auto& track : seq->get_tracks()) {
        if (!track || !track->is_visible()) continue;
        bool is_drum = track->get_channel().has_value() && track->get_channel().value() == 9;
        bool is_selected = seq->get_active_track_id().has_value() &&
                           seq->get_active_track_id().value() == track->get_id();

        for (const auto& note : track->get_notes()) {
            if (!note.start.has_value() || !note.length.has_value()) continue;
            int y = content_height - (note.note - MIN_NOTE + 1) * key_height;
            int x = note.start.value() * time_scale;
            int w = std::max(1, int(note.length.value() * time_scale));
            int h = key_height;
            if (!((x + w > visible_x0 && x < visible_x1) && (y + h > visible_y0 && y < visible_y1)))
                continue;
            draw_note(note, track, is_selected, is_drum, x, y, w, h);
        }
    }
}

void MidiEditorWidget::update_marker(const NoteNagaMIDISeq *seq) {
    if (!seq) return;
    int marker_x = engine->get_project()->get_current_tick() * time_scale;
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    if (marker_x > 0 && marker_x < content_width) {
        marker_line = scene->addLine(marker_x, visible_y0, marker_x, visible_y1, QPen(QColor(255, 88, 88), 2));
        marker_line->setZValue(1000);
    }
}

void MidiEditorWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        auto seq = engine->get_project()->get_active_sequence();
        if (seq) {
            int x = int(mapToScene(event->pos()).x());
            int tick = int(x / time_scale);
            emit set_position_signal(tick);
        }
    }
    QGraphicsView::mousePressEvent(event);
}