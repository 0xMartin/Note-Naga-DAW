#include "media_export_widget.h"

#include <QtWidgets>
#include <QScrollArea> 
#include <QColorDialog>
#include <QThread>
#include <QStackedWidget>

#include "../components/midi_seq_progress_bar.h" 
#include <note_naga_engine/nn_utils.h>

MediaExportWidget::MediaExportWidget(NoteNagaEngine* engine, QWidget* parent)
    : QWidget(parent), m_engine(engine), m_sequence(nullptr),
      m_previewThread(nullptr), m_previewWorker(nullptr),
      m_backgroundColor(QColor(25, 25, 35)),
      m_lightningColor(QColor(100, 200, 255)),
      m_currentTime(0.0), m_totalDuration(0.0),
      m_exportThread(nullptr), m_exporter(nullptr)
{
    setupUi();
    connectEngineSignals();
}

MediaExportWidget::~MediaExportWidget()
{
    cleanupPreviewWorker();
    
    if (m_exportThread && m_exportThread->isRunning())
    {
        m_exportThread->quit();
        m_exportThread->wait();
    }
}

void MediaExportWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create stacked widget for no-sequence placeholder and main content
    m_contentStack = new QStackedWidget(this);
    
    // No sequence placeholder
    m_noSequenceLabel = new QLabel(tr("No MIDI sequence loaded.\nOpen a MIDI file to enable export."));
    m_noSequenceLabel->setAlignment(Qt::AlignCenter);
    m_noSequenceLabel->setStyleSheet("color: #666; font-size: 16px; background-color: #1a1a1f;");
    m_contentStack->addWidget(m_noSequenceLabel);
    
    // Main content
    m_mainContent = new QWidget();
    setupMainContent();
    m_contentStack->addWidget(m_mainContent);
    
    mainLayout->addWidget(m_contentStack);
    
    // Start with no sequence view
    m_contentStack->setCurrentWidget(m_noSequenceLabel);
}

