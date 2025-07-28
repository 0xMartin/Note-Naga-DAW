#include "button_group_widget.h"
#include <QStyle>
#include <QDebug>

ButtonGroupWidget::ButtonGroupWidget(
    const QStringList& buttonNames,
    const QList<QIcon>& buttonIcons,
    const QStringList& tooltips,
    QSize buttonSize,
    bool checkable,
    QWidget* parent
)
    : QWidget(parent), m_buttonSize(buttonSize)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_layout = new QHBoxLayout(this);
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(3, 3, 3, 3);

    int count = buttonNames.size();
    for (int i = 0; i < count; ++i) {
        QPushButton* btn = new QPushButton(this);
        btn->setObjectName(buttonNames[i]);
        btn->setIcon(buttonIcons.value(i, QIcon()));
        btn->setIconSize(buttonSize);

        int w = buttonSize.width();
        int h = buttonSize.height();
        if (i == 0 || i == count - 1)
            w = int(w * 1.2);

        btn->setMinimumSize(w, h);
        btn->setMaximumSize(w, h);
        btn->setFixedSize(w, h);

        if (!tooltips.isEmpty())
            btn->setToolTip(tooltips.value(i, QString()));

        btn->setCheckable(checkable);

        connect(btn, &QPushButton::clicked, this, [this, name=buttonNames[i]](){
            emit buttonClicked(name);
        });

        m_layout->addWidget(btn);
        m_buttonOrder.append(btn);          // pořadí!
        m_buttons.insert(buttonNames[i], btn); // lookup
    }

    updateButtonStyles();
}

QPushButton* ButtonGroupWidget::button(const QString& name) const
{
    return m_buttons.value(name, nullptr);
}

void ButtonGroupWidget::updateButtonStyles()
{
    int n = m_buttonOrder.size();
    for (int i = 0; i < n; ++i) {
        QPushButton* btn = m_buttonOrder[i];
        QString style;
        // Base style + rozměry
        style += QString(R"(
            QPushButton {
                background-color: qlineargradient(spread:repeat, x1:1, y1:0, x2:1, y2:1, stop:0 #303239,stop:1 #2e3135);
                color: #fff;
                border-style: solid;
                border-width: 1px;
                border-color: #494d56;
                padding: 5px;
                min-width: %1px;
                max-width: %1px;
                min-height: %2px;
                max-height: %2px;
            }
            QPushButton:hover {
                background-color: #293f5b;
                border: 1px solid #3277c2;
            }
            QPushButton:pressed {
                background-color: #37404a;
                border: 1px solid #506080;
            }
            QPushButton:checked {
                background: #3477c0;
                border: 1.9px solid #79b8ff;
            }
        )").arg(btn->width()).arg(btn->height());

        // Zaoblení podle indexu:
        if (i == 0) {
            // První tlačítko: zaoblené zleva
            style += QString(
                "QPushButton {"
                " border-top-left-radius: %1px;"
                " border-bottom-left-radius: %1px;"
                " border-top-right-radius: 0px;"
                " border-bottom-right-radius: 0px;"
                "}"
            ).arg(m_buttonSize.height()/2);
        } else if (i == n-1) {
            // Poslední tlačítko: zaoblené zprava
            style += QString(
                "QPushButton {"
                " border-top-right-radius: %1px;"
                " border-bottom-right-radius: %1px;"
                " border-top-left-radius: 0px;"
                " border-bottom-left-radius: 0px;"
                "}"
            ).arg(m_buttonSize.height()/2);
        } else {
            // Prostřední tlačítka: hranaté
            style += "QPushButton { border-radius: 0px; }";
        }
        btn->setStyleSheet(style);
    }
    // Skupina jako celek: oválný vzhled
    setStyleSheet(QString(
        "ButtonGroupWidget { border-radius: %1px; background: #32353b; }"
    ).arg(m_buttonSize.height()/2));
}