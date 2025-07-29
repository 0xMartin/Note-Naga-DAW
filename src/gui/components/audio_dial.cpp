#include "audio_dial.h"
#include <QBrush>
#include <QConicalGradient>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRadialGradient>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

AudioDial::AudioDial(QWidget *parent)
    : QWidget(parent), _min(0.0f), _max(100.0f), _value(0.0f), _default_value(0.0f),
      _start_angle(-135), _angle_range(270), _pressed(false),
      _label("Volume"), _show_label(true), _show_value(true), _value_prefix(""),
      _value_postfix(""), _value_decimals(2) {
    setMinimumSize(40, 60);
    setMouseTracking(true);
    updateGeometryCache();
}

void AudioDial::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateGeometryCache();
}

void AudioDial::updateGeometryCache() {
    int w = width(), h = height();

    // Font sizes as fraction of dial size
    int max_dial_size = std::min(w, h);

    // Estimate font heights
    int label_font_size = std::max(8, int(max_dial_size * 0.13));
    int value_font_size = std::max(7, int(max_dial_size * 0.11));
    int label_height =
        (_show_label && !_label.isEmpty())
            ? QFontMetrics(QFont("Segoe UI", label_font_size, QFont::Bold)).height()
            : 0;

    // Reserve margins for text above/below and a small padding
    int margin_top = 2;
    int margin_bottom = 6;
    int reserved_height = label_height + margin_top + margin_bottom + 5;
    int dial_size = std::min(w - 2 * margin_top, h - reserved_height);
    // Prevent dial from being too small
    dial_size = std::max(dial_size, 10);

    QPointF center(w / 2.0f, h / 2.0f);

    float arc_thickness = std::max(2.0f, dial_size * 0.10f);
    float outer_radius = (dial_size / 2.0f) - (arc_thickness / 2.0f) - 1.0f;
    float inner_radius = outer_radius * 0.7f;
    float tick_length = std::max(arc_thickness * 1.1f, 5.0f);

    QRectF dial_rect(center.x() - outer_radius, center.y() - outer_radius,
                     outer_radius * 2, outer_radius * 2);
    QRectF inner_rect(center.x() - inner_radius, center.y() - inner_radius,
                      inner_radius * 2, inner_radius * 2);

    // Label position: above dial, but never outside widget
    int label_y = int(center.y() - dial_size / 2.0f - margin_top);
    if (label_y < margin_top) label_y = margin_top;

    // Value position: below dial, but never outside widget
    int value_y = int(center.y() + dial_size / 2.0f + margin_bottom);
    int max_value_y = h - 5;
    if (value_y > max_value_y) value_y = max_value_y;

    _geometry_cache = AudioDial::DialGeometry{
        label_font_size, value_font_size, dial_size, center,
        inner_radius,    outer_radius,    dial_rect, arc_thickness,
        tick_length,     inner_rect,      label_y,   value_y};
    _last_size = QSize(w, h);
    _last_label = _show_label;
    _last_value = _show_value;
    _last_label_text = _label;
    _last_decimals = _value_decimals;
}

const AudioDial::DialGeometry &AudioDial::geometry() const {
    bool needs_update = _last_size != QSize(width(), height()) ||
                        _last_label != _show_label || _last_value != _show_value ||
                        _last_label_text != _label || _last_decimals != _value_decimals;
    if (needs_update) const_cast<AudioDial *>(this)->updateGeometryCache();
    return _geometry_cache;
}

void AudioDial::setValue(float value) {
    value = std::clamp(value, _min, _max);
    if (value != _value) {
        _value = std::round(value * std::pow(10, _value_decimals)) /
                 std::pow(10, _value_decimals);
        emit valueChanged(_value);
        update();
    }
}

void AudioDial::setRange(float min_val, float max_val) {
    _min = min_val;
    _max = max_val;
    update();
}

void AudioDial::setGradient(const QColor &color_start, const QColor &color_end) {
    gradient_start = color_start;
    gradient_end = color_end;
    update();
}

void AudioDial::setLabel(const QString &label) {
    _label = label;
    _show_label = true;
    updateGeometryCache();
    update();
}

void AudioDial::showLabel(bool show) {
    _show_label = show;
    updateGeometryCache();
    update();
}

void AudioDial::showValue(bool show) {
    _show_value = show;
    updateGeometryCache();
    update();
}

void AudioDial::setValuePrefix(const QString &prefix) {
    _value_prefix = prefix;
    update();
}

void AudioDial::setValuePostfix(const QString &postfix) {
    _value_postfix = postfix;
    update();
}

void AudioDial::setValueDecimals(int decimals) {
    _value_decimals = decimals;
    updateGeometryCache();
    update();
}

