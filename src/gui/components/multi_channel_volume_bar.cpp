#include "multi_channel_volume_bar.h"

#include <QBrush>
#include <QContextMenuEvent>
#include <QFont>
#include <QLinearGradient>
#include <QMouseEvent>
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
      decay_steepness(2.2f),
      peak_hold_enabled(false),
      peak_hold_time_ms(1500),
      clip_threshold(0.95f),
      led_mode(true),
      led_segment_count(16),
      led_gap(1),
      hovered_channel(-1) {
    setMinimumHeight(80);
    setMinimumWidth(40 + channels * 12);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);

    current_values.resize(channels, 0.0f);
    initial_decay_values.resize(channels, 0.0f);
    decay_times.resize(channels, 400);
    anim_elapsed.resize(channels, nullptr);
    anim_active.resize(channels, false);
    target_values.resize(channels, 0.0f);
    peak_values.resize(channels, 0.0f);
    peak_timers.resize(channels, nullptr);
    clip_indicators.resize(channels, false);

    for (int i = 0; i < channels; ++i) {
        anim_elapsed[i] = new QElapsedTimer();
        peak_timers[i] = new QElapsedTimer();
    }

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MultiChannelVolumeBar::onAnimTick);
    
    peak_decay_timer = new QTimer(this);
    connect(peak_decay_timer, &QTimer::timeout, this, &MultiChannelVolumeBar::onPeakDecayTick);
    peak_decay_timer->start(50);
}

void MultiChannelVolumeBar::setChannelCount(int channels_) {
    if (channels_ != channels) {
        channels = channels_;
        current_values.resize(channels, 0.0f);
        initial_decay_values.resize(channels, 0.0f);
        decay_times.resize(channels, 400);
        peak_values.resize(channels, 0.0f);
        clip_indicators.resize(channels, false);

        for (auto *t : anim_elapsed)
            delete t;
        anim_elapsed.clear();
        for (auto *t : peak_timers)
            delete t;
        peak_timers.clear();
        
        for (int i = 0; i < channels; ++i) {
            anim_elapsed.push_back(new QElapsedTimer());
            peak_timers.push_back(new QElapsedTimer());
        }

        anim_active.resize(channels, false);
        target_values.resize(channels, 0.0f);
        update();
    }
}

int MultiChannelVolumeBar::getChannelCount() const { return channels; }