void MediaExportWidget::setupMainContent()
{
    QHBoxLayout* contentLayout = new QHBoxLayout(m_mainContent);
    contentLayout->setContentsMargins(10, 10, 10, 10);
    
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    contentLayout->addWidget(m_mainSplitter);

    // --- Left Side (Preview) ---
    m_leftWidget = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftWidget);
    leftLayout->setContentsMargins(5, 5, 5, 5); 

    QHBoxLayout *previewHeaderLayout = new QHBoxLayout;
    previewHeaderLayout->setContentsMargins(0, 0, 0, 5);
    QLabel* previewIcon = new QLabel;
    previewIcon->setPixmap(QIcon(":/icons/video.svg").pixmap(16, 16));
    QLabel* previewTitle = new QLabel(tr("Preview"));
    previewTitle->setStyleSheet("font-weight: bold;");
    previewHeaderLayout->addWidget(previewIcon);
    previewHeaderLayout->addWidget(previewTitle);
    previewHeaderLayout->addStretch();
    leftLayout->addLayout(previewHeaderLayout);

    m_previewGroup = new QGroupBox; 
    QVBoxLayout* previewLayout = new QVBoxLayout;
    previewLayout->setContentsMargins(0, 0, 0, 0); 

    QStackedWidget* previewStack = new QStackedWidget;
    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: black; border: 1px solid #444;");
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_audioOnlyLabel = new QLabel(tr("Audio Only Mode"));
    m_audioOnlyLabel->setAlignment(Qt::AlignCenter);
    m_audioOnlyLabel->setStyleSheet("background-color: black; border: 1px solid #444; color: #888; font-size: 20px; font-weight: bold;");
    m_audioOnlyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    previewStack->addWidget(m_previewLabel);
    previewStack->addWidget(m_audioOnlyLabel);
    
    previewLayout->addWidget(previewStack, 1);
    
    QHBoxLayout *timelineLayout = new QHBoxLayout;
    timelineLayout->setSpacing(6);

    int btnSize = 20; 
    QString buttonStyle = QString(R"(
        QPushButton {
            background-color: qlineargradient(spread:repeat, x1:1, y1:0, x2:1, y2:1, stop:0 #303239,stop:1 #2e3135);
            color: #fff;
            border-style: solid;
            border-width: 1px;
            border-color: #494d56;
            padding: 5px;
            min-width: %1px;
            max-width: %1px;
            min-height: %1px;
            max-height: %1px;
        }
        QPushButton:hover { background-color: #293f5b; border: 1px solid #3277c2; }
        QPushButton:pressed { background-color: #37404a; border: 1px solid #506080; }
        QPushButton:checked { background: #3477c0; border: 1.9px solid #79b8ff; }
    )").arg(btnSize);

    m_playPauseButton = new QPushButton;
    m_playPauseButton->setIcon(QIcon(":/icons/play.svg"));
    m_playPauseButton->setToolTip(tr("Play"));
    m_playPauseButton->setCheckable(true);
    m_playPauseButton->setStyleSheet(buttonStyle);
    m_playPauseButton->setIconSize(QSize(btnSize * 0.8, btnSize * 0.8)); 

    m_progressBar = new MidiSequenceProgressBar;
    m_progressBar->setFixedHeight(btnSize * 1.6); 

    timelineLayout->addWidget(m_playPauseButton);
    timelineLayout->addWidget(m_progressBar, 1);

    previewLayout->addLayout(timelineLayout); 
    m_previewGroup->setLayout(previewLayout);
    leftLayout->addWidget(m_previewGroup, 1);
    
    m_mainSplitter->addWidget(m_leftWidget);

    // --- Right Side (Settings and Export) ---
    m_rightWidget = new QWidget;
    QGridLayout *rightLayout = new QGridLayout(m_rightWidget);
    rightLayout->setContentsMargins(5, 5, 5, 5); 

    QHBoxLayout *settingsHeaderLayout = new QHBoxLayout;
    settingsHeaderLayout->setContentsMargins(0, 0, 0, 5);
    QLabel* settingsIcon = new QLabel;
    settingsIcon->setPixmap(QIcon(":/icons/settings.svg").pixmap(16, 16));
    QLabel* settingsTitle = new QLabel(tr("Settings"));
    settingsTitle->setStyleSheet("font-weight: bold;");
    settingsHeaderLayout->addWidget(settingsIcon);
    settingsHeaderLayout->addWidget(settingsTitle);
    settingsHeaderLayout->addStretch();
    rightLayout->addLayout(settingsHeaderLayout, 0, 0);

    m_settingsScrollArea = new QScrollArea;
    m_settingsScrollArea->setWidgetResizable(true);
    m_settingsScrollArea->setFrameShape(QFrame::NoFrame);
    m_settingsScrollArea->setMinimumWidth(360); 
    
    m_settingsWidget = new QWidget; 
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsWidget);
    settingsLayout->setContentsMargins(5, 5, 5, 5); 
    
    // --- Group 1: Export Settings ---
    m_exportSettingsGroup = new QGroupBox(tr("Export Settings")); 
    QFormLayout *exportFormLayout = new QFormLayout(m_exportSettingsGroup);
    exportFormLayout->setContentsMargins(5, 5, 5, 5); 
    
    m_exportTypeCombo = new QComboBox;
    m_exportTypeCombo->addItems({tr("Video (MP4)"), tr("Audio Only")});
    exportFormLayout->addRow(tr("Export Type:"), m_exportTypeCombo);
    
    settingsLayout->addWidget(m_exportSettingsGroup);

    // --- Group 1.A: Video Settings ---
    m_videoSettingsGroup = new QGroupBox(tr("Video Settings")); 
    QFormLayout *videoFormLayout = new QFormLayout(m_videoSettingsGroup);
    videoFormLayout->setContentsMargins(5, 5, 5, 5); 
    
    m_resolutionCombo = new QComboBox;
    m_resolutionCombo->addItems({"1280x720 (720p)", "1920x1080 (1080p)"});
    m_fpsCombo = new QComboBox;
    m_fpsCombo->addItems({"30 FPS", "60 FPS"});
    m_scaleSpinBox = new QDoubleSpinBox;
    m_scaleSpinBox->setRange(1.0, 15.0);
    m_scaleSpinBox->setValue(5.0);
    m_scaleSpinBox->setSuffix(tr(" s"));
    m_scaleSpinBox->setToolTip(tr("How many seconds of notes are visible on screen at once."));

    videoFormLayout->addRow(tr("Resolution:"), m_resolutionCombo);
    videoFormLayout->addRow(tr("Framerate:"), m_fpsCombo);
    videoFormLayout->addRow(tr("Vertical Scale:"), m_scaleSpinBox);

    settingsLayout->addWidget(m_videoSettingsGroup);

    // --- Group 1.B: Audio Settings ---
    m_audioSettingsGroup = new QGroupBox(tr("Audio Settings"));
    QFormLayout *audioFormLayout = new QFormLayout(m_audioSettingsGroup);
    audioFormLayout->setContentsMargins(5, 5, 5, 5);

    m_audioFormatCombo = new QComboBox;
    m_audioFormatCombo->addItems({"WAV", "MP3", "OGG"});
    m_audioBitrateSpin = new QSpinBox;
    m_audioBitrateSpin->setRange(64, 320);
    m_audioBitrateSpin->setValue(192);
    m_audioBitrateSpin->setSuffix(tr(" kbps"));
    
    audioFormLayout->addRow(tr("Format:"), m_audioFormatCombo);
    audioFormLayout->addRow(tr("Bitrate:"), m_audioBitrateSpin);
    
    settingsLayout->addWidget(m_audioSettingsGroup);

    // --- Group 2: Background Settings ---
    m_bgGroup = new QGroupBox(tr("Background Settings"));
    QGridLayout *bgLayout = new QGridLayout(m_bgGroup);
    bgLayout->setContentsMargins(5, 5, 5, 5); 
    
    m_bgColorButton = new QPushButton(tr("Select Color..."));
    m_bgColorPreview = new QLabel;
    m_bgColorPreview->setFixedSize(32, 32);
    m_bgColorPreview->setStyleSheet("border: 1px solid #555;");
    
    m_bgImageButton = new QPushButton(tr("Select Image..."));
    m_bgImagePreview = new QLabel(tr("None"));
    m_bgImagePreview->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_bgImagePreview->setStyleSheet("color: #888;");

    m_bgClearButton = new QPushButton(tr("Clear / Reset"));

    bgLayout->addWidget(m_bgColorButton, 0, 0);
    bgLayout->addWidget(m_bgColorPreview, 0, 1);
    bgLayout->addWidget(m_bgImageButton, 1, 0);
    bgLayout->addWidget(m_bgImagePreview, 1, 1);
    bgLayout->addWidget(m_bgClearButton, 2, 0, 1, 2);
    m_bgShakeCheck = new QCheckBox(tr("Enable background shake"));
    m_bgShakeSpin = new QDoubleSpinBox;
    m_bgShakeSpin->setRange(1.0, 50.0);
    m_bgShakeSpin->setValue(5.0);
    m_bgShakeSpin->setSuffix(tr(" px"));
    m_bgShakeSpin->setToolTip(tr("Max pixel distance for background shake"));
    m_bgShakeSpin->setEnabled(false);

    bgLayout->addWidget(m_bgShakeCheck, 3, 0);
    bgLayout->addWidget(m_bgShakeSpin, 3, 1);

    settingsLayout->addWidget(m_bgGroup);

    // --- Group 3: Render Settings ---
    m_renderGroup = new QGroupBox(tr("Render Settings"));
    QVBoxLayout *renderLayout = new QVBoxLayout(m_renderGroup);
    renderLayout->setContentsMargins(5, 5, 5, 5); 
    
    m_renderNotesCheck = new QCheckBox(tr("Render falling notes"));
    m_renderNotesCheck->setChecked(true);
    m_renderKeyboardCheck = new QCheckBox(tr("Render piano keyboard"));
    m_renderKeyboardCheck->setChecked(true);
    m_renderParticlesCheck = new QCheckBox(tr("Render particles"));
    m_renderParticlesCheck->setChecked(true);
    m_pianoGlowCheck = new QCheckBox(tr("Render piano glow effect"));
    m_pianoGlowCheck->setChecked(true);

    m_lightningEnableCheck = new QCheckBox(tr("Enable Lightning Effect"));
    m_lightningEnableCheck->setChecked(false);

    renderLayout->addWidget(m_lightningEnableCheck); 
    renderLayout->addWidget(m_renderNotesCheck);
    renderLayout->addWidget(m_renderKeyboardCheck);
    renderLayout->addWidget(m_renderParticlesCheck);
    renderLayout->addWidget(m_pianoGlowCheck);
    renderLayout->addSpacing(10);

    QFormLayout *noteOpacityLayout = new QFormLayout;
    m_noteStartOpacitySpin = new QDoubleSpinBox;
    m_noteStartOpacitySpin->setRange(0.0, 1.0);
    m_noteStartOpacitySpin->setSingleStep(0.1);
    m_noteStartOpacitySpin->setValue(1.0);
    m_noteEndOpacitySpin = new QDoubleSpinBox;
    m_noteEndOpacitySpin->setRange(0.0, 1.0);
    m_noteEndOpacitySpin->setSingleStep(0.1);
    m_noteEndOpacitySpin->setValue(1.0);
    noteOpacityLayout->addRow(tr("Note Opacity (Top):"), m_noteStartOpacitySpin);
    noteOpacityLayout->addRow(tr("Note Opacity (Bottom):"), m_noteEndOpacitySpin);
    renderLayout->addLayout(noteOpacityLayout);

    settingsLayout->addWidget(m_renderGroup); 

    // --- Group 4: Particle Settings ---
    m_particleSettingsGroup = new QGroupBox(tr("Particle Settings"));
    QFormLayout *particleForm = new QFormLayout(m_particleSettingsGroup);
    particleForm->setContentsMargins(5, 5, 5, 5); 
    
    m_particleTypeCombo = new QComboBox;
    m_particleTypeCombo->addItems({tr("Default (Sparkle)"), tr("Circle"), tr("Custom Image")});
    particleForm->addRow(tr("Particle Type:"), m_particleTypeCombo);

    QHBoxLayout* fileLayout = new QHBoxLayout;
    m_particleFileButton = new QPushButton(tr("Select..."));
    m_particlePreviewLabel = new QLabel;
    m_particlePreviewLabel->setFixedSize(32, 32);
    m_particlePreviewLabel->setStyleSheet("border: 1px solid #555;");
    m_particlePreviewLabel->setAlignment(Qt::AlignCenter);
    fileLayout->addWidget(m_particleFileButton);
    fileLayout->addWidget(m_particlePreviewLabel);
    fileLayout->addStretch();
    particleForm->addRow(tr("Custom File:"), fileLayout);

    m_particleCountSpin = new QSpinBox;
    m_particleCountSpin->setRange(1, 100);
    m_particleCountSpin->setValue(15);
    particleForm->addRow(tr("Count (per note):"), m_particleCountSpin);

    m_particleLifetimeSpin = new QDoubleSpinBox;
    m_particleLifetimeSpin->setRange(0.1, 5.0);
    m_particleLifetimeSpin->setValue(0.75);
    m_particleLifetimeSpin->setSuffix(" s");
    m_particleLifetimeSpin->setSingleStep(0.1);
    particleForm->addRow(tr("Lifetime:"), m_particleLifetimeSpin);

    m_particleSpeedSpin = new QDoubleSpinBox;
    m_particleSpeedSpin->setRange(10.0, 500.0);
    m_particleSpeedSpin->setValue(75.0);
    m_particleSpeedSpin->setSingleStep(5.0);
    particleForm->addRow(tr("Initial Speed:"), m_particleSpeedSpin);

    m_particleGravitySpin = new QDoubleSpinBox;
    m_particleGravitySpin->setRange(0.0, 1000.0);
    m_particleGravitySpin->setValue(200.0);
    m_particleGravitySpin->setSingleStep(10.0);
    particleForm->addRow(tr("Gravity:"), m_particleGravitySpin);
    
    m_particleStartSizeSpin = new QDoubleSpinBox;
    m_particleStartSizeSpin->setRange(0.1, 5.0);
    m_particleStartSizeSpin->setValue(0.5);
    m_particleStartSizeSpin->setSuffix("x");
    m_particleStartSizeSpin->setSingleStep(0.1);
    particleForm->addRow(tr("Start Size Multiplier:"), m_particleStartSizeSpin);

    m_particleEndSizeSpin = new QDoubleSpinBox;
    m_particleEndSizeSpin->setRange(0.1, 10.0);
    m_particleEndSizeSpin->setValue(1.0);
    m_particleEndSizeSpin->setSuffix("x");
    m_particleEndSizeSpin->setSingleStep(0.1);
    particleForm->addRow(tr("End Size Multiplier:"), m_particleEndSizeSpin);

    m_particleTintCheck = new QCheckBox(tr("Tint with note color"));
    m_particleTintCheck->setChecked(true);
    particleForm->addRow(m_particleTintCheck);

    settingsLayout->addWidget(m_particleSettingsGroup);

    // --- Group 5: Lightning Settings ---
    m_lightningGroup = new QGroupBox(tr("Lightning Effect")); 
    
    QFormLayout *lightningForm = new QFormLayout;
    lightningForm->setContentsMargins(5, 5, 5, 5);
    
    QHBoxLayout* colorLayout = new QHBoxLayout;
    m_lightningColorButton = new QPushButton(tr("Select Color...")); 
    m_lightningColorPreview = new QLabel; 
    m_lightningColorPreview->setFixedSize(32, 32);
    colorLayout->addWidget(m_lightningColorButton);
    colorLayout->addWidget(m_lightningColorPreview);
    colorLayout->addStretch();
    lightningForm->addRow(tr("Color:"), colorLayout);

    m_lightningThicknessSpin = new QDoubleSpinBox;
    m_lightningThicknessSpin->setRange(1.0, 10.0);
    m_lightningThicknessSpin->setValue(2.0);
    m_lightningThicknessSpin->setSuffix(" px");
    m_lightningThicknessSpin->setSingleStep(0.1);
    lightningForm->addRow(tr("Base Thickness:"), m_lightningThicknessSpin);

    m_lightningLinesSpin = new QSpinBox; 
    m_lightningLinesSpin->setRange(1, 10);
    m_lightningLinesSpin->setValue(3);
    lightningForm->addRow(tr("Number of Lines:"), m_lightningLinesSpin);

    m_lightningJitterYSpin = new QDoubleSpinBox;
    m_lightningJitterYSpin->setRange(0.0, 20.0);
    m_lightningJitterYSpin->setValue(3.0);
    m_lightningJitterYSpin->setSuffix(" px");
    m_lightningJitterYSpin->setSingleStep(0.5);
    lightningForm->addRow(tr("Vertical Jitter:"), m_lightningJitterYSpin);

    m_lightningJitterXSpin = new QDoubleSpinBox;
    m_lightningJitterXSpin->setRange(0.0, 20.0);
    m_lightningJitterXSpin->setValue(2.0);
    m_lightningJitterXSpin->setSuffix(" px");
    m_lightningJitterXSpin->setSingleStep(0.5);
    lightningForm->addRow(tr("Horizontal Jitter:"), m_lightningJitterXSpin);

    QVBoxLayout *lightningVLayout = new QVBoxLayout(m_lightningGroup);
    lightningVLayout->setContentsMargins(5, 5, 5, 5);
    lightningVLayout->addLayout(lightningForm);
    
    settingsLayout->addWidget(m_lightningGroup);
    settingsLayout->addStretch(1); 

    m_settingsScrollArea->setWidget(m_settingsWidget); 
    rightLayout->addWidget(m_settingsScrollArea, 1, 0);

    // --- Section for export and progress (bottom right) ---
    QVBoxLayout* exportLayout = new QVBoxLayout;
    exportLayout->setContentsMargins(10, 10, 10, 10);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_exportButton = new QPushButton(tr("Export..."));
    m_exportButton->setIcon(QIcon(":/icons/video.svg")); 
    m_exportButton->setFixedHeight(40);
    m_exportButton->setMinimumWidth(200); 

    buttonLayout->addStretch(1); 
    buttonLayout->addWidget(m_exportButton);
    exportLayout->addLayout(buttonLayout); 

    m_progressWidget = new QWidget;
    QFormLayout* progressFormLayout = new QFormLayout(m_progressWidget);
    progressFormLayout->setContentsMargins(0, 10, 0, 0);
    m_audioProgressBar = new QProgressBar;
    m_videoProgressBar = new QProgressBar;
    m_audioProgressLabel = new QLabel(tr("Audio Rendering:"));
    m_videoProgressLabel = new QLabel(tr("Video Rendering:"));
    progressFormLayout->addRow(m_audioProgressLabel, m_audioProgressBar);
    progressFormLayout->addRow(m_videoProgressLabel, m_videoProgressBar);
    exportLayout->addWidget(m_progressWidget);
    
    m_statusLabel = new QLabel;
    m_statusLabel->setAlignment(Qt::AlignRight); 
    exportLayout->addWidget(m_statusLabel);
    exportLayout->addStretch(1); 

    rightLayout->addLayout(exportLayout, 2, 0);
    rightLayout->setRowStretch(0, 0); 
    rightLayout->setRowStretch(1, 1); 
    rightLayout->setRowStretch(2, 0); 

    m_mainSplitter->addWidget(m_rightWidget);
    m_mainSplitter->setSizes({600, 300});

    m_progressWidget->setVisible(false);
    
    // Initial state
    m_lightningColorPreview->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_lightningColor.name()));
    m_lightningGroup->setEnabled(false);
    updateBgLabels();
    
    connectWidgetSignals();
}

