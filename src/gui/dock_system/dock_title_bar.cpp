#include "dock_title_bar.h"

#include "advanced_dock_widget.h"
#include <QStyle>
#include <QMouseEvent>
#include <QHBoxLayout>

CustomDockTitleBar::CustomDockTitleBar(
    AdvancedDockWidget* dock, const QString& title, const QIcon& icon, QWidget* customButtonWidget)
    : QFrame(dock), dockWidget(dock)
{
    setObjectName("CustomDockTitleBar");
    setStyleSheet(
        "QFrame#CustomDockTitleBar {"
        "  background: #2b2f37;"
        "  border-top-left-radius: 9px;"
        "  border-top-right-radius: 9px;"
        "  margin-bottom: 0px; "
        "}");

    QHBoxLayout* header_layout = new QHBoxLayout(this);
    header_layout->setContentsMargins(10, 0, 0, 0);
    header_layout->setSpacing(6);

    // Icon
    iconLabel = new QLabel(this);
    iconLabel->setPixmap(icon.pixmap(23, 23));
    iconLabel->setFixedSize(23, 23);
    header_layout->addWidget(iconLabel, 0, Qt::AlignVCenter);

    // Title
    titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #79b8ff; letter-spacing: 1.2px;");
    header_layout->addWidget(titleLabel, 0, Qt::AlignVCenter);

    header_layout->addStretch(1);

    // Custom button area
    customButtons = nullptr;
    setCustomButtonWidget(customButtonWidget);
    if (customButtons) {
        header_layout->addWidget(customButtons, 0, Qt::AlignVCenter);
        header_layout->addSpacing(4);
    }

    // Default control buttons
    defaultButtons = new QFrame(this);
    defaultButtons->setObjectName("DefaultDockButtonsFrame");
    defaultButtons->setStyleSheet(
        "QFrame#DefaultDockButtonsFrame {"
        " background: #26292f;"
        " border-radius: 7px;"
        " padding-left: 0px; padding-right: 0px;"
        "}");

    QHBoxLayout* btnLayout = new QHBoxLayout(defaultButtons);
    btnLayout->setContentsMargins(2, 2, 2, 2);
    btnLayout->setSpacing(0);

    QPushButton* floatBtn = createDefaultButton(QIcon(":/icons/maximize.svg"), "Undock", SLOT(onFloatClicked()));
    btnLayout->addWidget(floatBtn);

    QPushButton* closeBtn = createDefaultButton(QIcon(":/icons/close.svg"), "Close", SLOT(onCloseClicked()));
    btnLayout->addWidget(closeBtn);

    header_layout->addWidget(defaultButtons, 0, Qt::AlignVCenter);
}

void CustomDockTitleBar::setTitleText(const QString& text) {
    titleLabel->setText(text);
}

void CustomDockTitleBar::setTitleIcon(const QIcon& icon) {
    iconLabel->setPixmap(icon.pixmap(23, 23));
}

void CustomDockTitleBar::setCustomButtonWidget(QWidget* widget) {
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

QPushButton* CustomDockTitleBar::createDefaultButton(const QIcon& icon, const QString& tooltip, const char* slotName) {
    QPushButton* btn = new QPushButton(icon, "", this);
    btn->setToolTip(tooltip);
    btn->setFixedSize(24, 24);
    btn->setFlat(true);
    btn->setStyleSheet("QPushButton { background: transparent; border: none; "
                       "border-radius: 6px; min-width: 24px; max-width: 24px; "
                       "min-height: 24px; max-height: 24px; padding: 0px;}"
                       "QPushButton:hover { background: #3477c0; color: #fff; }");
    connect(btn, SIGNAL(clicked()), this, slotName);
    return btn;
}

void CustomDockTitleBar::onFloatClicked() {
    if (dockWidget) dockWidget->setFloating(!dockWidget->isFloating());
}

void CustomDockTitleBar::onCloseClicked() {
    if (dockWidget) dockWidget->close();
}

void CustomDockTitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        setCursor(Qt::SizeAllCursor);
        dragging = true;
        dragStartPos = event->globalPos();
        event->accept();
    } else {
        QFrame::mousePressEvent(event);
    }
}

void CustomDockTitleBar::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        if (dockWidget && dockWidget->isFloating()) {
            QPoint delta = event->globalPos() - dragStartPos;
            dockWidget->window()->move(dockWidget->window()->pos() + delta);
            dockWidget->startDragFromTitleBar(dragStartPos, event->globalPos());
            dragStartPos = event->globalPos();
        } else if (dockWidget) {
            dockWidget->startDragFromTitleBar(dragStartPos, event->globalPos());
        }
        event->accept();
    }
}

void CustomDockTitleBar::mouseReleaseEvent(QMouseEvent* event) {
    unsetCursor();
    if (dragging) {
        dragging = false;
        if (dockWidget) dockWidget->endDragFromTitleBar();
        event->accept();
    }
}

void CustomDockTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (dockWidget && dockWidget->isFloating()) {
        dockWidget->toggleMaximizeRestoreFloating();
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}