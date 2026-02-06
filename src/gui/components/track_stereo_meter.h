#pragma once

#include <QWidget>
#include <QElapsedTimer>

/**
 * @brief TrackStereoMeter displays a compact horizontal stereo level meter.
 * Shows left and right channel levels in dB with peak hold indicators.
 * Designed for use in track widgets to display per-track output levels.
 */
class TrackStereoMeter : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a TrackStereoMeter widget.
     * @param parent Parent widget.
     * @param minDb Minimum dB value (default -70).
     * @param maxDb Maximum dB value (default 0).
     */
    explicit TrackStereoMeter(QWidget* parent = nullptr, int minDb = -70, int maxDb = 0);

    /**
     * @brief Sets the dB range for the meter.
     * @param minDb Minimum dB value.
     * @param maxDb Maximum dB value.
     */
    void setDbRange(int minDb, int maxDb);

    /**
     * @brief Sets whether the meter is active and should update.
     * When inactive, setVolumesDb() will skip updates to save CPU.
     * @param active True to enable updates, false to disable.
     */
    void setActive(bool active);
    
    /**
     * @brief Gets whether the meter is currently active.
     * @return True if active, false otherwise.
     */
    bool isActive() const { return m_active; }

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    float leftDb_ = -100.0f;
    float rightDb_ = -100.0f;
    int minDb_ = -70;
    int maxDb_ = 0;
    bool m_active = true;

    // Peak hold
    float leftPeakDb_ = -100.0f;
    float rightPeakDb_ = -100.0f;
    QElapsedTimer leftPeakTimer_;
    QElapsedTimer rightPeakTimer_;
    int peakHoldMs_ = 2000;

    void updatePeakValues(float leftDb, float rightDb);

public slots:
    /**
     * @brief Sets the volume levels for left and right channels in dB.
     * @param leftDb Volume level for the left channel in dB.
     * @param rightDb Volume level for the right channel in dB.
     */
    void setVolumesDb(float leftDb, float rightDb);

    /**
     * @brief Resets the meter to minimum values.
     */
    void reset();
};