void MultiChannelVolumeBar::setValue(int channel_idx, float value, int time_ms) {
    if (!(0 <= channel_idx && channel_idx < channels)) return;
    value = std::clamp(value, min_value, max_value);
    
    // Update peak value
    float normalized = (value - min_value) / (max_value - min_value);
    if (normalized > peak_values[channel_idx]) {
        peak_values[channel_idx] = normalized;
        peak_timers[channel_idx]->restart();
    }
    
    // Check for clip
    if (normalized >= clip_threshold) {
        clip_indicators[channel_idx] = true;
    }
    
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

void MultiChannelVolumeBar::resetPeaks() {
    for (int i = 0; i < channels; ++i) {
        peak_values[i] = 0.0f;
    }
    update();
}

void MultiChannelVolumeBar::resetClips() {
    for (int i = 0; i < channels; ++i) {
        clip_indicators[i] = false;
    }
    update();
}

void MultiChannelVolumeBar::setLedMode(bool enabled) {
    led_mode = enabled;
    update();
}

void MultiChannelVolumeBar::setPeakHoldEnabled(bool enabled) {
    peak_hold_enabled = enabled;
    if (!enabled) {
        resetPeaks();
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

void MultiChannelVolumeBar::onPeakDecayTick() {
    if (!peak_hold_enabled) return;
    
    bool needs_update = false;
    for (int i = 0; i < channels; ++i) {
        if (peak_values[i] > 0.0f && peak_timers[i]->elapsed() > peak_hold_time_ms) {
            // Decay peak slowly
            peak_values[i] *= 0.92f;
            if (peak_values[i] < 0.01f) {
                peak_values[i] = 0.0f;
            }
            needs_update = true;
        }
    }
    if (needs_update) {
        update();
    }
}

int MultiChannelVolumeBar::getChannelAtPos(const QPoint &pos) {
    int w = width();
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

    for (int i = 0; i < channels; ++i) {
        int x = start_x + i * (bar_width + bar_space);
        if (pos.x() >= x && pos.x() < x + bar_width) {
            return i;
        }
    }
    return -1;
}

void MultiChannelVolumeBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int channel = getChannelAtPos(event->pos());
        if (channel >= 0) {
            emit channelClicked(channel);
        }
    }
    QWidget::mousePressEvent(event);
}

void MultiChannelVolumeBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int channel = getChannelAtPos(event->pos());
        if (channel >= 0) {
            emit channelSoloed(channel);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void MultiChannelVolumeBar::mouseMoveEvent(QMouseEvent *event) {
    int channel = getChannelAtPos(event->pos());
    if (channel != hovered_channel) {
        hovered_channel = channel;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void MultiChannelVolumeBar::leaveEvent(QEvent *event) {
    hovered_channel = -1;
    update();
    QWidget::leaveEvent(event);
}

void MultiChannelVolumeBar::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #2b2d31; border: 1px solid #404249; padding: 4px; }"
        "QMenu::item { padding: 6px 20px; color: #dbdee1; }"
        "QMenu::item:selected { background: #404249; }"
        "QMenu::separator { height: 1px; background: #404249; margin: 4px 8px; }");
    
    QAction *resetPeaksAction = menu.addAction("Reset Peak Indicators");
    QAction *resetClipsAction = menu.addAction("Reset Clip Indicators");
    
    menu.addSeparator();
    
    QAction *toggleLedAction = menu.addAction(led_mode ? "Switch to Solid Bars" : "Switch to LED Mode");
    QAction *togglePeakAction = menu.addAction(peak_hold_enabled ? "Disable Peak Hold" : "Enable Peak Hold");
    
    menu.addSeparator();
    
    int clicked_channel = getChannelAtPos(event->pos());
    QAction *soloAction = nullptr;
    if (clicked_channel >= 0) {
        soloAction = menu.addAction(QString("Solo Channel %1").arg(clicked_channel + 1));
    }
    
    QAction *selected = menu.exec(event->globalPos());
    
    if (selected == resetPeaksAction) {
        resetPeaks();
    } else if (selected == resetClipsAction) {
        resetClips();
    } else if (selected == toggleLedAction) {
        setLedMode(!led_mode);
    } else if (selected == togglePeakAction) {
        setPeakHoldEnabled(!peak_hold_enabled);
    } else if (soloAction && selected == soloAction) {
        emit channelSoloed(clicked_channel);
    }
}

QColor MultiChannelVolumeBar::getColorForLevel(float normalized_value) {
    // FL Studio style gradient: green -> yellow -> orange -> red
    if (normalized_value < 0.5f) {
        // Green to Yellow (0.0 - 0.5)
        float t = normalized_value * 2.0f;
        return QColor(
            int(0 + t * 255),      // R: 0 -> 255
            int(200 + t * 55),     // G: 200 -> 255
            int(0)                 // B: 0
        );
    } else if (normalized_value < 0.75f) {
        // Yellow to Orange (0.5 - 0.75)
        float t = (normalized_value - 0.5f) * 4.0f;
        return QColor(
            255,                   // R: 255
            int(255 - t * 100),    // G: 255 -> 155
            0                      // B: 0
        );
    } else {
        // Orange to Red (0.75 - 1.0)
        float t = (normalized_value - 0.75f) * 4.0f;
        return QColor(
            255,                   // R: 255
            int(155 - t * 155),    // G: 155 -> 0
            0                      // B: 0
        );
    }
}

void MultiChannelVolumeBar::drawLedSegments(QPainter &painter, int x, int y, 
                                             int width, int height, float value) {
    // Calculate segment height to exactly fill the available height
    // Total: led_segment_count segments + (led_segment_count - 1) gaps
    int total_gaps = (led_segment_count - 1) * led_gap;
    int segment_height = (height - total_gaps) / led_segment_count;
    if (segment_height < 2) segment_height = 2;
    
    // Calculate actual used height and any remainder
    int used_height = led_segment_count * segment_height + total_gaps;
    int remainder = height - used_height;
    
    int active_segments = static_cast<int>(value * led_segment_count + 0.5f);
    if (value > 0.01f && active_segments == 0) active_segments = 1;
    
    // Draw from bottom (seg=0) to top (seg=led_segment_count-1)
    int current_y = y + height;  // Start from bottom
    
    for (int seg = 0; seg < led_segment_count; ++seg) {
        // Add 1 extra pixel to first few segments to distribute remainder
        int this_seg_height = segment_height + (seg < remainder ? 1 : 0);
        current_y -= this_seg_height;
        
        float seg_level = float(seg) / float(led_segment_count - 1);
        
        if (seg < active_segments) {
            // Active segment
            QColor color = getColorForLevel(seg_level);
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(x, current_y, width, this_seg_height, 1, 1);
            
            // Subtle highlight
            QColor highlight = color.lighter(130);
            painter.setBrush(highlight);
            int hl_height = std::max(1, this_seg_height / 4);
            painter.drawRect(x + 1, current_y + 1, width - 2, hl_height);
        } else {
            // Inactive segment
            painter.setBrush(QColor(30, 32, 36));
            painter.setPen(QColor(40, 42, 46));
            painter.drawRoundedRect(x, current_y, width, this_seg_height, 1, 1);
        }
        
        // Add gap after segment (except for last one)
        if (seg < led_segment_count - 1) {
            current_y -= led_gap;
        }
    }
}

void MultiChannelVolumeBar::drawSolidBar(QPainter &painter, int x, int y, 
                                          int width, int height, float value) {
    int bar_h = int(height * value);
    int by = y + height - bar_h;
    
    // Multi-color gradient
    QLinearGradient gradient(0, y + height, 0, y);
    gradient.setColorAt(0.0, QColor(0, 200, 0));      // Green
    gradient.setColorAt(0.5, QColor(255, 255, 0));    // Yellow
    gradient.setColorAt(0.75, QColor(255, 165, 0));   // Orange
    gradient.setColorAt(1.0, QColor(255, 0, 0));      // Red
    
    painter.setBrush(QBrush(gradient));
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, by, width, bar_h);
    
    // Frame
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(80, 84, 94));
    painter.drawRect(x, y, width, height);
}

