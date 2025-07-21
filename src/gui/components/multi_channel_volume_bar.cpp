#include "multi_channel_volume_bar.h"

#include <QBrush>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

MultiChannelVolumeBar::MultiChannelVolumeBar(int channels_,
                                             const QString &start_color_str,
                                             const QString &end_color_str,
                                             bool dynamic_mode_, QWidget *parent)
    : QWidget(parent), channels(channels_), start_color(start_color_str),
      end_color(end_color_str), dynamic_mode(dynamic_mode_), min_value(0.0f),
      max_value(1.0f), bar_width_min(8), bar_width_max(30), bar_space_min(2),
      bar_space_max(10), bar_bottom_margin(28), bar_top_margin(8),
      labels({QString::number(min_value, 'f', 1),
              QString::number((min_value + max_value) / 2.0, 'f', 1),
              QString::number(max_value, 'f', 1)}),
      decay_steepness(2.2f) {
    setMinimumHeight(80);
    setMinimumWidth(40 + channels * 12);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    current_values.resize(channels, 0.0f);
    initial_decay_values.resize(channels, 0.0f);
    decay_times.resize(channels, 400);
    anim_elapsed.resize(channels, nullptr);
    anim_active.resize(channels, false);
    target_values.resize(channels, 0.0f);

    for (int i = 0; i < channels; ++i) {
        anim_elapsed[i] = new QElapsedTimer();
    }

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MultiChannelVolumeBar::onAnimTick);
}

void MultiChannelVolumeBar::setChannelCount(int channels_) {
    if (channels_ != channels) {
        channels = channels_;
        current_values.resize(channels, 0.0f);
        initial_decay_values.resize(channels, 0.0f);
        decay_times.resize(channels, 400);

        for (auto *t : anim_elapsed)
            delete t;
        anim_elapsed.clear();
        for (int i = 0; i < channels; ++i)
            anim_elapsed.push_back(new QElapsedTimer());

        anim_active.resize(channels, false);
        target_values.resize(channels, 0.0f);
        update();
    }
}

int MultiChannelVolumeBar::getChannelCount() const { return channels; }

void MultiChannelVolumeBar::setValue(int channel_idx, float value, int time_ms) {
    if (!(0 <= channel_idx && channel_idx < channels)) return;
    value = std::clamp(value, min_value, max_value);
    if (!dynamic_mode) {
        current_values[channel_idx] = value;
        target_values[channel_idx] = value;
        update();
        return;
    }

    if (value >= current_values[channel_idx] || !anim_active[channel_idx]) {
        target_values[channel_idx] = value;
        anim_elapsed[channel_idx]->restart();
        anim_active[channel_idx] = true;
        current_values[channel_idx] = value;
        initial_decay_values[channel_idx] = value;

        float norm_intensity = (value - min_value) / (max_value - min_value);
        float base_decay = 600.0f + norm_intensity * 1400.0f;
        if (time_ms >= 0) { base_decay += time_ms * 0.3f; }
        decay_times[channel_idx] = std::max(120, int(base_decay));
        if (!timer->isActive()) { timer->start(16); }
        update();
    }
}

void MultiChannelVolumeBar::setRange(float min_value_, float max_value_) {
    min_value = min_value_;
    max_value = max_value_;
    labels = {QString::number(min_value, 'f', 1),
              QString::number((min_value + max_value) / 2.0, 'f', 1),
              QString::number(max_value, 'f', 1)};
    update();
}

void MultiChannelVolumeBar::setLabels(const std::vector<QString> &labels_) {
    if (labels_.size() == 3) {
        labels = labels_;
        update();
    }
}

void MultiChannelVolumeBar::onAnimTick() {
    bool anim_still_running = false;
    for (int i = 0; i < channels; ++i) {
        if (!anim_active[i]) continue;
        int elapsed = anim_elapsed[i]->elapsed();
        float progress = std::min(float(elapsed) / float(decay_times[i]), 2.0f);
        float decay_progress = std::max(0.0f, std::min(progress, 2.0f));
        float factor = exponential_decay(decay_progress, decay_steepness);
        current_values[i] = initial_decay_values[i] * factor;
        if (progress >= 2.0f) {
            current_values[i] = 0.0f;
            anim_active[i] = false;
        } else {
            anim_still_running = true;
        }
    }
    if (!anim_still_running) { timer->stop(); }
    update();
}

void MultiChannelVolumeBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();
    int top = bar_top_margin;
    int bottom = h - bar_bottom_margin;
    int bar_area_height = bottom - top;

    int avail_w = w - 36;
    int min_total = channels * bar_width_min + (channels - 1) * bar_space_min;
    int max_total = channels * bar_width_max + (channels - 1) * bar_space_max;

    int bar_width, bar_space;
    if (avail_w >= max_total) {
        bar_width = bar_width_max;
        bar_space = bar_space_max;
    } else if (avail_w <= min_total) {
        bar_width = bar_width_min;
        bar_space = bar_space_min;
    } else {
        float ratio = float(avail_w - min_total) / float(max_total - min_total);
        bar_width = int(bar_width_min + ratio * (bar_width_max - bar_width_min));
        bar_space = int(bar_space_min + ratio * (bar_space_max - bar_space_min));
    }

    int total_bar_width = channels * bar_width + (channels - 1) * bar_space;
    int start_x = std::max(2, (w - total_bar_width - 36) / 2);

    QFont font(this->font());
    font.setPointSize(8);
    painter.setFont(font);

    for (int i = 0; i < channels; ++i) {
        int x = start_x + i * (bar_width + bar_space);
        float value = std::max(0.0f, std::min(1.0f, (current_values[i] - min_value) /
                                                        (max_value - min_value)));
        int bar_h = int(bar_area_height * value);
        int by = bottom - bar_h;
        QLinearGradient gradient(0, bottom, 0, top);
        gradient.setColorAt(0.0, start_color);
        gradient.setColorAt(1.0, end_color);
        painter.setBrush(QBrush(gradient));
        painter.setPen(Qt::NoPen);
        painter.drawRect(x, by, bar_width, bar_h);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor("#6d737f"));
        painter.drawRect(x, top, bar_width, bar_area_height);

        QString channel_label = QString::number(i + 1);
        int text_w = painter.fontMetrics().horizontalAdvance(channel_label);
        int label_x = x + (bar_width - text_w) / 2;
        int label_y = bottom + painter.fontMetrics().ascent() + 2;
        painter.setPen(QColor("#b5bac1"));
        painter.drawText(label_x, label_y, channel_label);
    }

    int scale_x = start_x + total_bar_width + 10;
    QColor scale_color("#999");
    int tick_length = 8;
    painter.setPen(scale_color);

    std::vector<int> positions = {bottom, (bottom + top) / 2, top};
    for (size_t i = 0; i < positions.size(); ++i) {
        int y = positions[i];
        painter.drawLine(scale_x, y, scale_x + tick_length, y);
        QString label = labels[i];
        int tx = scale_x + tick_length + 3;
        int ty = (i != 0) ? y + painter.fontMetrics().ascent() / 2 : y;
        painter.drawText(tx, ty, label);
    }

    painter.setPen(QPen(scale_color, 1));
    for (int i = 1; i < 10; ++i) {
        float frac = float(i) / 10.0f;
        int y = int(bottom - frac * (bottom - top));
        if (std::find(positions.begin(), positions.end(), y) != positions.end()) continue;
        painter.drawLine(scale_x + 3, y, scale_x + 5, y);
    }

    painter.end();
}

float MultiChannelVolumeBar::exponential_decay(float progress, float steepness) {
    return std::exp(-steepness * progress);
}