#pragma once

#include <QDialog>
#include <QColor>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_engine.h>
#include "video_renderer.h"
#include "../gui/components/midi_seq_progress_bar.h" 

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QComboBox;
class QProgressBar;
class QThread;
class QDoubleSpinBox;
class QGroupBox;
class QCheckBox;
class QSpinBox;
class QScrollArea;
class QSplitter;
QT_END_NAMESPACE

class VideoExporter;

/**
 * @brief Dialog for configuring and exporting a video rendering of a MIDI sequence.
 */
class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the ExportDialog.
     * @param sequence The MIDI sequence to export.
     * @param engine The NoteNagaEngine for playback control.
     * @param parent The parent widget.
     */
    explicit ExportDialog(NoteNagaMidiSeq *sequence, NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~ExportDialog();

private slots:
    // Playback controls
    void onPlayPauseClicked();
    void onPlaybackTickChanged(int tick);
    void seek(float seconds); 
    
    // Export process
    void onExportClicked();
    void onExportFinished();

    // Progress updates
    void updateAudioProgress(int percentage);
    void updateVideoProgress(int percentage);
    void updateStatusText(const QString &status);
    
    // Settings changes
    void onParticleTypeChanged(int index);
    void onSelectParticleFile();
    void updatePreviewSettings(); 
    void onSelectBgColor();
    void onSelectBgImage();
    void onClearBg();
    void updateBgLabels();

private:
    /**
     * @brief Gathers all current settings from the UI into a struct.
     * @return A VideoRenderer::RenderSettings struct.
     */
    VideoRenderer::RenderSettings getCurrentRenderSettings();
    
    /**
     * @brief Creates and lays out all UI components.
     */
    void setupUi();
    
    /**
     * @brief Connects signals from the NoteNagaEngine to this dialog's slots.
     */
    void connectEngineSignals();
    
    /**
     * @brief Enables or disables UI controls during the export process.
     * @param enabled True to enable controls, false to disable.
     */
    void setControlsEnabled(bool enabled);

    /**
     * @brief Gets the target resolution QSize from the UI.
    */
    QSize getTargetResolution();

    // Engine and data
    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    VideoRenderer *m_renderer;

    // --- UI Components ---
    QSplitter *m_mainSplitter;
    QWidget *m_leftWidget;
    QWidget *m_rightWidget;

    // Preview components
    QLabel *m_previewLabel;
    QPushButton *m_playPauseButton;
    MidiSequenceProgressBar *m_progressBar; 
    QPushButton *m_exportButton;
    
    // Progress components
    QProgressBar *m_audioProgressBar;
    QProgressBar *m_videoProgressBar;
    QLabel *m_audioProgressLabel;
    QLabel *m_videoProgressLabel;
    QLabel *m_statusLabel;
    
    QGroupBox *m_previewGroup;
    QWidget *m_progressWidget;
        
    // Settings components
    QScrollArea *m_settingsScrollArea;
    QWidget *m_settingsWidget; 
    
    // Export settings
    QComboBox *m_resolutionCombo;
    QComboBox *m_fpsCombo;
    QDoubleSpinBox *m_scaleSpinBox;

    // Background settings
    QPushButton *m_bgColorButton;
    QPushButton *m_bgImageButton;
    QPushButton *m_bgClearButton;
    QLabel *m_bgColorPreview;
    QLabel *m_bgImagePreview;

    // Render settings 
    QCheckBox *m_renderNotesCheck;
    QCheckBox *m_renderKeyboardCheck;
    QCheckBox *m_renderParticlesCheck;
    QCheckBox *m_pianoGlowCheck;
    QDoubleSpinBox *m_noteStartOpacitySpin;
    QDoubleSpinBox *m_noteEndOpacitySpin;
    
    // Particle settings
    QGroupBox *m_particleSettingsGroup; 
    QComboBox *m_particleTypeCombo;
    QPushButton *m_particleFileButton;
    QLabel *m_particlePreviewLabel;
    QSpinBox *m_particleCountSpin;
    QDoubleSpinBox *m_particleLifetimeSpin;
    QDoubleSpinBox *m_particleSpeedSpin;
    QDoubleSpinBox *m_particleGravitySpin;
    QCheckBox *m_particleTintCheck;
    QDoubleSpinBox *m_particleStartSizeSpin;
    QDoubleSpinBox *m_particleEndSizeSpin;

    // State variables
    QString m_particleFilePath; 
    QColor m_backgroundColor;
    QString m_backgroundImagePath;
    double m_currentTime;
    double m_totalDuration;

    // Export threading
    QThread *m_exportThread;
    VideoExporter *m_exporter;
};