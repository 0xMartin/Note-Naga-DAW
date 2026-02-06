#include "track_stereo_meter.h"
#include <QPainter>
#include <QLinearGradient>
#include <cmath>

TrackStereoMeter::TrackStereoMeter(QWidget* parent, int minDb, int maxDb)
    : QWidget(parent), minDb_(minDb), maxDb_(maxDb) {
    setMinimumHeight(22);
    setMaximumHeight(72);
    setAttribute(Qt::WA_TranslucentBackground);
    leftPeakTimer_.start();
    rightPeakTimer_.start();
}

void TrackStereoMeter::setVolumesDb(float leftDb, float rightDb) {
    // Skip updates when inactive to save CPU
    if (!m_active) return;
    
    if (std::abs(leftDb - leftDb_) < 0.1f && std::abs(rightDb - rightDb_) < 0.1f)
        return;
    leftDb_ = leftDb;
    rightDb_ = rightDb;
    updatePeakValues(leftDb, rightDb);
    update();
}

void TrackStereoMeter::setActive(bool active) {
    if (m_active != active) {
        m_active = active;
        if (!active) {
            reset();  // Reset meter when deactivated
        }
    }
}

void TrackStereoMeter::reset() {
    leftDb_ = -100.0f;
    rightDb_ = -100.0f;
    leftPeakDb_ = -100.0f;
    rightPeakDb_ = -100.0f;
    update();
}

void TrackStereoMeter::setDbRange(int minDb, int maxDb) {
    minDb_ = minDb;
    maxDb_ = maxDb;
    update();
}

void TrackStereoMeter::updatePeakValues(float leftDb, float rightDb) {
    if (leftDb > leftPeakDb_ || leftPeakTimer_.elapsed() > peakHoldMs_) {
        leftPeakDb_ = leftDb;
        leftPeakTimer_.restart();
    }
    if (rightDb > rightPeakDb_ || rightPeakTimer_.elapsed() > peakHoldMs_) {
        rightPeakDb_ = rightDb;
        rightPeakTimer_.restart();
    }
}

QSize TrackStereoMeter::minimumSizeHint() const {
    return QSize(60, 22);
}

QSize TrackStereoMeter::sizeHint() const {
    return QSize(120, 24);
}

