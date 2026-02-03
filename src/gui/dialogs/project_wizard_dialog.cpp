#include "project_wizard_dialog.h"
#include <note_naga_engine/note_naga_engine.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QSpacerItem>
#include <QDateTime>
#include <QFileInfo>
#include <QSvgRenderer>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QResizeEvent>

// Custom background widget that tiles SVG while preserving aspect ratio
class TiledSvgBackgroundWizard : public QWidget {
public:
    TiledSvgBackgroundWizard(const QString &svgPath, QWidget *parent = nullptr)
        : QWidget(parent), m_renderer(new QSvgRenderer(svgPath, this))
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setStyleSheet("background: transparent;");
    }
    
protected:
    void paintEvent(QPaintEvent *) override {
        if (!m_renderer->isValid()) return;
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        
        QSizeF svgSize = m_renderer->defaultSize();
        if (svgSize.isEmpty()) return;
        
        int tileWidth = static_cast<int>(svgSize.width());
        int tileHeight = static_cast<int>(svgSize.height());
        
        int tilesX = (width() + tileWidth - 1) / tileWidth;
        int tilesY = (height() + tileHeight - 1) / tileHeight;
        
        int totalTileWidth = tilesX * tileWidth;
        int totalTileHeight = tilesY * tileHeight;
        int offsetX = (width() - totalTileWidth) / 2;
        int offsetY = (height() - totalTileHeight) / 2;
        
        for (int row = 0; row < tilesY; ++row) {
            for (int col = 0; col < tilesX; ++col) {
                int x = offsetX + col * tileWidth;
                int y = offsetY + row * tileHeight;
                QRectF tileRect(x, y, tileWidth, tileHeight);
                m_renderer->render(&painter, tileRect);
            }
        }
    }
    
private:
    QSvgRenderer *m_renderer;
};

ProjectWizardDialog::ProjectWizardDialog(NoteNagaEngine *engine,
                                         RecentProjectsManager *recentManager,
                                         QWidget *parent)
    : QDialog(parent)
    , m_engine(engine)
    , m_recentManager(recentManager)
{
    setWindowTitle("NoteNaga - Project Wizard");
    setMinimumSize(600, 450);
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // Create a wrapper to hold background and content
    QWidget *wrapper = new QWidget(this);
    wrapper->setStyleSheet("background-color: #1a1a1f;");
    
    // Background SVG - tiled
    m_backgroundWidget = new TiledSvgBackgroundWizard(":/images/background.svg", wrapper);
    QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(m_backgroundWidget);
    opacityEffect->setOpacity(0.08);  // Subtle but visible texture
    m_backgroundWidget->setGraphicsEffect(opacityEffect);
    
    // Main layout for the dialog
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);
    dialogLayout->setContentsMargins(0, 0, 0, 0);
    dialogLayout->setSpacing(0);
    dialogLayout->addWidget(wrapper);
    
    // Main layout for wrapper content
    QVBoxLayout *mainLayout = new QVBoxLayout(wrapper);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    m_backgroundWidget->lower();
    
    // Stacked widget for pages
    m_stackedWidget = new QStackedWidget();
    m_stackedWidget->setStyleSheet("background: transparent;");
    mainLayout->addWidget(m_stackedWidget);
    
    // Setup pages
    setupMainPage();
    setupNewProjectPage();
    
    // Start on main page
    showMainPage();
    
    // Populate recent projects
    populateRecentProjects();
}

