#pragma once

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QAudioSource>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QIODevice>
#include <QWidget>
#include <QProgressBar>
#include <QScrollArea>
#include <QSpinBox>
#include <QCheckBox>

#include <vector>
#include <memory>

class NoteNagaEngine;

/**
 * @brief Custom widget for displaying real-time waveform during recording
 */
class RecordingWaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingWaveformWidget(QWidget *parent = nullptr);

    /// Add new audio samples to the display
    void addSamples(const std::vector<float> &samples);
    
    /// Clear all waveform data
    void clear();
    
    /// Set whether to auto-scroll to the end
    void setAutoScroll(bool autoScroll) { m_autoScroll = autoScroll; }
    
    /// Get the current recording length in samples
    int64_t getTotalSamples() const { return m_peakData.size() * m_samplesPerPeak; }
    
    /// Update the fixed height to match parent scroll area
    void updateHeight(int height);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updatePeaks(const std::vector<float> &samples);
    
    std::vector<std::pair<float, float>> m_peakData; // min/max pairs
    std::vector<float> m_currentPeakBuffer;
    int m_samplesPerPeak = 256;
    bool m_autoScroll = true;
    int m_scrollOffset = 0;
};

/**
 * @brief Custom widget for displaying real-time volume meter
 */
class VolumeMeterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeMeterWidget(Qt::Orientation orientation = Qt::Vertical, QWidget *parent = nullptr);

    /// Set the current level (0.0 - 1.0)
    void setLevel(float level);
    
    /// Set the peak level (0.0 - 1.0)
    void setPeakLevel(float peak);
    
    /// Reset peak hold
    void resetPeak();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    float m_level = 0.0f;
    float m_peakLevel = 0.0f;
    Qt::Orientation m_orientation;
    QTimer *m_peakDecayTimer;
};

/**
 * @brief Audio input handler for capturing audio data
 */
class AudioInputHandler : public QIODevice
{
    Q_OBJECT

public:
    explicit AudioInputHandler(QObject *parent = nullptr);

    void start();
    void stop();
    
    /// Get all recorded samples
    const std::vector<float>& getRecordedSamples() const { return m_recordedSamples; }
    
    /// Clear recorded data
    void clear();
    
    /// Get current RMS level
    float getCurrentLevel() const { return m_currentLevel; }

signals:
    void samplesAvailable(const std::vector<float> &samples);
    void levelChanged(float level);

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    std::vector<float> m_recordedSamples;
    float m_currentLevel = 0.0f;
};

/**
 * @brief Dialog for recording audio from microphone
 * 
 * Features:
 * - Device selection
 * - Real-time waveform visualization with auto-scroll
 * - Volume meter
 * - Record/Stop/Delete controls
 * - Save to project audio folder
 */
class AudioRecordingDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param engine NoteNaga engine for audio settings
     * @param projectPath Path to the project file (for determining audio folder)
     * @param parent Parent widget
     */
    explicit AudioRecordingDialog(NoteNagaEngine *engine, 
                                  const QString &projectPath,
                                  QWidget *parent = nullptr);
    ~AudioRecordingDialog();

    /// Get the path to the saved recording (empty if cancelled)
    QString getSavedFilePath() const { return m_savedFilePath; }
    
    /// Check if recording was saved
    bool wasRecordingSaved() const { return !m_savedFilePath.isEmpty(); }

signals:
    /// Emitted when a recording is saved successfully
    void recordingSaved(const QString &filePath);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onDeviceChanged(int index);
    void onRecordClicked();
    void onStopClicked();
    void onDeleteClicked();
    void onDoneClicked();
    void onCancelClicked();
    void onSamplesAvailable(const std::vector<float> &samples);
    void onLevelChanged(float level);
    void updateRecordingTime();

private:
    void initUI();
    void initAudio();
    void populateDevices();
    void startRecording();
    void stopRecording();
    void clearRecording();
    bool saveRecording();
    QString generateFileName() const;
    QString getAudioFolderPath() const;
    void updateButtonStates();

    NoteNagaEngine *m_engine;
    QString m_projectPath;
    QString m_savedFilePath;

    // UI elements
    QComboBox *m_deviceCombo;
    QLabel *m_statusLabel;
    QLabel *m_timeLabel;
    QLabel *m_formatLabel;
    RecordingWaveformWidget *m_waveformWidget;
    QScrollArea *m_waveformScrollArea;
    VolumeMeterWidget *m_volumeMeter;
    QPushButton *m_recordBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_doneBtn;
    QPushButton *m_cancelBtn;
    QSpinBox *m_sampleRateSpin;
    QCheckBox *m_monoCheck;

    // Audio
    std::unique_ptr<QAudioSource> m_audioSource;
    std::unique_ptr<AudioInputHandler> m_inputHandler;
    QAudioFormat m_audioFormat;
    QList<QAudioDevice> m_audioDevices;

    // Recording state
    bool m_isRecording = false;
    bool m_hasRecording = false;
    std::vector<float> m_recordedSamples;
    QTimer *m_recordingTimer;
    qint64 m_recordingStartTime = 0;
    int m_targetSampleRate = 44100;
};
