#include "media_export_section.h"

#include <QtWidgets>
#include <QScrollArea> 
#include <QColorDialog>
#include <QThread>
#include <QStackedWidget>
#include <QDockWidget>
#include <QDateTime>

#include "../components/midi_seq_progress_bar.h" 
#include "../dock_system/advanced_dock_widget.h"
#include <note_naga_engine/nn_utils.h>

MediaExportSection::MediaExportSection(NoteNagaEngine* engine, QWidget* parent)
    : QMainWindow(parent), m_engine(engine), m_sequence(nullptr),
      m_previewThread(nullptr), m_previewWorker(nullptr),
      m_backgroundColor(QColor(25, 25, 35)),
      m_lightningColor(QColor(100, 200, 255)),
      m_currentTime(0.0), m_totalDuration(0.0),
      m_exportThread(nullptr), m_exporter(nullptr),
      m_frameCount(0), m_lastFpsUpdate(0),
      m_sectionActive(false)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Remove central widget - we only use docks
    setCentralWidget(nullptr);
    
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupUi();
    connectEngineSignals();
}

MediaExportSection::~MediaExportSection()
{
    cleanupPreviewWorker();
    
    if (m_exportThread && m_exportThread->isRunning())
    {
        m_exportThread->quit();
        m_exportThread->wait();
    }
}

void MediaExportSection::onSectionActivated()
{
    m_sectionActive = true;
    
    // Start preview worker when section becomes visible
    if (m_sequence && !m_previewWorker) {
        initPreviewWorker();
        updatePreviewSettings();
    }
}

void MediaExportSection::onSectionDeactivated()
{
    m_sectionActive = false;
    
    // Stop preview worker to save resources when section is hidden
    cleanupPreviewWorker();
}

void MediaExportSection::setupUi()
{
    // For dock-only layout, we use a dummy empty central widget
    // The actual content is in dock widgets
    QWidget *dummyCentral = new QWidget(this);
    dummyCentral->setMaximumSize(0, 0);
    setCentralWidget(dummyCentral);

    // Create no-sequence placeholder as overlay
    m_noSequenceLabel = new QLabel(tr("No MIDI sequence loaded.\nOpen a MIDI file to enable export."), this);
    m_noSequenceLabel->setAlignment(Qt::AlignCenter);
    m_noSequenceLabel->setStyleSheet("color: #666; font-size: 16px; background-color: #1a1a1f;");
    m_noSequenceLabel->setGeometry(rect());
    m_noSequenceLabel->raise();
    
    // Setup dock layout with actual content
    setupDockLayout();
    
    // Initially hide docks and show placeholder
    for (auto dock : m_docks) {
        dock->hide();
    }
    m_noSequenceLabel->show();
}

void MediaExportSection::setupMainContent()
{
    // No longer needed - content is in setupDockLayout
}

