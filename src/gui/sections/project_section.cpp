#include "project_section.h"
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/project_serializer.h>
#include <note_naga_engine/synth/synth_fluidsynth.h>
#include <note_naga_engine/synth/synth_external_midi.h>

#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QDateTime>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>
#include <QResizeEvent>
#include <QSvgWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGraphicsOpacityEffect>
#include <QSvgRenderer>
#include <QPainter>

// Custom background widget that tiles SVG while preserving aspect ratio
class TiledSvgBackground : public QWidget {
public:
    TiledSvgBackground(const QString &svgPath, QWidget *parent = nullptr)
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
        
        // Get original SVG size
        QSizeF svgSize = m_renderer->defaultSize();
        if (svgSize.isEmpty()) return;
        
        // Tile the SVG across the widget, preserving aspect ratio
        int tileWidth = static_cast<int>(svgSize.width());
        int tileHeight = static_cast<int>(svgSize.height());
        
        // Calculate how many tiles we need
        int tilesX = (width() + tileWidth - 1) / tileWidth;
        int tilesY = (height() + tileHeight - 1) / tileHeight;
        
        // Calculate offset to center vertically (and horizontally)
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

// Shared styles
namespace {
    const QString cardStyle = R"(
        QWidget#card {
            background: #2a2d35;
            border-radius: 8px;
        }
    )";
    
    const QString labelStyle = "color: #6a7580; font-size: 11px; font-weight: 500; text-transform: uppercase;";
    
    const QString inputStyle = R"(
        QLineEdit, QTextEdit {
            background: #1e2228;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 4px;
            padding: 8px 12px;
            font-size: 13px;
        }
        QLineEdit:focus, QTextEdit:focus {
            border-color: #3477c0;
        }
    )";
    
    const QString spinBoxStyle = R"(
        QSpinBox, QDoubleSpinBox {
            background: #1e2228;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 4px;
            padding: 6px 10px;
            font-size: 13px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #3477c0;
        }
        QSpinBox::up-button, QSpinBox::down-button,
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            width: 16px;
            border: none;
            background: #2d3640;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover,
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
            background: #3a4654;
        }
    )";
    
    const QString valueStyle = "color: #d4d8de; font-size: 14px; font-weight: 500;";
    const QString readonlyStyle = "color: #8899a6; font-size: 12px;";
    
    const QString buttonStyle = R"(
        QPushButton {
            background: #2d3640;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 12px;
        }
        QPushButton:hover {
            background: #3a4654;
        }
        QPushButton:pressed {
            background: #4a6080;
        }
        QPushButton:disabled {
            background: #1e2228;
            color: #556677;
        }
    )";

    const QString primaryButtonStyle = R"(
        QPushButton {
            background: #3477c0;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 8px 20px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #4a8ad0;
        }
        QPushButton:pressed {
            background: #2a6090;
        }
    )";
}

