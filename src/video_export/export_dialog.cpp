#include "export_dialog.h"

#include "video_renderer.h"
#include "video_exporter.h"
#include <QtWidgets>
#include <QScrollArea> 
#include <QColorDialog>

#include "../gui/components/midi_seq_progress_bar.h" 

ExportDialog::ExportDialog(NoteNagaMidiSeq* sequence, NoteNagaEngine* engine, QWidget* parent)
    : QDialog(parent), m_engine(engine), m_sequence(sequence),
      m_renderer(new VideoRenderer(sequence)),
      m_backgroundColor(QColor(25, 25, 35)),
      m_currentTime(0.0), m_exportThread(nullptr), m_exporter(nullptr)
{
    setupUi();
    connectEngineSignals();

    m_totalDuration = nn_ticks_to_seconds(m_sequence->getMaxTick(), m_sequence->getPPQ(), m_sequence->getTempo());
    
    m_progressBar->setMidiSequence(m_sequence);
    m_progressBar->updateMaxTime(); 

    // --- Connect Signals ---
    connect(m_playPauseButton, &QPushButton::clicked, this, &ExportDialog::onPlayPauseClicked);
    
    connect(m_progressBar, &MidiSequenceProgressBar::positionPressed, this, &ExportDialog::seek);
    connect(m_progressBar, &MidiSequenceProgressBar::positionDragged, this, &ExportDialog::seek);
    connect(m_progressBar, &MidiSequenceProgressBar::positionReleased, this, &ExportDialog::seek);
    
    connect(m_exportButton, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    
    // Settings
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::updatePreviewSettings);
    connect(m_scaleSpinBox, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);

    // Background
    connect(m_bgColorButton, &QPushButton::clicked, this, &ExportDialog::onSelectBgColor);
    connect(m_bgImageButton, &QPushButton::clicked, this, &ExportDialog::onSelectBgImage);
    connect(m_bgClearButton, &QPushButton::clicked, this, &ExportDialog::onClearBg);

    // Render
    connect(m_renderNotesCheck, &QCheckBox::checkStateChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_renderKeyboardCheck, &QCheckBox::checkStateChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_renderParticlesCheck, &QCheckBox::checkStateChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_renderParticlesCheck, &QCheckBox::toggled, m_particleSettingsGroup, &QWidget::setEnabled);
    connect(m_pianoGlowCheck, &QCheckBox::checkStateChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_noteStartOpacitySpin, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_noteEndOpacitySpin, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);

    // Particles
    connect(m_particleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::onParticleTypeChanged);
    connect(m_particleFileButton, &QPushButton::clicked, this, &ExportDialog::onSelectParticleFile);
    connect(m_particleCountSpin, &QSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_particleLifetimeSpin, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_particleSpeedSpin, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_particleGravitySpin, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_particleStartSizeSpin, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_particleEndSizeSpin, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updatePreviewSettings);
    connect(m_particleTintCheck, &QCheckBox::checkStateChanged, this, &ExportDialog::updatePreviewSettings);

    // --- Initial State ---
    onParticleTypeChanged(m_particleTypeCombo->currentIndex());
    updateBgLabels();
    updatePreviewSettings(); 
    onPlaybackTickChanged(m_engine->getProject()->getCurrentTick());
}

ExportDialog::~ExportDialog()
{
    delete m_renderer;
    if (m_exportThread && m_exportThread->isRunning())
    {
        m_exportThread->quit();
        m_exportThread->wait();
    }
}

QSize ExportDialog::getTargetResolution()
{
    return (m_resolutionCombo->currentIndex() == 0) ? QSize(1280, 720) : QSize(1920, 1080);
}