void MediaExportSection::setupDockLayout()
{
    // === LEFT DOCK: Preview ===
    QWidget *previewContainer = new QWidget(this);
    previewContainer->setStyleSheet("background: transparent;");
    QVBoxLayout *previewLayout = new QVBoxLayout(previewContainer);
    previewLayout->setContentsMargins(5, 5, 5, 5);
    previewLayout->setSpacing(5);

    // Preview stack (video preview or audio bars)
    m_previewStack = new QStackedWidget;
    m_previewStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewStack->setMinimumSize(200, 150);  // Ensure minimum visible size
    
    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: black; border: 1px solid #444; border-radius: 4px;");
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->installEventFilter(this);  // Catch resize events for preview size update
    
    m_audioBarsVisualizer = new AudioBarsVisualizer(m_engine, this);
    m_audioBarsVisualizer->setBarCount(24);
    m_audioBarsVisualizer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_previewStack->addWidget(m_previewLabel);
    m_previewStack->addWidget(m_audioBarsVisualizer);
    
    previewLayout->addWidget(m_previewStack, 1);
    
    // Preview stats overlay
    m_previewStatsLabel = new QLabel(this);
    m_previewStatsLabel->setStyleSheet(R"(
        QLabel {
            background-color: rgba(0, 0, 0, 180);
            color: #aaffaa;
            font-family: monospace;
            font-size: 11px;
            padding: 4px 8px;
            border-radius: 4px;
        }
    )");
    m_previewStatsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_previewStatsLabel->setText(tr("FPS: -- | Frame: -- ms | Resolution: --"));
    previewLayout->addWidget(m_previewStatsLabel);
    
    // Timeline controls
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

    QHBoxLayout *timelineLayout = new QHBoxLayout;
    timelineLayout->setSpacing(6);

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

    auto *previewDock = new AdvancedDockWidget(
        tr("Preview"), 
        QIcon(":/icons/video.svg"),
        nullptr, 
        this
    );
    previewDock->setWidget(previewContainer);
    previewDock->setObjectName("preview");
    previewDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    previewDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, previewDock);
    m_docks["preview"] = previewDock; 

    // === RIGHT DOCK: Settings ===
    // Settings scroll area directly in dock - no extra container
    m_settingsScrollArea = new QScrollArea;
    m_settingsScrollArea->setWidgetResizable(true);
    m_settingsScrollArea->setFrameShape(QFrame::NoFrame);
    m_settingsScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_settingsScrollArea->setMinimumWidth(380);
    m_settingsScrollArea->setMaximumWidth(450);
    m_settingsScrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding); 
    
    m_settingsWidget = new QWidget;
    m_settingsWidget->setStyleSheet("background: transparent;");
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsWidget);
    settingsLayout->setContentsMargins(5, 5, 5, 5);
    settingsLayout->setSpacing(8);
    
    // Common styles for section headers
    QString sectionHeaderStyle = "font-size: 14px; font-weight: bold; color: #79b8ff; padding: 5px 0px;";
    QString groupBoxStyle = R"(
        QGroupBox {
            background: #2a2d35;
            border: 1px solid #3a3d45;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 5px;
            color: #79b8ff;
            font-weight: bold;
        }
    )";
    QString comboBoxStyle = R"(
        QComboBox {
            background: #232731;
            color: #fff;
            border: 1px solid #494d56;
            border-radius: 4px;
            padding: 4px 8px;
            min-width: 150px;
        }
        QComboBox:hover { border-color: #79b8ff; }
        QComboBox::drop-down { border: none; width: 20px; }
    )";
    QString spinBoxStyle = R"(
        QSpinBox, QDoubleSpinBox {
            background: #232731;
            color: #fff;
            border: 1px solid #494d56;
            border-radius: 4px;
            padding: 4px 8px;
            min-width: 100px;
        }
        QSpinBox:hover, QDoubleSpinBox:hover { border-color: #79b8ff; }
    )";
    QString labelStyle = "color: #ccc;";
    
    // --- Group 1: Export Settings ---
    m_exportSettingsGroup = new QGroupBox(tr("Export Settings"));
    m_exportSettingsGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *exportFormLayout = new QFormLayout(m_exportSettingsGroup);
    exportFormLayout->setContentsMargins(12, 20, 12, 12);
    exportFormLayout->setSpacing(8);
    exportFormLayout->setLabelAlignment(Qt::AlignLeft);
    
    m_exportTypeCombo = new QComboBox;
    m_exportTypeCombo->addItems({tr("Video (MP4)"), tr("Audio Only")});
    m_exportTypeCombo->setStyleSheet(comboBoxStyle);
    QLabel *exportTypeLabel = new QLabel(tr("Export Type:"));
    exportTypeLabel->setStyleSheet(labelStyle);
    exportFormLayout->addRow(exportTypeLabel, m_exportTypeCombo);
    
    settingsLayout->addWidget(m_exportSettingsGroup);

    // --- Group 1.A: Video Settings ---
    m_videoSettingsGroup = new QGroupBox(tr("Video Settings"));
    m_videoSettingsGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *videoFormLayout = new QFormLayout(m_videoSettingsGroup);
    videoFormLayout->setContentsMargins(12, 20, 12, 12);
    videoFormLayout->setSpacing(8);
    videoFormLayout->setLabelAlignment(Qt::AlignLeft);
    
    m_resolutionCombo = new QComboBox;
    m_resolutionCombo->addItems({"1280x720 (720p)", "1920x1080 (1080p)"});
    m_resolutionCombo->setStyleSheet(comboBoxStyle);
    m_fpsCombo = new QComboBox;
    m_fpsCombo->addItems({"30 FPS", "60 FPS"});
    m_fpsCombo->setStyleSheet(comboBoxStyle);
    m_scaleSpinBox = new QDoubleSpinBox;
    m_scaleSpinBox->setRange(1.0, 15.0);
    m_scaleSpinBox->setValue(5.0);
    m_scaleSpinBox->setSuffix(tr(" s"));
    m_scaleSpinBox->setToolTip(tr("How many seconds of notes are visible on screen at once."));
    m_scaleSpinBox->setStyleSheet(spinBoxStyle);

    videoFormLayout->addRow(tr("Resolution:"), m_resolutionCombo);
    videoFormLayout->addRow(tr("Framerate:"), m_fpsCombo);
    videoFormLayout->addRow(tr("Vertical Scale:"), m_scaleSpinBox);

    settingsLayout->addWidget(m_videoSettingsGroup);

    // --- Group 1.B: Audio Settings ---
    m_audioSettingsGroup = new QGroupBox(tr("Audio Settings"));
    m_audioSettingsGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *audioFormLayout = new QFormLayout(m_audioSettingsGroup);
    audioFormLayout->setContentsMargins(12, 20, 12, 12);
    audioFormLayout->setSpacing(8);
    audioFormLayout->setLabelAlignment(Qt::AlignLeft);

    m_audioFormatCombo = new QComboBox;
    m_audioFormatCombo->addItems({"WAV", "MP3", "OGG"});
    m_audioFormatCombo->setStyleSheet(comboBoxStyle);
    m_audioBitrateSpin = new QSpinBox;
    m_audioBitrateSpin->setRange(64, 320);
    m_audioBitrateSpin->setValue(192);
    m_audioBitrateSpin->setSuffix(tr(" kbps"));
    m_audioBitrateSpin->setStyleSheet(spinBoxStyle);
    
    QLabel *formatLabel = new QLabel(tr("Format:"));
    formatLabel->setStyleSheet(labelStyle);
    QLabel *bitrateLabel = new QLabel(tr("Bitrate:"));
    bitrateLabel->setStyleSheet(labelStyle);
    audioFormLayout->addRow(formatLabel, m_audioFormatCombo);
    audioFormLayout->addRow(bitrateLabel, m_audioBitrateSpin);
    
    settingsLayout->addWidget(m_audioSettingsGroup);

    // --- Group 2: Background Settings ---
    m_bgGroup = new QGroupBox(tr("Background Settings"));
    m_bgGroup->setStyleSheet(groupBoxStyle);
    QGridLayout *bgLayout = new QGridLayout(m_bgGroup);
    bgLayout->setContentsMargins(12, 20, 12, 12);
    bgLayout->setSpacing(8); 
    
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
    m_renderGroup->setStyleSheet(groupBoxStyle);
    QVBoxLayout *renderLayout = new QVBoxLayout(m_renderGroup);
    renderLayout->setContentsMargins(12, 20, 12, 12);
    renderLayout->setSpacing(6); 
    
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
    m_particleSettingsGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *particleForm = new QFormLayout(m_particleSettingsGroup);
    particleForm->setContentsMargins(12, 20, 12, 12);
    particleForm->setSpacing(8);
    particleForm->setLabelAlignment(Qt::AlignLeft);
    
    m_particleTypeCombo = new QComboBox;
    m_particleTypeCombo->addItems({tr("Default (Sparkle)"), tr("Circle"), tr("Custom Image")});
    m_particleTypeCombo->setStyleSheet(comboBoxStyle);
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
    m_lightningGroup->setStyleSheet(groupBoxStyle);
    
    QFormLayout *lightningForm = new QFormLayout;
    lightningForm->setContentsMargins(12, 20, 12, 12);
    lightningForm->setSpacing(8);
    lightningForm->setLabelAlignment(Qt::AlignLeft);
    
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
    m_lightningThicknessSpin->setStyleSheet(spinBoxStyle);
    lightningForm->addRow(tr("Base Thickness:"), m_lightningThicknessSpin);

    m_lightningLinesSpin = new QSpinBox; 
    m_lightningLinesSpin->setRange(1, 10);
    m_lightningLinesSpin->setValue(3);
    m_lightningLinesSpin->setStyleSheet(spinBoxStyle);
    lightningForm->addRow(tr("Number of Lines:"), m_lightningLinesSpin);

    m_lightningJitterYSpin = new QDoubleSpinBox;
    m_lightningJitterYSpin->setRange(0.0, 20.0);
    m_lightningJitterYSpin->setValue(3.0);
    m_lightningJitterYSpin->setSuffix(" px");
    m_lightningJitterYSpin->setSingleStep(0.5);
    m_lightningJitterYSpin->setStyleSheet(spinBoxStyle);
    lightningForm->addRow(tr("Vertical Jitter:"), m_lightningJitterYSpin);

    m_lightningJitterXSpin = new QDoubleSpinBox;
    m_lightningJitterXSpin->setRange(0.0, 20.0);
    m_lightningJitterXSpin->setValue(2.0);
    m_lightningJitterXSpin->setSuffix(" px");
    m_lightningJitterXSpin->setSingleStep(0.5);
    m_lightningJitterXSpin->setStyleSheet(spinBoxStyle);
    lightningForm->addRow(tr("Horizontal Jitter:"), m_lightningJitterXSpin);

    QVBoxLayout *lightningVLayout = new QVBoxLayout(m_lightningGroup);
    lightningVLayout->setContentsMargins(0, 0, 0, 0);
    lightningVLayout->addLayout(lightningForm);
    
    settingsLayout->addWidget(m_lightningGroup);

    // --- Export button and progress in settings ---
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_exportButton = new QPushButton(tr("Export..."));
    m_exportButton->setIcon(QIcon(":/icons/video.svg")); 
    m_exportButton->setFixedHeight(40);
    m_exportButton->setMinimumWidth(280);
    m_exportButton->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a7bd5, stop:1 #2868b8);
            color: #fff;
            border: 1px solid #2868b8;
            border-radius: 6px;
            font-weight: bold;
            font-size: 13px;
        }
        QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a8be5, stop:1 #3878c8); }
        QPushButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a6bc5, stop:1 #1858a8); }
    )");
    buttonLayout->addStretch(1); 
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch(1);
    settingsLayout->addLayout(buttonLayout);

    m_progressWidget = new QWidget;
    QFormLayout* progressFormLayout = new QFormLayout(m_progressWidget);
    progressFormLayout->setContentsMargins(0, 10, 0, 0);
    m_audioProgressBar = new QProgressBar;
    m_videoProgressBar = new QProgressBar;
    m_audioProgressLabel = new QLabel(tr("Audio Rendering:"));
    m_videoProgressLabel = new QLabel(tr("Video Rendering:"));
    progressFormLayout->addRow(m_audioProgressLabel, m_audioProgressBar);
    progressFormLayout->addRow(m_videoProgressLabel, m_videoProgressBar);
    settingsLayout->addWidget(m_progressWidget);
    
    m_statusLabel = new QLabel;
    m_statusLabel->setAlignment(Qt::AlignCenter); 
    settingsLayout->addWidget(m_statusLabel);
    settingsLayout->addStretch(1); 

    m_settingsScrollArea->setWidget(m_settingsWidget);

    auto *settingsDock = new AdvancedDockWidget(
        tr("Export Settings"), 
        QIcon(":/icons/settings.svg"),
        nullptr, 
        this
    );
    settingsDock->setWidget(m_settingsScrollArea);
    settingsDock->setObjectName("settings");
    settingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    settingsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, settingsDock);
    m_docks["settings"] = settingsDock;

    // === Configure dock layout ===
    splitDockWidget(previewDock, settingsDock, Qt::Horizontal);
    
    // Set horizontal ratio: preview:settings = 70:30 (preview larger)
    // Use QTimer to ensure layout is computed before resizing
    QTimer::singleShot(0, this, [this]() {
        QList<QDockWidget*> horizDocks = {m_docks["preview"], m_docks["settings"]};
        QList<int> horizSizes = {1000, 350};
        resizeDocks(horizDocks, horizSizes, Qt::Horizontal);
    });

    m_progressWidget->setVisible(false);
    
    // Initial state
    m_lightningColorPreview->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_lightningColor.name()));
    m_lightningGroup->setEnabled(false);
    updateBgLabels();
    
    connectWidgetSignals();
}

