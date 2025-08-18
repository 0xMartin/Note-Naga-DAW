#include "spectrum_analyzer.h"

#include <QColor>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <algorithm>
#include <vector>

SpectrumAnalyzer::SpectrumAnalyzer(NoteNagaSpectrumAnalyzer *spectrumAnalyzer, QWidget *parent)
    : QWidget(parent), spectrumAnalyzer_(spectrumAnalyzer) {
    setMinimumSize(420, 180);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(spectrumAnalyzer_, &NoteNagaSpectrumAnalyzer::spectrumUpdated, this,
            &SpectrumAnalyzer::updateSpectrum);

    precomputedX_.resize(0);
    cachedSpectrumPathValid_ = false;
    cachedPeakPathValid_ = false;
}

void SpectrumAnalyzer::updateSpectrum(const std::vector<float> &spectrum) {
    spectrum_ = spectrum;
    fftSize_ = int(spectrum_.size() * 2);
    if (peakHoldVals_.size() != spectrum_.size()) {
        peakHoldVals_.resize(spectrum_.size(), 0.0f);
        peakHoldTimes_.resize(spectrum_.size(), 0);
    }
    updatePeakHold();
    cachedSpectrumPathValid_ = false;
    cachedPeakPathValid_ = false;
    update();
}

void SpectrumAnalyzer::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    precomputeXPositions();
    cachedSpectrumPathValid_ = false;
    cachedPeakPathValid_ = false;
}

void SpectrumAnalyzer::precomputeXPositions() {
    int w = width();
    precomputedX_.resize(spectrum_.size());
    for (int i = 0; i < int(spectrum_.size()); ++i) {
        float freq = freqForBin(i);
        // Precompute log10 scaling
        precomputedX_[i] = log10(freq / 20.0f + 1) / log10(20000.0f / 20.0f + 1) * w;
    }
}

void SpectrumAnalyzer::paintEvent(QPaintEvent *) {
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    // Background
    p.fillRect(rect(), QColor(10, 10, 18));

    // Draw grid, axes, ticks
    drawGrid(p);

    // Draw main spectrum curve
    drawCurve(p);

    // Draw peak hold curve
    drawPeakCurve(p);

    qint64 endTime = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "paintEvent duration:" << (endTime - startTime) << "ms";
}

void SpectrumAnalyzer::drawGrid(QPainter &p) {
    int w = width();
    int h = height();

    p.setPen(QColor(54, 54, 64));
    // Horizontal grid lines (dB scale)
    for (int i = 0; i <= 5; ++i) {
        int y = int(h * i / 5.0);
        p.drawLine(0, y, w, y);
        p.setPen(QColor(140, 140, 180));
        p.drawText(4, y - 2, QString("%1 dB").arg(0 - i * 20));
        p.setPen(QColor(54, 54, 64));
    }

    // Vertical ticks (frequency, log scale)
    QFont font = p.font();
    font.setPointSize(9);
    p.setFont(font);

    std::vector<float> freqs = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    float lastLabelX = -100;
    int labelMinSpacing = std::max(40, width() / 30); // Dynamicky podle velikosti widgetu
    for (float f : freqs) {
        int bin = binForFreq(f);
        float x = float(bin) / (fftSize_ / 2) * w;
        p.setPen(QColor(54, 54, 74));
        p.drawLine(int(x), 0, int(x), h);
        // Only draw label if far enough from previous
        if (x - lastLabelX > labelMinSpacing) {
            p.setPen(QColor(140, 140, 180));
            p.drawText(int(x) + 2, h - 6, QString("%1").arg(int(f)));
            lastLabelX = x;
        }
        p.setPen(QColor(54, 54, 74));
    }

    // Axis labels
    p.setPen(QColor(160, 160, 210));
    p.drawText(w - 60, h - 6, "Hz");
    p.drawText(4, 14, "Spectrum");
}

