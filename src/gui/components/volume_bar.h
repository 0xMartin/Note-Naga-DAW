#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QTimer>
#include <QWidget>
#include <vector>

/**
 * @brief VolumeBar widget for displaying and controlling volume levels.
 */
class VolumeBar : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a VolumeBar widget.
     * @param value Initial volume value.
     * @param start_color Color for the start of the volume bar.
     * @param end_color Color for the end of the volume bar.
     * @param dynamic_mode If true, enables dynamic volume animation.
     * @param parent Parent widget.
     */
    explicit VolumeBar(float value = 0.0f, const QString &start_color = "#00ff00",
                       const QString &end_color = "#ff0000", bool dynamic_mode = true,
                       QWidget *parent = nullptr);

    /**
     * @brief Sets the current volume value.
     * @param value New volume value.
     * @param time_ms Animation duration in milliseconds. If negative, no animation.
     */
    void setValue(float value, int time_ms = -1);

    /**
     * @brief Sets the range of volume values.
     * @param min_value Minimum volume value.
     * @param max_value Maximum volume value.
     */
    void setRange(float min_value, float max_value);

    /**
     * @brief Sets custom labels for the volume bar.
     * @param labels Vector of three QStrings for min, mid, and max labels.
     */
    void setLabels(const std::vector<QString> &labels);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onAnimTick();

private:
    QColor start_color;
    QColor end_color;
    bool dynamic_mode;

    float min_value;
    float max_value;
    int bar_height;
    std::vector<QString> labels;

    // Animation
    float current_value;
    float target_value;
    float initial_decay_value;
    int decay_time;
    int min_decay_time;
    QTimer *timer;
    QElapsedTimer anim_elapsed;
    bool anim_active;
    float decay_steepness;

    /**
     * @brief Exponential decay function for animation.
     * @param progress Normalized progress value (0.0 to 1.0).
     * @param steepness Decay steepness factor.
     * @return Decayed value.
     */
    static float exponential_decay(float progress, float steepness = 4.0f);
};