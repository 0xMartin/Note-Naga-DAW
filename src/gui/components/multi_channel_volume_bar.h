#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <vector>

/**
 * @brief Multi-channel volume bar widget for displaying audio levels.
 *
 * This widget displays multiple channels of audio levels with customizable colors,
 * dynamic animations, and labels.
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

  protected:
    void paintEvent(QPaintEvent *event) override;

  private slots:
    void onAnimTick();

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

    /**
     * @brief Exponential decay function for animation.
     * @param progress Normalized progress value (0.0 to 1.0).
     * @param steepness Decay steepness factor.
     * @return Decayed value.
     */
    static float exponential_decay(float progress, float steepness = 4.0f);
};