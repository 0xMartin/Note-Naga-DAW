#include "vertical_label.h"

#include <QPainter>
#include <QFontMetrics>
#include <QStyleOption>

VerticalTitleLabel::VerticalTitleLabel(const QString& text, QWidget* parent)
    : QWidget(parent), m_text(text)
{
    m_font = QFont("Segoe UI", 18, QFont::Bold);
    setFont(m_font);
}

void VerticalTitleLabel::setText(const QString& text) {
    m_text = text;
    updateGeometry();
    update();
}

void VerticalTitleLabel::setFont(const QFont& font) {
    m_font = font;
    updateGeometry();
    update();
}

QSize VerticalTitleLabel::sizeHint() const {
    QFontMetrics fm(m_font);
    // Výška fontu = šířka widgetu, šířka textu = výška widgetu (kvůli rotaci)
    int w = fm.height() + 8; // +padding
    int h = fm.horizontalAdvance(m_text) + 8;
    return QSize(w, h);
}

QSize VerticalTitleLabel::minimumSizeHint() const {
    return sizeHint();
}

void VerticalTitleLabel::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(m_font);

    QFontMetrics fm(m_font);
    int textWidth = fm.horizontalAdvance(m_text);
    int textHeight = fm.height();

    int widgetWidth = width();
    int widgetHeight = height();

    // Zarovnej text na střed widgetu v ose X, horní okraj v ose Y
    painter.save();

    // Posuň na střed widgetu v X, horní Y
    painter.translate(widgetWidth / 2 - textHeight / 2, textWidth);

    // Otoč o -90°
    painter.rotate(-90);

    // Pozice textu po rotaci: střed textu v X, horní okraj v Y
    QRect textRect(0, 0, textWidth, textHeight);

    painter.setPen(QColor("#79b8ff"));
    painter.drawText(textRect, Qt::AlignCenter, m_text);
    painter.restore();
}