void MultiChannelVolumeBar::drawPeakIndicator(QPainter &painter, int x, int y, 
                                               int width, int bar_area_height, float peak) {
    if (peak <= 0.01f) return;
    
    int peak_y = y + bar_area_height - int(bar_area_height * peak);
    QColor peak_color = getColorForLevel(peak);
    
    // Draw peak line with glow
    painter.setPen(QPen(peak_color.darker(120), 1));
    painter.drawLine(x, peak_y + 1, x + width, peak_y + 1);
    painter.setPen(QPen(peak_color, 2));
    painter.drawLine(x, peak_y, x + width, peak_y);
}

void MultiChannelVolumeBar::drawClipIndicator(QPainter &painter, int x, int y, 
                                               int width, bool clipped) {
    int indicator_size = std::min(width - 2, 6);
    int cx = x + (width - indicator_size) / 2;
    int cy = y - indicator_size - 2;  // Draw above the bar area
    
    if (clipped) {
        // Red glowing clip indicator
        painter.setBrush(QColor(255, 40, 40));
        painter.setPen(QPen(QColor(255, 80, 80), 1));
    } else {
        // Dark inactive indicator
        painter.setBrush(QColor(50, 30, 30));
        painter.setPen(QColor(70, 45, 45));
    }
    painter.drawEllipse(cx, cy, indicator_size, indicator_size);
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
        
        // Draw bar (LED or solid mode)
        if (led_mode) {
            drawLedSegments(painter, x, top, bar_width, bar_area_height, value);
        } else {
            drawSolidBar(painter, x, top, bar_width, bar_area_height, value);
        }

        // Channel label
        QString channel_label = QString::number(i + 1);
        int text_w = painter.fontMetrics().horizontalAdvance(channel_label);
        int label_x = x + (bar_width - text_w) / 2;
        int label_y = bottom + painter.fontMetrics().ascent() + 4;
        
        painter.setPen(QColor("#b5bac1"));
        painter.drawText(label_x, label_y, channel_label);
    }

    // Scale on the right
    int scale_x = start_x + total_bar_width + 8;
    QColor scale_color("#888");
    int tick_length = 6;
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
        painter.drawLine(scale_x + 2, y, scale_x + 4, y);
    }

    painter.end();
}

float MultiChannelVolumeBar::exponential_decay(float progress, float steepness) {
    return std::exp(-steepness * progress);
}