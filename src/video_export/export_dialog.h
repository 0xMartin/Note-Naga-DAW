#pragma once

#include <QDialog>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_engine.h>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QProgressBar; 
class QThread;
class QDoubleSpinBox;
class QGroupBox;
QT_END_NAMESPACE

class VideoRenderer;
class VideoExporter;

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(NoteNagaMidiSeq *sequence, NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~ExportDialog();

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onPlaybackTickChanged(int tick);
    void seek(int value);
    void onExportClicked();
    void onExportFinished();

    // NOVÉ: Specifické sloty pro aktualizaci UI
    void updateAudioProgress(int percentage);
    void updateVideoProgress(int percentage);
    void updateStatusText(const QString &status);

private:
    void setupUi();
    void connectEngineSignals();
    void setControlsEnabled(bool enabled);

    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    VideoRenderer *m_renderer;

    // UI prvky
    QLabel *m_previewLabel;
    QPushButton *m_playPauseButton;
    QPushButton *m_stopButton;
    QSlider *m_timeSlider;
    QComboBox *m_resolutionCombo;
    QComboBox *m_fpsCombo;
    QDoubleSpinBox *m_scaleSpinBox;
    QPushButton *m_exportButton;
    
    // ZMĚNA: Oddělené prvky pro progress
    QProgressBar *m_audioProgressBar;
    QProgressBar *m_videoProgressBar;
    QLabel *m_audioProgressLabel;
    QLabel *m_videoProgressLabel;
    QLabel *m_statusLabel;
    
    QGroupBox *m_settingsGroup;
    QGroupBox *m_previewGroup;
    QWidget *m_progressWidget;

    double m_currentTime;
    double m_totalDuration;

    QThread *m_exportThread;
    VideoExporter *m_exporter;
};