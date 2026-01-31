#pragma once

#include <QWidget>
#include <QPushButton>
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

private slots:
    void updatePanData(const NN_PanData_t &data);
    void toggleEnabled();

private:
    void drawBackground(QPainter &p);
    void drawSemicircle(QPainter &p);
    void drawSegments(QPainter &p);
    void drawPulsingShape(QPainter &p);
    void drawLabels(QPainter &p);
    void drawPanIndicator(QPainter &p);
    void setupTitleWidget();

    // Data
    NoteNagaPanAnalyzer *m_panAnalyzer;
    NN_PanData_t m_currentData;
    std::vector<float> m_smoothedSegments;
    float m_smoothedPan = 0.0f;
    
    // Display options
    bool m_enabled = true;
    float m_smoothingFactor = 0.3f;

    // UI elements
    QWidget *m_titleWidget = nullptr;
    QPushButton *m_btnEnabled = nullptr;

    // Layout
    static constexpr int MARGIN = 10;
};