void TrackStereoMeter::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();
    
    // Compact layout for small heights (arrangement track headers)
    bool compactMode = (h <= 24);
    
    int topMargin, bottomMargin, centerGap, labelWidth;
    if (compactMode) {
        topMargin = 1;
        bottomMargin = 1;
        centerGap = 1;
        labelWidth = 10;  // Smaller L/R labels
    } else {
        topMargin = 12;
        bottomMargin = 12;
        centerGap = 6;
        labelWidth = 14;
    }
    
    int barAreaX = labelWidth;
    int barAreaW = w - labelWidth - 2;  // Extend closer to right edge
    
    // Two bars with gap between them
    int availableHeight = h - topMargin - bottomMargin - centerGap;
    int barHeight = qMax(2, availableHeight / 2);
    int topBarY = topMargin;
    int bottomBarY = topMargin + barHeight + centerGap;

    // Background - transparent (no fill, parent shows through)
    // Only draw the bar backgrounds, not full widget background

    // Gradient stops (green -> yellow -> orange -> red)
    QLinearGradient grad(barAreaX, 0, barAreaX + barAreaW, 0);
    grad.setColorAt(0.0, QColor("#28ff42"));   // Green
    grad.setColorAt(0.6, QColor("#f7ff3c"));   // Yellow
    grad.setColorAt(0.85, QColor("#ff9900"));  // Orange
    grad.setColorAt(1.0, QColor("#ff2929"));   // Red

    // Calculate normalized values
    // minDb_ is on the left (0.0), maxDb_ is on the right (1.0)
    auto dbToRatio = [this](float db) -> float {
        if (db <= minDb_) return 0.0f;
        if (db >= maxDb_) return 1.0f;
        return (db - minDb_) / (float)(maxDb_ - minDb_);
    };

    float leftRatio = dbToRatio(leftDb_);
    float rightRatio = dbToRatio(rightDb_);
    float leftPeakRatio = dbToRatio(leftPeakDb_);
    float rightPeakRatio = dbToRatio(rightPeakDb_);

    // Draw dashed style bars (segmented like stereo_volume_bar_widget)
    int tickWidth = 2;
    int tickGap = 1;
    int numTicks = barAreaW / (tickWidth + tickGap);
    
    // Helper to interpolate gradient color
    auto getGradientColor = [&grad](float pos) -> QColor {
        const auto& stops = grad.stops();
        for (int i = 1; i < stops.size(); ++i) {
            if (pos <= stops[i].first) {
                float t = (pos - stops[i-1].first) / (stops[i].first - stops[i-1].first);
                return QColor::fromRgbF(
                    stops[i-1].second.redF() * (1-t) + stops[i].second.redF() * t,
                    stops[i-1].second.greenF() * (1-t) + stops[i].second.greenF() * t,
                    stops[i-1].second.blueF() * (1-t) + stops[i].second.blueF() * t
                );
            }
        }
        return stops.back().second;
    };

    // Draw background for bar areas (darker)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#1e2128"));
    p.drawRect(barAreaX, topBarY, barAreaW, barHeight);
    p.drawRect(barAreaX, bottomBarY, barAreaW, barHeight);

    // Draw 10dB markers
    p.setPen(QColor("#3a3e48"));
    int dbRange = maxDb_ - minDb_;  // e.g., 0 - (-60) = 60
    for (int db = minDb_ + 10; db < maxDb_; db += 10) {
        float ratio = static_cast<float>(db - minDb_) / dbRange;
        int markerX = barAreaX + static_cast<int>(ratio * barAreaW);
        p.drawLine(markerX, topBarY, markerX, topBarY + barHeight);
        p.drawLine(markerX, bottomBarY, markerX, bottomBarY + barHeight);
    }

    // Draw Left channel bar (dashed/ticks)
    int leftFilledTicks = static_cast<int>(leftRatio * numTicks);
    for (int i = 0; i < leftFilledTicks && i < numTicks; ++i) {
        int tickX = barAreaX + i * (tickWidth + tickGap);
        float gradPos = static_cast<float>(i) / numTicks;
        QColor tickColor = getGradientColor(gradPos);
        p.setBrush(tickColor);
        p.drawRect(tickX, topBarY, tickWidth, barHeight);
    }

    // Draw Right channel bar (dashed/ticks)
    int rightFilledTicks = static_cast<int>(rightRatio * numTicks);
    for (int i = 0; i < rightFilledTicks && i < numTicks; ++i) {
        int tickX = barAreaX + i * (tickWidth + tickGap);
        float gradPos = static_cast<float>(i) / numTicks;
        QColor tickColor = getGradientColor(gradPos);
        p.setBrush(tickColor);
        p.drawRect(tickX, bottomBarY, tickWidth, barHeight);
    }

    // Draw peak indicators (white vertical lines)
    if (leftPeakRatio > 0.02f) {
        int peakX = barAreaX + static_cast<int>(leftPeakRatio * barAreaW) - 1;
        if (peakX > barAreaX) {
            p.setPen(QPen(QColor("#ffffff"), 1));
            p.drawLine(peakX, topBarY, peakX, topBarY + barHeight - 1);
        }
    }
    if (rightPeakRatio > 0.02f) {
        int peakX = barAreaX + static_cast<int>(rightPeakRatio * barAreaW) - 1;
        if (peakX > barAreaX) {
            p.setPen(QPen(QColor("#ffffff"), 1));
            p.drawLine(peakX, bottomBarY, peakX, bottomBarY + barHeight - 1);
        }
    }

    // Draw L/R labels on the left
    p.setPen(QColor("#888888"));
    QFont labelFont = font();
    if (h <= 24) {
        labelFont.setPointSize(7);
        labelFont.setBold(false);
    } else {
        labelFont.setPointSize(9);
        labelFont.setBold(true);
    }
    p.setFont(labelFont);
    p.drawText(0, topBarY, labelWidth - 1, barHeight, Qt::AlignVCenter | Qt::AlignRight, "L");
    p.drawText(0, bottomBarY, labelWidth - 1, barHeight, Qt::AlignVCenter | Qt::AlignRight, "R");
}
