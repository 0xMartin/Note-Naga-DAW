#include "dock_title_bar.h"

#include "advanced_dock_widget.h"
#include <QStyle>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QTransform>

AdvancedDockTitleBar::AdvancedDockTitleBar(
    AdvancedDockWidget* dock, const QString& title, const QIcon& icon, QWidget* customButtonWidget,
    bool horizontal)
    : QFrame(dock), dockWidget(dock), horizontal(horizontal)
{
    setObjectName("AdvancedDockTitleBar");
    setStyleSheet(
        "QFrame#AdvancedDockTitleBar {"
        "  background: #2b2f37;"
        "  border: 1px solid #19191f;"
        "  margin-bottom: 0px; "
        "}");

    if (horizontal) {
        // horizontal layout
        QHBoxLayout* main_layout = new QHBoxLayout(this);
        main_layout->setContentsMargins(10, 0, 0, 0);
        main_layout->setSpacing(6);

        // title icon
        iconLabel = new QLabel(this);
        iconLabel->setPixmap(icon.pixmap(20, 20));
        iconLabel->setFixedSize(20, 20);
        main_layout->addWidget(iconLabel, 0, Qt::AlignVCenter);

        // title label
        horizontalTitleLabel = new QLabel(title, this);
        horizontalTitleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #79b8ff; letter-spacing: 1.2px;");
        main_layout->addWidget(horizontalTitleLabel, 0, Qt::AlignVCenter);

        main_layout->addStretch(1);

        // custom buttons
        customButtons = nullptr;
        setCustomButtonWidget(customButtonWidget);
        if (customButtons) {
            main_layout->addWidget(customButtons, 0, Qt::AlignVCenter);
            main_layout->addSpacing(2);
        }

        // Default control buttons
        defaultButtons = new QFrame(this);
        defaultButtons->setObjectName("DefaultDockButtonsFrame");
        defaultButtons->setStyleSheet(
            "QFrame#DefaultDockButtonsFrame {"
            " background: #24272e;"
            " padding-left: 0px; padding-right: 0px;"
            "}");

        QHBoxLayout* btnLayout = new QHBoxLayout(defaultButtons);
        btnLayout->setContentsMargins(2, 2, 2, 2);
        btnLayout->setSpacing(0);

        QPushButton* floatBtn = createDefaultButton(QIcon(":/icons/maximize.svg"), "Undock / Dock", SLOT(onFloatClicked()));
        btnLayout->addWidget(floatBtn);

        QPushButton* closeBtn = createDefaultButton(QIcon(":/icons/close.svg"), "Close", SLOT(onCloseClicked()));
        btnLayout->addWidget(closeBtn);

        main_layout->addWidget(defaultButtons, 0, Qt::AlignVCenter);

    } else {
        // vertical layout
        QVBoxLayout* main_layout = new QVBoxLayout(this);
        main_layout->setContentsMargins(0, 8, 0, 0);
        main_layout->setSpacing(0);

        // title icon
        iconLabel = new QLabel(this);
        QPixmap pm = icon.pixmap(24, 24);
        QTransform tr;
        tr.rotate(-90);
        pm = pm.transformed(tr);
        iconLabel->setPixmap(pm);
        iconLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        iconLabel->setFixedSize(24, 24);
        main_layout->addWidget(iconLabel, 0, Qt::AlignHCenter | Qt::AlignTop);

        main_layout->addSpacing(4);

        // title label
        verticalTitleLabel = new VerticalTitleLabel(title, this);
        main_layout->addWidget(verticalTitleLabel, 1, Qt::AlignHCenter | Qt::AlignTop);

        main_layout->addSpacing(16);

        // custom buttons
        customButtons = nullptr;
        setCustomButtonWidget(customButtonWidget);
        if (customButtons) {
            main_layout->addWidget(customButtons, 0, Qt::AlignHCenter | Qt::AlignBottom);
            main_layout->addSpacing(2);
        }

        // Default control buttons
        defaultButtons = new QFrame(this);
        defaultButtons->setObjectName("DefaultDockButtonsFrame");
        defaultButtons->setStyleSheet(
            "QFrame#DefaultDockButtonsFrame {"
            " background: #24272e;"
            " padding-left: 0px; padding-right: 0px;"
            "}");

        QVBoxLayout* btnLayout = new QVBoxLayout(defaultButtons);
        btnLayout->setContentsMargins(2, 2, 2, 2);
        btnLayout->setSpacing(0);

        QPushButton* floatBtn = createDefaultButton(QIcon(":/icons/maximize.svg"), "Undock / Dock", SLOT(onFloatClicked()));
        btnLayout->addWidget(floatBtn);

        QPushButton* closeBtn = createDefaultButton(QIcon(":/icons/close.svg"), "Close", SLOT(onCloseClicked()));
        btnLayout->addWidget(closeBtn);

        main_layout->addWidget(defaultButtons, 0, Qt::AlignHCenter | Qt::AlignBottom);
    }
}

