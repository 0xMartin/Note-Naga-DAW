#pragma once

#include <QWidget>
#include <QColor>

class NoteNagaEngine;
class NoteNagaMidiSeq;
class NoteNagaTrack;

/**
 * @brief The TimelineOverviewWidget provides a minimap-style overview of a MIDI track.
 * It displays a simplified view of where notes exist in the timeline, along with
 * markers for the playback position and current viewport.
 */
class TimelineOverviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineOverviewWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~TimelineOverviewWidget() = default;

    QSize sizeHint() const override { return QSize(400, 24); }
    QSize minimumSizeHint() const override { return QSize(100, 16); }

    /**
     * @brief Set the viewport range (visible portion of the MIDI editor)
     * @param startTick Start tick of the viewport
     * @param endTick End tick of the viewport
     */
    void setViewportRange(int startTick, int endTick);

    /**
     * @brief Set the time scale for converting viewport coordinates
     * @param scale The time scale factor
     */
    void setTimeScale(double scale);

public slots:
    void refresh();
    void onPlaybackPositionChanged(int tick);
    void onActiveTrackChanged(NoteNagaTrack *track);
    void onSequenceChanged(NoteNagaMidiSeq *seq);

signals:
    /**
     * @brief Emitted when user clicks to seek to a position
     * @param tick The tick position to seek to
     */
    void seekRequested(int tick);

    /**
     * @brief Emitted when user clicks to navigate viewport
     * @param tick The center tick for the new viewport
     */
    void viewportNavigationRequested(int tick);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    NoteNagaTrack *m_activeTrack;

    int m_playbackTick;
    int m_viewportStartTick;
    int m_viewportEndTick;
    int m_maxTick;
    double m_timeScale;

    bool m_isDragging;
    bool m_dragViewport;  // true = dragging viewport, false = seeking

    // Colors
    QColor m_backgroundColor;
    QColor m_borderColor;
    QColor m_noteBlockColor;
    QColor m_playbackMarkerColor;
    QColor m_viewportColor;
    QColor m_startEndMarkerColor;

    // Helper methods
    int tickToX(int tick) const;
    int xToTick(int x) const;
    void updateMaxTick();
    void connectSignals();
};
