#include "spectrum_analyzer.h"

#include <QColor>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QHBoxLayout>
#include <QContextMenuEvent>
#include <cmath>
#include <algorithm>
#include <vector>

#include "../nn_gui_utils.h"

SpectrumAnalyzer::SpectrumAnalyzer(NoteNagaSpectrumAnalyzer *spectrumAnalyzer, QWidget *parent)
    : QWidget(parent), m_spectrumAnalyzer(spectrumAnalyzer)
{
    setMinimumSize(300, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(m_spectrumAnalyzer, &NoteNagaSpectrumAnalyzer::spectrumUpdated, this,
            &SpectrumAnalyzer::updateSpectrum);

    setupTitleWidget();
    setupContextMenu();
}

void SpectrumAnalyzer::setupTitleWidget()
{
    m_titleWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(m_titleWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    
    m_btnPeakHold = create_small_button(":/icons/chart-line.svg", "Toggle Peak Hold", "btnPeakHold", 20);
    m_btnPeakHold->setCheckable(true);
    m_btnPeakHold->setChecked(m_showPeakHold);
    connect(m_btnPeakHold, &QPushButton::clicked, this, &SpectrumAnalyzer::togglePeakHold);
    
    m_btnFill = create_small_button(":/icons/chart-area.svg", "Toggle Fill Mode", "btnFill", 20);
    m_btnFill->setCheckable(true);
    m_btnFill->setChecked(m_fillMode);
    connect(m_btnFill, &QPushButton::clicked, this, &SpectrumAnalyzer::toggleFillMode);
    
    layout->addWidget(m_btnPeakHold);
    layout->addWidget(m_btnFill);
}

void SpectrumAnalyzer::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    QAction *peakHoldAction = m_contextMenu->addAction("Show Peak Hold");
    peakHoldAction->setCheckable(true);
    peakHoldAction->setChecked(m_showPeakHold);
    connect(peakHoldAction, &QAction::triggered, this, &SpectrumAnalyzer::togglePeakHold);
    
    QAction *fillAction = m_contextMenu->addAction("Fill Mode");
    fillAction->setCheckable(true);
    fillAction->setChecked(m_fillMode);
    connect(fillAction, &QAction::triggered, this, &SpectrumAnalyzer::toggleFillMode);
    
    m_contextMenu->addSeparator();
    
    QMenu *rangeMenu = m_contextMenu->addMenu("dB Range");
    QAction *range60 = rangeMenu->addAction("60 dB");
    QAction *range80 = rangeMenu->addAction("80 dB");
    QAction *range100 = rangeMenu->addAction("100 dB");
    range80->setCheckable(true);
    range80->setChecked(true);
    connect(range60, &QAction::triggered, this, &SpectrumAnalyzer::setDbRange60);
    connect(range80, &QAction::triggered, this, &SpectrumAnalyzer::setDbRange80);
    connect(range100, &QAction::triggered, this, &SpectrumAnalyzer::setDbRange100);
    
    m_contextMenu->addSeparator();
    
    QAction *resetAction = m_contextMenu->addAction("Reset Peaks");
    connect(resetAction, &QAction::triggered, this, &SpectrumAnalyzer::resetPeaks);
}

void SpectrumAnalyzer::contextMenuEvent(QContextMenuEvent *event)
{
    m_contextMenu->exec(event->globalPos());
}

void SpectrumAnalyzer::togglePeakHold()
{
    m_showPeakHold = !m_showPeakHold;
    m_btnPeakHold->setChecked(m_showPeakHold);
    update();
}

void SpectrumAnalyzer::toggleFillMode()
{
    m_fillMode = !m_fillMode;
    m_btnFill->setChecked(m_fillMode);
    m_cachedSpectrumPathValid = false;
    update();
}

void SpectrumAnalyzer::resetPeaks()
{
    std::fill(m_peakHoldVals.begin(), m_peakHoldVals.end(), 0.0f);
    m_cachedPeakPathValid = false;
    update();
}

void SpectrumAnalyzer::setDbRange60() { m_dbRange = 60; m_cachedSpectrumPathValid = false; m_cachedPeakPathValid = false; update(); }
void SpectrumAnalyzer::setDbRange80() { m_dbRange = 80; m_cachedSpectrumPathValid = false; m_cachedPeakPathValid = false; update(); }
void SpectrumAnalyzer::setDbRange100() { m_dbRange = 100; m_cachedSpectrumPathValid = false; m_cachedPeakPathValid = false; update(); }

void SpectrumAnalyzer::updateSpectrum(const std::vector<float> &spectrum)
{
    m_spectrum = spectrum;
    m_fftSize = int(m_spectrum.size() * 2);
    if (m_peakHoldVals.size() != m_spectrum.size()) {
        m_peakHoldVals.resize(m_spectrum.size(), 0.0f);
        m_peakHoldTimes.resize(m_spectrum.size(), 0);
    }
    updatePeakHold();
    m_cachedSpectrumPathValid = false;
    m_cachedPeakPathValid = false;
    update();
}

void SpectrumAnalyzer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    precomputeXPositions();
    m_cachedSpectrumPathValid = false;
    m_cachedPeakPathValid = false;
}