void MediaExportWidget::connectWidgetSignals()
{
    // Playback
    connect(m_playPauseButton, &QPushButton::clicked, this, &MediaExportWidget::onPlayPauseClicked);
    connect(m_progressBar, &MidiSequenceProgressBar::positionPressed, this, &MediaExportWidget::seek);
    connect(m_progressBar, &MidiSequenceProgressBar::positionDragged, this, &MediaExportWidget::seek);
    connect(m_progressBar, &MidiSequenceProgressBar::positionReleased, this, &MediaExportWidget::seek);
    connect(m_exportButton, &QPushButton::clicked, this, &MediaExportWidget::onExportClicked);
    connect(m_exportTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaExportWidget::onExportTypeChanged);
    
    // Settings
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaExportWidget::updatePreviewSettings);
    connect(m_scaleSpinBox, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    
    // Audio bitrate suffix
    connect(m_audioBitrateSpin, &QSpinBox::valueChanged, this, [this](int val){
        if(m_audioFormatCombo->currentText().toLower() == "ogg") {
            int q_val = (val - 64) / 32 + 1;
            if (val < 64) q_val = 0;
            if (val > 320) q_val = 10;
            m_audioBitrateSpin->setSuffix(QString(" kbps (q: %1)").arg(q_val));
        } else {
            m_audioBitrateSpin->setSuffix(tr(" kbps"));
        }
    });
    connect(m_audioFormatCombo, &QComboBox::currentTextChanged, this, [this](const QString& text){
        if (text.toLower() == "ogg") {
            m_audioBitrateSpin->setSuffix(tr(" kbps (q: ...)"));
            m_audioBitrateSpin->setToolTip(tr("Pro OGG je bitrate převeden na škálu kvality (q) 0-10."));
        } else {
            m_audioBitrateSpin->setSuffix(tr(" kbps"));
            m_audioBitrateSpin->setToolTip(tr("Typický bitrate pro MP3 je 192 nebo 256 kbps."));
        }
        m_audioBitrateSpin->setValue(m_audioBitrateSpin->value());
    });

    // Background
    connect(m_bgColorButton, &QPushButton::clicked, this, &MediaExportWidget::onSelectBgColor);
    connect(m_bgImageButton, &QPushButton::clicked, this, &MediaExportWidget::onSelectBgImage);
    connect(m_bgClearButton, &QPushButton::clicked, this, &MediaExportWidget::onClearBg);
    connect(m_bgShakeCheck, &QCheckBox::toggled, m_bgShakeSpin, &QWidget::setEnabled);
    connect(m_bgShakeCheck, &QCheckBox::checkStateChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_bgShakeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);

    // Render
    connect(m_renderNotesCheck, &QCheckBox::checkStateChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_renderKeyboardCheck, &QCheckBox::checkStateChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_renderParticlesCheck, &QCheckBox::checkStateChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_renderParticlesCheck, &QCheckBox::toggled, m_particleSettingsGroup, &QWidget::setEnabled);
    connect(m_pianoGlowCheck, &QCheckBox::checkStateChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_noteStartOpacitySpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_noteEndOpacitySpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);

    // Particles
    connect(m_particleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaExportWidget::onParticleTypeChanged);
    connect(m_particleFileButton, &QPushButton::clicked, this, &MediaExportWidget::onSelectParticleFile);
    connect(m_particleCountSpin, &QSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_particleLifetimeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_particleSpeedSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_particleGravitySpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_particleStartSizeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_particleEndSizeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_particleTintCheck, &QCheckBox::checkStateChanged, this, &MediaExportWidget::updatePreviewSettings);

    // Lightning
    connect(m_lightningEnableCheck, &QCheckBox::toggled, m_lightningGroup, &QWidget::setEnabled);
    connect(m_lightningEnableCheck, &QCheckBox::checkStateChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_lightningColorButton, &QPushButton::clicked, this, &MediaExportWidget::onSelectLightningColor);
    connect(m_lightningThicknessSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_lightningLinesSpin, &QSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_lightningJitterYSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);
    connect(m_lightningJitterXSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportWidget::updatePreviewSettings);

    // Initial state
    onParticleTypeChanged(m_particleTypeCombo->currentIndex());
    onExportTypeChanged(m_exportTypeCombo->currentIndex());
}

