#include "midi_editor_widget.h"
#include <QResizeEvent>
#include <QScrollBar>
#include <algorithm>
#include <cmath>

#define MIN_NOTE 0
#define MAX_NOTE 127

MidiEditorWidget::MidiEditorWidget(AppContext* ctx_, QWidget* parent)
    : QGraphicsView(parent),
      ctx(ctx_),
      has_file(false),
      time_scale(0.2),
      key_height(16),
      _content_width(100),
      _content_height(100),
      ppq(480),
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

    connect(ctx, &AppContext::midi_file_loaded_signal, this, &MidiEditorWidget::_reload_notes);
    connect(ctx, &AppContext::selected_track_changed_signal, this, [this](int){ update_scene_items(); });
    connect(ctx, &AppContext::track_meta_changed_signal, this, [this](int){ update_scene_items(); });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &MidiEditorWidget::on_viewport_changed);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &MidiEditorWidget::on_viewport_changed);

    setBackgroundBrush(bg_color);
    setMouseTracking(true);

    recalculate_content_size();
}

QSize MidiEditorWidget::sizeHint() const {
    return QSize(_content_width, _content_height);
}
QSize MidiEditorWidget::minimumSizeHint() const {
    return QSize(10, 10);
}

void MidiEditorWidget::recalculate_content_size() {
    _content_width = int((ctx->max_tick + 1) * time_scale) + 16;
    _content_height = (MAX_NOTE - MIN_NOTE + 1) * key_height;
    setSceneRect(0, 0, _content_width, _content_height);
    update_scene_items();
}

void MidiEditorWidget::repaint_slot() {
    update_scene_items();
}

void MidiEditorWidget::update_marker_slot() {
    if (marker_line) {
        scene->removeItem(marker_line);
        delete marker_line;
        marker_line = nullptr;
    }
    update_marker();
}

void MidiEditorWidget::on_viewport_changed() {
    update_scene_items();
}

void MidiEditorWidget::_reload_notes() {
    has_file = !ctx->tracks.empty();
    ppq = ctx->ppq;
    recalculate_content_size();
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
    update_scene_items();
}

void MidiEditorWidget::clear_scene() {
    scene->clear();
    note_items.clear();
    grid_lines.clear();
    bar_grid_lines.clear();
    bar_grid_labels.clear();
    marker_line = nullptr;
}

void MidiEditorWidget::update_scene_items() {
    clear_scene();
    scene->setBackgroundBrush(bg_color);

    if (!has_file) {
        auto *txt = scene->addSimpleText("Open file");
        txt->setBrush(fg_color);
        txt->setFont(QFont("Arial", 22, QFont::Bold));
        txt->setPos(width()/2 - 100, height()/2 - 20);
        return;
    }

    update_grid();
    update_bar_grid();
    update_notes();
    update_marker_slot();
}

void MidiEditorWidget::update_grid() {
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    for (int idx = 0, note_val = MIN_NOTE; note_val <= MAX_NOTE; ++idx, ++note_val) {
        int y = _content_height - (idx + 1) * key_height;
        if (y + key_height < visible_y0 || y > visible_y1) continue;
        QColor row_bg = (note_val % 2 == 0) ? grid_row_color1 : grid_row_color2;
        auto row_bg_rect = scene->addRect(0, y, _content_width, key_height, QPen(Qt::NoPen), QBrush(row_bg));
        row_bg_rect->setZValue(-100);

        // Draw horizontal line
        auto l = scene->addLine(0, y, _content_width, y, QPen(line_color, 1));
        grid_lines.push_back(l);
    }
}