void ProjectWizardDialog::setupMainPage()
{
    m_mainPage = new QWidget();
    m_mainPage->setStyleSheet("background: transparent;");
    QVBoxLayout *layout = new QVBoxLayout(m_mainPage);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);
    
    // Logo
    QLabel *logoLabel = new QLabel();
    logoLabel->setStyleSheet("background: transparent;");
    QPixmap logoPixmap(":/icons/logo.svg");
    logoLabel->setPixmap(logoPixmap.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(logoLabel);
    
    // Title
    QLabel *titleLabel = new QLabel("NoteNaga");
    titleLabel->setFont(QFont("Segoe UI", 24, QFont::Bold));
    titleLabel->setStyleSheet("color: #7eb8f9; letter-spacing: 2px; background: transparent;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    QLabel *subtitleLabel = new QLabel("Professional MIDI Editor & Synthesizer");
    subtitleLabel->setFont(QFont("Segoe UI", 11));
    subtitleLabel->setStyleSheet("color: #8899a6; margin-bottom: 15px; background: transparent;");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitleLabel);
    
    // Separator
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("background-color: #263e54;");
    separator->setFixedHeight(1);
    layout->addWidget(separator);
    
    // Content area
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(25);
    
    // Left side - Action buttons
    QVBoxLayout *actionsLayout = new QVBoxLayout();
    actionsLayout->setSpacing(12);
    
    QLabel *actionsLabel = new QLabel("Get Started");
    actionsLabel->setFont(QFont("Segoe UI", 12, QFont::Bold));
    actionsLabel->setStyleSheet("color: #d4d8de; margin-bottom: 5px; background: transparent;");
    actionsLayout->addWidget(actionsLabel);
    
    QString buttonStyle = R"(
        QPushButton {
            background: #2d3640;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 8px;
            padding: 14px 20px;
            text-align: left;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: #3a4654;
            border-color: #4a6080;
            color: #ffffff;
        }
        QPushButton:pressed {
            background: #4a6080;
        }
    )";
    
    m_newProjectBtn = new QPushButton("📄  New Empty Project");
    m_newProjectBtn->setStyleSheet(buttonStyle);
    m_newProjectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_newProjectBtn, &QPushButton::clicked, this, &ProjectWizardDialog::onNewProjectClicked);
    actionsLayout->addWidget(m_newProjectBtn);
    
    m_openProjectBtn = new QPushButton("📂  Open Project...");
    m_openProjectBtn->setStyleSheet(buttonStyle);
    m_openProjectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openProjectBtn, &QPushButton::clicked, this, &ProjectWizardDialog::onOpenProjectClicked);
    actionsLayout->addWidget(m_openProjectBtn);
    
    m_importMidiBtn = new QPushButton("🎹  Import MIDI File...");
    m_importMidiBtn->setStyleSheet(buttonStyle);
    m_importMidiBtn->setCursor(Qt::PointingHandCursor);
    connect(m_importMidiBtn, &QPushButton::clicked, this, &ProjectWizardDialog::onImportMidiClicked);
    actionsLayout->addWidget(m_importMidiBtn);
    
    actionsLayout->addStretch();
    contentLayout->addLayout(actionsLayout, 4);
    
    // Vertical separator
    QFrame *vSeparator = new QFrame();
    vSeparator->setFrameShape(QFrame::VLine);
    vSeparator->setStyleSheet("background-color: #263e54;");
    vSeparator->setFixedWidth(1);
    contentLayout->addWidget(vSeparator);
    
    // Right side - Recent projects
    QVBoxLayout *recentLayout = new QVBoxLayout();
    recentLayout->setSpacing(8);
    
    QHBoxLayout *recentHeaderLayout = new QHBoxLayout();
    QLabel *recentLabel = new QLabel("Recent Projects");
    recentLabel->setFont(QFont("Segoe UI", 12, QFont::Bold));
    recentLabel->setStyleSheet("color: #d4d8de; background: transparent;");
    recentHeaderLayout->addWidget(recentLabel);
    
    m_removeRecentBtn = new QPushButton("✕");
    m_removeRecentBtn->setFixedSize(24, 24);
    m_removeRecentBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #667788;
            border: none;
            font-size: 14px;
        }
        QPushButton:hover {
            color: #ff6b6b;
        }
    )");
    m_removeRecentBtn->setToolTip("Remove selected from list");
    m_removeRecentBtn->setCursor(Qt::PointingHandCursor);
    connect(m_removeRecentBtn, &QPushButton::clicked, this, &ProjectWizardDialog::onRecentProjectRemove);
    recentHeaderLayout->addWidget(m_removeRecentBtn);
    
    recentLayout->addLayout(recentHeaderLayout);
    
    m_recentProjectsList = new QListWidget();
    m_recentProjectsList->setStyleSheet(R"(
        QListWidget {
            background: #1e242c;
            border: 1px solid #2d3640;
            border-radius: 6px;
            padding: 4px;
            font-size: 12px;
        }
        QListWidget::item {
            color: #b0b8c0;
            padding: 10px 12px;
            border-radius: 4px;
            margin: 2px 0;
        }
        QListWidget::item:hover {
            background: #2d3640;
            color: #d4d8de;
        }
        QListWidget::item:selected {
            background: #3a4654;
            color: #ffffff;
        }
    )");
    connect(m_recentProjectsList, &QListWidget::itemDoubleClicked, 
            this, &ProjectWizardDialog::onRecentProjectDoubleClicked);
    recentLayout->addWidget(m_recentProjectsList, 1);
    
    contentLayout->addLayout(recentLayout, 5);
    
    layout->addLayout(contentLayout, 1);
    
    m_stackedWidget->addWidget(m_mainPage);
}

