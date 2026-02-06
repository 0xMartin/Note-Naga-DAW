#include "audio_horizontal_slider.h"
#include <QDebug>
#include <QFontMetrics>
#include <QPainterPath>
#include <QToolTip>
#include <cmath>

AudioHorizontalSlider::AudioHorizontalSlider(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(30);
    setMinimumWidth(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    updateTextSizes();
}

void AudioHorizontalSlider::setRange(float min, float max) {
    m_min = min;
    m_max = max;
    if (m_value < m_min) m_value = m_min;
    if (m_value > m_max) m_value = m_max;
    update();
}

void AudioHorizontalSlider::setValue(float v) {
    float oldValue = m_value;
    v = std::max(m_min, std::min(v, m_max));
    if (std::abs(oldValue - v) > 1e-6f) {
        m_value = v;
        emit valueChanged(m_value);
        update();
    }
}

void AudioHorizontalSlider::setLabelVisible(bool visible) {
    m_labelVisible = visible;
    update();
}

void AudioHorizontalSlider::setValueVisible(bool visible) {
    m_valueVisible = visible;
    update();
}

void AudioHorizontalSlider::setLabelText(const QString &text) {
    m_labelText = text;
    update();
}

void AudioHorizontalSlider::setValuePrefix(const QString &prefix) {
    m_valuePrefix = prefix;
    update();
}

void AudioHorizontalSlider::setValuePostfix(const QString &postfix) {
    m_valuePostfix = postfix;
    update();
}

void AudioHorizontalSlider::setValueDecimals(int decimals) {
    m_valueDecimals = std::max(0, decimals);
    update();
}

void AudioHorizontalSlider::resizeEvent(QResizeEvent *) { updateTextSizes(); }

void AudioHorizontalSlider::updateTextSizes() {
    int h = height();
    m_labelFontSize = std::max(8, h / 4);
    m_valueFontSize = m_labelFontSize;
    update();
}

QSize AudioHorizontalSlider::minimumSizeHint() const {
    return QSize(80, 30);
}

QSize AudioHorizontalSlider::sizeHint() const {
    return QSize(120, 40);
}

// Groove rect calculation (horizontal version)
QRect AudioHorizontalSlider::sliderGrooveRect() const {
    int labelW = m_labelVisible ? m_labelFontSize * 3 + 6 : 4;
    int valueW = m_valueVisible ? m_valueFontSize * 4 + 8 : 8;
    int grooveH = std::max(10, height() / 3);
    return QRect(labelW + 4, height() / 2 - grooveH / 2, width() - labelW - valueW - 8, grooveH);
}

int AudioHorizontalSlider::limitHandleX(int x, int handleW, const QRect &groove) const {
    int minX = groove.left() + handleW / 2;
    int maxX = groove.right() - handleW / 2;
    return std::min(std::max(x, minX), maxX);
}

QRect AudioHorizontalSlider::handleRect() const {
    QRect groove = sliderGrooveRect();
    int handleH = int((groove.height() + 4) * 1.2);
    int handleW = int(std::max(20.0, groove.height() * 1.4) * 1.3);
    int x = positionFromValue(m_value);
    x = limitHandleX(x, handleW, groove);
    return QRect(x - handleW / 2, groove.center().y() - handleH / 2, handleW, handleH);
}

// Position/value mapping (horizontal: min is LEFT, max is RIGHT)
int AudioHorizontalSlider::positionFromValue(float value) const {
    QRect groove = sliderGrooveRect();
    int handleW = int(std::max(20.0, groove.height() * 1.4) * 1.3);
    int minX = groove.left() + handleW / 2;
    int maxX = groove.right() - handleW / 2;

    if (m_min < 0.0f && m_max > 0.0f) {
        // Zero is in the middle
        float zeroFrac = (-m_min) / (m_max - m_min);
        int zeroX = minX + zeroFrac * (maxX - minX);
        float frac = (value - 0.0f) / (m_max - m_min);
        int x = zeroX + frac * (maxX - minX);
        return limitHandleX(x, handleW, groove);
    }

    float frac = (value - m_min) / (m_max - m_min);
    int x = minX + frac * (maxX - minX);
    return limitHandleX(x, handleW, groove);
}

float AudioHorizontalSlider::valueFromPosition(int x) const {
    QRect groove = sliderGrooveRect();
    int handleW = int(std::max(20.0, groove.height() * 1.4) * 1.3);
    int minX = groove.left() + handleW / 2;
    int maxX = groove.right() - handleW / 2;

    if (m_min < 0.0f && m_max > 0.0f) {
        float zeroFrac = (-m_min) / (m_max - m_min);
        int zeroX = minX + zeroFrac * (maxX - minX);
        float frac = float(x - zeroX) / (maxX - minX);
        float value = 0.0f + frac * (m_max - m_min);
        return std::clamp(value, m_min, m_max);
    }

    float frac = float(x - minX) / (maxX - minX);
    float value = m_min + frac * (m_max - m_min);
    return std::clamp(value, m_min, m_max);
}

void AudioHorizontalSlider::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (handleRect().contains(event->pos())) {
            m_dragging = true;
            m_dragOffset = event->pos().x() - handleRect().center().x();
        } else {
            setValue(valueFromPosition(event->pos().x()));
        }
    } else if (event->button() == Qt::RightButton) {
        // Right-click resets to default value
        setValue(m_defaultValue);
    }
    event->accept();
}

