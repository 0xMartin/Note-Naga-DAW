#pragma once

#include <QFrame>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/module/playback_worker.h>
#include "../components/button_group_widget.h"

class TrackStereoMeter;
class MidiSequenceProgressBar;

/**
 * @brief GlobalTransportBar provides a compact, unified transport control bar.
 * 
 * This widget is designed to be placed at the bottom of the application
 * (in SectionSwitcher) and provides all playback controls across all sections:
 * - Transport controls: Go to Start, Play/Stop, Go to End
 * - Playback mode toggle: Sequence (single MIDI) vs Compose (full timeline)
 * - BPM display and editing
 * - Current time / Total time display
 * - Progress bar with seek capability
 * - Global RMS level meter
 */
class GlobalTransportBar : public QFrame {
    Q_OBJECT

public:
    /**
     * @brief Constructor for GlobalTransportBar.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param parent Parent widget.
     */
    explicit GlobalTransportBar(NoteNagaEngine* engine, QWidget* parent = nullptr);

    /**
     * @brief Get the current playback mode.
     * @return Current PlaybackMode (Sequence or Compose).
     */
    PlaybackMode getPlaybackMode() const { return m_playbackMode; }

    /**
     * @brief Set the playback mode.
     * @param mode The playback mode to set.
     */
    void setPlaybackMode(PlaybackMode mode);

    /**
     * @brief Get the stereo meter widget for external updates.
     * @return Pointer to the TrackStereoMeter widget.
     */
    TrackStereoMeter* getStereoMeter() const { return m_stereoMeter; }

public slots:
    /**
     * @brief Set playing state of the transport bar.
     * @param is_playing True if playing, false if stopped.
     */
    void setPlaying(bool is_playing);

signals:
    /**
     * @brief Signal emitted when the play button is toggled.
     */
    void playToggled();

    /**
     * @brief Signal emitted when the user navigates to the start of the sequence.
     */
    void goToStart();

    /**
     * @brief Signal emitted when the user navigates to the end of the sequence.
     */
    void goToEnd();

    /**
     * @brief Signal emitted when the tempo is changed.
     * @param tempo New tempo in microseconds per beat.
     */
    void tempoChanged(int tempo);

    /**
     * @brief Signal emitted when the metronome is toggled.
     * @param state True if metronome is on, false otherwise.
     */
    void metronomeToggled(bool state);

    /**
     * @brief Signal emitted when the progress bar position is changed.
     * @param seconds Position in seconds.
     * @param tick_position Position in ticks.
     */
    void playPositionChanged(float seconds, int tick_position);

    /**
     * @brief Signal emitted when playback mode changes.
     * @param mode The new playback mode.
     */
    void playbackModeChanged(PlaybackMode mode);

private slots:
    void updateBPM();
    void updateProgressBar();
    void metronomeBtnClicked();
    void onPlaybackModeToggled();

    void onProgressBarPositionPressed(float seconds);
    void onProgressBarPositionDragged(float seconds);
    void onProgressBarPositionReleased(float seconds);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    NoteNagaEngine* m_engine;

    // Playback state
    int m_ppq;
    int m_tempo;
    int m_maxTick;
    bool m_wasPlaying;
    double m_currentDisplayBPM;
    bool m_isPlaying;
    PlaybackMode m_playbackMode;

    // UI Components
    ButtonGroupWidget* m_transportBtnGroup;
    QPushButton* m_metronomeBtnLbl;
    QPushButton* m_playbackModeBtn;
    QLabel* m_tempoLabel;
    MidiSequenceProgressBar* m_progressBar;
    TrackStereoMeter* m_stereoMeter;

    void initUI();
    void setupConnections();
    void editTempo(QMouseEvent* event);
    void updateCurrentTempo(double bpm);
};
