#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QString>
#include <QList>
#include <QMap>

class ButtonGroupWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ButtonGroupWidget(
        const QStringList& buttonNames,
        const QList<QIcon>& buttonIcons,
        const QStringList& tooltips = {},
        QSize buttonSize = QSize(36, 36),
        bool checkable = false,
        QWidget* parent = nullptr);

    QPushButton* button(const QString& name) const;

signals:
    void buttonClicked(const QString& name);

private:
    QHBoxLayout* m_layout;
    QList<QPushButton*> m_buttonOrder;
    QMap<QString, QPushButton*> m_buttons; 
    QSize m_buttonSize;
    void updateButtonStyles();
};