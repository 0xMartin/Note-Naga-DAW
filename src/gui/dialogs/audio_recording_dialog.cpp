#include "audio_recording_dialog.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/audio/audio_manager.h>

#include <QPainter>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QGroupBox>
#include <QFrame>
#include <QScrollBar>
#include <QFile>
#include <QDataStream>
#include <QResizeEvent>

#include <cmath>
#include <algorithm>

/*******************************************************************************************************/
// RecordingWaveformWidget
/*******************************************************************************************************/

RecordingWaveformWidget::RecordingWaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: #1a1a20;");
}

void RecordingWaveformWidget::addSamples(const std::vector<float> &samples)
{
    if (samples.empty()) return;
    
    updatePeaks(samples);
    
    // Auto-scroll to end if enabled
    if (m_autoScroll && parentWidget()) {
        QScrollArea *scrollArea = qobject_cast<QScrollArea*>(parentWidget()->parentWidget());
        if (scrollArea) {
            QScrollBar *hBar = scrollArea->horizontalScrollBar();
            if (hBar) {
                hBar->setValue(hBar->maximum());
            }
        }
    }
    
    update();
}

void RecordingWaveformWidget::updatePeaks(const std::vector<float> &samples)
{
    for (float sample : samples) {
        m_currentPeakBuffer.push_back(sample);
        
        if (static_cast<int>(m_currentPeakBuffer.size()) >= m_samplesPerPeak) {
            float minVal = *std::min_element(m_currentPeakBuffer.begin(), m_currentPeakBuffer.end());
            float maxVal = *std::max_element(m_currentPeakBuffer.begin(), m_currentPeakBuffer.end());
            m_peakData.push_back({minVal, maxVal});
            m_currentPeakBuffer.clear();
            
            // Update widget width to accommodate new data
            int newWidth = static_cast<int>(m_peakData.size()) + 50;
            if (newWidth > width()) {
                setMinimumWidth(newWidth);
            }
        }
    }
}

void RecordingWaveformWidget::clear()
{
    m_peakData.clear();
    m_currentPeakBuffer.clear();
    setMinimumWidth(100);
    update();
}

void RecordingWaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), QColor("#1a1a20"));
    
    // Center line
    int centerY = height() / 2;
    painter.setPen(QPen(QColor("#3a3a45"), 1));
    painter.drawLine(0, centerY, width(), centerY);
    
    // Grid lines at -6dB and 0dB levels
    painter.setPen(QPen(QColor("#2a2a35"), 1, Qt::DotLine));
    int halfHeight = height() / 2 - 4;
    int sixDbY = static_cast<int>(halfHeight * 0.5f);
    painter.drawLine(0, centerY - sixDbY, width(), centerY - sixDbY);
    painter.drawLine(0, centerY + sixDbY, width(), centerY + sixDbY);
    
    if (m_peakData.empty()) {
        // Draw placeholder text
        painter.setPen(QColor("#666666"));
        painter.drawText(rect(), Qt::AlignCenter, tr("Waveform will appear here during recording"));
        return;
    }
    
    // Draw waveform
    painter.setPen(Qt::NoPen);
    
    QColor waveColor("#10b981");
    QColor waveColorLight("#34d399");
    
    int startX = 0;
    int endX = std::min(static_cast<int>(m_peakData.size()), width());
    
    for (int x = startX; x < endX; ++x) {
        if (x >= static_cast<int>(m_peakData.size())) break;
        
        float minVal = m_peakData[x].first;
        float maxVal = m_peakData[x].second;
        
        int y1 = centerY - static_cast<int>(maxVal * halfHeight);
        int y2 = centerY - static_cast<int>(minVal * halfHeight);
        
        // Gradient effect - brighter in center
        float amplitude = (maxVal - minVal) / 2.0f;
        QColor drawColor = amplitude > 0.7f ? waveColorLight : waveColor;
        
        painter.fillRect(x, y1, 1, std::max(1, y2 - y1), drawColor);
    }
    
    // Draw current position indicator
    if (!m_peakData.empty()) {
        int posX = static_cast<int>(m_peakData.size()) - 1;
        if (posX >= 0 && posX < width()) {
            painter.setPen(QPen(QColor("#f59e0b"), 2));
            painter.drawLine(posX, 0, posX, height());
        }
    }
}

void RecordingWaveformWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void RecordingWaveformWidget::updateHeight(int height)
{
    if (height > 0) {
        setFixedHeight(height);
    }
}

/*******************************************************************************************************/
// VolumeMeterWidget
/*******************************************************************************************************/

VolumeMeterWidget::VolumeMeterWidget(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent), m_orientation(orientation)
{
    if (orientation == Qt::Vertical) {
        setMinimumWidth(20);
        setMaximumWidth(30);
        setMinimumHeight(80);
    } else {
        setMinimumHeight(20);
        setMaximumHeight(30);
        setMinimumWidth(80);
    }
    
    m_peakDecayTimer = new QTimer(this);
    connect(m_peakDecayTimer, &QTimer::timeout, this, [this]() {
        if (m_peakLevel > m_level) {
            m_peakLevel = std::max(m_level, m_peakLevel - 0.02f);
            update();
        }
    });
    m_peakDecayTimer->start(50);
    
    setStyleSheet("background-color: #1a1a20;");
}

void VolumeMeterWidget::setLevel(float level)
{
    m_level = std::clamp(level, 0.0f, 1.0f);
    if (m_level > m_peakLevel) {
        m_peakLevel = m_level;
    }
    update();
}

void VolumeMeterWidget::setPeakLevel(float peak)
{
    m_peakLevel = std::clamp(peak, 0.0f, 1.0f);
    update();
}

void VolumeMeterWidget::resetPeak()
{
    m_peakLevel = 0.0f;
    update();
}

void VolumeMeterWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), QColor("#1a1a20"));
    
    // Border
    painter.setPen(QPen(QColor("#3a3a45"), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    
    QRect meterRect = rect().adjusted(2, 2, -2, -2);
    
    if (m_orientation == Qt::Vertical) {
        // Vertical meter - level fills from bottom
        int meterHeight = meterRect.height();
        int levelHeight = static_cast<int>(m_level * meterHeight);
        int peakY = meterRect.bottom() - static_cast<int>(m_peakLevel * meterHeight);
        
        // Draw level gradient (green -> yellow -> red from bottom to top)
        QLinearGradient gradient(0, meterRect.bottom(), 0, meterRect.top());
        gradient.setColorAt(0.0, QColor("#10b981"));  // Green at bottom
        gradient.setColorAt(0.6, QColor("#10b981"));  // Green up to 60%
        gradient.setColorAt(0.8, QColor("#f59e0b"));  // Yellow at 80%
        gradient.setColorAt(1.0, QColor("#ef4444"));  // Red at top
        
        // Fill the level portion
        QRect levelRect(meterRect.left(), meterRect.bottom() - levelHeight,
                        meterRect.width(), levelHeight);
        painter.fillRect(levelRect, gradient);
        
        // Draw peak indicator
        if (m_peakLevel > 0.01f) {
            painter.setPen(QPen(Qt::white, 2));
            painter.drawLine(meterRect.left(), peakY, meterRect.right(), peakY);
        }
        
        // Draw threshold lines
        painter.setPen(QPen(QColor("#3a3a45"), 1));
        int y6db = meterRect.bottom() - static_cast<int>(0.5f * meterHeight);
        int y12db = meterRect.bottom() - static_cast<int>(0.25f * meterHeight);
        painter.drawLine(meterRect.left(), y6db, meterRect.right(), y6db);
        painter.drawLine(meterRect.left(), y12db, meterRect.right(), y12db);
    } else {
        // Horizontal meter
        int meterWidth = meterRect.width();
        int levelWidth = static_cast<int>(m_level * meterWidth);
        int peakX = meterRect.left() + static_cast<int>(m_peakLevel * meterWidth);
        
        QLinearGradient gradient(meterRect.left(), 0, meterRect.right(), 0);
        gradient.setColorAt(0.0, QColor("#10b981"));
        gradient.setColorAt(0.6, QColor("#10b981"));
        gradient.setColorAt(0.8, QColor("#f59e0b"));
        gradient.setColorAt(1.0, QColor("#ef4444"));
        
        QRect levelRect(meterRect.left(), meterRect.top(), levelWidth, meterRect.height());
        painter.fillRect(levelRect, gradient);
        
        if (m_peakLevel > 0.01f) {
            painter.setPen(QPen(Qt::white, 2));
            painter.drawLine(peakX, meterRect.top(), peakX, meterRect.bottom());
        }
    }
}

/*******************************************************************************************************/
// AudioInputHandler
/*******************************************************************************************************/

AudioInputHandler::AudioInputHandler(QObject *parent)
    : QIODevice(parent)
{
}

void AudioInputHandler::start()
{
    open(QIODevice::WriteOnly);
}

void AudioInputHandler::stop()
{
    close();
}

void AudioInputHandler::clear()
{
    m_recordedSamples.clear();
    m_currentLevel = 0.0f;
}

qint64 AudioInputHandler::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data);
    Q_UNUSED(maxlen);
    return 0;  // Not used for input
}