void SpectrumAnalyzer::drawCurve(QPainter &p) {
    if (spectrum_.empty()) return;
    int w = width();
    int h = height();

    if (!cachedSpectrumPathValid_) {
        cachedSpectrumPath_ = QPainterPath();
        bool first = true;
        const float minShowValue = 0.01f; // -40dB threshold
        for (int i = 0; i < int(spectrum_.size()); ++i) {
            float x = (precomputedX_.size() == spectrum_.size()) ? precomputedX_[i] : 
                (log10(freqForBin(i) / 20.0f + 1) / log10(20000.0f / 20.0f + 1) * w);
            float value = std::clamp(spectrum_[i], 0.0f, 1.0f);
            float db = value > minShowValue ? 20.0f * log10(value) : -100.0f;
            float y = h - ((db + 100.0f) / 100.0f * h);
            if (first) {
                cachedSpectrumPath_.moveTo(x, y);
                first = false;
            } else {
                cachedSpectrumPath_.lineTo(x, y);
            }
        }
        cachedSpectrumPathValid_ = true;
    }

    // Color gradient for main curve
    QLinearGradient grad(0, 0, w, 0);
    grad.setColorAt(0.0, QColor(60, 170, 255));
    grad.setColorAt(0.5, QColor(120, 255, 150));
    grad.setColorAt(1.0, QColor(255, 90, 90));
    p.setPen(QPen(QBrush(grad), 2));
    p.drawPath(cachedSpectrumPath_);
}

void SpectrumAnalyzer::drawPeakCurve(QPainter &p) {
    if (peakHoldVals_.empty()) return;
    int w = width();
    int h = height();

    if (!cachedPeakPathValid_) {
        cachedPeakPath_ = QPainterPath();
        bool first = true;
        const float minShowValue = 0.01f; // -40dB threshold
        for (int i = 0; i < int(peakHoldVals_.size()); ++i) {
            float x = (precomputedX_.size() == peakHoldVals_.size()) ? precomputedX_[i] :
                (log10(freqForBin(i) / 20.0f + 1) / log10(20000.0f / 20.0f + 1) * w);
            float value = std::clamp(peakHoldVals_[i], 0.0f, 1.0f);
            float db = value > minShowValue ? 20.0f * log10(value) : -100.0f;
            float y = h - ((db + 100.0f) / 100.0f * h);
            if (first) {
                cachedPeakPath_.moveTo(x, y);
                first = false;
            } else {
                cachedPeakPath_.lineTo(x, y);
            }
        }
        cachedPeakPathValid_ = true;
    }

    // Color gradient for peak hold curve (yellow-orange)
    QLinearGradient grad(0, 0, w, 0);
    grad.setColorAt(0.0, QColor(255, 210, 40));
    grad.setColorAt(1.0, QColor(255, 110, 20));
    p.setPen(QPen(QBrush(grad), 2, Qt::DashLine));
    p.drawPath(cachedPeakPath_);
}

void SpectrumAnalyzer::updatePeakHold() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool spectrumAllZero = std::all_of(spectrum_.begin(), spectrum_.end(), [](float v){ return v < 0.00001f; }); // Potlač šum
    for (int i = 0; i < int(spectrum_.size()); ++i) {
        float v = spectrum_[i];
        if (spectrumAllZero) {
            peakHoldVals_[i] = 0.0f;
            peakHoldTimes_[i] = now;
        } else if (v > peakHoldVals_[i]) {
            peakHoldVals_[i] = v;
            peakHoldTimes_[i] = now;
        } else if (now - peakHoldTimes_[i] > 5000) {
            // Drop after hold time
            peakHoldVals_[i] = v;
            peakHoldTimes_[i] = now;
        }
    }
}

// Frequency helpers
float SpectrumAnalyzer::freqForBin(int bin) const {
    return (float(bin) * sampleRate_) / float(fftSize_);
}
int SpectrumAnalyzer::binForFreq(float freq) const { return int((freq * fftSize_) / sampleRate_); }