ProjectSection::ProjectSection(NoteNagaEngine *engine,
                               NoteNagaProjectSerializer *serializer,
                               QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_serializer(serializer)
{
    setStyleSheet("background-color: #1a1a1f;");
    setupUI();
}

void ProjectSection::setupUI()
{
    // Use a stacked layout: background (fixed) + scroll area (on top)
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create a wrapper widget to stack background and scroll area
    QWidget *stackWrapper = new QWidget();
    stackWrapper->setStyleSheet("background-color: #1a1a1f;");
    
    // Fixed background SVG - tiled while preserving aspect ratio
    m_backgroundWidget = new TiledSvgBackground(":/images/background.svg", stackWrapper);
    // Apply opacity effect to make it subtle like a texture
    QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(m_backgroundWidget);
    opacityEffect->setOpacity(0.04);  // Very subtle, like embossed texture
    m_backgroundWidget->setGraphicsEffect(opacityEffect);
    
    // Create scroll area
    m_scrollArea = new QScrollArea(stackWrapper);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(R"(
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #3a4654;
            border-radius: 4px;
            min-height: 40px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4a6080;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");
    
    // Wrapper widget for centering
    QWidget *scrollContent = new QWidget();
    scrollContent->setStyleSheet("background: transparent;");
    
    QHBoxLayout *centeringLayout = new QHBoxLayout(scrollContent);
    centeringLayout->setContentsMargins(0, 0, 0, 0);
    centeringLayout->setSpacing(0);
    
    // Left spacer
    centeringLayout->addStretch(1);
    
    // Container widget (centered content)
    m_container = new QWidget();
    m_container->setStyleSheet("background: transparent;");
    
    m_containerLayout = new QVBoxLayout(m_container);
    m_containerLayout->setContentsMargins(24, 32, 24, 48);
    m_containerLayout->setSpacing(20);
    m_containerLayout->setAlignment(Qt::AlignTop);
    
    // Add sections - actions first (compact, no card)
    m_containerLayout->addWidget(createHeaderSection());
    m_containerLayout->addWidget(createActionsBar());  // Compact action bar
    m_containerLayout->addWidget(createMetadataSection());
    m_containerLayout->addWidget(createSequenceSettingsSection());  // New section
    m_containerLayout->addWidget(createStatisticsSection());
    m_containerLayout->addWidget(createFileInfoSection());  // New section
    m_containerLayout->addWidget(createSynthesizerSection());
    m_containerLayout->addStretch();
    
    centeringLayout->addWidget(m_container, 0);
    
    // Right spacer
    centeringLayout->addStretch(1);
    
    m_scrollArea->setWidget(scrollContent);
    
    mainLayout->addWidget(stackWrapper);
    
    // Initial container width
    updateContainerWidth();
}

void ProjectSection::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateContainerWidth();
    
    // Resize background widget to fill the entire section (fixed position)
    if (m_backgroundWidget) {
        m_backgroundWidget->setGeometry(0, 0, width(), height());
    }
    
    // Resize scroll area to fill the entire section (on top of background)
    if (m_scrollArea) {
        m_scrollArea->setGeometry(0, 0, width(), height());
    }
}

void ProjectSection::updateContainerWidth()
{
    int sectionWidth = width();
    int containerWidth;
    
    if (sectionWidth < 600) {
        containerWidth = sectionWidth - 32;
    } else if (sectionWidth < 1200) {
        containerWidth = qMin(720, static_cast<int>(sectionWidth * 0.65));
    } else {
        containerWidth = qMin(800, static_cast<int>(sectionWidth * 0.5));
    }
    
    m_container->setFixedWidth(containerWidth);
}

QWidget* ProjectSection::createHeaderSection()
{
    QWidget *header = new QWidget();
    header->setStyleSheet("background: transparent;");
    
    QVBoxLayout *layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);
    
    // Logo - use QSvgWidget for crisp rendering
    QSvgWidget *logoWidget = new QSvgWidget(":/icons/logo.svg");
    logoWidget->setFixedSize(62, 62);  // 10% bigger than before
    logoWidget->setStyleSheet("background: transparent;");
    
    QHBoxLayout *logoLayout = new QHBoxLayout();
    logoLayout->setAlignment(Qt::AlignCenter);
    logoLayout->addWidget(logoWidget);
    layout->addLayout(logoLayout);
    
    // Title
    QLabel *titleLabel = new QLabel(tr("Project Settings"));
    titleLabel->setStyleSheet("color: #e8ecf0; font-size: 26px; font-weight: 600;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    return header;
}

QWidget* ProjectSection::createActionsBar()
{
    QWidget *bar = new QWidget();
    bar->setStyleSheet("background: transparent;");
    
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(10);
    
    layout->addStretch();
    
    m_saveBtn = new QPushButton(tr("💾 Save"));
    m_saveBtn->setStyleSheet(primaryButtonStyle);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProjectSection::onSaveClicked);
    layout->addWidget(m_saveBtn);
    
    m_saveAsBtn = new QPushButton(tr("Save As..."));
    m_saveAsBtn->setStyleSheet(buttonStyle);
    m_saveAsBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &ProjectSection::onSaveAsClicked);
    layout->addWidget(m_saveAsBtn);
    
    m_exportMidiBtn = new QPushButton(tr("Export MIDI"));
    m_exportMidiBtn->setStyleSheet(buttonStyle);
    m_exportMidiBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportMidiBtn, &QPushButton::clicked, this, &ProjectSection::onExportMidiClicked);
    layout->addWidget(m_exportMidiBtn);
    
    layout->addStretch();
    
    return bar;
}