void SpectrumAnalyzer::precomputeXPositions()
{
    int plotWidth = width() - LEFT_MARGIN - RIGHT_MARGIN;
    m_precomputedX.resize(m_spectrum.size());
    for (int i = 0; i < int(m_spectrum.size()); ++i) {
        float freq = freqForBin(i);
        m_precomputedX[i] = LEFT_MARGIN + xForFreq(freq) * plotWidth;
    }
}

float SpectrumAnalyzer::xForFreq(float freq) const
{
    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    if (freq < minFreq) freq = minFreq;
    if (freq > maxFreq) freq = maxFreq;
    return (log10(freq) - log10(minFreq)) / (log10(maxFreq) - log10(minFreq));
}

void SpectrumAnalyzer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    drawBackground(p);
    drawGrid(p);
    drawCurve(p);
    if (m_showPeakHold) {
        drawPeakCurve(p);
    }
    drawFrequencyLabels(p);
    drawDbLabels(p);
}

void SpectrumAnalyzer::drawBackground(QPainter &p)
{
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(15, 15, 22));
    bg.setColorAt(1.0, QColor(8, 8, 14));
    p.fillRect(rect(), bg);
    
    int plotLeft = LEFT_MARGIN;
    int plotRight = width() - RIGHT_MARGIN;
    int plotTop = TOP_MARGIN;
    int plotBottom = height() - BOTTOM_MARGIN;
    
    p.setPen(QColor(40, 40, 50));
    p.drawRect(plotLeft, plotTop, plotRight - plotLeft, plotBottom - plotTop);
}

void SpectrumAnalyzer::drawGrid(QPainter &p)
{
    int plotLeft = LEFT_MARGIN;
    int plotRight = width() - RIGHT_MARGIN;
    int plotTop = TOP_MARGIN;
    int plotBottom = height() - BOTTOM_MARGIN;
    int plotWidth = plotRight - plotLeft;
    int plotHeight = plotBottom - plotTop;

    p.setPen(QPen(QColor(35, 35, 45), 1, Qt::DotLine));
    int numDbLines = m_dbRange / 10;
    for (int i = 1; i < numDbLines; ++i) {
        int y = plotTop + (i * plotHeight / numDbLines);
        p.drawLine(plotLeft, y, plotRight, y);
    }

    std::vector<float> majorFreqs = {100, 1000, 10000};
    for (float f : majorFreqs) {
        float xRatio = xForFreq(f);
        int x = plotLeft + int(xRatio * plotWidth);
        p.drawLine(x, plotTop, x, plotBottom);
    }
    
    p.setPen(QPen(QColor(25, 25, 35), 1, Qt::DotLine));
    std::vector<float> minorFreqs = {50, 200, 500, 2000, 5000};
    for (float f : minorFreqs) {
        float xRatio = xForFreq(f);
        int x = plotLeft + int(xRatio * plotWidth);
        p.drawLine(x, plotTop, x, plotBottom);
    }
}

void SpectrumAnalyzer::drawFrequencyLabels(QPainter &p)
{
    int plotLeft = LEFT_MARGIN;
    int plotRight = width() - RIGHT_MARGIN;
    int plotWidth = plotRight - plotLeft;
    int labelY = height() - 4;

    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    p.setPen(QColor(130, 130, 160));
    
    struct FreqLabel { float freq; QString text; };
    std::vector<FreqLabel> labels = {
        {20, "20"}, {50, "50"}, {100, "100"}, {200, "200"}, {500, "500"},
        {1000, "1k"}, {2000, "2k"}, {5000, "5k"}, {10000, "10k"}, {20000, "20k"}
    };
    
    float lastLabelRight = -100;
    for (const auto &lbl : labels) {
        float xRatio = xForFreq(lbl.freq);
        int x = plotLeft + int(xRatio * plotWidth);
        
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(lbl.text);
        int textLeft = x - textWidth / 2;
        
        if (textLeft > lastLabelRight + 5 && textLeft >= plotLeft - 10 && x + textWidth/2 <= plotRight + 10) {
            p.drawText(textLeft, labelY, lbl.text);
            lastLabelRight = textLeft + textWidth;
        }
    }
}

