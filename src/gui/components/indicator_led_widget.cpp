#include "indicator_led_widget.h"
#include <QBrush>
#include <QPainter>
#include <QPen>

IndicatorLedWidget::IndicatorLedWidget(const QColor &color, QWidget *parent)
    : QWidget(parent), led_color(color), is_active(false) {
    setMinimumSize(18, 18);
    setMaximumSize(100, 100);
}

QSize IndicatorLedWidget::sizeHint() const { return QSize(22, 22); }

QSize IndicatorLedWidget::minimumSizeHint() const { return QSize(18, 18); }

void IndicatorLedWidget::setActive(bool active) {
    if (is_active != active) {
        is_active = active;
        update();
    }
}

void IndicatorLedWidget::setColor(const QColor &color) {
    if (led_color != color) {
        led_color = color;
        update();
    }
}

void IndicatorLedWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int s = qMin(w, h) - 2;

    QRectF ledRect((w - s) / 2, (h - s) / 2, s, s);

    // Draw dark border
    QPen borderPen(QColor(40, 40, 40), 2);
    p.setPen(borderPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ledRect);

    // LED fill (off = dark, on = color)
    QRadialGradient grad(ledRect.center(), s / 2);
    if (is_active) {
        grad.setColorAt(0.0, led_color.lighter(160));
        grad.setColorAt(0.7, led_color);
        grad.setColorAt(1.0, led_color.darker(180));
    } else {
        QColor offColor = led_color.darker(260);
        grad.setColorAt(0.0, offColor.lighter(110));
        grad.setColorAt(1.0, offColor);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(grad));
    p.drawEllipse(ledRect);
}