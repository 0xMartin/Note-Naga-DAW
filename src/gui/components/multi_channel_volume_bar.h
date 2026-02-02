#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QMenu>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <vector>

/**
 * @brief Multi-channel volume bar widget for displaying audio levels.
 *
 * Professional FL Studio-style meter with:
 * - LED segmented display
 * - Peak hold indicators
 * - Clip indicators
 * - Context menu with options
 * - Dynamic animations
 */
class MultiChannelVolumeBar : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a MultiChannelVolumeBar widget.
     * @param channels Number of audio channels.
     * @param start_color Color for the start of the volume bar.
     * @param end_color Color for the end of the volume bar.
     * @param dynamic_mode If true, enables dynamic volume animation.
     * @param parent Parent widget.
     */
    explicit MultiChannelVolumeBar(int channels = 16,
                                   const QString &start_color = "#00ff00",
                                   const QString &end_color = "#ff0000",
                                   bool dynamic_mode = true, QWidget *parent = nullptr);

    /**
     * @brief Sets the number of audio channels.
     * @param channels Number of channels to set.
     */
    void setChannelCount(int channels);

    /**
     * @brief Gets the current number of audio channels.
     * @return Number of channels.
     */
    int getChannelCount() const;

    /**
     * @brief Sets the volume level for a specific channel.
     * @param channel_idx Index of the channel to set.
     * @param value New volume value.
     * @param time_ms Animation duration in milliseconds. If negative, no animation.
     */
    void setValue(int channel_idx, float value, int time_ms = -1);

    /**
     * @brief Sets the range of volume values for all channels.
     * @param min_value Minimum volume value.
     * @param max_value Maximum volume value.
     */
    void setRange(float min_value, float max_value);

    /**
     * @brief Sets custom labels for the volume bar.
     * @param labels Vector of QStrings for channel labels.
     */
    void setLabels(const std::vector<QString> &labels);
    
    /**
     * @brief Resets all peak hold indicators.
     */
    void resetPeaks();
    
    /**
     * @brief Resets clip indicators for all channels.
     */
    void resetClips();
    
    /**
     * @brief Sets whether to show LED segments or solid bars.
     */
    void setLedMode(bool enabled);
    
    /**
     * @brief Sets whether to show peak hold indicators.
     */
    void setPeakHoldEnabled(bool enabled);

signals:
    /**
     * @brief Emitted when a channel is clicked.
     * @param channel_idx Index of the clicked channel.
     */
    void channelClicked(int channel_idx);
    
    /**
     * @brief Emitted when a channel is double-clicked (solo).
     * @param channel_idx Index of the double-clicked channel.
     */
    void channelSoloed(int channel_idx);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onAnimTick();
    void onPeakDecayTick();

private:
    int channels;
    QColor start_color;
    QColor end_color;
    bool dynamic_mode;

    float min_value;
    float max_value;
    int bar_width_min;
    int bar_width_max;
    int bar_space_min;
    int bar_space_max;
    int bar_bottom_margin;
    int bar_top_margin;
    std::vector<QString> labels;

    // Animation
    std::vector<float> current_values;
    std::vector<float> initial_decay_values;
    std::vector<int> decay_times;
    std::vector<QElapsedTimer *> anim_elapsed;
    std::vector<bool> anim_active;
    float decay_steepness;
    std::vector<float> target_values;
    QTimer *timer;
    
    // Peak hold
    std::vector<float> peak_values;
    std::vector<QElapsedTimer *> peak_timers;
    QTimer *peak_decay_timer;
    bool peak_hold_enabled;
    int peak_hold_time_ms;  // How long to hold peak before decay
    
    // Clip indicators
    std::vector<bool> clip_indicators;
    float clip_threshold;
    
    // LED mode
    bool led_mode;
    int led_segment_count;
    int led_gap;
    
    // Hover state
    int hovered_channel;
    
    // Helper methods
    int getChannelAtPos(const QPoint &pos);
    void drawLedSegments(QPainter &painter, int x, int y, int width, int height, float value);
    void drawSolidBar(QPainter &painter, int x, int y, int width, int height, float value);
    void drawPeakIndicator(QPainter &painter, int x, int y, int width, int bar_area_height, float peak);
    void drawClipIndicator(QPainter &painter, int x, int y, int width, bool clipped);
    QColor getColorForLevel(float normalized_value);

    /**
     * @brief Exponential decay function for animation.
     * @param progress Normalized progress value (0.0 to 1.0).
     * @param steepness Decay steepness factor.
     * @return Decayed value.
     */
    static float exponential_decay(float progress, float steepness = 4.0f);
};