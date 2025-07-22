#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QWidget>

/**
 * @brief A custom audio dial widget for displaying and adjusting audio levels or values.
 */
class AudioDial : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a new AudioDial widget.
     * @param parent The parent widget.
     */
    explicit AudioDial(QWidget *parent = nullptr);

    /**
     * @brief Returns the current value of the dial.
     * @return The current value.
     */
    float getValue() const { return _value; }

    /**
     * @brief Sets the value of the dial.
     * @param value The value to set, clamped between min and max.
     */
    void setValue(float value);

    /**
     * @brief Sets the default value of the dial.
     * @param value The default value to set.
     */
    void setDefaultValue(float value);

    /**
     * @brief Sets the range of the dial.
     * @param min_val The minimum value.
     * @param max_val The maximum value.
     */
    void setRange(float min_val, float max_val);

    /**
     * @brief Sets the gradient colors for the dial. Color of progress arc.
     * @param color_start The starting color of the gradient.
     * @param color_end The ending color of the gradient.
     */
    void setGradient(const QColor &color_start, const QColor &color_end);

    /**
     * @brief Sets label text for the dial.
     * @param label The label text to display.
     */
    void setLabel(const QString &label);

    /**
     * @brief Sets if the label should be shown.
     * @param show True to show the label, false to hide it.
     */
    void showLabel(bool show);

    /**
     * @brief Sets if the value should be shown.
     * @param show True to show the value, false to hide it.
     */
    void showValue(bool show);

    /**
     * @brief Sets the prefix for the value display.
     * @param prefix The prefix string to display before the value.
     */
    void setValuePrefix(const QString &prefix);

    /**
     * @brief Sets the postfix for the value display.
     * @param postfix The postfix string to display after the value.
     */
    void setValuePostfix(const QString &postfix);

    /**
     * @brief Sets the number of decimal places for the value display.
     * @param decimals The number of decimal places to display.
     */
    void setValueDecimals(int decimals);

signals:
    /**
     * @brief Signal emitted when the value changes.
     * @param value The new value of the dial.
     */
    void valueChanged(float value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateGeometryCache();
    std::tuple<int, int, int, QPointF, float, float> getCircleGeometry();
    float angleToValue(float angle_deg) const;
    bool inCircleArea(const QPoint &pos);

    float _min;
    float _max;
    float _value;
    float _default_value;

    int _start_angle;
    int _angle_range;

    QColor bg_color;
    QColor inner_color;
    QColor inner_outline;
    QColor arc_bg_color;
    QColor dot_color;
    QColor dot_end_color;
    QColor gradient_start;
    QColor gradient_end;

    bool _pressed;

    QString _label;
    bool _show_label;
    bool _show_value;
    QString _value_prefix;
    QString _value_postfix;
    int _value_decimals;

    // Geometry cache
    std::tuple<int, int, int, QPointF, float, float> _geometry_cache;
    std::tuple<int, int, bool, bool, QString, int> _geometry_cache_size;
};