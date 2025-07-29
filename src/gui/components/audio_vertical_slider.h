#pragma once

#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>

/**
 * @brief Vertical slider widget supporting float values. 
 */
class AudioVerticalSlider : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a vertical slider with default range [0.0, 100.0].
     * @param parent Parent widget.
     */
    explicit AudioVerticalSlider(QWidget* parent = nullptr);

    /** 
     * @brief Gets the current value of the slider.
     * @return Current value in the range [minimum, maximum].
     */
    float value() const { return m_value; }

    /**
     * @brief Sets the current value of the slider.
     * @param v New value in the range [minimum, maximum].
     */
    void setValue(float v);

    /**
     * @brief Gets the minimum value of the slider.
     * @return Minimum value.
     */
    float minimum() const { return m_min; }

    /**
     * @brief Sets the minimum value of the slider.
     * @param min New minimum value.
     */
    float maximum() const { return m_max; }

    /**
     * @brief Sets the maximum value of the slider.
     * @param max New maximum value.
     */
    void setRange(float min, float max);

    /**
     * @brief Sets the label visibility.
     * @param visible New visibility state.
     */
    void setLabelVisible(bool visible);

    /**
     * @brief Sets the value visibility.
     * @param visible New visibility state.
     */
    void setValueVisible(bool visible);

    /**
     * @brief Gets the current label visibility state.
     * @return True if label is visible, false otherwise.
     */
    bool isLabelVisible() const { return m_labelVisible; }

    /**
     * @brief Gets the current value visibility state.
     * @return True if value is visible, false otherwise.
     */
    bool isValueVisible() const { return m_valueVisible; }

    /**
     * @brief Sets the label text.
     * @param text New label text.
     */
    void setLabelText(const QString& text);

    /**
     * @brief Gets the current label text.
     * @return Current label text.
     */
    QString labelText() const { return m_labelText; }

    /**
     * @brief Sets the prefix for the value displayed below the slider.
     * @param prefix The prefix to display before the value.
     */
    void setValuePrefix(const QString& prefix);

    /**
     * @brief Sets the postfix for the value displayed below the slider.
     * @param postfix The postfix to display after the value.
     */
    void setValuePostfix(const QString& postfix);

    /**
     * @brief Set the number of decimal places for the value displayed below the dial.
     * @param decimals The number of decimal places to display.
     */
    void setValueDecimals(int decimals);

    // --- PUBLIC COLORS ---
    QColor grooveBgColor = QColor("#232731");
    QColor grooveOutlineColor = QColor("#4a4d56");
    QColor grooveGradientStart = QColor("#6cb0ff");
    QColor grooveGradientEnd = QColor("#ff50f9");
    QColor scaleMajorColor = QColor("#4a4d56");
    QColor scaleMinorColor = QColor("#494d56");
    QColor handleFillColor = QColor("#2b2e33");
    QColor handleOutlineColor = QColor("#232731");
    QColor handleGrooveColor = QColor("#44474c");
    QColor labelColor = Qt::white;
    QColor valueColor = QColor("#b1b1b1");

signals:
    void valueChanged(float value);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRect sliderGrooveRect() const;
    QRect handleRect() const;
    float valueFromPosition(int y) const;
    int positionFromValue(float value) const;
    void updateTextSizes();

    float m_min = 0.0f;
    float m_max = 100.0f;
    float m_value = 50.0f;

    bool m_dragging = false;
    int m_dragOffset = 0;

    bool m_labelVisible = true;
    bool m_valueVisible = true;
    QString m_labelText = "Volume";
    QString m_valuePrefix;
    QString m_valuePostfix;

    int m_labelFontSize = 10;
    int m_valueFontSize = 8;
    int m_valueDecimals = 2;

    int limitHandleY(int y, int handleH, const QRect& groove) const;
};