void MediaExportSection::connectWidgetSignals()
{
    // Playback
    connect(m_playPauseButton, &QPushButton::clicked, this, &MediaExportSection::onPlayPauseClicked);
    connect(m_progressBar, &MidiSequenceProgressBar::positionPressed, this, &MediaExportSection::seek);
    connect(m_progressBar, &MidiSequenceProgressBar::positionDragged, this, &MediaExportSection::seek);
    connect(m_progressBar, &MidiSequenceProgressBar::positionReleased, this, &MediaExportSection::seek);
    connect(m_exportButton, &QPushButton::clicked, this, &MediaExportSection::onExportClicked);
    connect(m_exportTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaExportSection::onExportTypeChanged);
    
    // Settings
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaExportSection::updatePreviewSettings);
    connect(m_scaleSpinBox, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    
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
    connect(m_bgColorButton, &QPushButton::clicked, this, &MediaExportSection::onSelectBgColor);
    connect(m_bgImageButton, &QPushButton::clicked, this, &MediaExportSection::onSelectBgImage);
    connect(m_bgClearButton, &QPushButton::clicked, this, &MediaExportSection::onClearBg);
    connect(m_bgShakeCheck, &QCheckBox::toggled, m_bgShakeSpin, &QWidget::setEnabled);
    connect(m_bgShakeCheck, &QCheckBox::checkStateChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_bgShakeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);

    // Render
    connect(m_renderNotesCheck, &QCheckBox::checkStateChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_renderKeyboardCheck, &QCheckBox::checkStateChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_renderParticlesCheck, &QCheckBox::checkStateChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_renderParticlesCheck, &QCheckBox::toggled, m_particleSettingsGroup, &QWidget::setEnabled);
    connect(m_pianoGlowCheck, &QCheckBox::checkStateChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_noteStartOpacitySpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_noteEndOpacitySpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);

    // Particles
    connect(m_particleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaExportSection::onParticleTypeChanged);
    connect(m_particleFileButton, &QPushButton::clicked, this, &MediaExportSection::onSelectParticleFile);
    connect(m_particleCountSpin, &QSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_particleLifetimeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_particleSpeedSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_particleGravitySpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_particleStartSizeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_particleEndSizeSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_particleTintCheck, &QCheckBox::checkStateChanged, this, &MediaExportSection::updatePreviewSettings);

    // Lightning
    connect(m_lightningEnableCheck, &QCheckBox::toggled, m_lightningGroup, &QWidget::setEnabled);
    connect(m_lightningEnableCheck, &QCheckBox::checkStateChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_lightningColorButton, &QPushButton::clicked, this, &MediaExportSection::onSelectLightningColor);
    connect(m_lightningThicknessSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_lightningLinesSpin, &QSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_lightningJitterYSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);
    connect(m_lightningJitterXSpin, &QDoubleSpinBox::valueChanged, this, &MediaExportSection::updatePreviewSettings);

    // Initial state
    onParticleTypeChanged(m_particleTypeCombo->currentIndex());
    onExportTypeChanged(m_exportTypeCombo->currentIndex());
}