QWidget* ProjectSection::createCard(const QString &title, QWidget *content)
{
    QWidget *card = new QWidget();
    card->setObjectName("card");
    card->setStyleSheet(cardStyle);
    
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 20);
    cardLayout->setSpacing(12);
    
    // Card title
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("color: #e8ecf0; font-size: 15px; font-weight: 600;");
    cardLayout->addWidget(titleLabel);
    
    // Separator
    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background: #3a4654; max-height: 1px;");
    cardLayout->addWidget(sep);
    
    // Content
    cardLayout->addWidget(content);
    
    return card;
}

QWidget* ProjectSection::createMetadataSection()
{
    QWidget *content = new QWidget();
    content->setStyleSheet("background: transparent;");
    
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    
    // Project Name
    QLabel *nameLabel = new QLabel(tr("PROJECT NAME"));
    nameLabel->setStyleSheet(labelStyle);
    layout->addWidget(nameLabel);
    
    m_projectNameEdit = new QLineEdit();
    m_projectNameEdit->setStyleSheet(inputStyle);
    m_projectNameEdit->setPlaceholderText(tr("Enter project name"));
    connect(m_projectNameEdit, &QLineEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    layout->addWidget(m_projectNameEdit);
    
    // Author
    QLabel *authorLabel = new QLabel(tr("AUTHOR"));
    authorLabel->setStyleSheet(labelStyle);
    layout->addWidget(authorLabel);
    
    m_authorEdit = new QLineEdit();
    m_authorEdit->setStyleSheet(inputStyle);
    m_authorEdit->setPlaceholderText(tr("Enter author name"));
    connect(m_authorEdit, &QLineEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    layout->addWidget(m_authorEdit);
    
    // Description
    QLabel *descLabel = new QLabel(tr("DESCRIPTION"));
    descLabel->setStyleSheet(labelStyle);
    layout->addWidget(descLabel);
    
    m_descriptionEdit = new QTextEdit();
    m_descriptionEdit->setStyleSheet(inputStyle);
    m_descriptionEdit->setPlaceholderText(tr("Optional project description"));
    m_descriptionEdit->setMaximumHeight(80);
    connect(m_descriptionEdit, &QTextEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    layout->addWidget(m_descriptionEdit);
    
    return createCard(tr("Project Information"), content);
}

QWidget* ProjectSection::createSequenceSettingsSection()
{
    QWidget *content = new QWidget();
    content->setStyleSheet("background: transparent;");
    
    QGridLayout *layout = new QGridLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(24);
    layout->setVerticalSpacing(14);
    
    // Tempo
    QLabel *tempoLabel = new QLabel(tr("TEMPO (BPM)"));
    tempoLabel->setStyleSheet(labelStyle);
    layout->addWidget(tempoLabel, 0, 0);
    
    m_tempoSpinBox = new QDoubleSpinBox();
    m_tempoSpinBox->setStyleSheet(spinBoxStyle);
    m_tempoSpinBox->setRange(20.0, 400.0);
    m_tempoSpinBox->setValue(120.0);
    m_tempoSpinBox->setDecimals(1);
    m_tempoSpinBox->setSuffix(" BPM");
    connect(m_tempoSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &ProjectSection::onTempoChanged);
    layout->addWidget(m_tempoSpinBox, 1, 0);
    
    // PPQ (Ticks per quarter note)
    QLabel *ppqLabel = new QLabel(tr("RESOLUTION (PPQ)"));
    ppqLabel->setStyleSheet(labelStyle);
    layout->addWidget(ppqLabel, 0, 1);
    
    m_ppqSpinBox = new QSpinBox();
    m_ppqSpinBox->setStyleSheet(spinBoxStyle);
    m_ppqSpinBox->setRange(24, 960);
    m_ppqSpinBox->setValue(480);
    m_ppqSpinBox->setSingleStep(24);
    m_ppqSpinBox->setToolTip(tr("Pulses Per Quarter note - higher values allow finer timing resolution"));
    connect(m_ppqSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ProjectSection::onPPQChanged);
    layout->addWidget(m_ppqSpinBox, 1, 1);
    
    // Time Signature (display only for now)
    QLabel *timeSigLabel = new QLabel(tr("TIME SIGNATURE"));
    timeSigLabel->setStyleSheet(labelStyle);
    layout->addWidget(timeSigLabel, 0, 2);
    
    m_timeSignatureLabel = new QLabel("4/4");
    m_timeSignatureLabel->setStyleSheet(valueStyle);
    layout->addWidget(m_timeSignatureLabel, 1, 2);
    
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 1);
    
    return createCard(tr("Sequence Settings"), content);
}

QWidget* ProjectSection::createStatisticsSection()
{
    QWidget *content = new QWidget();
    content->setStyleSheet("background: transparent;");
    
    QGridLayout *layout = new QGridLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(24);
    layout->setVerticalSpacing(16);
    
    auto addStat = [&](int row, int col, const QString &label, QLabel *&valueLabel, const QString &icon = "") {
        QVBoxLayout *statLayout = new QVBoxLayout();
        statLayout->setSpacing(4);
        
        QHBoxLayout *valueRow = new QHBoxLayout();
        valueRow->setSpacing(6);
        
        if (!icon.isEmpty()) {
            QLabel *iconLabel = new QLabel(icon);
            iconLabel->setStyleSheet("font-size: 18px;");
            valueRow->addWidget(iconLabel);
        }
        
        valueLabel = new QLabel("0");
        valueLabel->setStyleSheet("color: #3477c0; font-size: 22px; font-weight: 700;");
        valueRow->addWidget(valueLabel);
        valueRow->addStretch();
        
        statLayout->addLayout(valueRow);
        
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet(labelStyle);
        statLayout->addWidget(lbl);
        
        layout->addLayout(statLayout, row, col);
    };
    
    addStat(0, 0, tr("TRACKS"), m_trackCountLabel, "🎹");
    addStat(0, 1, tr("TOTAL NOTES"), m_noteCountLabel, "🎵");
    addStat(0, 2, tr("DURATION"), m_durationLabel, "⏱");
    
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 1);
    
    return createCard(tr("Statistics"), content);
}

QWidget* ProjectSection::createFileInfoSection()
{
    QWidget *content = new QWidget();
    content->setStyleSheet("background: transparent;");
    
    QGridLayout *layout = new QGridLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(24);
    layout->setVerticalSpacing(10);
    
    // Created
    QLabel *createdLabel = new QLabel(tr("CREATED"));
    createdLabel->setStyleSheet(labelStyle);
    layout->addWidget(createdLabel, 0, 0);
    m_createdAtLabel = new QLabel("-");
    m_createdAtLabel->setStyleSheet(readonlyStyle);
    layout->addWidget(m_createdAtLabel, 1, 0);
    
    // Modified
    QLabel *modifiedLabel = new QLabel(tr("LAST MODIFIED"));
    modifiedLabel->setStyleSheet(labelStyle);
    layout->addWidget(modifiedLabel, 0, 1);
    m_modifiedAtLabel = new QLabel("-");
    m_modifiedAtLabel->setStyleSheet(readonlyStyle);
    layout->addWidget(m_modifiedAtLabel, 1, 1);
    
    // File path (full width)
    QLabel *pathLabel = new QLabel(tr("FILE PATH"));
    pathLabel->setStyleSheet(labelStyle);
    layout->addWidget(pathLabel, 2, 0, 1, 2);
    m_filePathLabel = new QLabel(tr("Not saved yet"));
    m_filePathLabel->setStyleSheet(readonlyStyle);
    m_filePathLabel->setWordWrap(true);
    layout->addWidget(m_filePathLabel, 3, 0, 1, 2);
    
    // File format info
    QLabel *formatLabel = new QLabel(tr("FORMAT"));
    formatLabel->setStyleSheet(labelStyle);
    layout->addWidget(formatLabel, 4, 0);
    m_fileFormatLabel = new QLabel(tr("NoteNaga Binary (.nnproj)"));
    m_fileFormatLabel->setStyleSheet(readonlyStyle);
    layout->addWidget(m_fileFormatLabel, 5, 0);
    
    // Version
    QLabel *versionLabel = new QLabel(tr("FILE VERSION"));
    versionLabel->setStyleSheet(labelStyle);
    layout->addWidget(versionLabel, 4, 1);
    m_fileVersionLabel = new QLabel("2.0");
    m_fileVersionLabel->setStyleSheet(readonlyStyle);
    layout->addWidget(m_fileVersionLabel, 5, 1);
    
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    
    return createCard(tr("File Information"), content);
}

QWidget* ProjectSection::createSynthesizerSection()
{
    QWidget *content = new QWidget();
    content->setStyleSheet("background: transparent;");
    
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    
    QString listStyle = R"(
        QListWidget {
            background: #1e2228;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 4px;
            padding: 4px;
            font-size: 13px;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-radius: 3px;
        }
        QListWidget::item:selected {
            background: #3477c0;
        }
        QListWidget::item:hover:!selected {
            background: #2d3640;
        }
    )";
    
    m_synthList = new QListWidget();
    m_synthList->setStyleSheet(listStyle);
    m_synthList->setMinimumHeight(100);
    m_synthList->setMaximumHeight(150);
    connect(m_synthList, &QListWidget::itemSelectionChanged, this, &ProjectSection::onSynthSelectionChanged);
    layout->addWidget(m_synthList);
    
    QString comboStyle = R"(
        QComboBox {
            background: #1e2228;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 4px;
            padding: 6px 10px;
            font-size: 12px;
            min-width: 110px;
        }
        QComboBox:hover {
            border-color: #4a6080;
        }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox QAbstractItemView {
            background: #1e2228;
            border: 1px solid #3a4654;
            selection-background-color: #3477c0;
        }
    )";
    
    // Add row
    QHBoxLayout *addLayout = new QHBoxLayout();
    addLayout->setSpacing(8);
    
    m_synthTypeCombo = new QComboBox();
    m_synthTypeCombo->setStyleSheet(comboStyle);
    m_synthTypeCombo->addItem(tr("FluidSynth"), "fluidsynth");
    m_synthTypeCombo->addItem(tr("External MIDI"), "external_midi");
    addLayout->addWidget(m_synthTypeCombo);
    
    m_addSynthBtn = new QPushButton(tr("+ Add"));
    m_addSynthBtn->setStyleSheet(buttonStyle);
    m_addSynthBtn->setCursor(Qt::PointingHandCursor);
    connect(m_addSynthBtn, &QPushButton::clicked, this, &ProjectSection::onAddSynthClicked);
    addLayout->addWidget(m_addSynthBtn);
    
    addLayout->addStretch();
    
    m_configureSynthBtn = new QPushButton(tr("Configure"));
    m_configureSynthBtn->setStyleSheet(buttonStyle);
    m_configureSynthBtn->setEnabled(false);
    m_configureSynthBtn->setCursor(Qt::PointingHandCursor);
    connect(m_configureSynthBtn, &QPushButton::clicked, this, &ProjectSection::onConfigureSynthClicked);
    addLayout->addWidget(m_configureSynthBtn);
    
    m_renameSynthBtn = new QPushButton(tr("Rename"));
    m_renameSynthBtn->setStyleSheet(buttonStyle);
    m_renameSynthBtn->setEnabled(false);
    m_renameSynthBtn->setCursor(Qt::PointingHandCursor);
    connect(m_renameSynthBtn, &QPushButton::clicked, this, &ProjectSection::onRenameSynthClicked);
    addLayout->addWidget(m_renameSynthBtn);
    
    m_removeSynthBtn = new QPushButton(tr("Remove"));
    m_removeSynthBtn->setStyleSheet(buttonStyle);
    m_removeSynthBtn->setEnabled(false);
    m_removeSynthBtn->setCursor(Qt::PointingHandCursor);
    connect(m_removeSynthBtn, &QPushButton::clicked, this, &ProjectSection::onRemoveSynthClicked);
    addLayout->addWidget(m_removeSynthBtn);
    
    layout->addLayout(addLayout);
    
    return createCard(tr("Synthesizers"), content);
}