void MediaExportWidget::connectEngineSignals()
{
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::currentTickChanged, 
            this, &MediaExportWidget::onPlaybackTickChanged);
    
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::playingStateChanged, 
            this, [this](bool playing){
                m_playPauseButton->setChecked(playing);
                m_playPauseButton->setToolTip(playing ? tr("Pause") : tr("Play"));
                m_playPauseButton->setIcon(playing ? QIcon(":/icons/stop.svg") : QIcon(":/icons/play.svg"));
            });
    
    // Listen for sequence changes
    connect(m_engine->getProject(), &NoteNagaProject::activeSequenceChanged,
            this, [this](NoteNagaMidiSeq*) {
                refreshSequence();
            });
}

void MediaExportWidget::refreshSequence()
{
    m_sequence = m_engine->getProject()->getActiveSequence();
    
    if (!m_sequence) {
        m_contentStack->setCurrentWidget(m_noSequenceLabel);
        cleanupPreviewWorker();
        return;
    }
    
    m_contentStack->setCurrentWidget(m_mainContent);
    
    m_totalDuration = nn_ticks_to_seconds(m_sequence->getMaxTick(), m_sequence->getPPQ(), m_sequence->getTempo());
    m_progressBar->setMidiSequence(m_sequence);
    m_progressBar->updateMaxTime();
    
    initPreviewWorker();
    
    QTimer::singleShot(10, this, &MediaExportWidget::updatePreviewSettings);
    onPlaybackTickChanged(m_engine->getProject()->getCurrentTick());
}