void MediaExportSection::connectEngineSignals()
{
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::currentTickChanged, 
            this, &MediaExportSection::onPlaybackTickChanged);
    
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::playingStateChanged, 
            this, [this](bool playing){
                m_playPauseButton->setChecked(playing);
                m_playPauseButton->setToolTip(playing ? tr("Pause") : tr("Play"));
                m_playPauseButton->setIcon(playing ? QIcon(":/icons/stop.svg") : QIcon(":/icons/play.svg"));
            });
    
    // Listen for sequence changes
    connect(m_engine->getRuntimeData(), &NoteNagaRuntimeData::activeSequenceChanged,
            this, [this](NoteNagaMidiSeq*) {
                refreshSequence();
            });
}

void MediaExportSection::refreshSequence()
{
    m_sequence = m_engine->getRuntimeData()->getActiveSequence();
    
    if (!m_sequence) {
        // Show placeholder, hide docks
        for (auto dock : m_docks) {
            dock->hide();
        }
        m_noSequenceLabel->setGeometry(rect());
        m_noSequenceLabel->show();
        m_noSequenceLabel->raise();
        cleanupPreviewWorker();
        return;
    }
    
    // Hide placeholder, show docks
    m_noSequenceLabel->hide();
    for (auto dock : m_docks) {
        dock->show();
    }
    
    m_totalDuration = nn_ticks_to_seconds(m_sequence->getMaxTick(), m_sequence->getPPQ(), m_sequence->getTempo());
    m_progressBar->setMidiSequence(m_sequence);
    m_progressBar->updateMaxTime();
    
    // Only start preview worker if section is currently active
    if (m_sectionActive) {
        initPreviewWorker();
        QTimer::singleShot(10, this, &MediaExportSection::updatePreviewSettings);
    }
    
    onPlaybackTickChanged(m_engine->getRuntimeData()->getCurrentTick());
}