void SpectrumAnalyzer::drawDbLabels(QPainter &p)
{
    int plotLeft = LEFT_MARGIN;
    int plotTop = TOP_MARGIN;
    int plotBottom = height() - BOTTOM_MARGIN;
    int plotHeight = plotBottom - plotTop;

    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    p.setPen(QColor(130, 130, 160));
    
    int numLabels = m_dbRange / 20;
    for (int i = 0; i <= numLabels; ++i) {
        int db = -i * 20;
        int y = plotTop + (i * plotHeight / numLabels);
        QString text = QString::number(db);
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(text);
        p.drawText(plotLeft - textWidth - 5, y + 4, text);
    }
    
    p.save();
    p.translate(10, plotTop + plotHeight / 2);
    p.rotate(-90);
    p.drawText(-10, 0, "dB");
    p.restore();
}

void SpectrumAnalyzer::drawCurve(QPainter &p)
{
    if (m_spectrum.empty()) return;
    
    int plotLeft = LEFT_MARGIN;
    int plotRight = width() - RIGHT_MARGIN;
    int plotTop = TOP_MARGIN;
    int plotBottom = height() - BOTTOM_MARGIN;
    int plotWidth = plotRight - plotLeft;
    int plotHeight = plotBottom - plotTop;

    if (!m_cachedSpectrumPathValid) {
        m_cachedSpectrumPath = QPainterPath();
        
        // 1/3 octave center frequencies (ISO standard) - logarithmically uniform
        static const std::vector<float> bandFreqs = {
            20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500,
            630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000,
            10000, 12500, 16000, 20000
        };
        
        const float minShowValue = 0.0000001f;
        
        // Calculate values for each band
        std::vector<QPointF> points;
        points.reserve(bandFreqs.size());
        
        for (size_t i = 0; i < bandFreqs.size(); ++i) {
            float centerFreq = bandFreqs[i];
            
            // Calculate band edges (geometric mean between adjacent centers)
            float lowFreq = (i > 0) ? sqrt(bandFreqs[i-1] * centerFreq) : centerFreq * 0.891f;
            float highFreq = (i < bandFreqs.size()-1) ? sqrt(centerFreq * bandFreqs[i+1]) : centerFreq * 1.122f;
            
            int lowBin = binForFreq(lowFreq);
            int highBin = binForFreq(highFreq);
            lowBin = std::clamp(lowBin, 0, int(m_spectrum.size())-1);
            highBin = std::clamp(highBin, 0, int(m_spectrum.size())-1);
            
            // RMS average of bins in this band (more accurate for audio)
            float sumSq = 0.0f;
            int count = 0;
            for (int b = lowBin; b <= highBin; ++b) {
                sumSq += m_spectrum[b] * m_spectrum[b];
                count++;
            }
            float value = count > 0 ? sqrt(sumSq / count) : 0.0f;
            
            float x = plotLeft + xForFreq(centerFreq) * plotWidth;
            value = std::clamp(value, 0.0f, 1.0f);
            float db = value > minShowValue ? 20.0f * log10(value) : -float(m_dbRange);
            db = std::clamp(db, -float(m_dbRange), 0.0f);
            float y = plotTop + (-db / float(m_dbRange)) * plotHeight;
            
            points.push_back(QPointF(x, y));
        }
        
        // Draw smooth curve using Catmull-Rom spline
        if (points.size() >= 2) {
            m_cachedSpectrumPath.moveTo(points[0]);
            
            for (size_t i = 0; i < points.size() - 1; ++i) {
                QPointF p0 = (i > 0) ? points[i-1] : points[i];
                QPointF p1 = points[i];
                QPointF p2 = points[i+1];
                QPointF p3 = (i+2 < points.size()) ? points[i+2] : points[i+1];
                
                // Catmull-Rom to Bezier conversion
                QPointF cp1(p1.x() + (p2.x() - p0.x()) / 6.0,
                           p1.y() + (p2.y() - p0.y()) / 6.0);
                QPointF cp2(p2.x() - (p3.x() - p1.x()) / 6.0,
                           p2.y() - (p3.y() - p1.y()) / 6.0);
                
                m_cachedSpectrumPath.cubicTo(cp1, cp2, p2);
            }
        }
        
        if (m_fillMode && points.size() >= 2) {
            m_cachedSpectrumPath.lineTo(plotRight, plotBottom);
            m_cachedSpectrumPath.lineTo(plotLeft, plotBottom);
            m_cachedSpectrumPath.closeSubpath();
        }
        
        m_cachedSpectrumPathValid = true;
    }

    QLinearGradient grad(0, plotTop, 0, plotBottom);
    grad.setColorAt(0.0, QColor(80, 200, 255));
    grad.setColorAt(0.5, QColor(60, 180, 120));
    grad.setColorAt(1.0, QColor(40, 100, 80));
    
    if (m_fillMode) {
        QLinearGradient fillGrad(0, plotTop, 0, plotBottom);
        fillGrad.setColorAt(0.0, QColor(80, 200, 255, 120));
        fillGrad.setColorAt(0.5, QColor(60, 180, 120, 80));
        fillGrad.setColorAt(1.0, QColor(40, 100, 80, 40));
        p.setBrush(fillGrad);
        p.setPen(Qt::NoPen);
        p.drawPath(m_cachedSpectrumPath);
        
        p.setPen(QPen(QBrush(grad), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(m_cachedSpectrumPath);
    } else {
        p.setPen(QPen(QBrush(grad), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(m_cachedSpectrumPath);
    }
}

void SpectrumAnalyzer::drawPeakCurve(QPainter &p)
{
    if (m_peakHoldVals.empty()) return;
    
    int plotLeft = LEFT_MARGIN;
    int plotRight = width() - RIGHT_MARGIN;
    int plotTop = TOP_MARGIN;
    int plotBottom = height() - BOTTOM_MARGIN;
    int plotWidth = plotRight - plotLeft;
    int plotHeight = plotBottom - plotTop;

    if (!m_cachedPeakPathValid) {
        m_cachedPeakPath = QPainterPath();
        
        // Same 1/3 octave bands as main curve
        static const std::vector<float> bandFreqs = {
            20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500,
            630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000,
            10000, 12500, 16000, 20000
        };
        
        const float minShowValue = 0.0000001f;
        std::vector<QPointF> points;
        points.reserve(bandFreqs.size());
        
        for (size_t i = 0; i < bandFreqs.size(); ++i) {
            float centerFreq = bandFreqs[i];
            float lowFreq = (i > 0) ? sqrt(bandFreqs[i-1] * centerFreq) : centerFreq * 0.891f;
            float highFreq = (i < bandFreqs.size()-1) ? sqrt(centerFreq * bandFreqs[i+1]) : centerFreq * 1.122f;
            
            int lowBin = std::clamp(binForFreq(lowFreq), 0, int(m_peakHoldVals.size())-1);
            int highBin = std::clamp(binForFreq(highFreq), 0, int(m_peakHoldVals.size())-1);
            
            float maxVal = 0.0f;
            for (int b = lowBin; b <= highBin; ++b) {
                maxVal = std::max(maxVal, m_peakHoldVals[b]);
            }
            
            float x = plotLeft + xForFreq(centerFreq) * plotWidth;
            float value = std::clamp(maxVal, 0.0f, 1.0f);
            float db = value > minShowValue ? 20.0f * log10(value) : -float(m_dbRange);
            db = std::clamp(db, -float(m_dbRange), 0.0f);
            float y = plotTop + (-db / float(m_dbRange)) * plotHeight;
            
            points.push_back(QPointF(x, y));
        }
        
        // Smooth curve using Catmull-Rom spline
        if (points.size() >= 2) {
            m_cachedPeakPath.moveTo(points[0]);
            for (size_t i = 0; i < points.size() - 1; ++i) {
                QPointF p0 = (i > 0) ? points[i-1] : points[i];
                QPointF p1 = points[i];
                QPointF p2 = points[i+1];
                QPointF p3 = (i+2 < points.size()) ? points[i+2] : points[i+1];
                
                QPointF cp1(p1.x() + (p2.x() - p0.x()) / 6.0,
                           p1.y() + (p2.y() - p0.y()) / 6.0);
                QPointF cp2(p2.x() - (p3.x() - p1.x()) / 6.0,
                           p2.y() - (p3.y() - p1.y()) / 6.0);
                
                m_cachedPeakPath.cubicTo(cp1, cp2, p2);
            }
        }
        m_cachedPeakPathValid = true;
    }

    p.setPen(QPen(QColor(255, 180, 50, 180), 1.5, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(m_cachedPeakPath);
}

void SpectrumAnalyzer::updatePeakHold()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool spectrumAllZero = std::all_of(m_spectrum.begin(), m_spectrum.end(), 
        [](float v) { return v < 0.00001f; });
    
    for (int i = 0; i < int(m_spectrum.size()); ++i) {
        float v = m_spectrum[i];
        if (spectrumAllZero) {
            if (now - m_peakHoldTimes[i] > 500) {
                m_peakHoldVals[i] *= 0.95f;
            }
        } else if (v > m_peakHoldVals[i]) {
            m_peakHoldVals[i] = v;
            m_peakHoldTimes[i] = now;
        } else if (now - m_peakHoldTimes[i] > m_peakHoldTimeMs) {
            m_peakHoldVals[i] = std::max(v, m_peakHoldVals[i] * 0.97f);
            m_peakHoldTimes[i] = now;
        }
    }
}

float SpectrumAnalyzer::freqForBin(int bin) const
{
    return (float(bin) * m_sampleRate) / float(m_fftSize);
}

int SpectrumAnalyzer::binForFreq(float freq) const
{
    return int((freq * m_fftSize) / m_sampleRate);
}
