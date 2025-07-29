#include "audio_dial_grid_widget.h"

#include <QResizeEvent>

AudioDialGridWidget::AudioDialGridWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setContentsMargins(0, 0, 0, 0);
}

void AudioDialGridWidget::setDials(const std::vector<QWidget*>& dials) {
    // Remove old dials (if any)
    for (auto* w : dialWidgets_) {
        if (w->parent() == this) w->setParent(nullptr);
    }
    dialWidgets_ = dials;
    updateLayout();
}

void AudioDialGridWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateLayout();
}

void AudioDialGridWidget::updateLayout() {
    int w = width();
    int h = height();
    int n = dialWidgets_.size();
    if (n == 0) return;
    // Kolik dialů se vejde do jednoho sloupce (podle výšky widgetu)
    int rows = std::max(1, h / dialHeight_);
    int cols = (n + rows - 1) / rows;

    // Umísti dialy ručně
    for (int idx = 0; idx < n; ++idx) {
        int col = idx / rows;
        int row = idx % rows;
        QWidget* dial = dialWidgets_[idx];
        dial->setParent(this);
        dial->setGeometry(col * dialWidth_, row * dialHeight_, dialWidth_, dialHeight_);
        dial->show();
    }
    // Nastav minimální/maximální velikost podle dialů
    setMinimumWidth(cols * dialWidth_);
    setMinimumHeight(rows * dialHeight_);
    setMaximumWidth(cols * dialWidth_ + 40);
}