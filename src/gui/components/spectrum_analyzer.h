#pragma once

#include <QWidget>
#include <vector>
#include <note_naga_engine/module/spectrum_analyzer.h>

/**
 * SpectrumAnalyzer
 * 
 * - Dark background
 * - Colored, smooth curve
 * - Frequency axis (log), ticks, labels
 * - Peak-hold curve (not dots)
 * - Fixed size (not overlapping)
 */
class SpectrumAnalyzer : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumAnalyzer(NoteNagaSpectrumAnalyzer* spectrumAnalyzer, QWidget* parent = nullptr);

    QSize minimumSizeHint() const override { return QSize(420, 180); }
    QSize sizeHint() const override { return QSize(420, 180); }

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void updateSpectrum(const std::vector<float> &spectrum);

private:
    NoteNagaSpectrumAnalyzer* spectrumAnalyzer_;
    std::vector<float> spectrum_; // 0..1, linear
    int fftSize_ = 1024;
    int sampleRate_ = 44100;

    // Holded peak values
    std::vector<float> peakHoldVals_;
    std::vector<qint64> peakHoldTimes_; // ms

    void updatePeakHold();

    // Helper drawing
    void drawGrid(QPainter& p);
    void drawCurve(QPainter& p);
    void drawPeakCurve(QPainter& p);

    // Axis helpers
    float freqForBin(int bin) const;
    int binForFreq(float freq) const;
};