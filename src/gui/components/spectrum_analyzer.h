#pragma once

#include <QWidget>
#include <QPainterPath>
#include <vector>

#include <note_naga_engine/module/spectrum_analyzer.h>

class NoteNagaSpectrumAnalyzer;

class SpectrumAnalyzer : public QWidget {
    Q_OBJECT
public:
    SpectrumAnalyzer(NoteNagaSpectrumAnalyzer *spectrumAnalyzer, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateSpectrum(const std::vector<float> &spectrum);

private:
    void drawGrid(QPainter &p);
    void drawCurve(QPainter &p);
    void drawPeakCurve(QPainter &p);
    void updatePeakHold();

    // Optimalizace
    void precomputeXPositions();

    // Data
    NoteNagaSpectrumAnalyzer *spectrumAnalyzer_;
    std::vector<float> spectrum_;
    int fftSize_ = 0;
    std::vector<float> peakHoldVals_;
    std::vector<qint64> peakHoldTimes_;
    float sampleRate_ = 44100.0f; // nebo nastav podle projektu

    // Optimalizované cache
    std::vector<float> precomputedX_;
    QPainterPath cachedSpectrumPath_;
    QPainterPath cachedPeakPath_;
    bool cachedSpectrumPathValid_ = false;
    bool cachedPeakPathValid_ = false;

    // Frequency helpers
    float freqForBin(int bin) const;
    int binForFreq(float freq) const;
};
