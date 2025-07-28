#pragma once

#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>

class AudioVerticalSlider : public QWidget
{
    Q_OBJECT
public:
    explicit AudioVerticalSlider(QWidget* parent = nullptr);

    int value() const { return m_value; }
    void setValue(int v);

    int minimum() const { return m_min; }
    int maximum() const { return m_max; }
    void setRange(int min, int max);

    void setLabelVisible(bool visible);
    void setValueVisible(bool visible);
    bool isLabelVisible() const { return m_labelVisible; }
    bool isValueVisible() const { return m_valueVisible; }

    void setLabelText(const QString& text);
    QString labelText() const { return m_labelText; }

    void setValuePrefix(const QString& prefix);
    void setValuePostfix(const QString& postfix);

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRect sliderGrooveRect() const;
    QRect handleRect() const;
    int valueFromPosition(int y) const;
    int positionFromValue(int value) const;
    void updateTextSizes();

    int m_min = 0;
    int m_max = 100;
    int m_value = 50;

    bool m_dragging = false;
    int m_dragOffset = 0;

    bool m_labelVisible = true;
    bool m_valueVisible = true;
    QString m_labelText = "Volume";
    QString m_valuePrefix;
    QString m_valuePostfix;

    int m_labelFontSize = 10;
    int m_valueFontSize = 10;
};
