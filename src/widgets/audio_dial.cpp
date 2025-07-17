#include "audio_dial.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFont>
#include <QFontMetrics>
#include <QConicalGradient>
#include <cmath>
#include <algorithm>

AudioDial::AudioDial(QWidget* parent)
    : QWidget(parent),
      _min(0.0f),
      _max(100.0f),
      _value(0.0f),
      _default_value(0.0f),
      _start_angle(135),
      _angle_range(270),
      bg_color("#2b2f33"),
      inner_color("#3a3f45"),
      inner_outline("#111"),
      arc_bg_color("#1e1e20"),
      dot_color("#6cb0ff"),
      dot_end_color("#ff50f9"),
      gradient_start("#6cb0ff"),
      gradient_end("#ae6cff"),
      _pressed(false),
      _label("Volume"),
      _show_label(true),
      _show_value(true),
      _value_prefix(""),
      _value_postfix(""),
      _value_decimals(2)
{
    setMinimumSize(40, 60);
    setMouseTracking(true);
    updateGeometryCache();
}

void AudioDial::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateGeometryCache();
}

void AudioDial::updateGeometryCache() {
    int w = width(), h = height();
    int size_for_circle = std::min(w, h);
    int label_space = 0;
    int label_font_size = std::max(8, int(size_for_circle * 0.15));
    int value_font_size = std::max(7, int(size_for_circle * 0.11));
    if (_show_label && !_label.isEmpty()) {
        label_space += QFontMetrics(QFont("Segoe UI", label_font_size, QFont::Bold)).height();
    }
    if (_show_value) {
        label_space += QFontMetrics(QFont("Segoe UI", value_font_size, QFont::Normal)).height();
    }
    label_space += 4;
    int size = std::min(w, h - label_space);
    if (size < 10) size = 10;
    QPointF center(w / 2.0, size / 2.0 + 2);
    float arc_thickness = std::max(2.0f, size * 0.1f);
    float outer_radius = (size / 2.0f) - (arc_thickness / 2.0f) - 1.0f;
    float inner_radius = outer_radius * 0.7f;
    _geometry_cache = std::make_tuple(label_font_size, value_font_size, size, center, inner_radius, outer_radius);
    _geometry_cache_size = std::make_tuple(w, h, _show_label, _show_value, _label, _value_decimals);
}

std::tuple<int, int, int, QPointF, float, float> AudioDial::getCircleGeometry() {
    int w = width(), h = height();
    auto cache_key = std::make_tuple(w, h, _show_label, _show_value, _label, _value_decimals);
    if (_geometry_cache_size != cache_key) {
        updateGeometryCache();
    }
    return _geometry_cache;
}

float AudioDial::value() const {
    return _value;
}

void AudioDial::setValue(float value) {
    value = std::clamp(value, _min, _max);
    if (value != _value) {
        _value = std::round(value * std::pow(10, _value_decimals)) / std::pow(10, _value_decimals);
        emit valueChanged(_value);
        update();
    }
}

void AudioDial::setDefaultValue(float value) { _default_value = value; }
void AudioDial::setRange(float min_val, float max_val) { _min = min_val; _max = max_val; update(); }
void AudioDial::setGradient(const QColor& color_start, const QColor& color_end) { gradient_start = color_start; gradient_end = color_end; update(); }
void AudioDial::setLabel(const QString& label) { _label = label; _show_label = true; updateGeometryCache(); update(); }
void AudioDial::showLabel(bool show) { _show_label = show; updateGeometryCache(); update(); }
void AudioDial::showValue(bool show) { _show_value = show; updateGeometryCache(); update(); }
void AudioDial::setValuePrefix(const QString& prefix) { _value_prefix = prefix; update(); }
void AudioDial::setValuePostfix(const QString& postfix) { _value_postfix = postfix; update(); }
void AudioDial::setValueDecimals(int decimals) { _value_decimals = decimals; updateGeometryCache(); update(); }