void ProjectSection::refreshSynthesizerList()
{
    m_synthList->clear();
    
    if (!m_engine) return;
    
    const auto &synths = m_engine->getSynthesizers();
    for (NoteNagaSynthesizer *synth : synths) {
        if (synth) {
            QString name = QString::fromStdString(synth->getName());
            if (name.isEmpty()) {
                name = tr("Unnamed Synth");
            }
            
            // Add type indicator
            QString typeStr;
            if (dynamic_cast<NoteNagaSynthFluidSynth*>(synth)) {
                typeStr = " [FluidSynth]";
            } else if (dynamic_cast<NoteNagaSynthExternalMidi*>(synth)) {
                typeStr = " [External MIDI]";
            }
            
            QListWidgetItem *item = new QListWidgetItem(name + typeStr);
            item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(synth)));
            m_synthList->addItem(item);
        }
    }
    
    bool hasSelection = m_synthList->currentRow() >= 0;
    m_renameSynthBtn->setEnabled(hasSelection);
    m_removeSynthBtn->setEnabled(hasSelection);
    m_configureSynthBtn->setEnabled(hasSelection);
}

void ProjectSection::onSynthSelectionChanged()
{
    bool hasSelection = m_synthList->currentRow() >= 0;
    m_renameSynthBtn->setEnabled(hasSelection);
    m_removeSynthBtn->setEnabled(hasSelection);
    m_configureSynthBtn->setEnabled(hasSelection);
}