void AdvancedDockTitleBar::setTitleText(const QString& text) {
    if (horizontal) {
        horizontalTitleLabel->setText(text);
    } else {
        verticalTitleLabel->setText(text);
    }
    update();
}

void AdvancedDockTitleBar::setTitleIcon(const QIcon& icon) {
    if (horizontal) {
        iconLabel->setPixmap(icon.pixmap(23, 23));
    } else {
        QPixmap pm = icon.pixmap(20, 20);
        QTransform tr;
        tr.rotate(-90);
        pm = pm.transformed(tr);
        iconLabel->setPixmap(pm);
    }
    update();
}

void AdvancedDockTitleBar::setCustomButtonWidget(QWidget* widget) {
    if (customButtons && customButtons->parent() == this) {
        customButtons->hide();
        customButtons->deleteLater();
    }
    customButtons = widget;
    if (customButtons) {
        customButtons->setParent(this);
        customButtons->setStyleSheet("QWidget { background: transparent; }");
        customButtons->show();
    }
}

QPushButton* AdvancedDockTitleBar::createDefaultButton(const QIcon& icon, const QString& tooltip, const char* slotName) {
    QPushButton* btn = new QPushButton(icon, "", this);
    btn->setToolTip(tooltip);
    btn->setFixedSize(24, 24);
    btn->setFlat(true);
    btn->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 0px; min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px; padding: 0px;}"
                       "QPushButton:hover { background: #5a35a0; color: #fff; }");
    connect(btn, SIGNAL(clicked()), this, slotName);
    return btn;
}

void AdvancedDockTitleBar::onFloatClicked() {
    if (dockWidget) dockWidget->setFloating(!dockWidget->isFloating());
}

void AdvancedDockTitleBar::onCloseClicked() {
    if (dockWidget) dockWidget->close();
}

void AdvancedDockTitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dockWidget) {
        setCursor(Qt::SizeAllCursor);
        dragging = true;
        dragStartPos = event->globalPos();
        event->accept();
    } else {
        QFrame::mousePressEvent(event);
    }
}

void AdvancedDockTitleBar::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        if (dockWidget && dockWidget->isFloating()) {
            // For floating docks, just move the window - don't trigger dock overlay logic
            QWidget* window = dockWidget->window();
            if (window && window != dockWidget) {
                QPoint delta = event->globalPos() - dragStartPos;
                window->move(window->pos() + delta);
                dragStartPos = event->globalPos();
            }
        } else if (dockWidget && !dockWidget->isFloating()) {
            // For docked widgets, use our custom drag logic only if still docked
            dockWidget->startDragFromTitleBar(dragStartPos, event->globalPos());
        }
        event->accept();
    }
}

void AdvancedDockTitleBar::mouseReleaseEvent(QMouseEvent* event) {
    unsetCursor();
    if (dragging) {
        dragging = false;
        if (dockWidget) dockWidget->endDragFromTitleBar();
        event->accept();
    }
}

void AdvancedDockTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (dockWidget && dockWidget->isFloating()) {
        dockWidget->toggleMaximizeRestoreFloating();
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}