void ExportDialog::setupUi()
{
    setWindowTitle(tr("Export Video"));
    setMinimumSize(900, 700); 

    // Hlavní layout je nyní horizontální a drží splitter
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(m_mainSplitter);

    // --- Levá strana (Náhled) ---
    m_leftWidget = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftWidget);
    leftLayout->setContentsMargins(0,0,0,0);

    m_previewGroup = new QGroupBox(tr("Preview"));
    QVBoxLayout* previewLayout = new QVBoxLayout;
    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: black; border: 1px solid #444;");
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    previewLayout->addWidget(m_previewLabel, 1); // 1 = roztahovat vertikálně
    
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
    timelineLayout->addWidget(m_progressBar, 1); // 1 = roztahovat horizontálně

    previewLayout->addLayout(timelineLayout); 
    m_previewGroup->setLayout(previewLayout);
    leftLayout->addWidget(m_previewGroup);
    
    m_mainSplitter->addWidget(m_leftWidget);

    // --- Pravá strana (Nastavení a Export) ---
    m_rightWidget = new QWidget;
    QGridLayout *rightLayout = new QGridLayout(m_rightWidget);
    rightLayout->setContentsMargins(0,0,0,0);

    // --- ScrollArea pro nastavení ---
    m_settingsScrollArea = new QScrollArea;
    m_settingsScrollArea->setWidgetResizable(true);
    m_settingsScrollArea->setFrameShape(QFrame::NoFrame);
    m_settingsScrollArea->setMinimumWidth(360); 
    
    m_settingsWidget = new QWidget; 
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsWidget);
    settingsLayout->setContentsMargins(5, 5, 5, 5); 
    
    // --- Skupina 1: Export Settings ---
    QGroupBox *exportGroup = new QGroupBox(tr("Export Settings"));
    QFormLayout *exportFormLayout = new QFormLayout(exportGroup);
    
    m_resolutionCombo = new QComboBox;
    m_resolutionCombo->addItems({"1280x720 (720p)", "1920x1080 (1080p)"});
    m_fpsCombo = new QComboBox;
    m_fpsCombo->addItems({"30 FPS", "60 FPS"});
    m_scaleSpinBox = new QDoubleSpinBox;
    m_scaleSpinBox->setRange(1.0, 15.0);
    m_scaleSpinBox->setValue(5.0);
    m_scaleSpinBox->setSuffix(tr(" s"));
    m_scaleSpinBox->setToolTip(tr("How many seconds of notes are visible on screen at once."));

    exportFormLayout->addRow(tr("Resolution:"), m_resolutionCombo);
    exportFormLayout->addRow(tr("Framerate:"), m_fpsCombo);
    exportFormLayout->addRow(tr("Vertical Scale:"), m_scaleSpinBox);

    settingsLayout->addWidget(exportGroup);

    // --- Skupina 2: Background Settings ---
    QGroupBox *bgGroup = new QGroupBox(tr("Background Settings"));
    QGridLayout *bgLayout = new QGridLayout(bgGroup);
    
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
    
    settingsLayout->addWidget(bgGroup);


    // --- Skupina 3: Render Settings ---
    QGroupBox *renderGroup = new QGroupBox(tr("Render Settings"));
    QVBoxLayout *renderLayout = new QVBoxLayout(renderGroup);
    
    m_renderNotesCheck = new QCheckBox(tr("Render falling notes"));
    m_renderNotesCheck->setChecked(true);
    m_renderKeyboardCheck = new QCheckBox(tr("Render piano keyboard"));
    m_renderKeyboardCheck->setChecked(true);
    m_renderParticlesCheck = new QCheckBox(tr("Render particles"));
    m_renderParticlesCheck->setChecked(true);
    m_pianoGlowCheck = new QCheckBox(tr("Render piano glow effect"));
    m_pianoGlowCheck->setChecked(true);
    
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

    settingsLayout->addWidget(renderGroup); 

    // --- Skupina 4: Particle Settings (přesunuta sem) ---
    m_particleSettingsGroup = new QGroupBox(tr("Particle Settings"));
    QFormLayout *particleForm = new QFormLayout(m_particleSettingsGroup);
    
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
    settingsLayout->addStretch(1); 

    m_settingsScrollArea->setWidget(m_settingsWidget); 
    rightLayout->addWidget(m_settingsScrollArea, 0, 0); // (řádek 0, sloupec 0)

    // --- Sekce pro export a progress (vpravo dole) ---
    QVBoxLayout* exportLayout = new QVBoxLayout;
    exportLayout->setContentsMargins(10, 10, 10, 10);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    m_exportButton = new QPushButton(tr("Export to MP4"));
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

    rightLayout->addLayout(exportLayout, 1, 0); // (řádek 1, sloupec 0)

    rightLayout->setRowStretch(0, 1); // ScrollArea se roztahuje
    rightLayout->setRowStretch(1, 0); // Export sekce se neroztahuje

    m_mainSplitter->addWidget(m_rightWidget);
    m_mainSplitter->setSizes({600, 300}); // Počáteční rozdělení

    m_progressWidget->setVisible(false);
}

void ExportDialog::connectEngineSignals() {
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::currentTickChanged, this, &ExportDialog::onPlaybackTickChanged);
    
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::playingStateChanged, this, [this](bool playing){
        m_playPauseButton->setChecked(playing);
        m_playPauseButton->setToolTip(playing ? tr("Pause") : tr("Play"));
        m_playPauseButton->setIcon(playing ? QIcon(":/icons/stop.svg") : QIcon(":/icons/play.svg"));
    });
}

void ExportDialog::onPlayPauseClicked()
{
    if (m_engine->isPlaying()) {
        m_engine->stopPlayback();
    } else {
        m_engine->startPlayback();
    }
}

