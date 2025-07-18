#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QSize>
#include <QRect>

class AnimatedTimeLabel : public QLabel {
    Q_OBJECT
public:
    explicit AnimatedTimeLabel(QWidget* parent = nullptr);

    void animateTick(); 

    void setText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QTimer* anim_timer;
    int anim_progress; 

    // Font size caching
    int cached_font_point_size;
    QRect cached_text_rect;
    QSize cached_last_size;

    void updateAnim();
    void recalculateFontSize();
};