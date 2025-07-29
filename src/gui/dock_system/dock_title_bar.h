#pragma once

#include <QWidget>
#include <QIcon>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

#include "../components/vertical_label.h"

// Forward declaration of AdvancedDockWidget to avoid circular dependency
class AdvancedDockWidget;

/**
 * @brief CustomDockTitleBar is a custom title bar for AdvancedDockWidget.
 * It includes the title, icon, and optional custom buttons.
 */
class AdvancedDockTitleBar : public QFrame {
    Q_OBJECT
public:
    explicit AdvancedDockTitleBar(
        AdvancedDockWidget* dock,
        const QString& title,
        const QIcon& icon,
        QWidget* customButtonWidget = nullptr,
        bool horizontal = true);

    void setTitleText(const QString& text);
    void setTitleIcon(const QIcon& icon);
    void setCustomButtonWidget(QWidget* widget);

    QString getTitleText() const { return horizontal ? horizontalTitleLabel->text() : verticalTitleLabel->text(); }
    QWidget* getCustomButtonWidget() const { return customButtons; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void onFloatClicked();
    void onCloseClicked();

private:
    AdvancedDockWidget* dockWidget = nullptr;
    QLabel* iconLabel = nullptr;
    QLabel* horizontalTitleLabel = nullptr;
    VerticalTitleLabel* verticalTitleLabel = nullptr;
    QWidget* customButtons = nullptr;
    QWidget* defaultButtons = nullptr;

    bool horizontal = true;
    bool dragging = false;
    QPoint dragStartPos;

    QPushButton* createDefaultButton(const QIcon& icon, const QString& tooltip, const char* slotName);
    void updateDefaultButtons();
};