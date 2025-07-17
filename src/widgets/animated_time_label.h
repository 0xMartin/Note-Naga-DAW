#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>

class AnimatedTimeLabel : public QLabel {
    Q_OBJECT
public:
    explicit AnimatedTimeLabel(QWidget* parent = nullptr);

    void animateTick(); 

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QTimer* anim_timer;
    int anim_progress; 
    void updateAnim();
};