void AudioDial::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    const DialGeometry &geom = geometry();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw arc b.  vackground
    QPen arcPen(arc_bg_color, geom.arc_thickness);
    arcPen.setCapStyle(Qt::RoundCap);
    painter.setPen(arcPen);
    painter.drawArc(geom.dial_rect, int((90 - _start_angle) * 16),
                    int(-_angle_range * 16));

    // Draw value arc (gradient FL style)
    float value_frac = (_value - _min) / (_max - _min);
    QConicalGradient gradient(geom.center, 100 - _start_angle);
    gradient.setColorAt(1.0, gradient_start);
    gradient.setColorAt(0.0, gradient_end);
    QPen pen(QBrush(gradient), geom.arc_thickness);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawArc(geom.dial_rect, int((90 - _start_angle) * 16),
                    int(-_angle_range * value_frac * 16));

    // Draw inner circle (center dial) with radial gradient
    QRadialGradient innerGrad(geom.center, geom.inner_radius, geom.center);
    innerGrad.setColorAt(0.0, center_gradient_start);
    innerGrad.setColorAt(1.0, center_gradient_end);
    painter.setBrush(QBrush(innerGrad));
    painter.setPen(QPen(inner_outline, 2));
    painter.drawEllipse(geom.inner_rect);

    // Draw position tick (on inner circle edge)
    float angle_deg = valueToAngle(_value);
    float angle_rad = angle_deg * M_PI / 180.0f;
    QPointF tick_outer(geom.center.x() + std::cos(angle_rad) * geom.inner_radius * 0.8f,
                       geom.center.y() + std::sin(angle_rad) * geom.inner_radius * 0.8f);
    QPointF tick_inner(geom.center.x() + std::cos(angle_rad) * (geom.inner_radius * 0.8f -
                                                                geom.tick_length),
                       geom.center.y() + std::sin(angle_rad) * (geom.inner_radius * 0.8f -
                                                                geom.tick_length));

    QPen tickPen(_value <= _min || _value >= _max ? tick_end_color : tick_color,
                 geom.arc_thickness * 0.4);
    tickPen.setCapStyle(Qt::RoundCap);
    painter.setPen(tickPen);
    painter.drawLine(tick_inner, tick_outer);

    // Draw label above dial, centered, but fix position if dial is big
    if (_show_label && !_label.isEmpty()) {
        painter.setFont(QFont("Segoe UI", geom.label_font_size, QFont::Bold));
        painter.setPen(QColor("#fff"));
        QFontMetrics fm(painter.font());
        int text_w = fm.horizontalAdvance(_label);
        int label_x = (width() - text_w) / 2;
        painter.drawText(label_x, geom.label_y, _label);
    }

    // Draw value below dial, centered, but fix position if dial is big
    if (_show_value) {
        QString value_str;
        if (_value_decimals <= 0) {
            value_str =
                QString("%1%2%3").arg(_value_prefix).arg(int(_value)).arg(_value_postfix);
        } else {
            value_str = QString("%1%2%3")
                            .arg(_value_prefix)
                            .arg(QString::number(_value, 'f', _value_decimals))
                            .arg(_value_postfix);
        }
        painter.setFont(QFont("Segoe UI", geom.value_font_size, QFont::Normal));
        painter.setPen(QColor("#b5bac1"));
        QFontMetrics fm(painter.font());
        int text_w = fm.horizontalAdvance(value_str);
        int value_x = (width() - text_w) / 2;
        painter.drawText(value_x, geom.value_y, value_str);
    }

    painter.end();
}

float AudioDial::valueToAngle(float value) const {
    float frac = (_max == _min) ? 0 : (value - _min) / (_max - _min);
    return - _start_angle + _angle_range * frac;
}

bool AudioDial::inCircleArea(const QPoint &pos) {
    const DialGeometry &geom = geometry();
    float dx = pos.x() - geom.center.x();
    float dy = pos.y() - geom.center.y();
    float dist = std::hypot(dx, dy);
    return dist <= geom.outer_radius * 1.15f;
}

void AudioDial::mousePressEvent(QMouseEvent *event) {
    if (!inCircleArea(event->pos())) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        _pressed = true;
        mouseMoveEvent(event); // update on press
        event->accept();
    }
    if (event->button() == Qt::RightButton) { setValue(_default_value); }
}

void AudioDial::mouseMoveEvent(QMouseEvent *event) {
    if (_pressed) {
        const DialGeometry &geom = geometry();
        float dx = event->pos().x() - geom.center.x();
        float dy = event->pos().y() - geom.center.y();
        float angle = std::atan2(dy, dx) * 180.0f / float(M_PI) + 90.0f;
        if (angle < -180.0f) {
            angle += 360.0f;
        } else if (angle > 180.0f) {
            angle -= 360.0f;
        }

        float value = _min + ((angle - _start_angle) / _angle_range) * (_max - _min);
        qDebug() << "Angle:" << angle << "Value:" << value;
        setValue(value);
        event->accept();
    }
}

void AudioDial::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        _pressed = false;
        event->accept();
    }
}

void AudioDial::wheelEvent(QWheelEvent *event) {
    float step = 1.0f;
    if (_value_decimals > 0) { step = (_max - _min) / 50.0f; }
    if (event->angleDelta().y() > 0) {
        setValue(_value + step);
    } else {
        setValue(_value - step);
    }
    event->accept();
}