qint64 AudioInputHandler::writeData(const char *data, qint64 len)
{
    // Convert bytes to float samples (assuming 16-bit PCM)
    const int16_t *samples = reinterpret_cast<const int16_t*>(data);
    int numSamples = static_cast<int>(len / sizeof(int16_t));
    
    std::vector<float> floatSamples;
    floatSamples.reserve(numSamples);
    
    float sumSquared = 0.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        float sample = samples[i] / 32768.0f;
        floatSamples.push_back(sample);
        m_recordedSamples.push_back(sample);
        sumSquared += sample * sample;
    }
    
    // Calculate RMS level
    if (numSamples > 0) {
        m_currentLevel = std::sqrt(sumSquared / numSamples);
        emit levelChanged(m_currentLevel);
    }
    
    emit samplesAvailable(floatSamples);
    
    return len;
}

/*******************************************************************************************************/
// AudioRecordingDialog
/*******************************************************************************************************/

AudioRecordingDialog::AudioRecordingDialog(NoteNagaEngine *engine, 
                                           const QString &projectPath,
                                           QWidget *parent)
    : QDialog(parent)
    , m_engine(engine)
    , m_projectPath(projectPath)
{
    setWindowTitle(tr("Record Audio"));
    setMinimumSize(700, 550);
    setModal(true);
    
    // Get target sample rate from engine
    if (m_engine && m_engine->getRuntimeData()) {
        m_targetSampleRate = m_engine->getRuntimeData()->getAudioManager().getSampleRate();
    }
    
    initUI();
    initAudio();
    populateDevices();
    updateButtonStates();
    
    // Timer for updating recording time
    m_recordingTimer = new QTimer(this);
    connect(m_recordingTimer, &QTimer::timeout, this, &AudioRecordingDialog::updateRecordingTime);
}

AudioRecordingDialog::~AudioRecordingDialog()
{
    if (m_isRecording) {
        stopRecording();
    }
}

bool AudioRecordingDialog::eventFilter(QObject *watched, QEvent *event)
{
    // Sync waveform widget height with scroll area viewport
    if (watched == m_waveformScrollArea->viewport() && event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
        m_waveformWidget->updateHeight(resizeEvent->size().height());
    }
    return QDialog::eventFilter(watched, event);
}