void MidiEditorWidget::update_bar_grid() {
    // --- Main bar lines ---
    int bar_length = ppq * 4;
    int first_bar = 0;
    int last_bar = (ctx->max_tick / bar_length) + 2;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();

    // Dynamically skip bar lines if too close
    double px_per_bar = time_scale * bar_length;
    int bar_skip = 1;
    double min_bar_dist_px = 58;
    while (px_per_bar * bar_skip < min_bar_dist_px) bar_skip *= 2;

    QFont label_font("Arial", 11, QFont::Bold);

    for (int bar = first_bar; bar < last_bar; bar += bar_skip) {
        int tick = bar * bar_length;
        int x = tick * time_scale;
        if (x < visible_x0 - 200 || x > visible_x1 + 200) continue;
        auto l = scene->addLine(x, 0, x, _content_height, QPen(grid_bar_color, 2));
        l->setZValue(2);
        bar_grid_lines.push_back(l);

        // Draw label always visible at the top of the viewport
        if (px_per_bar > 30) {
            auto label = scene->addSimpleText(QString::number(bar + 1));
            label->setFont(label_font);
            label->setBrush(grid_bar_label_color);
            // Always at the top of the visible area
            label->setPos(x + 4, visible_y0 + 4);
            label->setZValue(9999);
            bar_grid_labels.push_back(label);
        }

        // --- Draw subdivisions inside each bar ---
        // subdiv skip (if zoomed out, skip some subdivs)
        int subdiv_skip = 1;
        double px_per_div = px_per_bar / tact_subdiv;
        while (px_per_div * subdiv_skip < 18.0 && subdiv_skip < tact_subdiv) subdiv_skip *= 2;
        for (int sub = 1; sub < tact_subdiv; ++sub) {
            if (sub % subdiv_skip != 0) continue;
            int sub_tick = tick + (bar_length * sub) / tact_subdiv;
            int sub_x = sub_tick * time_scale;
            if (sub_x < visible_x0 - 200 || sub_x > visible_x1 + 200) continue;
            auto lsub = scene->addLine(sub_x, 0, sub_x, _content_height, QPen(grid_subdiv_color, 1));
            lsub->setZValue(1);
        }
    }
}

void MidiEditorWidget::draw_note(const MidiNote& note, const Track& track, bool is_selected, bool is_drum, int x, int y, int w, int h)
{
    QGraphicsItem* shape = nullptr;
    QColor t_color = is_selected ? track.color : color_blend(track.color, bg_color, 0.3);
    QPen outline = is_selected ? QPen(t_color.lightness() < 128 ? Qt::white : Qt::black, 2) : 
                                 QPen(t_color.lightness() < 128 ? t_color.lighter(150) : t_color.darker(150));

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
    shape->setZValue(is_selected ? 999 : track.track_id);

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
    int track_id = ctx->active_track_id.has_value() && is_selected
                   ? ctx->active_track_id.value()
                   : -1;
    if (track_id >= 0)
        note_items[track_id].push_back({shape, txt});
}

void MidiEditorWidget::update_notes() {
    if (!ctx) return;
    int visible_x0 = horizontalScrollBar()->value();
    int visible_x1 = visible_x0 + viewport()->width();
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    for (const auto& track_ptr : ctx->tracks) {
        if (!track_ptr || !track_ptr->visible) continue;

        bool is_drum = track_ptr->channel.has_value() && track_ptr->channel.value() == 9;
        bool is_selected = ctx->active_track_id.has_value() && ctx->active_track_id.value() == track_ptr->track_id;

        for (const MidiNote& note : track_ptr->midi_notes) {
            if (!note.start.has_value() || !note.length.has_value()) continue;
            int y = _content_height - (note.note - MIN_NOTE + 1) * key_height;
            int x = note.start.value() * time_scale;
            int w = std::max(1, int(note.length.value() * time_scale));
            int h = key_height;
            if (!((x + w > visible_x0 && x < visible_x1) && (y + h > visible_y0 && y < visible_y1)))
                continue;
            draw_note(note, *track_ptr, is_selected, is_drum, x, y, w, h);
        }
    }
}

void MidiEditorWidget::update_marker() {
    int marker_x = ctx->current_tick * time_scale;
    int visible_y0 = verticalScrollBar()->value();
    int visible_y1 = visible_y0 + viewport()->height();

    if (marker_x > 0 && marker_x < _content_width) {
        marker_line = scene->addLine(marker_x, visible_y0, marker_x, visible_y1, QPen(QColor(255, 88, 88), 2));
        marker_line->setZValue(1000);
    }
}

void MidiEditorWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && has_file) {
        int x = int(mapToScene(event->pos()).x());
        int tick = int(x / time_scale);
        emit set_play_position_signal(tick);
    }
    QGraphicsView::mousePressEvent(event);
}