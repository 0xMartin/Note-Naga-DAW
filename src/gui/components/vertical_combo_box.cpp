#include "vertical_combo_box.h"
#include <QPainter>
#include <QFontMetrics>
#include <QStyleOptionButton>
#include <QDebug>
#include <QTextLayout>

// --- VerticalTextButton ---

VerticalTextButton::VerticalTextButton(QWidget *parent)
    : QPushButton(parent)
{
    setFixedWidth(kFixedWidth);
    setMaximumHeight(kMaxHeight);
    setStyleSheet("QPushButton { min-width: 20px; min-height: 55px; }");
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void VerticalTextButton::setText(const QString &text) {
    m_text = text;
    updateGeometry();
    update();
}

QString VerticalTextButton::text() const {
    return m_text;
}

QSize VerticalTextButton::sizeHint() const {
    return QSize(kFixedWidth, kMaxHeight);
}

QSize VerticalTextButton::minimumSizeHint() const {
    return QSize(kFixedWidth, 28);
}

void VerticalTextButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QStyleOptionButton option;
    initStyleOption(&option);

    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButtonBevel, &option, &painter, this);

    painter.save();
    painter.translate(width() / 2, height() / 2);
    painter.rotate(-90);
    painter.translate(-height() / 2, -width() / 2);

    // Elide text if too long
    QFontMetrics fm(font());
    QString elided = fm.elidedText(m_text, Qt::ElideRight, height() - 10); // nech trochu paddingu

    QRect rect(0, 0, height(), width());
    painter.setPen(option.palette.buttonText().color());
    painter.setFont(font());
    painter.drawText(rect, Qt::AlignCenter, elided);

    painter.restore();
}

// --- VerticalComboBox ---

VerticalComboBox::VerticalComboBox(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    currentButton = new VerticalTextButton(this);

    connect(currentButton, &QPushButton::clicked, this, [this]() {
        QMenu menu;
        for (size_t i = 0; i < items.size(); ++i) {
            QAction *action = menu.addAction(items[i].text);
            connect(action, &QAction::triggered, this, [this, i]() {
                if (currentIdx != static_cast<int>(i)) {
                    currentIdx = static_cast<int>(i);
                    updateButtonText();
                    emit currentIndexChanged(currentIdx);
                }
            });
        }
        menu.exec(currentButton->mapToGlobal(QPoint(0, currentButton->height())));
    });

    layout->addWidget(currentButton);
    updateButtonText();
}

void VerticalComboBox::addItem(const QString &text, const QVariant &userData) {
    items.push_back({text, userData});
    if (items.size() == 1) {
        currentIdx = 0;
        updateButtonText();
    }
}

void VerticalComboBox::clear() {
    items.clear();
    currentIdx = -1;
    updateButtonText();
}

int VerticalComboBox::currentIndex() const { return currentIdx; }

void VerticalComboBox::setCurrentIndex(int index) {
    if (index >= 0 && index < static_cast<int>(items.size()) && index != currentIdx) {
        currentIdx = index;
        updateButtonText();
        emit currentIndexChanged(currentIdx);
    }
}

QString VerticalComboBox::currentText() const {
    if (currentIdx >= 0 && currentIdx < static_cast<int>(items.size()))
        return items[currentIdx].text;
    return QString();
}

QVariant VerticalComboBox::currentData() const {
    if (currentIdx >= 0 && currentIdx < static_cast<int>(items.size()))
        return items[currentIdx].userData;
    return QVariant();
}

int VerticalComboBox::findText(const QString &text) const {
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].text == text)
            return static_cast<int>(i);
    }
    return -1;
}

void VerticalComboBox::blockSignals(bool block) {
    QWidget::blockSignals(block);
}

QString VerticalComboBox::itemText(int index) const {
    if (index >= 0 && index < static_cast<int>(items.size()))
        return items[index].text;
    return QString();
}

QVariant VerticalComboBox::itemData(int index) const {
    if (index >= 0 && index < static_cast<int>(items.size()))
        return items[index].userData;
    return QVariant();
}

void VerticalComboBox::updateButtonText() {
    if (currentIdx >= 0 && currentIdx < static_cast<int>(items.size()))
        currentButton->setText(items[currentIdx].text);
    else
        currentButton->setText("None");
}