void AudioHorizontalSlider::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        QRect groove = sliderGrooveRect();
        int handleW = int(std::max(20.0, groove.height() * 1.4) * 1.3);
        int x = event->pos().x() - m_dragOffset;
        x = limitHandleX(x, handleW, groove);
        setValue(valueFromPosition(x));
        
        // Show tooltip at cursor
        QString valueStr = QString("%1%2%3")
            .arg(m_valuePrefix)
            .arg(QString::number(m_value, 'f', m_valueDecimals))
            .arg(m_valuePostfix);
        QToolTip::showText(event->globalPosition().toPoint(), valueStr, this);
    }
}

void AudioHorizontalSlider::mouseReleaseEvent(QMouseEvent *) { m_dragging = false; }

void AudioHorizontalSlider::wheelEvent(QWheelEvent *event) {
    float step = (m_max - m_min) / 100.0f;
    if (event->angleDelta().y() > 0) {
        setValue(value() + step);
    } else {
        setValue(value() - step);
    }
    event->accept();
}

void AudioHorizontalSlider::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect groove = sliderGrooveRect();
    groove.setY(groove.top() - 1);

    // --- LABEL (left side) ---
    if (m_labelVisible) {
        QFont font = p.font();
        font.setPointSize(m_labelFontSize);
        font.setBold(true);
        p.setFont(font);
        p.setPen(labelColor);
        QRect labelRect(2, 0, m_labelFontSize * 3 + 4, height());
        p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignRight, m_labelText);
    }

    // --- VALUE (right side) ---
    if (m_valueVisible) {
        QFont font = p.font();
        font.setPointSize(m_valueFontSize);
        font.setBold(false);
        p.setFont(font);
        p.setPen(valueColor);
        QString valueStr = QString("%1%2%3")
            .arg(m_valuePrefix)
            .arg(QString::number(m_value, 'f', m_valueDecimals))
            .arg(m_valuePostfix);
        QRect valueRect(width() - (m_valueFontSize * 4 + 8), 0, m_valueFontSize * 4 + 4, height());
        p.drawText(valueRect, Qt::AlignVCenter | Qt::AlignLeft, valueStr);
    }

    // --- GROOVE ---
    p.setPen(Qt::NoPen);
    p.setBrush(grooveBgColor);
    int grooveRadius = 2;
    p.drawRoundedRect(groove, grooveRadius, grooveRadius);

    QPen groovePen(grooveOutlineColor, 1, Qt::SolidLine, Qt::RoundCap);
    p.setPen(groovePen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(groove, grooveRadius, grooveRadius);

    // --- PROGRESS FILL ---
    int valueX = positionFromValue(m_value);
    QRect grooveFillRect;
    if (m_min < 0.0f && m_max > 0.0f) {
        // Progress from zero
        int zeroX = positionFromValue(0.0f);
        if (m_value > 0.0f) {
            grooveFillRect = QRect(zeroX, groove.top() + 2,
                                   valueX - zeroX, groove.height() - 4);
        } else {
            grooveFillRect = QRect(valueX, groove.top() + 2,
                                   zeroX - valueX, groove.height() - 4);
        }
        QLinearGradient fillGrad(groove.left(), groove.top(), groove.right(), groove.top());
        float zeroFrac = float(zeroX - groove.left()) / groove.width();
        fillGrad.setColorAt(zeroFrac, grooveGradientStart);
        fillGrad.setColorAt(0.0, grooveGradientEnd);
        fillGrad.setColorAt(1.0, grooveGradientEnd);
        p.setPen(Qt::NoPen);
        p.setBrush(fillGrad);
        p.drawRect(grooveFillRect);
    } else {
        // Standard progress from left to right
        grooveFillRect = QRect(groove.left() + 2, groove.top() + 2,
                               valueX - groove.left() - 2, groove.height() - 4);
        QLinearGradient fillGrad(groove.left(), groove.top(), groove.right(), groove.top());
        fillGrad.setColorAt(0.0, grooveGradientStart);
        fillGrad.setColorAt(1.0, grooveGradientEnd);
        p.setPen(Qt::NoPen);
        p.setBrush(fillGrad);
        p.drawRect(grooveFillRect);
    }

    // --- SCALE TICKS (below groove) ---
    int scaleY = groove.bottom() + 3;
    int tickLenMajor = 7, tickLenMinor = 3;
    int nTicks = 9;
    for (int i = 0; i < nTicks; ++i) {
        float tickValue = m_min + (i / float(nTicks - 1)) * (m_max - m_min);
        int x = positionFromValue(tickValue);
        bool major = (i == 0 || i == nTicks - 1 || std::abs(tickValue) < 1e-6f);
        p.setPen(major ? scaleMajorColor : scaleMinorColor);
        int len = major ? tickLenMajor : tickLenMinor;
        p.drawLine(x, scaleY, x, scaleY + len);
    }

    // --- HANDLE ---
    QRect hRect = handleRect();
    int handleRadius = 3;
    p.setPen(handleOutlineColor);
    p.setBrush(handleFillColor);
    p.drawRoundedRect(hRect, handleRadius, handleRadius);

    // --- GROOVE LINES on handle (vertical lines for horizontal slider) ---
    p.setPen(handleGrooveColor);
    int nGrooves = 6;
    int grooveSpacing = hRect.width() / (nGrooves + 1);
    int grooveTop = hRect.top() + 1 + hRect.height() * 0.2;
    int grooveBottom = hRect.bottom() + 1 - hRect.height() * 0.2;
    for (int i = 1; i <= nGrooves; ++i) {
        int x = hRect.left() + i * grooveSpacing;
        p.drawLine(x, grooveTop, x, grooveBottom);
    }
}
