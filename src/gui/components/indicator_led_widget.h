#pragma once

#include <QColor>
#include <QWidget>

/**
 * @brief The IndicatorLedWidget class provides a simple LED indicator widget.
 * It can be used to visually indicate the status of a process or component.
 */
class IndicatorLedWidget : public QWidget {
    Q_OBJECT
  public:
    /**
     * @brief Constructor for the IndicatorLedWidget.
     * @param color The color of the LED indicator. Default is green.
     * @param parent The parent widget. Default is nullptr.
     */
    explicit IndicatorLedWidget(const QColor &color = Qt::green,
                                QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /**
     * @brief Gets the current color of the LED indicator.
     * @return The current color.
     */
    QColor getColor() const { return led_color; }

    /**
     * @brief Gets the active state of the LED indicator.
     * @return True if the LED is active, false otherwise.
     */
    bool isActive() const { return is_active; }

  public slots:
    /**
     * @brief Sets the active state of the LED indicator.
     * @param active If true, the LED is turned on; otherwise, it is turned off.
     */
    void setActive(bool active);

    /**
     * @brief Sets the color of the LED indicator.
     * @param color The new color for the LED indicator.
     */
    void setColor(const QColor &color);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QColor led_color;
    bool is_active;
};