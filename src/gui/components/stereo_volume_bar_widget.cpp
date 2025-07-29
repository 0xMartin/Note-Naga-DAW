#include "stereo_volume_bar_widget.h"
#include <QPainter>
#include <QLinearGradient>
#include <QTimer>
#include <cmath>

StereoVolumeBarWidget::StereoVolumeBarWidget(QWidget* parent, int minDb, int maxDb, int barWidth)
    : QWidget(parent), minDb_(minDb), maxDb_(maxDb), barWidth_(barWidth) {
    setMinimumWidth(48);
    setMinimumHeight(180);
    updateBoldTicks();
    leftPeakTimer_.start();
    rightPeakTimer_.start();
}

void StereoVolumeBarWidget::setVolumesDb(float leftDb, float rightDb) {
    if (abs(leftDb - leftDb_) < 0.01f && abs(rightDb - rightDb_) < 0.01f)
        return;
    leftDb_ = leftDb;
    rightDb_ = rightDb;
    updatePeakValues(leftDb, rightDb);
    update();
}

void StereoVolumeBarWidget::setDbRange(int minDb, int maxDb) {
    minDb_ = minDb;
    maxDb_ = maxDb;
    updateBoldTicks();
    update();
}

void StereoVolumeBarWidget::updateBoldTicks() {
    boldTicks_.clear();
    boldTicks_ << maxDb_;
    if (minDb_ <= -100 && maxDb_ >= 0) {
        boldTicks_ << -60 << minDb_;
    } else {
        boldTicks_ << (maxDb_ + minDb_)/2;
        boldTicks_ << minDb_;
    }
}

void StereoVolumeBarWidget::updatePeakValues(float leftDb, float rightDb) {
    // Left
    if (leftDb > leftPeakDb_ || leftPeakTimer_.elapsed() > peakHoldMs_) {
        leftPeakDb_ = leftDb;
        leftPeakTimer_.restart();
    }
    // Right
    if (rightDb > rightPeakDb_ || rightPeakTimer_.elapsed() > peakHoldMs_) {
        rightPeakDb_ = rightDb;
        rightPeakTimer_.restart();
    }
}

QSize StereoVolumeBarWidget::minimumSizeHint() const {
    return QSize(48, 180);
}

QSize StereoVolumeBarWidget::sizeHint() const {
    return QSize(64, 220);
}

void StereoVolumeBarWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int margin = 8;
    int label_width = 26;
    int total_bar_area = width() - label_width - 2 * margin;
    int bar_w = 15;
    int spacing = 10;
    int bar_group_width = 2 * bar_w + spacing;
    int bar_group_x = margin + (total_bar_area - bar_group_width) / 2;

    int left_x = bar_group_x;
    int right_x = left_x + bar_w + spacing;
    int bar_h = height() - margin * 2 - 8;

    // draw background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#232731"));
    p.drawRect(left_x - 2, margin, bar_w + 4, bar_h);
    p.drawRect(right_x - 2, margin, bar_w + 4, bar_h);

    // Gradient
    QLinearGradient grad(0, margin + bar_h, 0, margin);
    grad.setColorAt(0.0, QColor("#28ff42"));
    grad.setColorAt(0.7, QColor("#f7ff3c"));
    grad.setColorAt(1.0, QColor("#ff2929"));

    // Draw bars
    drawValueBar(p, left_x, bar_w, bar_h, margin, leftDb_, grad);
    drawValueBar(p, right_x, bar_w, bar_h, margin, rightDb_, grad);

    // dB scale
    int scale_x = width() - label_width + 2;
    drawDbScale(p, left_x, right_x, bar_w, bar_h, margin, label_width, scale_x);

    // L/R labels
    drawLabels(p, left_x, right_x, bar_w, margin);

    // Draw peak indicators
    drawPeakIndicator(p, left_x, bar_w, bar_h, margin, leftPeakDb_);
    drawPeakIndicator(p, right_x, bar_w, bar_h, margin, rightPeakDb_);
}

