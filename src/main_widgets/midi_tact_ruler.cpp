#include "midi_tact_ruler.h"

#include <QPainter>
#include <QMouseEvent>
#include <QRect>
#include <QPen>

MidiTactRuler::MidiTactRuler(int ticks_per_beat_, double time_scale_, int max_tick_, QWidget* parent)
    : QWidget(parent),
      ticks_per_beat(ticks_per_beat_),
      time_scale(time_scale_),
      max_tick(max_tick_),
      horizontalScroll(0),
      font("Arial", 9, QFont::Bold),
      bg_color("#32353b"),
      fg_color("#e0e6ef"),
      subline_color("#464a56"),
      tact_bg_color("#3c3f4f"),
      tact_line_color("#6f6fa6")
{
    setObjectName("MidiTactRuler");
    setFixedHeight(32);
}

void MidiTactRuler::set_params(int ticks_per_beat_, double time_scale_, int max_tick_) {
    ticks_per_beat = ticks_per_beat_;
    time_scale = time_scale_;
    max_tick = max_tick_;
    update();
}

void MidiTactRuler::set_horizontal_scroll_slot(int val) {
    set_horizontal_scroll(val);
}

void MidiTactRuler::set_horizontal_scroll(int val) {
    horizontalScroll = val;
    update();
}

void MidiTactRuler::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int click_x = int(event->position().x()) + horizontalScroll;
        int tick = int(double(click_x) / (ticks_per_beat * time_scale) * ticks_per_beat);
        emit play_position_set_signal(tick);
    }
}

void MidiTactRuler::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    QRect r = rect();

    painter.fillRect(r, bg_color);
    painter.setFont(font);
    double beat_px = ticks_per_beat * time_scale;
    int min_step_px = 60;
    int takt_step = 1;
    if (beat_px < min_step_px) {
        int pow2 = 1;
        while (beat_px * pow2 < min_step_px && pow2 <= 32) {
            pow2 *= 2;
        }
        takt_step = pow2;
    }
    int sub_beats = 4;
    double sub_beat_px = beat_px / sub_beats;
    int width = r.width();
    int num_beats = int(max_tick / ticks_per_beat) + 3;

    for (int i = 0; i < num_beats; i += takt_step) {
        int x = int(i * beat_px - horizontalScroll);
        if (x > width) break;
        if (x + int(beat_px * takt_step) < 0) continue;
        painter.fillRect(
            QRect(x, 0, int(beat_px * takt_step), r.height()),
            ((i / takt_step) % 2 == 0) ? tact_bg_color : bg_color
        );
        painter.setPen(QPen(tact_line_color, 2));
        painter.drawLine(x, 0, x, r.height());
        if (-20 < x && x < width) {
            painter.setPen(fg_color);
            painter.drawText(x + 5, r.height() - 7, QString::number(i + 1));
        }
        if (takt_step == 1 && sub_beat_px > 15) {
            painter.setPen(QPen(subline_color, 1));
            for (int j = 1; j < sub_beats; ++j) {
                int subx = int(x + j * sub_beat_px);
                if (0 < subx && subx < width) {
                    painter.drawLine(subx, r.height() / 2, subx, r.height());
                }
            }
        }
    }
}