void MediaExportSection::initPreviewWorker()
{
    if (!m_sequence) return;
    
    cleanupPreviewWorker();
    
    m_previewThread = new QThread(this);
    m_previewWorker = new PreviewWorker(m_sequence);
    m_previewWorker->moveToThread(m_previewThread);

    connect(this, &MediaExportSection::destroyed, m_previewWorker, &PreviewWorker::deleteLater);
    connect(m_previewThread, &QThread::started, m_previewWorker, &PreviewWorker::init);
    connect(m_previewWorker, &PreviewWorker::frameReady, this, &MediaExportSection::onPreviewFrameReady, Qt::QueuedConnection);
    
    // Initialize frame stats tracking
    m_frameTimer.start();
    m_frameCount = 0;
    m_lastFpsUpdate = QDateTime::currentMSecsSinceEpoch();
    
    m_previewThread->start();
}

void MediaExportSection::cleanupPreviewWorker()
{
    if (m_previewThread) {
        m_previewThread->quit();
        m_previewThread->wait();
        m_previewThread = nullptr;
        m_previewWorker = nullptr;
    }
}

void MediaExportSection::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // Update placeholder geometry
    if (m_noSequenceLabel && m_noSequenceLabel->isVisible()) {
        m_noSequenceLabel->setGeometry(rect());
    }
    updatePreviewRenderSize();
}