void ProjectWizardDialog::setupNewProjectPage()
{
    m_newProjectPage = new QWidget();
    m_newProjectPage->setStyleSheet("background: transparent;");
    QVBoxLayout *layout = new QVBoxLayout(m_newProjectPage);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(20);
    
    // Title
    QLabel *titleLabel = new QLabel("📄 Create New Project");
    titleLabel->setFont(QFont("Segoe UI", 18, QFont::Bold));
    titleLabel->setStyleSheet("color: #7eb8f9; letter-spacing: 1px; background: transparent;");
    layout->addWidget(titleLabel);
    
    // Separator
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("background-color: #263e54;");
    separator->setFixedHeight(1);
    layout->addWidget(separator);
    
    // Form
    QGridLayout *formLayout = new QGridLayout();
    formLayout->setHorizontalSpacing(15);
    formLayout->setVerticalSpacing(12);
    
    QString labelStyle = "color: #b0b8c0; font-size: 13px;";
    QString inputStyle = R"(
        QLineEdit {
            background: #1e242c;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 6px;
            padding: 10px 14px;
            font-size: 13px;
        }
        QLineEdit:focus {
            border-color: #5590c7;
            background: #232b38;
        }
    )";
    
    QLabel *nameLabel = new QLabel("Project Name:");
    nameLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(nameLabel, 0, 0);
    
    m_projectNameEdit = new QLineEdit();
    m_projectNameEdit->setPlaceholderText("My New Project");
    m_projectNameEdit->setStyleSheet(inputStyle);
    formLayout->addWidget(m_projectNameEdit, 0, 1);
    
    QLabel *authorLabel = new QLabel("Author:");
    authorLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(authorLabel, 1, 0);
    
    m_authorEdit = new QLineEdit();
    m_authorEdit->setPlaceholderText("Your Name (optional)");
    m_authorEdit->setStyleSheet(inputStyle);
    formLayout->addWidget(m_authorEdit, 1, 1);
    
    formLayout->setColumnStretch(1, 1);
    layout->addLayout(formLayout);
    
    layout->addStretch();
    
    // Buttons
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(12);
    
    m_backBtn = new QPushButton("← Back");
    m_backBtn->setStyleSheet(R"(
        QPushButton {
            background: #2d3640;
            color: #b0b8c0;
            border: 1px solid #3a4654;
            border-radius: 6px;
            padding: 10px 24px;
            font-size: 13px;
        }
        QPushButton:hover {
            background: #3a4654;
            color: #ffffff;
        }
    )");
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, &ProjectWizardDialog::onBackToMain);
    buttonsLayout->addWidget(m_backBtn);
    
    buttonsLayout->addStretch();
    
    m_createProjectBtn = new QPushButton("Create Project");
    m_createProjectBtn->setStyleSheet(R"(
        QPushButton {
            background: #3477c0;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 10px 28px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #4a8ad0;
        }
        QPushButton:pressed {
            background: #2a6090;
        }
    )");
    m_createProjectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_createProjectBtn, &QPushButton::clicked, this, &ProjectWizardDialog::onCreateProject);
    buttonsLayout->addWidget(m_createProjectBtn);
    
    layout->addLayout(buttonsLayout);
    
    m_stackedWidget->addWidget(m_newProjectPage);
}

