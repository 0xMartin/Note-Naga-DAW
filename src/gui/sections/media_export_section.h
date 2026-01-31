#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QColor>
#include <QMap>
#include <QElapsedTimer>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_engine.h>

#include "../components/midi_seq_progress_bar.h"
#include "../components/audio_bars_visualizer.h" 
#include "../../media_export/media_exporter.h" 
#include "../../media_export/media_renderer.h" 
#include "../../media_export/preview_worker.h" 

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
class QStackedWidget;
class QDockWidget;
QT_END_NAMESPACE

class AdvancedDockWidget;

/**
 * @brief Section for configuring and exporting video/audio rendering of a MIDI sequence.
 *        This is an embedded version of ExportDialog for use in the Media Export section.
 */
class MediaExportSection : public QMainWindow
{
    Q_OBJECT

public:
    explicit MediaExportSection(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~MediaExportSection();

    /**
     * @brief Refreshes the widget when the active sequence changes
     */
    void refreshSequence();

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
    void onExportTypeChanged(int index); 
    void onParticleTypeChanged(int index);
    void onSelectParticleFile();
    void updatePreviewSettings(); 
    void onSelectBgColor();
    void onSelectBgImage();
    void onClearBg();
    void updateBgLabels();
    void onSelectLightningColor(); 
    
    /**
     * @brief Receives the finished frame from the PreviewWorker thread and displays it.
     */
    void onPreviewFrameReady(const QImage& frame);

private:
    // Engine and data
    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    
    // --- Preview thread ---
    QThread* m_previewThread;
    PreviewWorker* m_previewWorker;

    // --- UI Components ---

    // No sequence placeholder (overlay)
    QLabel *m_noSequenceLabel;

    // Preview components
    QLabel *m_previewLabel;
    QLabel *m_previewStatsLabel;
    AudioBarsVisualizer *m_audioBarsVisualizer;
    QStackedWidget *m_previewStack;
    QPushButton *m_playPauseButton;
    MidiSequenceProgressBar *m_progressBar; 
    QPushButton *m_exportButton;
    
    // Preview stats tracking
    QElapsedTimer m_frameTimer;
    int m_frameCount;
    qint64 m_lastFpsUpdate;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Progress components
    QProgressBar *m_audioProgressBar;
    QProgressBar *m_videoProgressBar;
    QLabel *m_audioProgressLabel;
    QLabel *m_videoProgressLabel;
    QLabel *m_statusLabel;
    
    QWidget *m_progressWidget;
        
    // Settings components
    QScrollArea *m_settingsScrollArea;
    QWidget *m_settingsWidget; 
    
    // Export settings
    QGroupBox *m_exportSettingsGroup;
    QComboBox *m_exportTypeCombo;
    
    // Video-specific settings
    QGroupBox *m_videoSettingsGroup;
    QComboBox *m_resolutionCombo;
    QComboBox *m_fpsCombo;
    QDoubleSpinBox *m_scaleSpinBox;

    // Audio-specific settings
    QGroupBox *m_audioSettingsGroup;
    QComboBox *m_audioFormatCombo;
    QSpinBox *m_audioBitrateSpin;

    // Background settings
    QGroupBox *m_bgGroup;
    QPushButton *m_bgColorButton;
    QPushButton *m_bgImageButton;
    QPushButton *m_bgClearButton;
    QLabel *m_bgColorPreview;
    QLabel *m_bgImagePreview;
    QCheckBox *m_bgShakeCheck;
    QDoubleSpinBox *m_bgShakeSpin;

    // Render settings 
    QGroupBox *m_renderGroup;
    QCheckBox *m_renderNotesCheck;
    QCheckBox *m_renderKeyboardCheck;
    QCheckBox *m_renderParticlesCheck;
    QCheckBox *m_pianoGlowCheck;
    QCheckBox *m_lightningEnableCheck;
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

    // Lightning settings
    QGroupBox *m_lightningGroup;
    QPushButton *m_lightningColorButton;
    QLabel *m_lightningColorPreview;
    QDoubleSpinBox *m_lightningThicknessSpin;
    QSpinBox *m_lightningLinesSpin;
    QDoubleSpinBox *m_lightningJitterYSpin;
    QDoubleSpinBox *m_lightningJitterXSpin; 

    // State variables
    QString m_particleFilePath; 
    QColor m_backgroundColor;
    QString m_backgroundImagePath;
    QColor m_lightningColor; 
    double m_currentTime;
    double m_totalDuration;
    QSize m_lastRenderSize; 

    // Export threading
    QThread *m_exportThread;
    MediaExporter *m_exporter;

    MediaRenderer::RenderSettings getCurrentRenderSettings();
    void setupUi();
    void setupMainContent();
    void setupDockLayout();
    void connectEngineSignals();
    void connectWidgetSignals();
    void setControlsEnabled(bool enabled);
    QSize getTargetResolution();
    void updatePreviewRenderSize();
    void initPreviewWorker();
    void cleanupPreviewWorker();
    
protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;
    virtual bool eventFilter(QObject *watched, QEvent *event) override;
};