void MediaExportSection::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    refreshSequence();
    
    // Force dock widgets to update their geometry after switching sections
    // This fixes the preview being small until manual resize
    // Use multiple delayed updates to ensure layout is fully computed
    for (int delay : {0, 50, 150}) {
        QTimer::singleShot(delay, this, [this]() {
            // Force layout recalculation
            if (auto* previewDock = m_docks.value("preview")) {
                if (previewDock->isVisible() && previewDock->widget()) {
                    // Force the dock widget to resize properly
                    previewDock->widget()->adjustSize();
                    
                    // Force preview stack to fill available space
                    if (m_previewStack) {
                        m_previewStack->updateGeometry();
                    }
                    
                    // Force preview label resize
                    if (m_previewLabel) {
                        m_previewLabel->updateGeometry();
                    }
                }
            }
            
            // Update preview render size with current label dimensions
            updatePreviewRenderSize();
        });
    }
}

void MediaExportSection::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    // Optionally pause preview when hidden
}

bool MediaExportSection::eventFilter(QObject *watched, QEvent *event)
{
    // Catch resize events on preview label to update render size when dock is resized
    if (watched == m_previewLabel && event->type() == QEvent::Resize) {
        updatePreviewRenderSize();
    }
    return QMainWindow::eventFilter(watched, event);
}

QSize MediaExportSection::getTargetResolution()
{
    return (m_resolutionCombo->currentIndex() == 0) ? QSize(1280, 720) : QSize(1920, 1080);
}

void MediaExportSection::updatePreviewRenderSize()
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

MediaRenderer::RenderSettings MediaExportSection::getCurrentRenderSettings()
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

void MediaExportSection::updatePreviewSettings()
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

void MediaExportSection::onPlayPauseClicked()
{
    if (m_engine->isPlaying()) {
        m_engine->stopPlayback();
    } else {
        m_engine->startPlayback();
    }
}

void MediaExportSection::seek(float seconds)
{
    if (m_engine->isPlaying()) {
        m_engine->stopPlayback();
    }
    m_currentTime = (double)seconds;
    int tick = nn_seconds_to_ticks(m_currentTime, m_sequence->getPPQ(), m_sequence->getTempo());
    m_engine->setPlaybackPosition(tick);
    onPlaybackTickChanged(tick);
}

void MediaExportSection::onPlaybackTickChanged(int tick)
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

