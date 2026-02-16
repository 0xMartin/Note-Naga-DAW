#pragma once

#include <QPushButton>

/**
 * @brief Floating circular AI button overlay.
 *        Positioned in bottom-right corner of parent widget.
 */
class AiChatButton : public QPushButton {
    Q_OBJECT
    
public:
    explicit AiChatButton(QWidget *parent = nullptr);
    
    /**
     * @brief Updates button position relative to parent.
     *        Call this when parent is resized.
     */
    void updatePosition();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupStyle();
    
    int m_margin = 16;
    int m_size = 36;
};
