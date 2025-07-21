#include "volume_bar.h"
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

float exponential_decay(float progress, float steepness) {
    return std::exp(-steepness * progress);
}

VolumeBar::VolumeBar(float value,
                     const QString& start_color_str,
                     const QString& end_color_str,
                     bool dynamic_mode,
                     QWidget* parent)
    : QWidget(parent),
      start_color(QColor(start_color_str)),
      end_color(QColor(end_color_str)),
      dynamic_mode(dynamic_mode),
      min_value(0.0f),
      max_value(1.0f),
      bar_height(6),
      labels({QString::number(min_value, 'f', 1),
              QString::number((min_value + max_value) / 2.0, 'f', 1),
              QString::number(max_value, 'f', 1)}),
      current_value(dynamic_mode ? 0.0f : value),
      target_value(0.0f),
      initial_decay_value(0.0f),
      decay_time(400),
      min_decay_time(120),
      timer(new QTimer(this)),
      anim_active(false),
      decay_steepness(2.2f)
{
    setFixedHeight(30);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(timer, &QTimer::timeout, this, &VolumeBar::onAnimTick);
}

void VolumeBar::setValue(float value, int time_ms) {
    value = std::clamp(value, min_value, max_value);
    if (!dynamic_mode) {
        current_value = value;
        target_value = value;
        update();
        return;
    }

    if (value >= current_value || !anim_active) {
        target_value = value;
        anim_elapsed.restart();
        anim_active = true;
        current_value = value;
        initial_decay_value = value;

        float norm_intensity = (value - min_value) / (max_value - min_value);
        float base_decay = 600.0f + norm_intensity * 1400.0f;
        if (time_ms >= 0) {
            base_decay += time_ms * 0.3f;
        }
        decay_time = std::max(min_decay_time, int(base_decay));

        timer->start(16); // ~60 FPS
        update();
    }
}

void VolumeBar::onAnimTick() {
    int elapsed = anim_elapsed.elapsed();
    float progress = std::min(float(elapsed) / float(decay_time), 2.0f);

    float decay_progress = std::max(0.0f, std::min(progress, 2.0f));
    float factor = exponential_decay(decay_progress, decay_steepness);

    current_value = initial_decay_value * factor;
    if (progress >= 2.0f) {
        current_value = 0.0f;
        anim_active = false;
        timer->stop();
    }
    update();
}

void VolumeBar::setRange(float min_value_, float max_value_) {
    min_value = min_value_;
    max_value = max_value_;
    labels = {
        QString::number(min_value, 'f', 1),
        QString::number((min_value + max_value) / 2.0, 'f', 1),
        QString::number(max_value, 'f', 1)
    };
    update();
}

void VolumeBar::setLabels(const std::vector<QString>& labels_) {
    if (labels_.size() == 3) {
        labels = labels_;
        update();
    }
}

void VolumeBar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    int bar_y = 0;
    int width = this->width();

    int current_value_clamped = std::max(this->min_value, std::min(this->max_value, current_value));
    int bar_width = int(width * current_value_clamped / (this->max_value - this->min_value));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Bar
    QLinearGradient gradient(0, 0, width, 0);
    gradient.setColorAt(0.0, start_color);
    gradient.setColorAt(1.0, end_color);
    painter.setBrush(QBrush(gradient));
    painter.setPen(Qt::NoPen);
    painter.drawRect(0, bar_y, bar_width, bar_height);

    // Okraj
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor("#6d737f"));
    painter.drawRect(0, bar_y, width - 1, bar_height);

    // Pravítko (měřítko)
    int scale_y = bar_height + 3;
    QColor scale_color("#999");
    int tick_length = 7;
    QFont font(this->font());
    font.setPointSize(8);
    painter.setFont(font);
    painter.setPen(scale_color);

    std::vector<int> positions = {0, width / 2, width - 1};
    for (size_t i = 0; i < positions.size(); ++i) {
        int x = positions[i];
        painter.drawLine(x, scale_y, x, scale_y + tick_length);
        QString label = labels[i];
        int text_width = painter.fontMetrics().horizontalAdvance(label);
        int tx = 0;
        if (i == 0) tx = x;
        else if (i == 1) tx = x - text_width / 2;
        else tx = x - text_width;
        int ty = scale_y + tick_length + painter.fontMetrics().ascent() + 1;
        painter.drawText(tx, ty, label);
    }

    painter.setPen(QPen(scale_color, 1));
    for (int i = 1; i < 10; ++i) {
        float frac = float(i) / 10.0f;
        int x = int(frac * width);
        if (std::find(positions.begin(), positions.end(), x) != positions.end())
            continue;
        painter.drawLine(x, scale_y + 2, x, scale_y + 4);
    }

    painter.end();
}