void ProjectSection::onRenameSynthClicked()
{
    int row = m_synthList->currentRow();
    if (row < 0) return;
    
    const auto &synths = m_engine->getSynthesizers();
    if (row >= static_cast<int>(synths.size())) return;
    
    NoteNagaSynthesizer *synth = synths[row];
    if (!synth) return;
    
    bool ok;
    QString newName = QInputDialog::getText(
        this,
        tr("Rename Synthesizer"),
        tr("Enter new name:"),
        QLineEdit::Normal,
        QString::fromStdString(synth->getName()),
        &ok
    );
    
    if (ok && !newName.isEmpty()) {
        synth->setName(newName.toStdString());
        refreshSynthesizerList();
        onMetadataEdited();
    }
}

void ProjectSection::onAddSynthClicked()
{
    QString type = m_synthTypeCombo->currentData().toString();
    QString baseName;
    
    if (type == "fluidsynth") {
        baseName = "FluidSynth";
    } else if (type == "external_midi") {
        baseName = "External MIDI";
    } else {
        return;
    }
    
    // Generate unique name
    int suffix = 1;
    QString finalName;
    bool nameExists;
    do {
        nameExists = false;
        finalName = suffix == 1 ? baseName : QString("%1 %2").arg(baseName).arg(suffix);
        
        for (int i = 0; i < m_synthList->count(); ++i) {
            QString itemText = m_synthList->item(i)->text();
            if (itemText.startsWith(finalName)) {
                nameExists = true;
                break;
            }
        }
        if (nameExists) suffix++;
    } while (nameExists);
    
    try {
        NoteNagaSynthesizer *newSynth = nullptr;
        
        if (type == "fluidsynth") {
            newSynth = new NoteNagaSynthFluidSynth(finalName.toStdString(), "");
        } else if (type == "external_midi") {
            newSynth = new NoteNagaSynthExternalMidi(finalName.toStdString());
        }
        
        if (newSynth) {
            m_engine->addSynthesizer(newSynth);
            refreshSynthesizerList();
            
            // Select new synth
            for (int i = 0; i < m_synthList->count(); ++i) {
                if (m_synthList->item(i)->data(Qt::UserRole).value<void*>() == newSynth) {
                    m_synthList->setCurrentRow(i);
                    break;
                }
            }
            onMetadataEdited();
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to create synthesizer: %1").arg(e.what()));
    }
}

void ProjectSection::onRemoveSynthClicked()
{
    int row = m_synthList->currentRow();
    if (row < 0) return;
    
    QListWidgetItem *item = m_synthList->item(row);
    if (!item) return;
    
    NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer*>(
        item->data(Qt::UserRole).value<void*>());
    
    if (!synth) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Remove Synthesizer"),
        tr("Are you sure you want to remove this synthesizer?"),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_engine->removeSynthesizer(synth);
        refreshSynthesizerList();
        onMetadataEdited();
    }
}