void ExportDialog::seek(float seconds) {
    if (m_engine->isPlaying()) {
        m_engine->stopPlayback();
    }
    m_currentTime = (double)seconds;
    int tick = nn_seconds_to_ticks(m_currentTime, m_sequence->getPPQ(), m_sequence->getTempo());
    m_engine->setPlaybackPosition(tick);
    onPlaybackTickChanged(tick); 
}

VideoRenderer::RenderSettings ExportDialog::getCurrentRenderSettings()
{
    VideoRenderer::RenderSettings settings;

    // Background
    settings.backgroundColor = m_backgroundColor;
    if (!m_backgroundImagePath.isEmpty()) {
        settings.backgroundImage = QImage(m_backgroundImagePath);
    }

    // Render
    settings.renderNotes = m_renderNotesCheck->isChecked();
    settings.renderKeyboard = m_renderKeyboardCheck->isChecked();
    settings.renderParticles = m_renderParticlesCheck->isChecked();
    settings.renderPianoGlow = m_pianoGlowCheck->isChecked();
    settings.noteStartOpacity = m_noteStartOpacitySpin->value();
    settings.noteEndOpacity = m_noteEndOpacitySpin->value();

    // Particles
    settings.particleType = (VideoRenderer::RenderSettings::ParticleType)m_particleTypeCombo->currentIndex();
    if (settings.particleType == VideoRenderer::RenderSettings::Custom && !m_particleFilePath.isEmpty()) {
        settings.customParticleImage = QImage(m_particleFilePath);
    }
    settings.particleCount = m_particleCountSpin->value();
    settings.particleLifetime = m_particleLifetimeSpin->value();
    settings.particleSpeed = m_particleSpeedSpin->value();
    settings.particleGravity = m_particleGravitySpin->value();
    settings.tintParticles = m_particleTintCheck->isChecked();
    settings.particleStartSize = m_particleStartSizeSpin->value();
    settings.particleEndSize = m_particleEndSizeSpin->value();

    return settings;
}

void ExportDialog::updatePreviewSettings()
{
    m_renderer->setSecondsVisible(m_scaleSpinBox->value());
    m_renderer->setRenderSettings(getCurrentRenderSettings());
    
    if (!m_engine->isPlaying()) {
        onPlaybackTickChanged(m_engine->getProject()->getCurrentTick());
    }
}

