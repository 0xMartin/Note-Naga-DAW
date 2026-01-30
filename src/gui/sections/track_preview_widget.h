#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>

class QPushButton;
class QScrollArea;

// Internal canvas that draws the piano roll
class TrackPreviewCanvas : public QWidget {
    Q_OBJECT
public:
    explicit TrackPreviewCanvas(QWidget *parent = nullptr);
    
    void setSequence(NoteNagaMidiSeq *seq);
    void setCurrentTick(int tick);
    void setTimeWindowSeconds(double seconds);
    void setNoteHeight(int height);
    int getNoteHeight() const { return m_noteHeight; }
    double getTimeWindowSeconds() const { return m_timeWindowSeconds; }
    
protected:
    void paintEvent(QPaintEvent *event) override;
    
private:
    NoteNagaMidiSeq *m_sequence;
    int m_currentTick;
    double m_timeWindowSeconds;
    int m_noteHeight;
    int m_lowestNote;
    int m_highestNote;
    double m_pixelsPerTick;
    
    void updateNoteRange();
    void recalculateSize();
    QColor getTrackColor(int trackIndex) const;
};

/**
 * @brief TrackPreviewWidget provides a piano-roll style visualization
 *        with all tracks merged together, showing notes in their track colors.
 *        Displays a time window centered on the current playback position.
 */
class TrackPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrackPreviewWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~TrackPreviewWidget();

    /**
     * @brief Gets the title widget with scale controls for dock title bar
     */
    QWidget *getTitleWidget() const { return m_titleWidget; }

private slots:
    void onSequenceChanged(NoteNagaMidiSeq *seq);
    void onTickChanged(int tick);
    void onPlayingStateChanged(bool playing);
    
    // Scale controls
    void onZoomInTime();
    void onZoomOutTime();
    void onZoomInPitch();
    void onZoomOutPitch();

private:
    NoteNagaEngine *m_engine;
    
    QWidget *m_titleWidget;
    QPushButton *m_btnZoomInTime;
    QPushButton *m_btnZoomOutTime;
    QPushButton *m_btnZoomInPitch;
    QPushButton *m_btnZoomOutPitch;
    
    QScrollArea *m_scrollArea;
    TrackPreviewCanvas *m_canvas;
    
    bool m_isPlaying;
};