void MediaExportWidget::initPreviewWorker()
{
    if (!m_sequence) return;
    
    cleanupPreviewWorker();
    
    m_previewThread = new QThread(this);
    m_previewWorker = new PreviewWorker(m_sequence);
    m_previewWorker->moveToThread(m_previewThread);

    connect(this, &MediaExportWidget::destroyed, m_previewWorker, &PreviewWorker::deleteLater);
    connect(m_previewThread, &QThread::started, m_previewWorker, &PreviewWorker::init);
    connect(m_previewWorker, &PreviewWorker::frameReady, this, &MediaExportWidget::onPreviewFrameReady, Qt::QueuedConnection);
    
    m_previewThread->start();
}

void MediaExportWidget::cleanupPreviewWorker()
{
    if (m_previewThread) {
        m_previewThread->quit();
        m_previewThread->wait();
        m_previewThread = nullptr;
        m_previewWorker = nullptr;
    }
}

void MediaExportWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePreviewRenderSize();
}

void MediaExportWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshSequence();
}

void MediaExportWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    // Optionally pause preview when hidden
}

QSize MediaExportWidget::getTargetResolution()
{
    return (m_resolutionCombo->currentIndex() == 0) ? QSize(1280, 720) : QSize(1920, 1080);
}

void MediaExportWidget::updatePreviewRenderSize()
{
    if (!m_previewWorker) return;
    
    QSize targetRes = getTargetResolution();
    QSize labelSize = m_previewLabel->size();
    if (labelSize.isEmpty()) return;

    QSize renderSize = targetRes;
    renderSize.scale(labelSize, Qt::KeepAspectRatio);
    
    if (renderSize != m_lastRenderSize) {
        m_lastRenderSize = renderSize;
        QMetaObject::invokeMethod(m_previewWorker, "updateSize", Qt::QueuedConnection,
                                  Q_ARG(QSize, m_lastRenderSize));
    }
}

