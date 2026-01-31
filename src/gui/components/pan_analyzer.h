#pragma once

#include <QWidget>
#include <QPushButton>
#include <QMenu>
#include <QActionGroup>
#include <vector>

#include <note_naga_engine/module/pan_analyzer.h>

/**
 * @brief PanAnalyzer provides a real-time stereo pan visualization
 *        with a semicircular display showing left/right balance.
 */
class PanAnalyzer : public QWidget {
    Q_OBJECT
public:
    explicit PanAnalyzer(NoteNagaPanAnalyzer *panAnalyzer, QWidget *parent = nullptr);

    /**
     * @brief Get title widget with controls for dock title bar
     */
    QWidget *getTitleWidget() const { return m_titleWidget; }

    /**
     * @brief Enable or disable the analyzer
     */
    void setEnabled(bool enabled);
    bool isAnalyzerEnabled() const { return m_enabled; }

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void updatePanData(const NN_PanData_t &data);
    void toggleEnabled();
    void setRefreshRate(int divisor);

private:
    void drawBackground(QPainter &p);
    void drawSemicircle(QPainter &p);
    void drawSegments(QPainter &p);
    void drawPulsingShape(QPainter &p);
    void drawLabels(QPainter &p);
    void drawPanIndicator(QPainter &p);
    void drawRenderTime(QPainter &p);
    void setupTitleWidget();
    void setupContextMenu();

    // Data
    NoteNagaPanAnalyzer *m_panAnalyzer;
    NN_PanData_t m_currentData;
    std::vector<float> m_smoothedSegments;
    float m_smoothedPan = 0.0f;
    
    // Display options
    bool m_enabled = true;
    float m_smoothingFactor = 0.3f;
    
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
    
    // Extended display options
    bool m_showRenderTime = true;
    float m_pulseIntensity = 1.0f;  // 0.5 = subtle, 1.0 = normal, 1.5 = intense

    // UI elements
    QWidget *m_titleWidget = nullptr;
    QPushButton *m_btnEnabled = nullptr;
    QMenu *m_contextMenu = nullptr;
    QActionGroup *m_refreshRateGroup = nullptr;

    // Layout
    static constexpr int MARGIN = 10;
};