void ProjectSection::onConfigureSynthClicked()
{
    int row = m_synthList->currentRow();
    if (row < 0) return;
    
    QListWidgetItem *item = m_synthList->item(row);
    if (!item) return;
    
    NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer*>(
        item->data(Qt::UserRole).value<void*>());
    
    if (!synth) return;
    
    NoteNagaSynthFluidSynth *fluidSynth = dynamic_cast<NoteNagaSynthFluidSynth*>(synth);
    NoteNagaSynthExternalMidi *externalMidi = dynamic_cast<NoteNagaSynthExternalMidi*>(synth);
    
    if (fluidSynth) {
        QString currentSF = QString::fromStdString(fluidSynth->getSoundFontPath());
        QString sfPath = QFileDialog::getOpenFileName(
            this,
            tr("Select SoundFont"),
            currentSF.isEmpty() ? QDir::homePath() : QFileInfo(currentSF).absolutePath(),
            tr("SoundFont Files (*.sf2 *.sf3 *.dls);;All Files (*)")
        );
        
        if (!sfPath.isEmpty()) {
            fluidSynth->setSoundFont(sfPath.toStdString());
            onMetadataEdited();
        }
    } else if (externalMidi) {
        auto ports = NoteNagaSynthExternalMidi::getAvailableMidiOutputPorts();
        QStringList portNames;
        for (const auto &port : ports) {
            portNames << QString::fromStdString(port);
        }
        
        if (portNames.isEmpty()) {
            QMessageBox::information(this, tr("No MIDI Ports"),
                tr("No external MIDI ports are available."));
            return;
        }
        
        bool ok;
        QString selected = QInputDialog::getItem(
            this,
            tr("Select MIDI Port"),
            tr("Choose MIDI output port:"),
            portNames,
            0,
            false,
            &ok
        );
        
        if (ok && !selected.isEmpty()) {
            externalMidi->setMidiOutputPort(selected.toStdString());
            onMetadataEdited();
        }
    }
}

