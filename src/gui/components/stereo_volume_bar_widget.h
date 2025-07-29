#pragma once

#include <QWidget>
#include <QList>
#include <QElapsedTimer>

/**
 * @brief StereoVolumeBarWidget displays stereo volume levels in dB with a visual bar representation.
 * It supports setting volume levels for left and right channels, and displays a dB scale.
 * The widget also includes peak hold functionality to indicate the maximum volume levels over time.
 */
class StereoVolumeBarWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a StereoVolumeBarWidget.
     * @param parent The parent widget.
     * @param minDb Minimum dB value for the volume bar.
     * @param maxDb Maximum dB value for the volume bar.
     * @param barWidth Width of the volume bar in pixels.
     */
    explicit StereoVolumeBarWidget(QWidget* parent = nullptr,
                                   int minDb = -100,
                                   int maxDb = 0,
                                   int barWidth = 6);

    /**
     * @brief Sets the dB range for the volume bar.
     * @param minDb Minimum dB value.
     * @param maxDb Maximum dB value.
     */
    void setDbRange(int minDb, int maxDb);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override;

private:
    // Volume levels in dB
    float leftDb_ = -100.0f;
    float rightDb_ = -100.0f;

    // dB range and bar width
    int minDb_ = -100;
    int maxDb_ = 0;

    // Bar width in pixels
    int barWidth_ = 6;
    QList<int> boldTicks_;

    // Peak hold tracking
    float leftPeakDb_ = -100.0f;
    float rightPeakDb_ = -100.0f;
    QElapsedTimer leftPeakTimer_;
    QElapsedTimer rightPeakTimer_;
    int peakHoldMs_ = 5000; // peak hold duration in milliseconds

    void updateBoldTicks();
    void drawValueBar(QPainter& p, int x, int bar_w, int bar_h, int margin, float dbValue, const QLinearGradient& grad);
    void drawPeakIndicator(QPainter& p, int x, int bar_w, int bar_h, int margin, float peakDb);
    void drawDbScale(QPainter& p, int left_x, int right_x, int bar_w, int bar_h, int margin, int label_width, int scale_x);
    void drawLabels(QPainter& p, int left_x, int right_x, int bar_w, int margin);
    void updatePeakValues(float leftDb, float rightDb);

public slots:
    /**
     * @brief Sets the volume levels for left and right channels in dB.
     * @param leftDb Volume level for the left channel in dB.
     * @param rightDb Volume level for the right channel in dB.
     */
    void setVolumesDb(float leftDb, float rightDb);
};