MediaRenderer::RenderSettings MediaExportWidget::getCurrentRenderSettings()
{
    MediaRenderer::RenderSettings settings;
    settings.backgroundColor = m_backgroundColor;
    if (!m_backgroundImagePath.isEmpty()) {
        settings.backgroundImage = QImage(m_backgroundImagePath);
    }
    settings.renderBgShake = m_bgShakeCheck->isChecked();
    settings.bgShakeIntensity = m_bgShakeSpin->value();
    settings.renderNotes = m_renderNotesCheck->isChecked();
    settings.renderKeyboard = m_renderKeyboardCheck->isChecked();
    settings.renderParticles = m_renderParticlesCheck->isChecked();
    settings.renderPianoGlow = m_pianoGlowCheck->isChecked();
    settings.noteStartOpacity = m_noteStartOpacitySpin->value();
    settings.noteEndOpacity = m_noteEndOpacitySpin->value();
    settings.particleType = (MediaRenderer::RenderSettings::ParticleType)m_particleTypeCombo->currentIndex();
    if (settings.particleType == MediaRenderer::RenderSettings::Custom && !m_particleFilePath.isEmpty()) {
        settings.customParticleImage = QImage(m_particleFilePath);
    }
    settings.particleCount = m_particleCountSpin->value();
    settings.particleLifetime = m_particleLifetimeSpin->value();
    settings.particleSpeed = m_particleSpeedSpin->value();
    settings.particleGravity = m_particleGravitySpin->value();
    settings.tintParticles = m_particleTintCheck->isChecked();
    settings.particleStartSize = m_particleStartSizeSpin->value();
    settings.particleEndSize = m_particleEndSizeSpin->value();
    
    settings.renderLightning = m_lightningEnableCheck->isChecked();
    settings.lightningColor = m_lightningColor;
    settings.lightningThickness = m_lightningThicknessSpin->value();
    settings.lightningLines = m_lightningLinesSpin->value();
    settings.lightningJitterY = m_lightningJitterYSpin->value(); 
    settings.lightningJitterX = m_lightningJitterXSpin->value();

    return settings;
}

void MediaExportWidget::updatePreviewSettings()
{
    if (!m_previewWorker || m_exportTypeCombo->currentIndex() != 0) {
        return;
    }
    
    QMetaObject::invokeMethod(m_previewWorker, "updateSettings", Qt::QueuedConnection,
                              Q_ARG(MediaRenderer::RenderSettings, getCurrentRenderSettings()));
                              
    QMetaObject::invokeMethod(m_previewWorker, "updateScale", Qt::QueuedConnection,
                              Q_ARG(double, m_scaleSpinBox->value()));

    updatePreviewRenderSize();
    
    if (!m_engine->isPlaying()) {
        QMetaObject::invokeMethod(m_previewWorker, "updateTime", Qt::QueuedConnection,
                                  Q_ARG(double, m_currentTime));
    }
}

void MediaExportWidget::onPlayPauseClicked()
{
    if (m_engine->isPlaying()) {
        m_engine->stopPlayback();
    } else {
        m_engine->startPlayback();
    }
}