void ProjectSection::onTempoChanged(double bpm)
{
    if (!m_engine) return;
    
    NoteNagaRuntimeData *project = m_engine->getRuntimeData();
    if (!project) return;
    
    NoteNagaMidiSeq *seq = project->getActiveSequence();
    if (!seq) return;
    
    // Convert BPM to microseconds per quarter note
    int tempoMicros = static_cast<int>(60000000.0 / bpm);
    seq->setTempo(tempoMicros);
    onMetadataEdited();
}

void ProjectSection::onPPQChanged(int ppq)
{
    if (!m_engine) return;
    
    NoteNagaRuntimeData *project = m_engine->getRuntimeData();
    if (!project) return;
    
    NoteNagaMidiSeq *seq = project->getActiveSequence();
    if (!seq) return;
    
    seq->setPPQ(ppq);
    onMetadataEdited();
}

void ProjectSection::onSectionActivated()
{
    refreshUI();
    updateStatistics();
    refreshSynthesizerList();
    updateSequenceSettings();
}

void ProjectSection::onSectionDeactivated()
{
    // Nothing to stop
}

void ProjectSection::setProjectMetadata(const NoteNagaProjectMetadata &metadata)
{
    m_metadata = metadata;
    refreshUI();
}

NoteNagaProjectMetadata ProjectSection::getProjectMetadata() const
{
    NoteNagaProjectMetadata meta = m_metadata;
    meta.name = m_projectNameEdit->text().toStdString();
    meta.author = m_authorEdit->text().toStdString();
    meta.description = m_descriptionEdit->toPlainText().toStdString();
    return meta;
}

void ProjectSection::setProjectFilePath(const QString &filePath)
{
    m_projectFilePath = filePath;
    m_filePathLabel->setText(filePath.isEmpty() ? tr("Not saved yet") : filePath);
}

void ProjectSection::markAsSaved()
{
    m_hasUnsavedChanges = false;
    m_metadata.modifiedAt = NoteNagaProjectMetadata::currentTimestamp();
    QDateTime dt = QDateTime::fromSecsSinceEpoch(m_metadata.modifiedAt);
    m_modifiedAtLabel->setText(dt.toString("yyyy-MM-dd hh:mm:ss"));
    emit unsavedChangesChanged(false);
}

void ProjectSection::onMetadataEdited()
{
    if (!m_hasUnsavedChanges) {
        m_hasUnsavedChanges = true;
        emit unsavedChangesChanged(true);
    }
    emit metadataChanged();
}

void ProjectSection::onSaveClicked()
{
    emit saveRequested();
}

void ProjectSection::onSaveAsClicked()
{
    emit saveAsRequested();
}

void ProjectSection::onExportMidiClicked()
{
    emit exportMidiRequested();
}

