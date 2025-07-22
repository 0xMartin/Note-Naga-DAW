#pragma once

#include <QLabel>
#include <QRect>
#include <QSize>
#include <QTimer>
#include <QWidget>

/**
 * @brief A QLabel subclass that animates the text size and appearance.
 */
class AnimatedTimeLabel : public QLabel {
    Q_OBJECT
public:
    explicit AnimatedTimeLabel(QWidget *parent = nullptr);

    /**
     * @brief Starts the animation with a specified duration.
     */
    void animateTick();

    /**
     * @brief Sets the text to be displayed in the label.
     * @param text The text to display.
     */
    void setText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QTimer *anim_timer;
    int anim_progress;

    // Font size caching
    int cached_font_point_size;
    QRect cached_text_rect;
    QSize cached_last_size;

    void updateAnim();
    void recalculateFontSize();
};