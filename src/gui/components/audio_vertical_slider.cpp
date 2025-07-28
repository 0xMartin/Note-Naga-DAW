#include "audio_vertical_slider.h"

#include <QFontMetrics>
#include <QDebug>

AudioVerticalSlider::AudioVerticalSlider(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(30);
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    updateTextSizes();
}

void AudioVerticalSlider::setRange(int min, int max)
{
    m_min = min;
    m_max = max;
    if (m_value < m_min) m_value = m_min;
    if (m_value > m_max) m_value = m_max;
    update();
}

void AudioVerticalSlider::setValue(int v)
{
    if (v < m_min) v = m_min;
    if (v > m_max) v = m_max;
    if (m_value != v) {
        m_value = v;
        emit valueChanged(m_value);
        update();
    }
}

void AudioVerticalSlider::setLabelVisible(bool visible)
{
    m_labelVisible = visible;
    update();
}

void AudioVerticalSlider::setValueVisible(bool visible)
{
    m_valueVisible = visible;
    update();
}

void AudioVerticalSlider::setLabelText(const QString& text)
{
    m_labelText = text;
    update();
}

void AudioVerticalSlider::setValuePrefix(const QString& prefix)
{
    m_valuePrefix = prefix;
    update();
}

void AudioVerticalSlider::setValuePostfix(const QString& postfix)
{
    m_valuePostfix = postfix;
    update();
}

void AudioVerticalSlider::resizeEvent(QResizeEvent*)
{
    updateTextSizes();
}

void AudioVerticalSlider::updateTextSizes()
{
    int w = width();
    m_labelFontSize = std::max(8, w / 4);
    m_valueFontSize = std::max(8, w / 3);
    update();
}

QRect AudioVerticalSlider::sliderGrooveRect() const
{
    int margin = width() / 3;
    return QRect(width() / 2 - margin / 2, m_labelVisible ? m_labelFontSize + 6 : 4,
                 margin, height() - (m_labelVisible ? m_labelFontSize + 6 : 4) - (m_valueVisible ? m_valueFontSize + 6 : 8));
}

QRect AudioVerticalSlider::handleRect() const
{
    QRect groove = sliderGrooveRect();
    int handleW = groove.width() + 8;
    int handleH = groove.width() + 8;
    int y = positionFromValue(m_value);
    return QRect(groove.center().x() - handleW / 2, y - handleH / 2, handleW, handleH);
}

int AudioVerticalSlider::positionFromValue(int value) const
{
    QRect groove = sliderGrooveRect();
    double f = 1.0 - double(value - m_min) / (m_max - m_min);
    int y = groove.top() + f * groove.height();
    return y;
}

int AudioVerticalSlider::valueFromPosition(int y) const
{
    QRect groove = sliderGrooveRect();
    double f = double(y - groove.top()) / groove.height();
    int value = m_min + (1.0 - f) * (m_max - m_min);
    return std::clamp(value, m_min, m_max);
}

void AudioVerticalSlider::mousePressEvent(QMouseEvent* event)
{
    if (handleRect().contains(event->pos())) {
        m_dragging = true;
        m_dragOffset = event->pos().y() - handleRect().center().y();
    } else {
        setValue(valueFromPosition(event->pos().y()));
    }
}

void AudioVerticalSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        int y = event->pos().y() - m_dragOffset;
        setValue(valueFromPosition(y));
    }
}

void AudioVerticalSlider::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
}

void AudioVerticalSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect groove = sliderGrooveRect();

    // draw background (transparent)
    p.fillRect(rect(), Qt::transparent);

    // groove shadow (dark thin line)
    QPen grooveShadowPen(QColor(30, 34, 38), groove.width() + 2, Qt::SolidLine, Qt::RoundCap);
    p.setPen(grooveShadowPen);
    p.drawLine(groove.center().x(), groove.top(), groove.center().x(), groove.bottom());

    // groove (green, thick, flat)
    QPen groovePen(QColor(183, 215, 101), groove.width(), Qt::SolidLine, Qt::RoundCap);
    p.setPen(groovePen);
    p.drawLine(groove.center().x(), groove.top(), groove.center().x(), groove.bottom());

    // handle (rounded, green/yellow gradient, subtle shadow)
    QRect hRect = handleRect();
    QLinearGradient grad(hRect.topLeft(), hRect.bottomLeft());
    grad.setColorAt(0, QColor(200, 240, 140));
    grad.setColorAt(0.45, QColor(180, 220, 100));
    grad.setColorAt(0.5, QColor(170, 210, 60));
    grad.setColorAt(1, QColor(154, 176, 87));
    p.setBrush(grad);
    p.setPen(QPen(QColor(100, 110, 65, 150), 2));
    p.drawRoundedRect(hRect.adjusted(1, 1, -1, -1), hRect.height() / 3.4, hRect.height() / 3.4);

    // subtle shadow under handle
    QRectF shadowRect = hRect.translated(0, 2);
    QRadialGradient shadowGrad(shadowRect.center(), hRect.width() / 1.6);
    shadowGrad.setColorAt(0, QColor(0,0,0,60));
    shadowGrad.setColorAt(1, Qt::transparent);
    p.setBrush(shadowGrad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(shadowRect);

    // draw label
    if (m_labelVisible) {
        QFont font = p.font();
        font.setPointSize(m_labelFontSize);
        p.setFont(font);
        p.setPen(Qt::white);
        QRect labelRect(0, 2, width(), m_labelFontSize + 4);
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter, m_labelText);
    }

    // draw value
    if (m_valueVisible) {
        QFont font = p.font();
        font.setPointSize(m_valueFontSize);
        p.setFont(font);
        p.setPen(QColor(210, 235, 180));
        QString valueStr = QString("%1%2%3").arg(m_valuePrefix).arg(m_value).arg(m_valuePostfix);
        QRect valueRect(0, height() - (m_valueFontSize + 6), width(), m_valueFontSize + 4);
        p.drawText(valueRect, Qt::AlignHCenter | Qt::AlignVCenter, valueStr);
    }
}