void ProjectSection::showSaveSuccess(const QString &filePath)
{
    QString message = filePath.isEmpty() 
        ? tr("Project saved successfully.") 
        : tr("Project saved successfully to:\n%1").arg(filePath);
    QMessageBox::information(this, tr("Save Successful"), message);
}

void ProjectSection::showSaveError(const QString &error)
{
    QMessageBox::critical(this, tr("Save Failed"), 
        tr("Failed to save project:\n%1").arg(error));
}

void ProjectSection::showExportSuccess(const QString &filePath)
{
    QMessageBox::information(this, tr("Export Successful"), 
        tr("MIDI exported successfully to:\n%1").arg(filePath));
}

void ProjectSection::showExportError(const QString &error)
{
    QMessageBox::critical(this, tr("Export Failed"), 
        tr("Failed to export MIDI:\n%1").arg(error));
}

void ProjectSection::refreshUI()
{
    m_projectNameEdit->blockSignals(true);
    m_authorEdit->blockSignals(true);
    m_descriptionEdit->blockSignals(true);

    m_projectNameEdit->setText(QString::fromStdString(m_metadata.name));
    m_authorEdit->setText(QString::fromStdString(m_metadata.author));
    m_descriptionEdit->setPlainText(QString::fromStdString(m_metadata.description));

    m_projectNameEdit->blockSignals(false);
    m_authorEdit->blockSignals(false);
    m_descriptionEdit->blockSignals(false);

    if (m_metadata.createdAt > 0) {
        QDateTime createdDt = QDateTime::fromSecsSinceEpoch(m_metadata.createdAt);
        m_createdAtLabel->setText(createdDt.toString("yyyy-MM-dd hh:mm:ss"));
    } else {
        m_createdAtLabel->setText("-");
    }

    if (m_metadata.modifiedAt > 0) {
        QDateTime modifiedDt = QDateTime::fromSecsSinceEpoch(m_metadata.modifiedAt);
        m_modifiedAtLabel->setText(modifiedDt.toString("yyyy-MM-dd hh:mm:ss"));
    } else {
        m_modifiedAtLabel->setText("-");
    }

    m_filePathLabel->setText(m_projectFilePath.isEmpty() ? tr("Not saved yet") : m_projectFilePath);
}

void ProjectSection::updateSequenceSettings()
{
    if (!m_engine) return;

    NoteNagaRuntimeData *project = m_engine->getRuntimeData();
    if (!project) return;

    NoteNagaMidiSeq *seq = project->getActiveSequence();
    if (!seq) return;

    // Block signals while updating
    m_tempoSpinBox->blockSignals(true);
    m_ppqSpinBox->blockSignals(true);

    // Tempo
    int tempoMicros = seq->getTempo();
    double bpm = tempoMicros > 0 ? (60000000.0 / tempoMicros) : 120.0;
    m_tempoSpinBox->setValue(bpm);

    // PPQ
    m_ppqSpinBox->setValue(seq->getPPQ());

    m_tempoSpinBox->blockSignals(false);
    m_ppqSpinBox->blockSignals(false);
}

void ProjectSection::updateStatistics()
{
    if (!m_engine) return;

    NoteNagaRuntimeData *project = m_engine->getRuntimeData();
    if (!project) return;

    NoteNagaMidiSeq *seq = project->getActiveSequence();
    if (!seq) {
        m_trackCountLabel->setText("0");
        m_noteCountLabel->setText("0");
        m_durationLabel->setText("0:00");
        return;
    }

    // Track count
    int trackCount = static_cast<int>(seq->getTracks().size());
    m_trackCountLabel->setText(QString::number(trackCount));

    // Total notes
    int totalNotes = 0;
    for (NoteNagaTrack *track : seq->getTracks()) {
        if (track) {
            totalNotes += static_cast<int>(track->getNotes().size());
        }
    }
    m_noteCountLabel->setText(QString::number(totalNotes));

    // Duration
    int maxTick = seq->getMaxTick();
    int ppq = seq->getPPQ();
    int tempo = seq->getTempo();
    
    if (ppq > 0 && tempo > 0) {
        double secondsPerBeat = tempo / 1000000.0;
        double beats = static_cast<double>(maxTick) / ppq;
        double seconds = beats * secondsPerBeat;
        int minutes = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        m_durationLabel->setText(QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0')));
    } else {
        m_durationLabel->setText("0:00");
    }
}
