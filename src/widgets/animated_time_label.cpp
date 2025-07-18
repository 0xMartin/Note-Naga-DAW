#include "animated_time_label.h"

#include <QPainter>
#include <QLinearGradient>
#include <QFont>

AnimatedTimeLabel::AnimatedTimeLabel(QWidget* parent)
    : QLabel(parent), anim_timer(new QTimer(this)), anim_progress(0)
{
    setObjectName("AnimatedTimeLabel");
    setMinimumWidth(130);
    setAlignment(Qt::AlignCenter);

    anim_timer->setInterval(16); // cca 60 FPS
    connect(anim_timer, &QTimer::timeout, this, &AnimatedTimeLabel::updateAnim);

    setStyleSheet(R"(
        QLabel#AnimatedTimeLabel {
            color: #d6eaff;
            font-size: 19px;
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-weight: bold;
            padding: 4px 18px;
            border-radius: 7px;
            border: 1.4px solid #4866a0;
            letter-spacing: 1.2px;
        }
    )");
}

void AnimatedTimeLabel::animateTick() {
    anim_progress = 100;
    anim_timer->start();
    update();
}

void AnimatedTimeLabel::updateAnim() {
    if (anim_progress > 0) {
        anim_progress -= 8;
        update();
    } else {
        anim_timer->stop();
    }
}

void AnimatedTimeLabel::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    QRect r = rect();

    // Gradient background - zesvětlit při ticku
    QColor baseColor1(40, 48, 64);
    QColor baseColor2(50, 64, 96);

    // Pulsace: zesvětlit barvy při animaci, max o 40
    int lighten = anim_progress * 40 / 100;
    QColor color1 = baseColor1.lighter(100 + lighten);
    QColor color2 = baseColor2.lighter(100 + lighten);

    QLinearGradient grad(r.topLeft(), r.topRight());
    grad.setColorAt(0, color1);
    grad.setColorAt(1, color2);

    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(r, 7, 7);

    // Border
    p.setPen(QColor("#4866a0"));
    p.drawRoundedRect(r, 7, 7);

    // Text (čas) - přizpůsobení velikosti fontu
    p.setPen(QColor("#d6eaff"));
    QFont f = font();
    f.setBold(true);

    // Dynamicky zmenšit font, pokud se nevejde
    QString txt = text();
    QRect textRect = r.adjusted(6, 2, -6, -2); // padding
    int fontSize = f.pointSize();
    if (fontSize <= 0) fontSize = 19; // fallback

    QFontMetrics fm(f);
    while ((fm.horizontalAdvance(txt) > textRect.width() || fm.height() > textRect.height()) && fontSize > 6) {
        fontSize--;
        f.setPointSize(fontSize);
        fm = QFontMetrics(f);
    }
    p.setFont(f);
    p.drawText(textRect, Qt::AlignCenter, txt);
}