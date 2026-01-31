#pragma once

#include <QWidget>
#include <QPainterPath>
#include <QMenu>
#include <QActionGroup>
#include <QPushButton>
#include <QElapsedTimer>
#include <vector>

#include <note_naga_engine/module/spectrum_analyzer.h>

class NoteNagaSpectrumAnalyzer;

/**
 * @brief SpectrumAnalyzer provides a real-time frequency spectrum visualization
 *        with peak hold, logarithmic frequency scale, and customizable display options.
 */
class SpectrumAnalyzer : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumAnalyzer(NoteNagaSpectrumAnalyzer *spectrumAnalyzer, QWidget *parent = nullptr);

    /**
     * @brief Get title widget with controls for dock title bar
     */
    QWidget *getTitleWidget() const { return m_titleWidget; }

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void updateSpectrum(const std::vector<float> &spectrum);
    void togglePeakHold();
    void toggleFillMode();
    void toggleEnabled();
    void resetPeaks();
    void setDbRange60();
    void setDbRange80();
    void setDbRange100();
    void setRefreshRate(int divisor);

private:
    void drawBackground(QPainter &p);
    void drawGrid(QPainter &p);
    void drawCurve(QPainter &p);
    void drawPeakCurve(QPainter &p);
    void drawFrequencyLabels(QPainter &p);
    void drawDbLabels(QPainter &p);
    void drawRenderTime(QPainter &p);
    void updatePeakHold();
    void setupTitleWidget();
    void setupContextMenu();

    // Optimizations
    void precomputeXPositions();
    float xForFreq(float freq) const;

    // Data
    NoteNagaSpectrumAnalyzer *m_spectrumAnalyzer;
    std::vector<float> m_spectrum;
    int m_fftSize = 0;
    std::vector<float> m_peakHoldVals;
    std::vector<qint64> m_peakHoldTimes;
    float m_sampleRate = 44100.0f;

    // Display options
    bool m_enabled = true;
    bool m_showPeakHold = true;
    bool m_fillMode = true;
    bool m_showRenderTime = true;
    int m_dbRange = 80;  // 60, 80, or 100 dB range
    int m_peakHoldTimeMs = 3000;

    // UI elements
    QWidget *m_titleWidget = nullptr;
    QPushButton *m_btnEnabled = nullptr;
    QPushButton *m_btnPeakHold = nullptr;
    QPushButton *m_btnFill = nullptr;
    QMenu *m_contextMenu = nullptr;
    QActionGroup *m_refreshRateGroup = nullptr;

    // Refresh rate and render time metrics
    int m_refreshDivisor = 1;  // 1 = full rate, 2 = half, 4 = quarter
    int m_updateCounter = 0;
    qint64 m_lastFrameTimeNs = 0;      // Last single frame render time in ns
    float m_avgFrameTimeNs = 0.0f;     // Average single frame time in ns
    float m_totalRenderTimeMs = 0.0f;  // Total render time per second in ms
    qint64 m_lastStatsUpdate = 0;
    double m_renderTimeAccum = 0.0;
    int m_renderTimeCount = 0;
    int m_targetFps = 60;              // Target FPS for total calculation

    // Optimized cache
    std::vector<float> m_precomputedX;
    QPainterPath m_cachedSpectrumPath;
    QPainterPath m_cachedPeakPath;
    bool m_cachedSpectrumPathValid = false;
    bool m_cachedPeakPathValid = false;

    // Layout margins
    static constexpr int LEFT_MARGIN = 45;
    static constexpr int RIGHT_MARGIN = 10;
    static constexpr int TOP_MARGIN = 5;
    static constexpr int BOTTOM_MARGIN = 20;

    // Frequency helpers
    float freqForBin(int bin) const;
    int binForFreq(float freq) const;
};
