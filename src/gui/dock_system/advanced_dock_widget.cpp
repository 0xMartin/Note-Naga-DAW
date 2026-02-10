#include "advanced_dock_widget.h"

#include <QApplication>
#include <QMainWindow>
#include <QMouseEvent>
#include <QStyleOptionDockWidget>
#include <QStylePainter>

#include "dock_indicator_overlay.h"
#include "dock_title_bar.h"

AdvancedDockWidget::AdvancedDockWidget(const QString &title, const QIcon &icon,
                                       QWidget *customButtonWidget, QWidget *parent,
                                       TitleBarPosition titleBarPosition)
    : QDockWidget(title, parent) {
        
    this->setFeatures(DockWidgetMovable | DockWidgetFloatable | DockWidgetClosable);
    this->setMouseTracking(true);
    this->setStyleSheet("QDockWidget { border: 1px solid #19191f; }");

    this->title_bar_position = titleBarPosition;
    this->title_bar = new AdvancedDockTitleBar(this, title, icon, customButtonWidget,
                                               title_bar_position == TitleTop);

    if (titleBarPosition == TitleTop) {
        QDockWidget::setTitleBarWidget(title_bar);
    } else {
        // Create a composite widget with layout: [titleBar][mainWidget]
        composite_widget = new QFrame();
        composite_widget->setObjectName("CompositeWidget");
        composite_widget->setStyleSheet("QFrame#CompositeWidget { border: 1px solid #19191f; }");
        QHBoxLayout *hbox = new QHBoxLayout(composite_widget);
        hbox->setContentsMargins(0, 0, 0, 0);
        hbox->setSpacing(0);
        hbox->addWidget(title_bar, 0);
        QWidget *dummy = new QWidget();
        hbox->addWidget(dummy, 1);
        composite_widget->setLayout(hbox);
        QDockWidget::setWidget(composite_widget);

        // hide default title bar
        QWidget *emptyTitleBar = new QWidget();
        emptyTitleBar->setFixedHeight(0);
        QDockWidget::setTitleBarWidget(emptyTitleBar);
    }
}

void AdvancedDockWidget::setTitleText(const QString &text) {
    if (title_bar) title_bar->setTitleText(text);
}

void AdvancedDockWidget::setTitleIcon(const QIcon &icon) {
    if (title_bar) title_bar->setTitleIcon(icon);
}

void AdvancedDockWidget::setCustomButtonWidget(QWidget *widget) {
    if (title_bar) title_bar->setCustomButtonWidget(widget);
}

void AdvancedDockWidget::setWidget(QWidget *widget) {
    if (this->title_bar_position == TitleTop) {
        QDockWidget::setWidget(widget);
    } else {
        if (!composite_widget) return;
        QHBoxLayout *hbox = qobject_cast<QHBoxLayout *>(composite_widget->layout());
        if (!hbox) return;
        QWidget *old = hbox->itemAt(1)->widget();
        if (old) {
            hbox->removeWidget(old);
            old->deleteLater();
        }
        if (widget->parent() == composite_widget) { widget->setParent(nullptr); }
        hbox->insertWidget(1, widget, 1);
    }
}