void MediaExportWidget::seek(float seconds)
{
    if (m_engine->isPlaying()) {
        m_engine->stopPlayback();
    }
    m_currentTime = (double)seconds;
    int tick = nn_seconds_to_ticks(m_currentTime, m_sequence->getPPQ(), m_sequence->getTempo());
    m_engine->setPlaybackPosition(tick);
    onPlaybackTickChanged(tick);
}

void MediaExportWidget::onPlaybackTickChanged(int tick)
{
    if (!m_sequence) return;
    
    m_currentTime = nn_ticks_to_seconds(tick, m_sequence->getPPQ(), m_sequence->getTempo());

    if (m_previewWorker && m_exportTypeCombo->currentIndex() == 0) {
        QMetaObject::invokeMethod(m_previewWorker, "updateTime", Qt::QueuedConnection,
                                  Q_ARG(double, m_currentTime));
    }

    m_progressBar->blockSignals(true);
    m_progressBar->setCurrentTime(m_currentTime);
    m_progressBar->blockSignals(false);
}

void MediaExportWidget::onPreviewFrameReady(const QImage& frame)
{
    if (m_exportTypeCombo->currentIndex() != 0) {
        return;
    }
    
    QPixmap scaledPixmap(m_previewLabel->size());
    scaledPixmap.fill(Qt::black);

    QPainter p(&scaledPixmap);
    int x = (scaledPixmap.width() - frame.width()) / 2;
    int y = (scaledPixmap.height() - frame.height()) / 2;
    p.drawPixmap(x, y, QPixmap::fromImage(frame));
    p.end();
    
    m_previewLabel->setPixmap(scaledPixmap);
}

void MediaExportWidget::onExportTypeChanged(int index)
{
    bool isVideo = (index == 0);
    
    QStackedWidget* stack = m_previewLabel->parentWidget()->findChild<QStackedWidget*>();
    if (stack) {
        stack->setCurrentIndex(isVideo ? 0 : 1);
    }
    
    m_exportButton->setText(isVideo ? tr("Export to MP4") : tr("Export Audio..."));
    m_exportButton->setIcon(isVideo ? QIcon(":/icons/video.svg") : QIcon(":/icons/audio-signal.svg"));

    m_videoSettingsGroup->setVisible(isVideo);
    m_audioSettingsGroup->setVisible(!isVideo);
    m_bgGroup->setVisible(isVideo);
    m_renderGroup->setVisible(isVideo);
    m_particleSettingsGroup->setVisible(isVideo);
    m_lightningGroup->setVisible(isVideo);

    if (!m_engine->isPlaying() && isVideo) {
        updatePreviewSettings();
    }
}

