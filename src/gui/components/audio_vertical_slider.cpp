#include "audio_vertical_slider.h"
#include <QDebug>
#include <QFontMetrics>
#include <QPainterPath>
#include <cmath>

AudioVerticalSlider::AudioVerticalSlider(QWidget *parent) : QWidget(parent) {
    setMinimumWidth(30);
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    updateTextSizes();
}

void AudioVerticalSlider::setRange(float min, float max) {
    m_min = min;
    m_max = max;
    if (m_value < m_min) m_value = m_min;
    if (m_value > m_max) m_value = m_max;
    update();
}

void AudioVerticalSlider::setValue(float v) {
    float oldValue = m_value;
    v = std::max(m_min, std::min(v, m_max));
    if (std::abs(oldValue - v) > 1e-6f) {
        m_value = v;
        emit valueChanged(m_value);
        update();
    }
}

void AudioVerticalSlider::setLabelVisible(bool visible) {
    m_labelVisible = visible;
    update();
}

void AudioVerticalSlider::setValueVisible(bool visible) {
    m_valueVisible = visible;
    update();
}

void AudioVerticalSlider::setLabelText(const QString &text) {
    m_labelText = text;
    update();
}

void AudioVerticalSlider::setValuePrefix(const QString &prefix) {
    m_valuePrefix = prefix;
    update();
}

void AudioVerticalSlider::setValuePostfix(const QString &postfix) {
    m_valuePostfix = postfix;
    update();
}

void AudioVerticalSlider::setValueDecimals(int decimals) {
    m_valueDecimals = std::max(0, decimals);
    update();
}

void AudioVerticalSlider::resizeEvent(QResizeEvent *) { updateTextSizes(); }

void AudioVerticalSlider::updateTextSizes() {
    int w = width();
    m_labelFontSize = std::max(8, w / 4);
    m_valueFontSize = m_labelFontSize; // value size = label size
    update();
}

// --- pevné mapování handle: min je DOLNÍ střed, max je HORNÍ střed ---
QRect AudioVerticalSlider::sliderGrooveRect() const {
    int labelH = m_labelVisible ? m_labelFontSize + 6 : 4;
    int valueH = m_valueVisible ? m_valueFontSize + 8 : 8;
    int grooveW = std::max(10, width() / 3);
    return QRect(width() / 2 - grooveW / 2, labelH + 4, grooveW, height() - labelH - valueH - 8);
}

int AudioVerticalSlider::limitHandleY(int y, int handleH, const QRect &groove) const {
    int minY = groove.top() + handleH / 2;
    int maxY = groove.bottom() - handleH / 2;
    return std::min(std::max(y, minY), maxY);
}

QRect AudioVerticalSlider::handleRect() const {
    QRect groove = sliderGrooveRect();
    int handleW = int((groove.width() + 4) * 1.2);
    int handleH = int(std::max(20.0, groove.width() * 1.4) * 1.3);
    int y = positionFromValue(m_value);
    y = limitHandleY(y, handleH, groove);
    return QRect(groove.center().x() - handleW / 2, y - handleH / 2, handleW, handleH);
}

// --- PEVNÉ MAPOVÁNÍ: max je nahoře, min je dole ---
// Pokud min < 0 < max, nula je uprostřed (specialita pro progress bar)
int AudioVerticalSlider::positionFromValue(float value) const {
    QRect groove = sliderGrooveRect();
    int handleH = int(std::max(20.0, groove.width() * 1.4) * 1.3);
    int minY = groove.top() + handleH / 2;
    int maxY = groove.bottom() - handleH / 2;

    if (m_min < 0.0f && m_max > 0.0f) {
        // nula je uprostřed
        float zeroFrac = (-m_min) / (m_max - m_min);
        int zeroY = minY + zeroFrac * (maxY - minY);
        float frac = (value - 0.0f) / (m_max - m_min);
        int y = zeroY - frac * (maxY - minY);
        return limitHandleY(y, handleH, groove);
    }

    float frac = (value - m_min) / (m_max - m_min);
    int y = maxY - frac * (maxY - minY);
    return limitHandleY(y, handleH, groove);
}

float AudioVerticalSlider::valueFromPosition(int y) const {
    QRect groove = sliderGrooveRect();
    int handleH = int(std::max(20.0, groove.width() * 1.4) * 1.3);
    int minY = groove.top() + handleH / 2;
    int maxY = groove.bottom() - handleH / 2;

    if (m_min < 0.0f && m_max > 0.0f) {
        float zeroFrac = (-m_min) / (m_max - m_min);
        int zeroY = minY + zeroFrac * (maxY - minY);
        float frac = float(zeroY - y) / (maxY - minY);
        float value = 0.0f + frac * (m_max - m_min);
        return std::clamp(value, m_min, m_max);
    }

    float frac = float(maxY - y) / (maxY - minY);
    float value = m_min + frac * (m_max - m_min);
    return std::clamp(value, m_min, m_max);
}

