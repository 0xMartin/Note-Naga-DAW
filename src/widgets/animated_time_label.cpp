#include "animated_time_label.h"

#include <QPainter>
#include <QLinearGradient>
#include <QFont>
#include <QResizeEvent>
#include <algorithm>

AnimatedTimeLabel::AnimatedTimeLabel(QWidget* parent)
    : QLabel(parent), anim_timer(new QTimer(this)), anim_progress(0),
      cached_font_point_size(-1), cached_text_rect(), cached_last_size(-1, -1)
{
    setObjectName("AnimatedTimeLabel");
    setMinimumWidth(130);
    setAlignment(Qt::AlignCenter);

    anim_timer->setInterval(16); // cca 60 FPS
    connect(anim_timer, &QTimer::timeout, this, &AnimatedTimeLabel::updateAnim);
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

void AnimatedTimeLabel::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    // Přepočítej font velikost při změně velikosti prvku
    recalculateFontSize();
}

void AnimatedTimeLabel::setText(const QString& text) {
    QLabel::setText(text);
    // Pokud je nově nastavený text delší než poslední, zkontroluj velikost fontu
    recalculateFontSize();
}

void AnimatedTimeLabel::recalculateFontSize() {
    // Nastav základní rezervu (pro případné větší hodnoty, např. 99:59.999)
    static const QString prototype = "99:59.999 / 99:59.999";
    QRect r = rect();
    QRect textRect = r.adjusted(6, 2, -6, -2); // padding

    // Jen pokud se změnila velikost widgetu
    if (cached_last_size == r.size() && cached_font_point_size > 0) {
        cached_text_rect = textRect;
        return;
    }

    QFont f = font();
    f.setBold(true);

    int fontSize = std::max(8, f.pointSize());
    if (fontSize <= 0) fontSize = 19; // fallback

    QFontMetrics fm(f);
    // Najdi největší font, který se vejde do textRect (s rezervou 10%)
    while ((fm.horizontalAdvance(prototype) > textRect.width() * 0.90 || fm.height() > textRect.height() * 0.90) && fontSize > 6) {
        fontSize--;
        f.setPointSize(fontSize);
        fm = QFontMetrics(f);
    }
    cached_font_point_size = fontSize;
    cached_text_rect = textRect;
    cached_last_size = r.size();
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

    // Text (čas)
    p.setPen(QColor("#d6eaff"));
    QFont f = font();
    f.setBold(true);

    // Použij vypočtenou velikost fontu
    if (cached_font_point_size <= 0)
        recalculateFontSize();

    f.setPointSize(cached_font_point_size);
    p.setFont(f);

    QString txt = text();
    QRect textRect = cached_text_rect;
    p.drawText(textRect, Qt::AlignCenter, txt);
}