#pragma once

#include <QWidget>
#include <QPushButton>
#include <QMenu>
#include <QVBoxLayout>
#include <QVariant>
#include <vector>

// --- Vertikální tlačítko ---
class VerticalTextButton : public QPushButton {
    Q_OBJECT
public:
    explicit VerticalTextButton(QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    static constexpr int kFixedWidth = 20;
    static constexpr int kMaxHeight = 110;
};

// --- VerticalComboBox ---
class VerticalComboBox : public QWidget {
    Q_OBJECT
public:
    explicit VerticalComboBox(QWidget *parent = nullptr);

    void addItem(const QString &text, const QVariant &userData = QVariant());
    void clear();
    int currentIndex() const;
    void setCurrentIndex(int index);
    QString currentText() const;
    QVariant currentData() const;
    int findText(const QString &text) const;
    void blockSignals(bool block);
    QString itemText(int index) const;
    QVariant itemData(int index) const;

signals:
    void currentIndexChanged(int index);

private:
    struct Item {
        QString text;
        QVariant userData;
    };

    VerticalTextButton *currentButton;
    std::vector<Item> items;
    int currentIdx = -1;

    void updateButtonText();
};