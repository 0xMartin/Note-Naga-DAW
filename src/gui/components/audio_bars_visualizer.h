#pragma once

#include <QWidget>
#include <QTimer>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>

/**
 * @brief AudioBarsVisualizer is a simple audio visualization widget
 *        that displays animated vertical bars pulsing to music.
 *        This is a decorative visualizer for audio-only export mode.
 */
class AudioBarsVisualizer : public QWidget
{
    Q_OBJECT

public:
    explicit AudioBarsVisualizer(NoteNagaEngine *engine = nullptr, QWidget *parent = nullptr);
    ~AudioBarsVisualizer();

    /**
     * @brief Set the engine for RMS data
     */
    void setEngine(NoteNagaEngine *engine);

    /**
     * @brief Start the animation
     */
    void start();

    /**
     * @brief Stop the animation
     */
    void stop();

    /**
     * @brief Set the number of bars
     */
    void setBarCount(int count);

    /**
     * @brief Feed audio levels to the visualizer
     * @param levels Vector of levels (0.0 - 1.0) for each bar
     */
    void setLevels(const std::vector<float> &levels);
    
    /**
     * @brief Set stereo RMS levels (dB)
     */
    void setVolumesDb(float leftDb, float rightDb);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateAnimation();

private:
    NoteNagaEngine *m_engine;
    QTimer *m_animationTimer;
    int m_barCount;
    std::vector<float> m_currentLevels;
    std::vector<float> m_targetLevels;
    std::vector<float> m_velocities;
    std::vector<float> m_peakLevels;      // Peak hold for each bar
    std::vector<float> m_peakDecay;       // Peak decay velocity
    std::vector<float> m_hueOffsets;      // Per-bar hue variation
    bool m_isPlaying;
    float m_leftLevel;
    float m_rightLevel;
    
    // Animation parameters
    float m_smoothingFactor;
    float m_decayRate;
    float m_randomFactor;
    float m_time;                          // Animation time for wave effects
    float m_pulsePhase;                    // Global pulse phase
    
    // Colors
    QColor m_barColor;
    QColor m_barColorBright;
    QColor m_backgroundColor;
};