void ProjectWizardDialog::populateRecentProjects()
{
    m_recentProjectsList->clear();
    
    QList<RecentProjectEntry> recentProjects = m_recentManager->getRecentProjects(false);
    
    if (recentProjects.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem("No recent projects");
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsEnabled);
        emptyItem->setForeground(QColor("#667788"));
        m_recentProjectsList->addItem(emptyItem);
        return;
    }
    
    for (const RecentProjectEntry &entry : recentProjects) {
        QString displayText = entry.projectName.isEmpty() 
            ? QFileInfo(entry.filePath).baseName() 
            : entry.projectName;
        
        QString tooltip = entry.filePath + "\n\nLast opened: " + 
                         entry.lastOpened.toString("yyyy-MM-dd hh:mm");
        
        QListWidgetItem *item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, entry.filePath);
        item->setToolTip(tooltip);
        m_recentProjectsList->addItem(item);
    }
}

void ProjectWizardDialog::showMainPage()
{
    m_stackedWidget->setCurrentWidget(m_mainPage);
}

void ProjectWizardDialog::showNewProjectPage()
{
    m_projectNameEdit->clear();
    m_authorEdit->clear();
    m_projectNameEdit->setFocus();
    m_stackedWidget->setCurrentWidget(m_newProjectPage);
}

void ProjectWizardDialog::onNewProjectClicked()
{
    showNewProjectPage();
}

void ProjectWizardDialog::onOpenProjectClicked()
{
    QString startDir = m_recentManager->getLastProjectDirectory();
    
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open NoteNaga Project",
        startDir,
        "NoteNaga Projects (*.nnproj);;All Files (*)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    m_wizardResult = WizardResult::OpenProject;
    m_selectedFilePath = filePath;
    
    // Update last directory
    m_recentManager->setLastProjectDirectory(QFileInfo(filePath).absolutePath());
    
    accept();
}

void ProjectWizardDialog::onImportMidiClicked()
{
    QString startDir = m_recentManager->getLastProjectDirectory();
    
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Import MIDI File",
        startDir,
        "MIDI Files (*.mid *.midi);;All Files (*)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    m_wizardResult = WizardResult::ImportMidi;
    m_selectedFilePath = filePath;
    
    // Update last directory
    m_recentManager->setLastProjectDirectory(QFileInfo(filePath).absolutePath());
    
    accept();
}

void ProjectWizardDialog::onRecentProjectDoubleClicked(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
        return;
    }
    
    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        return;
    }
    
    // Check if file exists
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, "File Not Found",
            QString("The project file no longer exists:\n%1").arg(filePath));
        m_recentManager->removeRecentProject(filePath);
        populateRecentProjects();
        return;
    }
    
    m_wizardResult = WizardResult::OpenRecent;
    m_selectedFilePath = filePath;
    
    accept();
}

void ProjectWizardDialog::onRecentProjectRemove()
{
    QListWidgetItem *currentItem = m_recentProjectsList->currentItem();
    if (!currentItem || !(currentItem->flags() & Qt::ItemIsEnabled)) {
        return;
    }
    
    QString filePath = currentItem->data(Qt::UserRole).toString();
    if (!filePath.isEmpty()) {
        m_recentManager->removeRecentProject(filePath);
        populateRecentProjects();
    }
}

void ProjectWizardDialog::onCreateProject()
{
    QString projectName = m_projectNameEdit->text().trimmed();
    if (projectName.isEmpty()) {
        projectName = "Untitled Project";
    }
    
    m_metadata.name = projectName.toStdString();
    m_metadata.author = m_authorEdit->text().trimmed().toStdString();
    m_metadata.createdAt = NoteNagaProjectMetadata::currentTimestamp();
    m_metadata.modifiedAt = NoteNagaProjectMetadata::currentTimestamp();
    
    m_wizardResult = WizardResult::NewProject;
    
    accept();
}

void ProjectWizardDialog::onBackToMain()
{
    showMainPage();
}

void ProjectWizardDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    
    if (m_backgroundWidget && m_backgroundWidget->parentWidget()) {
        m_backgroundWidget->setGeometry(0, 0, 
            m_backgroundWidget->parentWidget()->width(),
            m_backgroundWidget->parentWidget()->height());
    }
}
