#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QFuture>
#include <QFutureWatcher>
#include <QTimer>
#include <vector>
#include <QImage>
#include <atomic>

#include <note_naga_engine/core/types.h>

/**
 * @brief A progress bar widget for displaying MIDI sequence playback progress.
 */
class MidiSequenceProgressBar : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor for MidiSequenceProgressBar.
     * @param parent Parent widget.
     */
    explicit MidiSequenceProgressBar(QWidget *parent = nullptr);
    
    /**
     * @brief Destructor.
     */
    ~MidiSequenceProgressBar();

    /**
     * @brief Sets the MIDI sequence to be displayed in the progress bar.
     * @param seq Pointer to the NoteNagaMidiSeq object representing the MIDI sequence.
     */
    void setMidiSequence(NoteNagaMidiSeq *seq);

    /**
     * @brief Schedules an async refresh of the waveform display.
     *        Multiple calls are debounced to avoid excessive recomputation.
     */
    void scheduleWaveformRefresh();

    /**
     * @brief Updates the progress bar with the current playback position.
     * @param current_time Current playback time in seconds.
     */
    void updateMaxTime();

    /**
     * @brief Sets the current playback time in seconds.
     * @param seconds Current playback time in seconds.
     */
    void setCurrentTime(float seconds);

    float getCurrentTime() const { return current_time; }
    float getTotalTime() const { return total_time; }

    // Colors
    QColor bar_bg;
    QColor box_bg;
    QColor outline_color;
    QColor waveform_fg;
    QColor waveform_fg_active;
    QColor position_indicator_color;

signals:
    void positionPressed(float seconds);
    void positionDragged(float seconds);
    void positionReleased(float seconds);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;
    QString formatTime(float seconds) const;

private slots:
    void onWaveformComputeFinished();

private:
    void refreshWaveform();          // Immediate sync refresh (for setMidiSequence)
    void computeWaveformAsync();
    std::vector<float> computeWaveformData() const;

    NoteNagaMidiSeq *midi_seq;
    float current_time;
    float total_time;

    std::vector<float> waveform;
    int waveform_resolution;

    QImage waveform_img;
    int waveform_img_width;
    int waveform_img_height;

    // Async waveform computation
    QTimer* m_refreshDebounceTimer;
    QFutureWatcher<std::vector<float>>* m_waveformWatcher;
    std::atomic<bool> m_computePending;

    float mapMouseEventToTime(QMouseEvent *event) const;
};