void AdvancedDockWidget::startDragFromTitleBar(const QPoint &from, const QPoint &current) {
    if (!dragging) {
        dragging = true;
        drag_on_title_bar = true;
        drag_start_pos = from;
    }
    QMouseEvent fakeMove(QEvent::MouseMove, mapFromGlobal(current), current, Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
    mouseMoveEvent(&fakeMove);
}

void AdvancedDockWidget::endDragFromTitleBar() {
    for (QWidget *w : QApplication::topLevelWidgets()) {
        QMainWindow *mw = qobject_cast<QMainWindow *>(w);
        if (!mw) continue;
        auto docks = mw->findChildren<AdvancedDockWidget *>();
        for (auto dock : docks)
            dock->hideDockOverlay();
    }
    dragging = false;
    drag_on_title_bar = false;
}

void AdvancedDockWidget::mousePressEvent(QMouseEvent *event) {
    QDockWidget::mousePressEvent(event);
}

void AdvancedDockWidget::mouseMoveEvent(QMouseEvent *event) {
    if (dragging && drag_on_title_bar && (event->buttons() & Qt::LeftButton)) {
        // Use a copy of the top-level widgets list to avoid issues during iteration
        QWidgetList topLevelList = QApplication::topLevelWidgets();
        for (QWidget *w : topLevelList) {
            QMainWindow *mw = qobject_cast<QMainWindow *>(w);
            if (!mw) continue;
            auto docks = mw->findChildren<AdvancedDockWidget *>();
            for (auto dock : docks) {
                if (!dock || dock == this) continue;
                QRect gr = dock->rect();
                QPoint rel = dock->mapFromGlobal(event->globalPosition().toPoint());
                if (gr.contains(rel)) {
                    dock->showDockOverlay();
                    dock->updateDockOverlay(event->globalPosition().toPoint());
                } else {
                    dock->hideDockOverlay();
                }
            }
        }
        // Don't call base class when handling our custom drag to avoid conflicts
        // with Qt's internal dock widget handling
        event->accept();
        return;
    }

    QDockWidget::mouseMoveEvent(event);
}

void AdvancedDockWidget::mouseReleaseEvent(QMouseEvent *event) {
    dragging = false;
    QDockWidget::mouseReleaseEvent(event);
}

void AdvancedDockWidget::paintEvent(QPaintEvent *event) {
    QStyleOptionDockWidget opt;
    opt.rect = rect();
    opt.title = windowTitle();
    opt.closable = features() & QDockWidget::DockWidgetClosable;
    opt.floatable = features() & QDockWidget::DockWidgetFloatable;
    opt.movable = features() & QDockWidget::DockWidgetMovable;
    opt.verticalTitleBar = false;
    QStylePainter p(this);
    p.drawPrimitive(QStyle::PE_Widget, opt);
    QDockWidget::paintEvent(event);
}

void AdvancedDockWidget::setFloating(bool floating) {
    QDockWidget::setFloating(floating);

    if (floating) {
        setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
        show();
    } else {
        setWindowFlags(Qt::Widget);
        show();
    }
}

void AdvancedDockWidget::showDockOverlay() {
    if (!overlay) {
        overlay = new DockIndicatorOverlay(this);
        overlay->setGeometry(rect());
        overlay->raise();
        overlay->show();
    }
}

void AdvancedDockWidget::hideDockOverlay() {
    if (overlay) {
        overlay->applyDocking();
        overlay->hide();
        overlay->deleteLater();
        overlay = nullptr;
    }
}

void AdvancedDockWidget::updateDockOverlay(const QPoint &globalPos) {
    if (overlay) {
        overlay->setGeometry(rect());
        overlay->raise();
        overlay->update();
        QPoint rel = mapFromGlobal(globalPos);
        int a = overlay->areaAt(rel);
        overlay->highlightArea(a);
        overlay->update();
    }
}

void AdvancedDockWidget::dockToArea(AdvancedDockWidget::Area area) {
    QMainWindow *mw = qobject_cast<QMainWindow *>(parentWidget());
    if (!mw) return;

    AdvancedDockWidget *dragged_dock = nullptr;
    auto docks = mw->findChildren<AdvancedDockWidget *>();
    for (auto d : docks) {
        if (d->dragging) {
            dragged_dock = d;
            break;
        }
    }

    if (!dragged_dock || dragged_dock == this) return;

    if (dragged_dock->isFloating()) { dragged_dock->setFloating(false); }

    switch (area) {
    case AdvancedDockWidget::Area::Center:
        mw->tabifyDockWidget(this, dragged_dock);
        break;
    case AdvancedDockWidget::Area::Top:
        mw->splitDockWidget(dragged_dock, this, Qt::Vertical);
        break;
    case AdvancedDockWidget::Area::Bottom:
        mw->splitDockWidget(this, dragged_dock, Qt::Vertical);
        break;
    case AdvancedDockWidget::Area::Left:
        mw->splitDockWidget(dragged_dock, this, Qt::Horizontal);
        break;
    case AdvancedDockWidget::Area::Right:
        mw->splitDockWidget(this, dragged_dock, Qt::Horizontal);
        break;
    }
}

void AdvancedDockWidget::toggleMaximizeRestoreFloating() {
    if (!isFloating()) return;
    QWidget *floatWin = this->window();
    if (!floating_maximized) {
        floating_restore_geometry = floatWin->geometry();
        auto screen = floatWin->screen()->availableGeometry();
        floatWin->setGeometry(screen);
        floating_maximized = true;
    } else {
        floatWin->setGeometry(floating_restore_geometry);
        floating_maximized = false;
    }
}