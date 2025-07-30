#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

/**
 * @brief A custom dial widget for audio controls.
 * This widget allows users to adjust a value within a specified range using a circular dial.
 * It supports features like gradient backgrounds, labels, and value display.
 */
class AudioDialCentered : public QWidget {
    Q_OBJECT
public:
    explicit AudioDialCentered(QWidget *parent = nullptr);

    /**
     * @brief Get the current value of the dial.
     * @return The current value of the dial.
     */
    float getValue() const { return _value; }

    /**
     * @brief Set the value of the dial.
     * @param value The new value to set.
     */
    void setValue(float value);

    /**
     * @brief Set default value for the dial.
     * @param value The default value to set.
     */
    void setDefaultValue(float value) { _default_value = value; }

    /**
     * @brief Set the range of values for the dial.
     * @param min_val The minimum value.
     * @param max_val The maximum value.
     */
    void setRange(float min_val, float max_val);

    /**
     * @brief Set fill gradient colors for the dial value progress arc.
     * @param color_start The starting color of the gradient.
     * @param color_end The ending color of the gradient.
     */
    void setGradient(const QColor &color_start, const QColor &color_end);

    /**
     * @brief Set label text displayed above the dial.
     * @param label The text to display as the label.
     */
    void setLabel(const QString &label);

    /**
     * @brief Show or hide the label above the dial.
     * @param show True to show the label, false to hide it.
     */
    void showLabel(bool show);

    /**
     * @brief Show or hide the value displayed below the dial.
     * @param show True to show the value, false to hide it.
     */
    void showValue(bool show);

    /**
     * @brief Set the prefix for the value displayed below the dial.
     * @param prefix The prefix string to display before the value.
     */
    void setValuePrefix(const QString &prefix);

    /**
     * @brief Set the postfix for the value displayed below the dial.
     * @param postfix The postfix string to display after the value.
     */
    void setValuePostfix(const QString &postfix);

    /**
     * @brief Set the number of decimal places for the value displayed below the dial.  Swith to
     * integer mode if set to 0.
     * @param decimals The number of decimal places to display.
     */
    void setValueDecimals(int decimals);

    /**
     * @brief Set the names of options for the dial. Available only in integer mode (set number of
     * decimals to 0).
     * @param names A list of option names to display.
     */
    void setOptionNames(QStringList names) { _option_names = std::move(names); }

    // --- PUBLIC COLORS ---
    QColor bg_color = QColor("#3a3f45");
    QColor inner_outline = QColor("#111");
    QColor arc_bg_color = QColor("#1e1e20");
    QColor tick_color = QColor("#6cb0ff");
    QColor tick_end_color = QColor("#ff50f9");
    QColor gradient_start = QColor("#6cb0ff");
    QColor gradient_end = QColor("#ae6cff");
    QColor center_gradient_start = QColor("#232731");
    QColor center_gradient_end = QColor("#3e4a5a");

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct DialGeometry {
        int label_font_size;
        int value_font_size;
        int dial_size;
        QPointF center;
        float inner_radius;
        float outer_radius;
        QRectF dial_rect;
        float arc_thickness;
        float tick_length;
        QRectF inner_rect;
        int label_y;
        int value_y;
    };

    void updateGeometryCache();
    const DialGeometry &geometry() const;

    float valueToAngle(float value) const;
    bool inCircleArea(const QPoint &pos);

    float _min;
    float _max;
    float _value;
    float _default_value;

    int _start_angle;
    int _angle_range;

    bool _pressed;

    QString _label;
    bool _show_label;
    bool _show_value;
    QStringList _option_names;
    QString _value_prefix;
    QString _value_postfix;
    int _value_decimals;

    mutable DialGeometry _geometry_cache;
    mutable QSize _last_size;
    mutable bool _last_label, _last_value;
    mutable QString _last_label_text;
    mutable int _last_decimals;

signals:
    void valueChanged(float value);
};