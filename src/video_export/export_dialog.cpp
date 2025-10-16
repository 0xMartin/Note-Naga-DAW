#include "export_dialog.h"
#include "video_renderer.h"
#include "video_exporter.h"
#include <QtWidgets>

ExportDialog::ExportDialog(NoteNagaMidiSeq* sequence, NoteNagaEngine* engine, QWidget* parent)
    : QDialog(parent), m_engine(engine), m_sequence(sequence),
      m_renderer(new VideoRenderer(sequence)),
      m_currentTime(0.0), m_exportThread(nullptr), m_exporter(nullptr)
{
    setupUi();
    connectEngineSignals();

    m_totalDuration = nn_ticks_to_seconds(m_sequence->getMaxTick(), m_sequence->getPPQ(), m_sequence->getTempo());
    m_timeSlider->setRange(0, (int)(m_totalDuration * 100));

    connect(m_playPauseButton, &QPushButton::clicked, this, &ExportDialog::onPlayPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &ExportDialog::onStopClicked);
    connect(m_timeSlider, &QSlider::valueChanged, this, &ExportDialog::seek);
    connect(m_exportButton, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    connect(m_scaleSpinBox, &QDoubleSpinBox::valueChanged, this, [this](double value){
        m_renderer->setSecondsVisible(value);
        if (!m_engine->isPlaying()) {
            onPlaybackTickChanged(m_engine->getProject()->getCurrentTick());
        }
    });

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

void ExportDialog::setupUi()
{
    setWindowTitle(tr("Export Video"));
    setMinimumSize(800, 600);

    QGridLayout* mainLayout = new QGridLayout(this);

    // --- Skupina pro náhled (beze změny) ---
    m_previewGroup = new QGroupBox(tr("Preview"));
    QVBoxLayout* previewLayout = new QVBoxLayout;

    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: black; border: 1px solid #444;");
    m_previewLabel->setMinimumHeight(200);
    previewLayout->addWidget(m_previewLabel, 1);

    m_timeSlider = new QSlider(Qt::Horizontal);
    previewLayout->addWidget(m_timeSlider);

    QHBoxLayout *controlsLayout = new QHBoxLayout;
    m_playPauseButton = new QPushButton(tr("Play"));
    m_stopButton = new QPushButton(tr("Stop"));
    controlsLayout->addWidget(m_playPauseButton);
    controlsLayout->addWidget(m_stopButton);
    controlsLayout->addStretch(1);
    previewLayout->addLayout(controlsLayout);
    m_previewGroup->setLayout(previewLayout);

    mainLayout->addWidget(m_previewGroup, 0, 0);

    // --- Skupina pro nastavení (beze změny) ---
    m_settingsGroup = new QGroupBox(tr("Export Settings"));
    QFormLayout *formLayout = new QFormLayout;

    m_resolutionCombo = new QComboBox;
    m_resolutionCombo->addItems({"1280x720 (720p)", "1920x1080 (1080p)"});
    m_fpsCombo = new QComboBox;
    m_fpsCombo->addItems({"30 FPS", "60 FPS"});
    m_scaleSpinBox = new QDoubleSpinBox;
    m_scaleSpinBox->setRange(1.0, 15.0);
    m_scaleSpinBox->setValue(5.0);
    m_scaleSpinBox->setSuffix(tr(" s"));
    m_scaleSpinBox->setToolTip(tr("How many seconds of notes are visible on screen at once."));

    formLayout->addRow(tr("Resolution:"), m_resolutionCombo);
    formLayout->addRow(tr("Framerate:"), m_fpsCombo);
    formLayout->addRow(tr("Vertical Scale:"), m_scaleSpinBox);
    m_settingsGroup->setLayout(formLayout);

    mainLayout->addWidget(m_settingsGroup, 0, 1);

    // --- Přepracovaná sekce pro export a progress ---
    QVBoxLayout* exportLayout = new QVBoxLayout;
    m_exportButton = new QPushButton(tr("Export to MP4"));
    m_exportButton->setFixedHeight(40);
    exportLayout->addWidget(m_exportButton);

    // Widget, který seskupuje progress bary a bude se skrývat/zobrazovat
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
    m_statusLabel->setAlignment(Qt::AlignCenter);
    exportLayout->addWidget(m_statusLabel);
    exportLayout->addStretch(1);

    mainLayout->addLayout(exportLayout, 1, 0, 1, 2);

    mainLayout->setColumnStretch(0, 3);
    mainLayout->setColumnStretch(1, 1);

    m_progressWidget->setVisible(false); // Na začátku je skrytý
}

void ExportDialog::connectEngineSignals() {
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::currentTickChanged, this, &ExportDialog::onPlaybackTickChanged);
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::playingStateChanged, this, [this](bool playing){
        m_playPauseButton->setText(playing ? tr("Pause") : tr("Play"));
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

void ExportDialog::onStopClicked() {
    m_engine->stopPlayback();
    m_engine->setPlaybackPosition(0);
}

void ExportDialog::seek(int value) {
    if (m_engine->isPlaying()) {
        m_engine->stopPlayback();
    }
    m_currentTime = value / 100.0;
    int tick = nn_seconds_to_ticks(m_currentTime, m_sequence->getPPQ(), m_sequence->getTempo());
    m_engine->setPlaybackPosition(tick);
    onPlaybackTickChanged(tick);
}

void ExportDialog::onExportClicked()
{
    QString outputPath = QFileDialog::getSaveFileName(this, tr("Save Video"), "", tr("MPEG-4 Video (*.mp4)"));
    if (outputPath.isEmpty())
        return;

    QSize resolution = (m_resolutionCombo->currentIndex() == 0) ? QSize(1280, 720) : QSize(1920, 1080);
    int fps = (m_fpsCombo->currentIndex() == 0) ? 30 : 60;
    double secondsVisible = m_scaleSpinBox->value();

    setControlsEnabled(false);

    m_exportThread = new QThread;
    m_exporter = new VideoExporter(m_sequence, outputPath, resolution, fps, this->m_engine, secondsVisible);
    m_exporter->moveToThread(m_exportThread);

    // Propojení nových, specifických signálů a slotů
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
    m_settingsGroup->setEnabled(enabled);
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

    QImage frame = m_renderer->renderFrame(m_currentTime, m_previewLabel->size());
    m_previewLabel->setPixmap(QPixmap::fromImage(frame.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));

    m_timeSlider->blockSignals(true);
    m_timeSlider->setValue((int)(m_currentTime * 100));
    m_timeSlider->blockSignals(false);
}