void AudioRecordingDialog::initUI()
{
    setStyleSheet(R"(
        QDialog {
            background-color: #1e1e24;
            color: #cccccc;
        }
        QGroupBox {
            border: 1px solid #3a3a45;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 10px;
            font-weight: bold;
            color: #aaaaaa;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QLabel {
            color: #cccccc;
        }
        QComboBox {
            background-color: #2a2a35;
            color: #cccccc;
            border: 1px solid #3a3a45;
            border-radius: 4px;
            padding: 6px 12px;
            min-width: 200px;
        }
        QComboBox:hover {
            border-color: #10b981;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background-color: #2a2a35;
            color: #cccccc;
            selection-background-color: #10b981;
            border: 1px solid #3a3a45;
        }
        QPushButton {
            background-color: #3a3a45;
            color: #cccccc;
            border: none;
            border-radius: 6px;
            padding: 10px 20px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #4a4a55;
        }
        QPushButton:pressed {
            background-color: #10b981;
        }
        QPushButton:disabled {
            background-color: #2a2a35;
            color: #666666;
        }
        QPushButton#recordBtn {
            background-color: #dc2626;
        }
        QPushButton#recordBtn:hover {
            background-color: #ef4444;
        }
        QPushButton#stopBtn {
            background-color: #f59e0b;
        }
        QPushButton#stopBtn:hover {
            background-color: #fbbf24;
        }
        QPushButton#deleteBtn {
            background-color: #7f1d1d;
        }
        QPushButton#deleteBtn:hover {
            background-color: #991b1b;
        }
        QPushButton#doneBtn {
            background-color: #10b981;
        }
        QPushButton#doneBtn:hover {
            background-color: #34d399;
        }
        QSpinBox {
            background-color: #2a2a35;
            color: #cccccc;
            border: 1px solid #3a3a45;
            border-radius: 4px;
            padding: 4px 8px;
        }
        QCheckBox {
            color: #cccccc;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }
        QScrollArea {
            border: 1px solid #3a3a45;
            border-radius: 4px;
        }
    )");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    
    // === Device selection group ===
    QGroupBox *deviceGroup = new QGroupBox(tr("Audio Input Device"), this);
    QHBoxLayout *deviceLayout = new QHBoxLayout(deviceGroup);
    deviceLayout->setContentsMargins(12, 16, 12, 12);
    
    m_deviceCombo = new QComboBox(this);
    deviceLayout->addWidget(m_deviceCombo);
    
    deviceLayout->addStretch();
    
    // Sample rate selection
    QLabel *sampleRateLabel = new QLabel(tr("Sample Rate:"), this);
    deviceLayout->addWidget(sampleRateLabel);
    
    m_sampleRateSpin = new QSpinBox(this);
    m_sampleRateSpin->setRange(22050, 96000);
    m_sampleRateSpin->setValue(m_targetSampleRate);
    m_sampleRateSpin->setSuffix(" Hz");
    m_sampleRateSpin->setSingleStep(1000);
    deviceLayout->addWidget(m_sampleRateSpin);
    
    m_monoCheck = new QCheckBox(tr("Mono"), this);
    m_monoCheck->setChecked(false);
    deviceLayout->addWidget(m_monoCheck);
    
    mainLayout->addWidget(deviceGroup);
    
    // === Waveform display ===
    QGroupBox *waveformGroup = new QGroupBox(tr("Recording Waveform"), this);
    QHBoxLayout *waveformLayout = new QHBoxLayout(waveformGroup);
    waveformLayout->setContentsMargins(12, 16, 12, 12);
    
    // Volume meter on the left
    m_volumeMeter = new VolumeMeterWidget(Qt::Vertical, this);
    waveformLayout->addWidget(m_volumeMeter);
    
    // Scrollable waveform area - only horizontal scroll, waveform fills vertical space
    m_waveformScrollArea = new QScrollArea(this);
    m_waveformScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_waveformScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_waveformScrollArea->setMinimumHeight(150);
    m_waveformScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_waveformWidget = new RecordingWaveformWidget(this);
    m_waveformWidget->setMinimumWidth(600);
    m_waveformScrollArea->setWidget(m_waveformWidget);
    
    // Install event filter to sync waveform height with scroll area
    m_waveformScrollArea->viewport()->installEventFilter(this);
    
    waveformLayout->addWidget(m_waveformScrollArea, 1);
    
    mainLayout->addWidget(waveformGroup, 1);
    
    // === Status and time display ===
    QFrame *statusFrame = new QFrame(this);
    statusFrame->setStyleSheet("QFrame { background-color: #252530; border-radius: 6px; padding: 8px; }");
    QHBoxLayout *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(16, 8, 16, 8);
    
    m_statusLabel = new QLabel(tr("Ready to record"), this);
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #10b981;");
    statusLayout->addWidget(m_statusLabel);
    
    statusLayout->addStretch();
    
    m_timeLabel = new QLabel("00:00.000", this);
    m_timeLabel->setStyleSheet("font-size: 24px; font-weight: bold; font-family: 'Menlo', 'Consolas', monospace; color: #f59e0b;");
    statusLayout->addWidget(m_timeLabel);
    
    statusLayout->addStretch();
    
    m_formatLabel = new QLabel(this);
    m_formatLabel->setStyleSheet("font-size: 12px; color: #888888;");
    statusLayout->addWidget(m_formatLabel);
    
    mainLayout->addWidget(statusFrame);
    
    // === Control buttons ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    m_recordBtn = new QPushButton(tr("⏺ Record"), this);
    m_recordBtn->setObjectName("recordBtn");
    m_recordBtn->setMinimumWidth(100);
    connect(m_recordBtn, &QPushButton::clicked, this, &AudioRecordingDialog::onRecordClicked);
    buttonLayout->addWidget(m_recordBtn);
    
    m_stopBtn = new QPushButton(tr("⏹ Stop"), this);
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setMinimumWidth(100);
    connect(m_stopBtn, &QPushButton::clicked, this, &AudioRecordingDialog::onStopClicked);
    buttonLayout->addWidget(m_stopBtn);
    
    m_playBtn = new QPushButton(tr("▶ Play"), this);
    m_playBtn->setObjectName("playBtn");
    m_playBtn->setMinimumWidth(100);
    connect(m_playBtn, &QPushButton::clicked, this, &AudioRecordingDialog::onPlayClicked);
    buttonLayout->addWidget(m_playBtn);
    
    m_deleteBtn = new QPushButton(tr("🗑 Delete"), this);
    m_deleteBtn->setObjectName("deleteBtn");
    m_deleteBtn->setMinimumWidth(100);
    connect(m_deleteBtn, &QPushButton::clicked, this, &AudioRecordingDialog::onDeleteClicked);
    buttonLayout->addWidget(m_deleteBtn);
    
    buttonLayout->addStretch();
    
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setMinimumWidth(100);
    connect(m_cancelBtn, &QPushButton::clicked, this, &AudioRecordingDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelBtn);
    
    m_doneBtn = new QPushButton(tr("✓ Done"), this);
    m_doneBtn->setObjectName("doneBtn");
    m_doneBtn->setMinimumWidth(100);
    connect(m_doneBtn, &QPushButton::clicked, this, &AudioRecordingDialog::onDoneClicked);
    buttonLayout->addWidget(m_doneBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // Update format label
    updateButtonStates();
}