void ExportDialog::onParticleTypeChanged(int index)
{
    bool isCustom = (index == (int)VideoRenderer::RenderSettings::Custom);
    m_particleFileButton->setVisible(isCustom);
    m_particlePreviewLabel->setVisible(isCustom);

    bool isPixmap = (index == (int)VideoRenderer::RenderSettings::Resource || isCustom);
    m_particleTintCheck->setVisible(isPixmap);
    m_particleStartSizeSpin->setVisible(isPixmap);
    m_particleEndSizeSpin->setVisible(isPixmap);

    if (isCustom && !m_particleFilePath.isEmpty()) {
        QPixmap pixmap(m_particleFilePath);
        m_particlePreviewLabel->setPixmap(pixmap.scaled(m_particlePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (index == (int)VideoRenderer::RenderSettings::Resource) {
        QPixmap pixmap(":/images/sparkle.png"); 
        m_particlePreviewLabel->setPixmap(pixmap.scaled(m_particlePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_particlePreviewLabel->setVisible(true); 
    } else {
        m_particlePreviewLabel->clear(); 
        m_particlePreviewLabel->setVisible(false); 
    }

    updatePreviewSettings();
}

void ExportDialog::onSelectParticleFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Particle Image"), "", tr("Images (*.png *.jpg *.bmp)"));
    if (!path.isEmpty()) {
        m_particleFilePath = path;
        QPixmap pixmap(path);
        m_particlePreviewLabel->setPixmap(pixmap.scaled(m_particlePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        
        updatePreviewSettings();
    }
}

void ExportDialog::onSelectBgColor()
{
    QColor color = QColorDialog::getColor(m_backgroundColor, this, tr("Select Background Color"));
    if (color.isValid()) {
        m_backgroundColor = color;
        m_backgroundImagePath.clear();
        updateBgLabels();
        updatePreviewSettings();
    }
}

void ExportDialog::onSelectBgImage()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Background Image"), "", tr("Images (*.png *.jpg *.bmp)"));
    if (!path.isEmpty()) {
        m_backgroundImagePath = path;
        updateBgLabels();
        updatePreviewSettings();
    }
}

void ExportDialog::onClearBg()
{
    m_backgroundImagePath.clear();
    m_backgroundColor = QColor(25, 25, 35); // Výchozí tmavá barva
    updateBgLabels();
    updatePreviewSettings();
}

void ExportDialog::updateBgLabels()
{
    m_bgColorPreview->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_backgroundColor.name()));
    
    if (!m_backgroundImagePath.isEmpty()) {
        QFileInfo info(m_backgroundImagePath);
        m_bgImagePreview->setText(info.fileName());
        m_bgImagePreview->setStyleSheet("color: #DDD;"); // Světlejší text pro soubor
    } else {
        m_bgImagePreview->setText(tr("None"));
        m_bgImagePreview->setStyleSheet("color: #888;"); // Tmavší text pro "None"
    }
}


void ExportDialog::onExportClicked()
{
    QString outputPath = QFileDialog::getSaveFileName(this, tr("Save Video"), "", tr("MPEG-4 Video (*.mp4)"));
    if (outputPath.isEmpty())
        return;

    QSize resolution = getTargetResolution();
    int fps = (m_fpsCombo->currentIndex() == 0) ? 30 : 60;
    double secondsVisible = m_scaleSpinBox->value();

    VideoRenderer::RenderSettings settings = getCurrentRenderSettings();

    setControlsEnabled(false);

    m_exportThread = new QThread;
    m_exporter = new VideoExporter(m_sequence, outputPath, resolution, fps, this->m_engine, secondsVisible, settings);
    
    m_exporter->moveToThread(m_exportThread);

    connect(m_exportThread, &QThread::started, m_exporter, &VideoExporter::doExport);
    connect(m_exporter, &VideoExporter::finished, this, &ExportDialog::onExportFinished);
    connect(m_exporter, &VideoExporter::error, this, [this](const QString &msg) {
        QMessageBox::critical(this, tr("Error"), msg);
        onExportFinished();
    });

    connect(m_exporter, &VideoExporter::audioProgressUpdated, this, &ExportDialog::updateAudioProgress);
    connect(m_exporter, &VideoExporter::videoProgressUpdated, this, &ExportDialog::updateVideoProgress);
    connect(m_exporter, &VideoExporter::statusTextChanged, this, &ExportDialog::updateStatusText);

    connect(m_exporter, &VideoExporter::finished, m_exportThread, &QThread::quit);
    connect(m_exporter, &VideoExporter::finished, m_exporter, &VideoExporter::deleteLater);
    connect(m_exportThread, &QThread::finished, m_exportThread, &QThread::deleteLater);

    m_exportThread->start();
}

void ExportDialog::updateAudioProgress(int percentage)
{
    m_audioProgressBar->setValue(percentage);
}

void ExportDialog::updateVideoProgress(int percentage)
{
    m_videoProgressBar->setValue(percentage);
}

void ExportDialog::updateStatusText(const QString &status)
{
    m_statusLabel->setText(status);
}

void ExportDialog::onExportFinished()
{
    setControlsEnabled(true);
    if (!m_statusLabel->text().contains(tr("Error"), Qt::CaseInsensitive)) {
        QMessageBox::information(this, tr("Success"), tr("Video export finished successfully."));
    }
    m_exportThread = nullptr;
    m_exporter = nullptr;
}

void ExportDialog::setControlsEnabled(bool enabled)
{
    m_previewGroup->setEnabled(enabled);
    m_settingsScrollArea->setEnabled(enabled); 
    m_exportButton->setEnabled(enabled);
    m_progressWidget->setVisible(!enabled);

    if (enabled)
    {
        m_audioProgressBar->setValue(0);
        m_audioProgressBar->setMaximum(100);
        m_videoProgressBar->setValue(0);
        m_videoProgressBar->setMaximum(100);
        m_statusLabel->clear();
    }
}

void ExportDialog::onPlaybackTickChanged(int tick) {
    m_currentTime = nn_ticks_to_seconds(tick, m_sequence->getPPQ(), m_sequence->getTempo());

    // --- Logika pro správný poměr stran náhledu ---
    QSize targetRes = getTargetResolution();
    QSize labelSize = m_previewLabel->size();

    // Vypočítáme velikost renderu, která se vejde do labelu při zachování poměru stran
    QSize renderSize = targetRes;
    renderSize.scale(labelSize, Qt::KeepAspectRatio);
    
    // Vykreslíme frame v této vypočítané velikosti
    QImage frame = m_renderer->renderFrame(m_currentTime, renderSize);
    QPixmap pixmap = QPixmap::fromImage(frame);

    // Vytvoříme finální pixmapu o velikosti labelu a vyplníme ji černě (pro letterbox)
    QPixmap scaledPixmap(labelSize);
    scaledPixmap.fill(Qt::black);

    // Vykreslíme náš vyrenderovaný frame doprostřed
    QPainter p(&scaledPixmap);
    int x = (labelSize.width() - renderSize.width()) / 2;
    int y = (labelSize.height() - renderSize.height()) / 2;
    p.drawPixmap(x, y, pixmap);
    p.end();
    
    m_previewLabel->setPixmap(scaledPixmap);

    // --- Aktualizace progress baru ---
    m_progressBar->blockSignals(true);
    m_progressBar->setCurrentTime(m_currentTime);
    m_progressBar->blockSignals(false);
}