#include "ai_preview_banner.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>

AiPreviewBanner::AiPreviewBanner(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    
    // Style the banner with a distinctive AI preview look
    setStyleSheet(R"(
        AiPreviewBanner {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2d3a4a, stop:1 #1e2630);
            border-bottom: 2px solid #4a6fa5;
        }
        QLabel {
            color: #e0e8f0;
        }
        QLabel#TitleLabel {
            font-weight: bold;
            font-size: 12pt;
            color: #7eb8ff;
        }
        QLabel#StatsLabel {
            font-size: 9pt;
            color: #a0b8d0;
        }
        QPushButton#KeepBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3d8c40, stop:1 #2d6a30);
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 20px;
            font-weight: bold;
            font-size: 10pt;
        }
        QPushButton#KeepBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4da050, stop:1 #3d7a40);
        }
        QPushButton#KeepBtn:pressed {
            background: #2d6a30;
        }
        QPushButton#DiscardBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #8c3d3d, stop:1 #6a2d2d);
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 20px;
            font-weight: bold;
            font-size: 10pt;
        }
        QPushButton#DiscardBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #a04d4d, stop:1 #7a3d3d);
        }
        QPushButton#DiscardBtn:pressed {
            background: #6a2d2d;
        }
    )");
    
    // Main layout
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 10, 16, 10);
    mainLayout->setSpacing(16);
    
    // Icon (sparkle/magic wand)
    m_iconLabel = new QLabel("✨");
    m_iconLabel->setStyleSheet("font-size: 18pt;");
    mainLayout->addWidget(m_iconLabel);
    
    // Text content
    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    
    m_titleLabel = new QLabel(tr("AI Preview Mode"));
    m_titleLabel->setObjectName("TitleLabel");
    textLayout->addWidget(m_titleLabel);
    
    m_statsLabel = new QLabel(tr("Review the changes before applying"));
    m_statsLabel->setObjectName("StatsLabel");
    textLayout->addWidget(m_statsLabel);
    
    mainLayout->addLayout(textLayout, 1);
    
    // Legend for colors
    auto *legendLayout = new QHBoxLayout();
    legendLayout->setSpacing(12);
    
    QLabel *addedLegend = new QLabel("● " + tr("Added"));
    addedLegend->setStyleSheet("color: #50ff50; font-size: 9pt;");
    legendLayout->addWidget(addedLegend);
    
    QLabel *removedLegend = new QLabel("● " + tr("Removed"));
    removedLegend->setStyleSheet("color: #ff5050; font-size: 9pt;");
    legendLayout->addWidget(removedLegend);
    
    QLabel *modifiedLegend = new QLabel("● " + tr("Modified"));
    modifiedLegend->setStyleSheet("color: #ffff50; font-size: 9pt;");
    legendLayout->addWidget(modifiedLegend);
    
    mainLayout->addLayout(legendLayout);
    
    // Spacer
    mainLayout->addSpacing(20);
    
    // Buttons
    m_discardBtn = new QPushButton(tr("✗ Discard"));
    m_discardBtn->setObjectName("DiscardBtn");
    m_discardBtn->setCursor(Qt::PointingHandCursor);
    m_discardBtn->setToolTip(tr("Discard AI changes and restore original"));
    connect(m_discardBtn, &QPushButton::clicked, this, &AiPreviewBanner::discardClicked);
    mainLayout->addWidget(m_discardBtn);
    
    m_keepBtn = new QPushButton(tr("✓ Keep Changes"));
    m_keepBtn->setObjectName("KeepBtn");
    m_keepBtn->setCursor(Qt::PointingHandCursor);
    m_keepBtn->setToolTip(tr("Apply AI changes to the sequence"));
    connect(m_keepBtn, &QPushButton::clicked, this, &AiPreviewBanner::keepClicked);
    mainLayout->addWidget(m_keepBtn);
    
    // Shadow effect
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(10);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 100));
    setGraphicsEffect(shadow);
    
    setFixedHeight(60);
}

void AiPreviewBanner::setMessage(const QString &message) {
    m_statsLabel->setText(message);
}

void AiPreviewBanner::setStats(int added, int removed, int modified) {
    QStringList parts;
    if (added > 0) {
        parts << tr("%1 note(s) added").arg(added);
    }
    if (removed > 0) {
        parts << tr("%1 note(s) removed").arg(removed);
    }
    if (modified > 0) {
        parts << tr("%1 note(s) modified").arg(modified);
    }
    
    if (parts.isEmpty()) {
        m_statsLabel->setText(tr("No note changes in this preview"));
    } else {
        m_statsLabel->setText(parts.join(" • "));
    }
}