void AudioRecordingDialog::initAudio()
{
    m_inputHandler = std::make_unique<AudioInputHandler>(this);
    
    connect(m_inputHandler.get(), &AudioInputHandler::samplesAvailable,
            this, &AudioRecordingDialog::onSamplesAvailable);
    connect(m_inputHandler.get(), &AudioInputHandler::levelChanged,
            this, &AudioRecordingDialog::onLevelChanged);
    
    // Connect device selection
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioRecordingDialog::onDeviceChanged);
    
    // Connect sample rate changes
    connect(m_sampleRateSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_targetSampleRate = value;
        if (!m_isRecording) {
            m_formatLabel->setText(tr("%1 Hz, %2")
                .arg(value)
                .arg(m_monoCheck->isChecked() ? tr("Mono") : tr("Stereo")));
        }
    });
    
    connect(m_monoCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_isRecording) {
            m_formatLabel->setText(tr("%1 Hz, %2")
                .arg(m_targetSampleRate)
                .arg(checked ? tr("Mono") : tr("Stereo")));
        }
    });
}

void AudioRecordingDialog::populateDevices()
{
    m_deviceCombo->clear();
    m_audioDevices = QMediaDevices::audioInputs();
    
    if (m_audioDevices.isEmpty()) {
        m_deviceCombo->addItem(tr("No audio input devices found"));
        m_recordBtn->setEnabled(false);
        m_statusLabel->setText(tr("No microphone detected"));
        m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #ef4444;");
        return;
    }
    
    for (const QAudioDevice &device : m_audioDevices) {
        m_deviceCombo->addItem(device.description());
    }
    
    // Select default device
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
    for (int i = 0; i < m_audioDevices.size(); ++i) {
        if (m_audioDevices[i].id() == defaultDevice.id()) {
            m_deviceCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_formatLabel->setText(tr("%1 Hz, %2")
        .arg(m_targetSampleRate)
        .arg(m_monoCheck->isChecked() ? tr("Mono") : tr("Stereo")));
}

void AudioRecordingDialog::onDeviceChanged(int index)
{
    Q_UNUSED(index);
    // Device will be used when recording starts
}

void AudioRecordingDialog::onRecordClicked()
{
    startRecording();
}

void AudioRecordingDialog::onStopClicked()
{
    if (m_isPlaying) {
        stopPlayback();
    } else {
        stopRecording();
    }
}

void AudioRecordingDialog::onPlayClicked()
{
    if (m_isPlaying) {
        stopPlayback();
    } else {
        startPlayback();
    }
}

void AudioRecordingDialog::onDeleteClicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Recording"),
        tr("Are you sure you want to delete this recording?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        clearRecording();
    }
}

void AudioRecordingDialog::onDoneClicked()
{
    if (!m_hasRecording) {
        QMessageBox::warning(this, tr("No Recording"),
                             tr("Please record something first."));
        return;
    }
    
    if (saveRecording()) {
        accept();
    }
}

void AudioRecordingDialog::onCancelClicked()
{
    if (m_hasRecording) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("Discard Recording"),
            tr("You have an unsaved recording. Discard it?"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    reject();
}

void AudioRecordingDialog::onSamplesAvailable(const std::vector<float> &samples)
{
    m_recordedSamples.insert(m_recordedSamples.end(), samples.begin(), samples.end());
    m_waveformWidget->addSamples(samples);
}

void AudioRecordingDialog::onLevelChanged(float level)
{
    // Convert to more visible range
    float displayLevel = std::pow(level, 0.5f) * 1.5f;  // Make quieter sounds more visible
    m_volumeMeter->setLevel(std::min(1.0f, displayLevel));
}

void AudioRecordingDialog::updateRecordingTime()
{
    if (!m_isRecording) return;
    
    qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_recordingStartTime;
    
    int minutes = static_cast<int>(elapsedMs / 60000);
    int seconds = static_cast<int>((elapsedMs % 60000) / 1000);
    int milliseconds = static_cast<int>(elapsedMs % 1000);
    
    m_timeLabel->setText(QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds, 3, 10, QChar('0')));
}

void AudioRecordingDialog::startRecording()
{
    if (m_audioDevices.isEmpty()) return;
    
    int deviceIndex = m_deviceCombo->currentIndex();
    if (deviceIndex < 0 || deviceIndex >= m_audioDevices.size()) return;
    
    // Clear previous recording
    if (m_hasRecording) {
        clearRecording();
    }
    
    // Setup audio format
    m_audioFormat.setSampleRate(m_targetSampleRate);
    m_audioFormat.setChannelCount(m_monoCheck->isChecked() ? 1 : 2);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);
    
    QAudioDevice selectedDevice = m_audioDevices[deviceIndex];
    
    if (!selectedDevice.isFormatSupported(m_audioFormat)) {
        // Try with preferred format
        m_audioFormat = selectedDevice.preferredFormat();
        m_audioFormat.setSampleFormat(QAudioFormat::Int16);
        
        if (!selectedDevice.isFormatSupported(m_audioFormat)) {
            QMessageBox::critical(this, tr("Audio Error"),
                                  tr("Selected audio format is not supported by the device."));
            return;
        }
    }
    
    // Create audio source
    m_audioSource = std::make_unique<QAudioSource>(selectedDevice, m_audioFormat, this);
    
    // Start recording
    m_inputHandler->start();
    m_audioSource->start(m_inputHandler.get());
    
    m_isRecording = true;
    m_recordingStartTime = QDateTime::currentMSecsSinceEpoch();
    m_recordingTimer->start(50);  // Update every 50ms
    
    m_statusLabel->setText(tr("Recording..."));
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #ef4444;");
    m_formatLabel->setText(tr("%1 Hz, %2 channel(s)")
        .arg(m_audioFormat.sampleRate())
        .arg(m_audioFormat.channelCount()));
    
    updateButtonStates();
}

