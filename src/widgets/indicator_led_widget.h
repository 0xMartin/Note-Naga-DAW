#pragma once

#include <QWidget>
#include <QColor>

class IndicatorLedWidget : public QWidget
{
    Q_OBJECT
public:
    explicit IndicatorLedWidget(const QColor &color = Qt::green, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor ledColor;
    bool isActive;
};