void MediaExportWidget::onParticleTypeChanged(int index)
{
    bool isCustom = (index == (int)MediaRenderer::RenderSettings::Custom);
    m_particleFileButton->setVisible(isCustom);
    m_particlePreviewLabel->setVisible(isCustom);

    bool isPixmap = (index == (int)MediaRenderer::RenderSettings::Resource || isCustom);
    m_particleTintCheck->setVisible(isPixmap);
    m_particleStartSizeSpin->setVisible(isPixmap);
    m_particleEndSizeSpin->setVisible(isPixmap);

    if (isCustom && !m_particleFilePath.isEmpty()) {
        QPixmap pixmap(m_particleFilePath);
        m_particlePreviewLabel->setPixmap(pixmap.scaled(m_particlePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (index == (int)MediaRenderer::RenderSettings::Resource) {
        QPixmap pixmap(":/images/sparkle.png"); 
        m_particlePreviewLabel->setPixmap(pixmap.scaled(m_particlePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_particlePreviewLabel->setVisible(true); 
    } else {
        m_particlePreviewLabel->clear(); 
        m_particlePreviewLabel->setVisible(false); 
    }

    updatePreviewSettings();
}

void MediaExportWidget::onSelectParticleFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Particle Image"), "", tr("Images (*.png *.jpg *.bmp)"));
    if (!path.isEmpty()) {
        m_particleFilePath = path;
        QPixmap pixmap(path);
        m_particlePreviewLabel->setPixmap(pixmap.scaled(m_particlePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        updatePreviewSettings();
    }
}

void MediaExportWidget::onSelectBgColor()
{
    QColor color = QColorDialog::getColor(m_backgroundColor, this, tr("Select Background Color"));
    if (color.isValid()) {
        m_backgroundColor = color;
        m_backgroundImagePath.clear();
        updateBgLabels();
        updatePreviewSettings();
    }
}

void MediaExportWidget::onSelectBgImage()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Background Image"), "", tr("Images (*.png *.jpg *.bmp)"));
    if (!path.isEmpty()) {
        m_backgroundImagePath = path;
        updateBgLabels();
        updatePreviewSettings();
    }
}

void MediaExportWidget::onClearBg()
{
    m_backgroundImagePath.clear();
    m_backgroundColor = QColor(25, 25, 35);
    updateBgLabels();
    updatePreviewSettings();
}

void MediaExportWidget::updateBgLabels()
{
    m_bgColorPreview->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_backgroundColor.name()));
    
    if (!m_backgroundImagePath.isEmpty()) {
        QFileInfo info(m_backgroundImagePath);
        m_bgImagePreview->setText(info.fileName());
        m_bgImagePreview->setStyleSheet("color: #DDD;");
    } else {
        m_bgImagePreview->setText(tr("None"));
        m_bgImagePreview->setStyleSheet("color: #888;");
    }
}

void MediaExportWidget::onSelectLightningColor()
{
    QColor color = QColorDialog::getColor(m_lightningColor, this, tr("Select Lightning Color"));
    if (color.isValid()) {
        m_lightningColor = color;
        m_lightningColorPreview->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_lightningColor.name()));
        updatePreviewSettings();
    }
}

void MediaExportWidget::onExportClicked()
{
    if (!m_sequence) {
        QMessageBox::warning(this, tr("No Sequence"), tr("No MIDI sequence loaded."));
        return;
    }
    
    MediaExporter::ExportMode mode = (m_exportTypeCombo->currentIndex() == 0) 
                                     ? MediaExporter::Video 
                                     : MediaExporter::AudioOnly;
    
    QString audioFormat = m_audioFormatCombo->currentText().toLower();
    int audioBitrate = m_audioBitrateSpin->value();
    
    QString filter;
    QString defaultSuffix;
    
    if (mode == MediaExporter::AudioOnly) {
        if (audioFormat == "mp3") {
            filter = tr("MP3 Audio (*.mp3)");
            defaultSuffix = ".mp3";
        } else if (audioFormat == "ogg") {
            filter = tr("OGG Vorbis Audio (*.ogg)");
            defaultSuffix = ".ogg";
        } else {
            filter = tr("WAV Audio (*.wav)");
            defaultSuffix = ".wav";
        }
    } else {
        filter = tr("MPEG-4 Video (*.mp4)");
        defaultSuffix = ".mp4";
    }

    QString outputPath = QFileDialog::getSaveFileName(this, tr("Save File"), "", filter);
    if (outputPath.isEmpty())
        return;
        
    if (!outputPath.endsWith(defaultSuffix, Qt::CaseInsensitive)) {
        outputPath += defaultSuffix;
    }

    QSize resolution = getTargetResolution();
    int fps = (m_fpsCombo->currentIndex() == 0) ? 30 : 60;
    double secondsVisible = m_scaleSpinBox->value();

    MediaRenderer::RenderSettings settings = getCurrentRenderSettings();

    setControlsEnabled(false);

    m_exportThread = new QThread;
    
    m_exporter = new MediaExporter(m_sequence, outputPath, resolution, fps, m_engine, 
                                   secondsVisible, settings, 
                                   mode, audioFormat, audioBitrate);
    
    m_exporter->moveToThread(m_exportThread);

    connect(m_exportThread, &QThread::started, m_exporter, &MediaExporter::doExport);
    connect(m_exporter, &MediaExporter::finished, this, &MediaExportWidget::onExportFinished);
    connect(m_exporter, &MediaExporter::error, this, [this](const QString &msg) {
        QMessageBox::critical(this, tr("Error"), msg);
        onExportFinished();
    });

    connect(m_exporter, &MediaExporter::audioProgressUpdated, this, &MediaExportWidget::updateAudioProgress);
    connect(m_exporter, &MediaExporter::videoProgressUpdated, this, &MediaExportWidget::updateVideoProgress);
    connect(m_exporter, &MediaExporter::statusTextChanged, this, &MediaExportWidget::updateStatusText);

    connect(m_exporter, &MediaExporter::finished, m_exportThread, &QThread::quit);
    connect(m_exporter, &MediaExporter::finished, m_exporter, &MediaExporter::deleteLater);
    connect(m_exportThread, &QThread::finished, m_exportThread, &QThread::deleteLater);

    m_exportThread->start();
}

void MediaExportWidget::updateAudioProgress(int percentage)
{
    m_audioProgressBar->setValue(percentage);
}

void MediaExportWidget::updateVideoProgress(int percentage)
{
    m_videoProgressBar->setValue(percentage);
}

void MediaExportWidget::updateStatusText(const QString &status)
{
    m_statusLabel->setText(status);
}

void MediaExportWidget::onExportFinished()
{
    setControlsEnabled(true);
    
    if (!m_statusLabel->text().contains(tr("Error"), Qt::CaseInsensitive)) {
        if (m_exportTypeCombo->currentIndex() == 0) {
            QMessageBox::information(this, tr("Success"), tr("Video export finished successfully."));
        } else {
            QMessageBox::information(this, tr("Success"), tr("Audio export finished successfully."));
        }
    }
    m_exportThread = nullptr;
    m_exporter = nullptr;
}

void MediaExportWidget::setControlsEnabled(bool enabled)
{
    m_previewGroup->setEnabled(enabled);
    m_settingsScrollArea->setEnabled(enabled); 
    m_exportButton->setEnabled(enabled);
    m_progressWidget->setVisible(!enabled);

    bool isAudioOnly = (m_exportTypeCombo->currentIndex() == 1);

    m_videoProgressLabel->setVisible(!enabled && !isAudioOnly);
    m_videoProgressBar->setVisible(!enabled && !isAudioOnly);
    
    m_audioProgressLabel->setText((!enabled && isAudioOnly) ? tr("Progress:") : tr("Audio Rendering:"));

    if (m_previewThread) {
        m_previewThread->setPriority(enabled ? QThread::InheritPriority : QThread::IdlePriority);
    }

    if (enabled && !isAudioOnly) {
        updatePreviewSettings();
    }

    if (enabled) {
        m_audioProgressBar->setValue(0);
        m_audioProgressBar->setMaximum(100);
        m_videoProgressBar->setValue(0);
        m_videoProgressBar->setMaximum(100);
        m_statusLabel->clear();
    }
}