void AudioDial::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    int label_font_size, value_font_size, size;
    QPointF center;
    float inner_radius, outer_radius;
    std::tie(label_font_size, value_font_size, size, center, inner_radius, outer_radius) = getCircleGeometry();
    float arc_thickness = std::max(2.0f, size * 0.1f);

    QRectF adjusted_rect(center.x() - outer_radius,
                        center.y() - outer_radius,
                        outer_radius * 2,
                        outer_radius * 2);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw dial background
    painter.setBrush(QBrush(inner_color));
    painter.setPen(QPen(inner_outline, 1));
    painter.drawEllipse(center, inner_radius, inner_radius);

    // Background arc
    QPen arcPen(arc_bg_color, arc_thickness);
    arcPen.setCapStyle(Qt::RoundCap);
    painter.setPen(arcPen);
    painter.drawArc(adjusted_rect,
                   int((360 - _start_angle) * 16),
                   int(-_angle_range * 16));

    // Value (gradient arc)
    float value_frac = (_value - _min) / (_max - _min);
    QConicalGradient gradient(center, _start_angle + 110);
    gradient.setColorAt(1.0, gradient_start);
    gradient.setColorAt(0.0, gradient_end);
    QPen pen(QBrush(gradient), arc_thickness);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawArc(adjusted_rect,
                   int((360 - _start_angle) * 16),
                   int(-_angle_range * value_frac * 16));

    // Indicator dot
    float angle = float(M_PI) / 180.0f * (_start_angle + _angle_range * value_frac);
    float dot_radius = inner_radius * 0.6f;
    float dot_size = std::max(2.0f, size * 0.04f);
    float dx = std::cos(angle);
    float dy = std::sin(angle);
    QPointF dot_pos(center.x() + dot_radius * dx, center.y() + dot_radius * dy);

    painter.setBrush(_value <= _min || _value >= _max ? dot_end_color : dot_color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(dot_pos, dot_size, dot_size);

    // --- Labels below dial ---
    float label_y = size * 0.92f;

    if (_show_label && !_label.isEmpty()) {
        painter.setFont(QFont("Segoe UI", label_font_size, QFont::Bold));
        painter.setPen(QColor("#fff"));
        QFontMetrics fm(painter.font());
        int text_w = fm.horizontalAdvance(_label);
        int label_x = (width() - text_w) / 2;
        painter.drawText(label_x, int(label_y) + fm.ascent(), _label);
        label_y += fm.height() - 1;
    }

    if (_show_value) {
        QString value_str;
        if (_value_decimals <= 0) {
            value_str = QString("%1%2%3").arg(_value_prefix).arg(int(_value)).arg(_value_postfix);
        } else {
            value_str = QString("%1%2%3").arg(_value_prefix).arg(QString::number(_value, 'f', _value_decimals)).arg(_value_postfix);
        }
        painter.setFont(QFont("Segoe UI", value_font_size, QFont::Normal));
        painter.setPen(QColor("#b5bac1"));
        QFontMetrics fm(painter.font());
        int text_w = fm.horizontalAdvance(value_str);
        int value_x = (width() - text_w) / 2;
        painter.drawText(value_x, int(label_y) + fm.ascent(), value_str);
    }

    painter.end();
}

float AudioDial::angleToValue(float angle_deg) const {
    float relative_angle = (angle_deg + _start_angle) / float(_angle_range);
    qDebug() << "angle_deg:" << angle_deg << "relative_angle:" << relative_angle;
    float val = _min + relative_angle * (_max - _min);
    return std::clamp(val, _min, _max);
}

bool AudioDial::inCircleArea(const QPoint& pos) {
    int _, __, ___;
    QPointF center;
    float ____, outer_radius;
    std::tie(_, __, ___, center, ____, outer_radius) = getCircleGeometry();
    float dx = pos.x() - center.x();
    float dy = pos.y() - center.y();
    float dist = std::hypot(dx, dy);
    return dist <= outer_radius * 1.15f;
}

void AudioDial::mousePressEvent(QMouseEvent* event) {
    if (!inCircleArea(event->pos())) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        _pressed = true;
        mouseMoveEvent(event); // update on press
        event->accept();
    }
    if (event->button() == Qt::RightButton) {
        setValue(_default_value);
    }
}

void AudioDial::mouseMoveEvent(QMouseEvent* event) {
    if (_pressed) {
        auto [label_font_size, value_font_size, size, center, inner_radius, outer_radius] = getCircleGeometry();
        float dx = event->pos().x() - center.x();
        float dy = event->pos().y() - center.y();
        float angle = std::atan2(dy, dx) * 180.0f / float(M_PI) + 90.0f;
        if (angle < -180.0f) {
            angle += 360.0f;
        } else if (angle > 180.0f) {
            angle -= 360.0f;
        }
        float value = angleToValue(angle);
        setValue(value);
        event->accept();
    }
}

void AudioDial::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _pressed = false;
        event->accept();
    }
}

void AudioDial::wheelEvent(QWheelEvent* event) {
    float step = 1.0f;
    if (_value_decimals > 0) {
        step = (_max - _min) / 50.0f;
    }
    if (event->angleDelta().y() > 0) {
        setValue(_value + step);
    } else {
        setValue(_value - step);
    }
    event->accept();
}