void AudioRecordingDialog::stopRecording()
{
    if (!m_isRecording) return;
    
    m_recordingTimer->stop();
    
    if (m_audioSource) {
        m_audioSource->stop();
        m_audioSource.reset();
    }
    
    m_inputHandler->stop();
    
    m_isRecording = false;
    m_hasRecording = !m_recordedSamples.empty();
    
    if (m_hasRecording) {
        double durationSec = static_cast<double>(m_recordedSamples.size()) / m_audioFormat.sampleRate();
        int minutes = static_cast<int>(durationSec / 60);
        int seconds = static_cast<int>(durationSec) % 60;
        int milliseconds = static_cast<int>((durationSec - std::floor(durationSec)) * 1000);
        
        m_statusLabel->setText(tr("Recording complete: %1:%2.%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(milliseconds, 3, 10, QChar('0')));
        m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #10b981;");
    } else {
        m_statusLabel->setText(tr("Ready to record"));
        m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #10b981;");
    }
    
    m_volumeMeter->setLevel(0);
    m_volumeMeter->resetPeak();
    
    updateButtonStates();
}

void AudioRecordingDialog::startPlayback()
{
    if (m_recordedSamples.empty() || m_isPlaying) return;
    
    // Get default output device
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isNull()) {
        QMessageBox::warning(this, tr("No Audio Output"),
                             tr("No audio output device available."));
        return;
    }
    
    // Setup playback format
    QAudioFormat format;
    format.setSampleRate(m_targetSampleRate);
    format.setChannelCount(m_monoCheck->isChecked() ? 1 : 2);
    format.setSampleFormat(QAudioFormat::Float);
    
    // Convert samples to byte array
    QByteArray audioData;
    audioData.resize(static_cast<int>(m_recordedSamples.size() * sizeof(float)));
    memcpy(audioData.data(), m_recordedSamples.data(), m_recordedSamples.size() * sizeof(float));
    
    // Create buffer for playback
    if (m_playbackBuffer) {
        delete m_playbackBuffer;
    }
    m_playbackBuffer = new QBuffer(this);
    m_playbackBuffer->setData(audioData);
    m_playbackBuffer->open(QIODevice::ReadOnly);
    
    // Create and start audio sink
    m_audioSink = std::make_unique<QAudioSink>(outputDevice, format);
    
    connect(m_audioSink.get(), &QAudioSink::stateChanged,
            this, &AudioRecordingDialog::onPlaybackStateChanged);
    
    m_audioSink->start(m_playbackBuffer);
    m_isPlaying = true;
    
    m_statusLabel->setText(tr("Playing recording..."));
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #3b82f6;");
    
    updateButtonStates();
}

void AudioRecordingDialog::stopPlayback()
{
    if (!m_isPlaying) return;
    
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
    }
    
    if (m_playbackBuffer) {
        m_playbackBuffer->close();
        delete m_playbackBuffer;
        m_playbackBuffer = nullptr;
    }
    
    m_isPlaying = false;
    
    m_statusLabel->setText(tr("Recording stopped"));
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #f59e0b;");
    
    updateButtonStates();
}

