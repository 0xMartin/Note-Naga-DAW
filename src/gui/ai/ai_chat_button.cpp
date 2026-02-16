#include "ai_chat_button.h"

#include <QPainter>
#include <QSvgRenderer>
#include <QEvent>
#include <QResizeEvent>
#include <QGraphicsDropShadowEffect>

AiChatButton::AiChatButton(QWidget *parent)
    : QPushButton(parent)
{
    // Ensure truly fixed square size
    setMinimumSize(m_size, m_size);
    setMaximumSize(m_size, m_size);
    setFixedSize(m_size, m_size);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("AI Assistant"));
    
    setupStyle();
    
    // Add shadow effect for better visibility
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 160));
    setGraphicsEffect(shadow);
    
    // Install event filter on parent for resize events
    if (parent) {
        parent->installEventFilter(this);
        updatePosition();
    }
}

void AiChatButton::setupStyle() {
    // Use explicit min/max width/height in stylesheet to ensure circular shape
    setStyleSheet(QString(R"(
        QPushButton {
            background-color: rgba(40, 40, 45, 220);
            border: 2px solid rgba(100, 100, 110, 180);
            border-radius: %1px;
            min-width: %2px;
            max-width: %2px;
            min-height: %2px;
            max-height: %2px;
        }
        QPushButton:hover {
            background-color: rgba(60, 60, 70, 240);
            border: 2px solid rgba(140, 140, 160, 200);
        }
        QPushButton:pressed {
            background-color: rgba(50, 50, 60, 250);
        }
    )").arg(m_size / 2).arg(m_size));
}

void AiChatButton::updatePosition() {
    QWidget *p = parentWidget();
    if (!p) return;
    
    int x = p->width() - m_size - m_margin;
    int y = p->height() - m_size - m_margin;
    move(x, y);
}

void AiChatButton::paintEvent(QPaintEvent *event) {
    QPushButton::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Load and render AI icon
    QSvgRenderer renderer(QString(":/icons/ai.svg"));
    if (renderer.isValid()) {
        // Calculate icon rect - keep square aspect ratio, centered
        int padding = 12;
        int iconSize = qMin(width(), height()) - 2 * padding;
        int x = (width() - iconSize) / 2;
        int y = (height() - iconSize) / 2;
        QRect iconRect(x, y, iconSize, iconSize);
        
        // Render icon
        painter.setOpacity(1.0);
        renderer.render(&painter, iconRect);
    } else {
        // Fallback: draw "AI" text
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "AI");
    }
}

bool AiChatButton::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QPushButton::eventFilter(watched, event);
}