void StereoVolumeBarWidget::drawValueBar(QPainter& p, int x, int bar_w, int bar_h, int margin, float dbValue, const QLinearGradient& grad) {
    float ratio = std::min(1.0f, std::max(0.0f, (dbValue - minDb_) / float(maxDb_ - minDb_)));
    int filled_height = int(ratio * bar_h);
    int tick_height = 2;
    int tick_gap = 2;

    int num_ticks = bar_h / (tick_height + tick_gap);

    for (int i = 0; i < num_ticks; ++i) {
        int tick_y = margin + bar_h - (i * (tick_height + tick_gap)) - tick_height;
        if (tick_y < margin) break;

        // Barva podle gradientu
        qreal grad_pos = 1.0 - (double(i) * (tick_height + tick_gap)) / bar_h;
        QColor grad_color = grad.stops().front().second;
        for (int s = 1; s < grad.stops().size(); ++s) {
            if (grad_pos <= grad.stops()[s].first) {
                qreal t = (grad_pos - grad.stops()[s-1].first) / (grad.stops()[s].first - grad.stops()[s-1].first);
                grad_color = QColor::fromRgbF(
                    grad.stops()[s-1].second.redF() * (1-t) + grad.stops()[s].second.redF() * t,
                    grad.stops()[s-1].second.greenF() * (1-t) + grad.stops()[s].second.greenF() * t,
                    grad.stops()[s-1].second.blueF() * (1-t) + grad.stops()[s].second.blueF() * t
                );
                break;
            }
        }
        if (margin + bar_h - tick_y > filled_height) continue;
        p.setPen(Qt::NoPen);
        p.setBrush(grad_color);
        p.drawRect(x, tick_y, bar_w, tick_height);
    }
}

void StereoVolumeBarWidget::drawPeakIndicator(QPainter& p, int x, int bar_w, int bar_h, int margin, float peakDb) {
    float ratio = std::min(1.0f, std::max(0.0f, (peakDb - minDb_) / float(maxDb_ - minDb_)));
    int y = margin + bar_h - int(ratio * bar_h);
    QColor peakColor(255,255,255,220);
    p.setPen(Qt::NoPen);
    p.setBrush(peakColor);
    p.drawRect(x, y, bar_w, 2);
}

void StereoVolumeBarWidget::drawDbScale(QPainter& p, int left_x, int right_x, int bar_w, int bar_h, int margin, int label_width, int scale_x) {
    QFont normalFont = font();
    normalFont.setPointSize(8);
    QFont boldFont = normalFont;
    boldFont.setBold(true);

    p.setFont(normalFont);

    for (int db = maxDb_; db >= minDb_; db -= 10) {
        int y = margin + bar_h - int((db - minDb_) / float(maxDb_ - minDb_) * bar_h);
        p.setPen(QColor("#3e4a5a"));
        p.drawLine(left_x - 2, y, right_x + bar_w + 2, y);

        if (boldTicks_.contains(db)) {
            p.setFont(boldFont);
            p.setPen(QColor("#fff"));
        } else {
            p.setFont(normalFont);
            p.setPen(QColor("#888"));
        }
        p.drawText(scale_x, y - 7, label_width - 4, 14, Qt::AlignLeft | Qt::AlignVCenter, QString::number(db));
    }
}

void StereoVolumeBarWidget::drawLabels(QPainter& p, int left_x, int right_x, int bar_w, int margin) {
    p.setPen(QColor("#ccc"));
    QFont labelFont = font();
    labelFont.setPointSize(10);
    p.setFont(labelFont);
    p.drawText(left_x, height() - margin - 3, bar_w, 12, Qt::AlignCenter, "L");
    p.drawText(right_x, height() - margin - 3, bar_w, 12, Qt::AlignCenter, "R");
}