void AudioVerticalSlider::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (handleRect().contains(event->pos())) {
            m_dragging = true;
            m_dragOffset = event->pos().y() - handleRect().center().y();
        } else {
            setValue(valueFromPosition(event->pos().y()));
        }
    } else if (event->button() == Qt::RightButton) {
        // Right-click resets to default value
        setValue(m_default_value);
    }
    event->accept();
}

void AudioVerticalSlider::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        QRect groove = sliderGrooveRect();
        int handleH = int(std::max(20.0, groove.width() * 1.4) * 1.3);
        int y = event->pos().y() - m_dragOffset;
        y = limitHandleY(y, handleH, groove); // pevný doraz
        setValue(valueFromPosition(y));
    }
}

void AudioVerticalSlider::mouseReleaseEvent(QMouseEvent *) { m_dragging = false; }

void AudioVerticalSlider::wheelEvent(QWheelEvent *event) {
    float step = (m_max - m_min) / 500.0f;
    if (event->angleDelta().y() > 0) {
        setValue(value() + step);
    } else {
        setValue(value() - step);
    }
}

void AudioVerticalSlider::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect groove = sliderGrooveRect();
    groove.setX(groove.left() - 1);

    // --- LABEL ---
    if (m_labelVisible) {
        QFont font = p.font();
        font.setPointSize(m_labelFontSize);
        font.setBold(true);
        p.setFont(font);
        p.setPen(labelColor);
        QRect labelRect(0, 2, width(), m_labelFontSize + 4);
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter, m_labelText);
    }

    // --- VALUE ---
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
        QRect valueRect(0, height() - (m_valueFontSize + 8), width(), m_valueFontSize + 4);
        p.drawText(valueRect, Qt::AlignHCenter | Qt::AlignVCenter, valueStr);
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
    int valueY = positionFromValue(m_value);
    QRect grooveFillRect;
    if (m_min < 0.0f && m_max > 0.0f) {
        // progress od nuly
        int zeroY = positionFromValue(0.0f);
        if (m_value > 0.0f) {
            grooveFillRect = QRect(groove.left() + 2, valueY,
                                   groove.width() - 4, zeroY - valueY);
        } else {
            grooveFillRect = QRect(groove.left() + 2, zeroY,
                                   groove.width() - 4, valueY - zeroY);
        }
        QLinearGradient fillGrad(groove.left(), groove.top(), groove.left(), groove.bottom());
        float zeroFrac = float(zeroY - groove.top()) / groove.height();
        fillGrad.setColorAt(zeroFrac, grooveGradientStart);
        fillGrad.setColorAt(0.0, grooveGradientEnd);
        fillGrad.setColorAt(1.0, grooveGradientEnd);
        p.setPen(Qt::NoPen);
        p.setBrush(fillGrad);
        p.drawRect(grooveFillRect);
    } else {
        // standardní progress odspoda nahoru
        grooveFillRect = QRect(groove.left() + 2, valueY,
                               groove.width() - 4, groove.bottom() - valueY);
        QLinearGradient fillGrad(groove.left(), groove.top(), groove.left(), groove.bottom());
        fillGrad.setColorAt(1.0, grooveGradientStart);
        fillGrad.setColorAt(0.0, grooveGradientEnd);
        p.setPen(Qt::NoPen);
        p.setBrush(fillGrad);
        p.drawRect(grooveFillRect);
    }

    // --- SCALE TICKS ---
    int scaleX = groove.right() + 3;
    int tickLenMajor = 7, tickLenMinor = 3;
    int nTicks = 9;
    for (int i = 0; i < nTicks; ++i) {
        float tickValue = m_min + (i / float(nTicks - 1)) * (m_max - m_min);
        int y = positionFromValue(tickValue);
        bool major = (i == 0 || i == nTicks - 1 || std::abs(tickValue) < 1e-6f);
        p.setPen(major ? scaleMajorColor : scaleMinorColor);
        int len = major ? tickLenMajor : tickLenMinor;
        p.drawLine(scaleX, y, scaleX + len, y);
    }

    // --- HANDLE ---
    QRect hRect = handleRect();
    int handleRadius = 3;
    p.setPen(handleOutlineColor);
    p.setBrush(handleFillColor);
    p.drawRoundedRect(hRect, handleRadius, handleRadius);

    // --- GROOVE LINES ---
    p.setPen(handleGrooveColor);
    int nGrooves = 6;
    int grooveSpacing = hRect.height() / (nGrooves + 1);
    int grooveLeft = hRect.left() + 1 + hRect.width() * 0.2;
    int grooveRight = hRect.right() + 1 - hRect.width() * 0.2;
    for (int i = 1; i <= nGrooves; ++i) {
        int y = hRect.top() + i * grooveSpacing;
        p.drawLine(grooveLeft, y, grooveRight, y);
    }
}