void MediaExportSection::onPreviewFrameReady(const QImage& frame)
{
    if (m_exportTypeCombo->currentIndex() != 0) {
        return;
    }
    
    // Calculate frame render time
    qint64 frameTimeMs = m_frameTimer.elapsed();
    m_frameTimer.restart();
    m_frameCount++;
    
    // Update FPS counter every 500ms
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastFpsUpdate >= 500) {
        double elapsedSec = (now - m_lastFpsUpdate) / 1000.0;
        double fps = m_frameCount / elapsedSec;
        
        QString statsText = QString("FPS: %1 | Frame: %2 ms | Resolution: %3x%4")
            .arg(fps, 0, 'f', 1)
            .arg(frameTimeMs)
            .arg(frame.width())
            .arg(frame.height());
        m_previewStatsLabel->setText(statsText);
        
        m_frameCount = 0;
        m_lastFpsUpdate = now;
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

void MediaExportSection::onExportTypeChanged(int index)
{
    bool isVideo = (index == 0);
    
    // Switch between video preview and audio visualizer
    m_previewStack->setCurrentIndex(isVideo ? 0 : 1);
    
    // Start/stop audio visualizer animation
    if (isVideo) {
        m_audioBarsVisualizer->stop();
    } else {
        m_audioBarsVisualizer->start();
    }
    
    // Show/hide preview stats (only for video mode)
    m_previewStatsLabel->setVisible(isVideo);
    
    m_exportButton->setText(isVideo ? tr("Export to MP4") : tr("Export Audio..."));
    m_exportButton->setIcon(isVideo ? QIcon(":/icons/video.svg") : QIcon(":/icons/audio-signal.svg"));

    m_videoSettingsGroup->setVisible(isVideo);
    m_audioSettingsGroup->setVisible(true);  // Audio settings always visible
    m_bgGroup->setVisible(isVideo);
    m_renderGroup->setVisible(isVideo);
    m_particleSettingsGroup->setVisible(isVideo);
    m_lightningGroup->setVisible(isVideo);

    if (!m_engine->isPlaying() && isVideo) {
        updatePreviewSettings();
    }
}

void MediaExportSection::onParticleTypeChanged(int index)
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

void MediaExportSection::onSelectParticleFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Particle Image"), "", tr("Images (*.png *.jpg *.bmp)"));
    if (!path.isEmpty()) {
        m_particleFilePath = path;
        QPixmap pixmap(path);
        m_particlePreviewLabel->setPixmap(pixmap.scaled(m_particlePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        updatePreviewSettings();
    }
}

void MediaExportSection::onSelectBgColor()
{
    QColor color = QColorDialog::getColor(m_backgroundColor, this, tr("Select Background Color"));
    if (color.isValid()) {
        m_backgroundColor = color;
        m_backgroundImagePath.clear();
        updateBgLabels();
        updatePreviewSettings();
    }
}

void MediaExportSection::onSelectBgImage()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Background Image"), "", tr("Images (*.png *.jpg *.bmp)"));
    if (!path.isEmpty()) {
        m_backgroundImagePath = path;
        updateBgLabels();
        updatePreviewSettings();
    }
}

void MediaExportSection::onClearBg()
{
    m_backgroundImagePath.clear();
    m_backgroundColor = QColor(25, 25, 35);
    updateBgLabels();
    updatePreviewSettings();
}

void MediaExportSection::updateBgLabels()
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

void MediaExportSection::onSelectLightningColor()
{
    QColor color = QColorDialog::getColor(m_lightningColor, this, tr("Select Lightning Color"));
    if (color.isValid()) {
        m_lightningColor = color;
        m_lightningColorPreview->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_lightningColor.name()));
        updatePreviewSettings();
    }
}

void MediaExportSection::onExportClicked()
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
    connect(m_exporter, &MediaExporter::finished, this, &MediaExportSection::onExportFinished);
    connect(m_exporter, &MediaExporter::error, this, [this](const QString &msg) {
        QMessageBox::critical(this, tr("Error"), msg);
        onExportFinished();
    });

    connect(m_exporter, &MediaExporter::audioProgressUpdated, this, &MediaExportSection::updateAudioProgress);
    connect(m_exporter, &MediaExporter::videoProgressUpdated, this, &MediaExportSection::updateVideoProgress);
    connect(m_exporter, &MediaExporter::statusTextChanged, this, &MediaExportSection::updateStatusText);

    connect(m_exporter, &MediaExporter::finished, m_exportThread, &QThread::quit);
    connect(m_exporter, &MediaExporter::finished, m_exporter, &MediaExporter::deleteLater);
    connect(m_exportThread, &QThread::finished, m_exportThread, &QThread::deleteLater);

    m_exportThread->start();
}

void MediaExportSection::updateAudioProgress(int percentage)
{
    m_audioProgressBar->setValue(percentage);
}

void MediaExportSection::updateVideoProgress(int percentage)
{
    m_videoProgressBar->setValue(percentage);
}

void MediaExportSection::updateStatusText(const QString &status)
{
    m_statusLabel->setText(status);
}

void MediaExportSection::onExportFinished()
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

void MediaExportSection::setControlsEnabled(bool enabled)
{
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
