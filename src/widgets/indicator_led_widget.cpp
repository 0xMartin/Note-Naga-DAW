#include "indicator_led_widget.h"
#include <QPainter>
#include <QPen>
#include <QBrush>

IndicatorLedWidget::IndicatorLedWidget(const QColor &color, QWidget *parent)
    : QWidget(parent), ledColor(color), isActive(false)
{
    setMinimumSize(18, 18);
    setMaximumSize(100, 100);
}

QSize IndicatorLedWidget::sizeHint() const
{
    return QSize(22, 22);
}

QSize IndicatorLedWidget::minimumSizeHint() const
{
    return QSize(18, 18);
}

void IndicatorLedWidget::setActive(bool active)
{
    if (isActive != active)
    {
        isActive = active;
        update();
    }
}

void IndicatorLedWidget::paintEvent(QPaintEvent *event)
{
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
    if (isActive)
    {
        grad.setColorAt(0.0, ledColor.lighter(160));
        grad.setColorAt(0.7, ledColor);
        grad.setColorAt(1.0, ledColor.darker(180));
    }
    else
    {
        QColor offColor = ledColor.darker(260);
        grad.setColorAt(0.0, offColor.lighter(110));
        grad.setColorAt(1.0, offColor);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(grad));
    p.drawEllipse(ledRect);
}