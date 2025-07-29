#pragma once

#include <QWidget>
#include <vector>

/**
 * Dynamický grid widget pro dialy, které se vkládají sloupec po sloupci zleva nahoře.
 * Dialy mají fixní velikost 40x60.
 */
class AudioDialGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioDialGridWidget(QWidget* parent = nullptr);

    void setDials(const std::vector<QWidget*>& dials);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void updateLayout();

private:
    std::vector<QWidget*> dialWidgets_;
    int dialWidth_ = 40;
    int dialHeight_ = 60;
};