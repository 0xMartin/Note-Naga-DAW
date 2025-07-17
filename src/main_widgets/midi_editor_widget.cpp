#include "midi_editor_widget.h"
#include <algorithm>
#include <QMouseEvent>
#include <cmath>

MidiEditorWidget::MidiEditorWidget(AppContext* ctx_, QWidget* parent)
    : QWidget(parent), ctx(ctx_), has_file(false),
      time_scale(0.2), key_height(16), _content_width(100), _content_height(100),
      bg_color("#32353c"), fg_color("#e0e6ef"), line_color("#232731"), subline_color("#464a56")
{
    setObjectName("MidiViewerWidget");
    // Connect signals
    connect(ctx, &AppContext::midi_file_loaded_signal, this, &MidiEditorWidget::_reload_notes);
    connect(ctx, &AppContext::selected_track_changed_signal, this, [this](int){ update(); });
    connect(ctx, &AppContext::track_meta_changed_signal, this, [this](int){ update(); });

    ppq = ctx->ppq;

    setMouseTracking(true);
    setAutoFillBackground(false);

    recalculate_content_size();
}

void MidiEditorWidget::recalculate_content_size() {
    _content_width = int((ctx->max_tick + 1) * time_scale) + 16;
    _content_height = (MAX_NOTE - MIN_NOTE + 1) * key_height;
    resize(sizeHint());
}

void MidiEditorWidget::_reload_notes() {
    notes_starts.clear();
    notes_ends.clear();
    for (const auto& track_ptr : ctx->tracks) {
        if (!track_ptr) continue;
        int tid = track_ptr->track_id;
        notes_starts[tid].clear();
        notes_ends[tid].clear();
        for (const MidiNote& n : track_ptr->midi_notes) {
            if (n.start.has_value() && n.length.has_value()) {
                notes_starts[tid].push_back(n.start.value());
                notes_ends[tid].push_back(n.start.value() + n.length.value());
            }
        }
    }
    ppq = ctx->ppq;
    has_file = !ctx->tracks.empty();
    recalculate_content_size();
    update();
}

void MidiEditorWidget::set_time_scale(double scale) {
    time_scale = std::max(0.02, scale);
    recalculate_content_size();
    update();
}

void MidiEditorWidget::set_key_height(int h) {
    key_height = std::max(4, std::min(48, h));
    recalculate_content_size();
    update();
}

QSize MidiEditorWidget::sizeHint() const {
    return QSize(_content_width, _content_height);
}

QSize MidiEditorWidget::minimumSizeHint() const {
    return QSize(10, 10);
}

void MidiEditorWidget::draw_note(QPainter& painter, const MidiNote& note, const QColor& note_color,
                                 int visible_x0, int visible_x1, int visible_y0, int visible_y1, bool active)
{
    int y = _content_height - (note.note - MIN_NOTE + 1) * key_height;
    int x = note.start.value_or(0) * time_scale;
    int w = std::max(1, int(note.length.value_or(0) * time_scale));
    int h = key_height;
    if (!((x + w > visible_x0 && x < visible_x1) &&
          (y + h > visible_y0 && y < visible_y1))) {
        return;
    }

    if (active) {
        painter.setBrush(QBrush(note_color));
    } else {
        QColor c(note_color);
        c.setAlpha(50);
        painter.setBrush(QBrush(c));
    }

    // Kontrastní barva pro okraj/text
    QColor stroke_color = note_color.lightness() > 128 ? QColor(0, 0, 0) : QColor(255, 255, 255);
    painter.setPen(QPen(stroke_color, 1));
    painter.drawRect(int(x), int(y), int(w), int(h));
    painter.setPen(stroke_color);
    QFont font("Arial", std::max(6, key_height - 6));
    painter.setFont(font);
    if (w >= 30) 
    {
        QString note_label = note_name(note.note);
        QRect text_rect(int(x)+2, int(y)+int(h)-font.pointSize()-4, int(w)-4, font.pointSize()+4);
        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignBottom, note_label);
    }
}

void MidiEditorWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    QRect rect = event->rect();

    painter.fillRect(rect, bg_color);
    if (!has_file) {
        painter.setPen(fg_color);
        painter.setFont(QFont("Arial", 22, QFont::Bold));
        painter.drawText(rect, Qt::AlignCenter, "Open file");
        return;
    }

    int visible_x0 = rect.left();
    int visible_x1 = rect.right();
    int visible_y0 = rect.top();
    int visible_y1 = rect.bottom();

    int left_tick = std::max(0, int(visible_x0 / time_scale) - 1);
    int right_tick = int((visible_x1 + 1) / time_scale) + 1;

    int bottom_note = MIN_NOTE + std::max(0, int((_content_height - visible_y1) / key_height) - 1);
    int top_note = MIN_NOTE + int((_content_height - visible_y0) / key_height) + 1;

    // Draw horizontal note lines
    for (int idx = 0, note_val = MIN_NOTE; note_val <= MAX_NOTE; ++idx, ++note_val) {
        int y = _content_height - (idx + 1) * key_height;
        if (y + key_height < visible_y0 || y > visible_y1) continue;
        painter.setPen(QPen(line_color, 1));
        painter.drawLine(visible_x0, y, visible_x1, y);
    }

    // Draw non-active tracks first
    for (const auto& track_ptr : ctx->tracks) {
        if (!track_ptr || !track_ptr->visible) continue;
        if (ctx->active_track_id.has_value() && track_ptr->track_id == ctx->active_track_id.value()) continue;
        int tid = track_ptr->track_id;
        auto& starts = notes_starts[tid];
        auto& ends = notes_ends[tid];
        auto i0 = std::lower_bound(ends.begin(), ends.end(), left_tick - 1500);
        auto i1 = std::upper_bound(starts.begin(), starts.end(), right_tick + 100);
        int idx0 = std::distance(ends.begin(), i0);
        int idx1 = std::distance(starts.begin(), i1);
        for (int i = idx0; i < idx1 && i < int(track_ptr->midi_notes.size()); ++i) {
            const auto& note = track_ptr->midi_notes[i];
            if (note.note < bottom_note || note.note > top_note) continue;
            draw_note(painter, note, track_ptr->color, visible_x0, visible_x1, visible_y0, visible_y1, false);
        }
    }

    // Draw active track notes
    if (ctx->active_track_id.has_value()) {
        for (const auto& track_ptr : ctx->tracks) {
            if (!track_ptr || !track_ptr->visible) continue;
            if (track_ptr->track_id != ctx->active_track_id.value()) continue;
            int tid = track_ptr->track_id;
            auto& starts = notes_starts[tid];
            auto& ends = notes_ends[tid];
            auto i0 = std::lower_bound(ends.begin(), ends.end(), left_tick - 1500);
            auto i1 = std::upper_bound(starts.begin(), starts.end(), right_tick + 100);
            int idx0 = std::distance(ends.begin(), i0);
            int idx1 = std::distance(starts.begin(), i1);
            for (int i = idx0; i < idx1 && i < int(track_ptr->midi_notes.size()); ++i) {
                const auto& note = track_ptr->midi_notes[i];
                if (note.note < bottom_note || note.note > top_note) continue;
                draw_note(painter, note, track_ptr->color, visible_x0, visible_x1, visible_y0, visible_y1, true);
            }
        }
    }

    // --- Red Marker Line ---
    int marker_x = ctx->current_tick * time_scale;
    if (visible_x0 <= marker_x && marker_x <= visible_x1) {
        painter.setPen(QPen(QColor(255, 88, 88), 2));
        painter.drawLine(marker_x, visible_y0, marker_x, visible_y1);
    }

    // --- Bar Grid by Ticks ---
    int bar_length = ppq * 4;
    int first_bar = (left_tick / bar_length);
    int last_bar = (right_tick / bar_length) + 1;

    QPen grid_pen(subline_color, 1, Qt::SolidLine);
    QFont label_font("Arial", 11, QFont::Bold);
    painter.setFont(label_font);

    for (int bar = first_bar; bar < last_bar; ++bar) {
        int tick = bar * bar_length;
        int x = tick * time_scale;
        if (visible_x0 <= x && x <= visible_x1) {
            painter.setPen(grid_pen);
            painter.drawLine(x, visible_y0, x, visible_y1);
            painter.setPen(subline_color.lighter(200));
            painter.drawText(x + 4, visible_y0 + 12, QString::number(bar + 1));
        }
    }
}

void MidiEditorWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && has_file) {
        int x = event->pos().x();
        int tick = int(x / time_scale);
        emit set_play_position_signal(tick);
    }
}

void MidiEditorWidget::repaint_slot() {
    update();
}

void MidiEditorWidget::set_time_scale_slot(double scale) {
    set_time_scale(scale);
}

void MidiEditorWidget::set_key_height_slot(int h) {
    set_key_height(h);
}