void AudioRecordingDialog::onPlaybackStateChanged(QAudio::State state)
{
    if (state == QAudio::IdleState) {
        // Playback finished
        stopPlayback();
    } else if (state == QAudio::StoppedState) {
        // Check for errors
        if (m_audioSink && m_audioSink->error() != QAudio::NoError) {
            stopPlayback();
        }
    }
}

void AudioRecordingDialog::clearRecording()
{
    if (m_isRecording) {
        stopRecording();
    }
    
    m_recordedSamples.clear();
    m_inputHandler->clear();
    m_waveformWidget->clear();
    m_hasRecording = false;
    
    m_statusLabel->setText(tr("Ready to record"));
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #10b981;");
    m_timeLabel->setText("00:00.000");
    
    updateButtonStates();
}

QString AudioRecordingDialog::getAudioFolderPath() const
{
    if (m_projectPath.isEmpty()) {
        return QString();
    }
    
    QFileInfo projectInfo(m_projectPath);
    QString projectDir = projectInfo.absolutePath();
    QString projectName = projectInfo.baseName();  // filename without extension
    
    return projectDir + "/" + projectName + "_audio";
}

QString AudioRecordingDialog::generateFileName() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("recording_%1.wav").arg(timestamp);
}

bool AudioRecordingDialog::saveRecording()
{
    if (m_recordedSamples.empty()) {
        return false;
    }
    
    QString audioFolder = getAudioFolderPath();
    
    if (audioFolder.isEmpty()) {
        // Project not saved yet - ask for location
        QMessageBox::warning(this, tr("Project Not Saved"),
                             tr("Please save your project first before recording audio.\n"
                                "Audio files will be saved in a folder next to the project file."));
        return false;
    }
    
    // Create audio folder if it doesn't exist
    QDir dir;
    if (!dir.exists(audioFolder)) {
        if (!dir.mkpath(audioFolder)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Failed to create audio folder:\n%1").arg(audioFolder));
            return false;
        }
    }
    
    QString fileName = generateFileName();
    QString filePath = audioFolder + "/" + fileName;
    
    // Save as WAV file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to create audio file:\n%1").arg(filePath));
        return false;
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    int channels = m_audioFormat.channelCount();
    int sampleRate = m_audioFormat.sampleRate();
    int bitsPerSample = 16;
    int byteRate = sampleRate * channels * bitsPerSample / 8;
    int blockAlign = channels * bitsPerSample / 8;
    int dataSize = static_cast<int>(m_recordedSamples.size()) * sizeof(int16_t);
    
    // WAV header
    file.write("RIFF", 4);
    stream << static_cast<quint32>(36 + dataSize);
    file.write("WAVE", 4);
    
    // fmt chunk
    file.write("fmt ", 4);
    stream << static_cast<quint32>(16);  // chunk size
    stream << static_cast<quint16>(1);   // PCM format
    stream << static_cast<quint16>(channels);
    stream << static_cast<quint32>(sampleRate);
    stream << static_cast<quint32>(byteRate);
    stream << static_cast<quint16>(blockAlign);
    stream << static_cast<quint16>(bitsPerSample);
    
    // data chunk
    file.write("data", 4);
    stream << static_cast<quint32>(dataSize);
    
    // Write samples
    for (float sample : m_recordedSamples) {
        int16_t intSample = static_cast<int16_t>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
        stream << intSample;
    }
    
    file.close();
    
    m_savedFilePath = filePath;
    
    // Import into audio manager
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaAudioResource *resource = 
            m_engine->getRuntimeData()->getAudioManager().importAudio(filePath.toStdString());
        if (resource) {
            emit recordingSaved(filePath);
        } else {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("Recording saved but failed to import into project."));
        }
    }
    
    return true;
}

void AudioRecordingDialog::updateButtonStates()
{
    bool hasDevice = !m_audioDevices.isEmpty();
    
    m_recordBtn->setEnabled(hasDevice && !m_isRecording && !m_isPlaying);
    m_stopBtn->setEnabled(m_isRecording || m_isPlaying);
    m_playBtn->setEnabled(!m_isRecording && m_hasRecording && !m_isPlaying);
    m_deleteBtn->setEnabled(!m_isRecording && !m_isPlaying && m_hasRecording);
    m_doneBtn->setEnabled(!m_isRecording && !m_isPlaying && m_hasRecording);
    
    m_deviceCombo->setEnabled(!m_isRecording && !m_isPlaying);
    m_sampleRateSpin->setEnabled(!m_isRecording && !m_isPlaying);
    m_monoCheck->setEnabled(!m_isRecording && !m_isPlaying);
    
    // Update play button text
    if (m_isPlaying) {
        m_playBtn->setText(tr("⏹ Stop"));
    } else {
        m_playBtn->setText(tr("▶ Play"));
    }
}
