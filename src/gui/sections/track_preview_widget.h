#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMenu>
#include <QMap>
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
    
    // Display options
    void setShowGrid(bool show);
    void setShowPianoKeys(bool show);
    void setColorMode(int mode);  // 0=track, 1=velocity, 2=pitch
    void resetZoom();
    
    bool showGrid() const { return m_showGrid; }
    bool showPianoKeys() const { return m_showPianoKeys; }
    int colorMode() const { return m_colorMode; }
    
    // Viewport info for optimized rendering
    void setViewportRect(const QRect &rect);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    
private:
    NoteNagaMidiSeq *m_sequence;
    int m_currentTick;
    double m_timeWindowSeconds;
    int m_noteHeight;
    int m_lowestNote;
    int m_highestNote;
    double m_pixelsPerTick;
    
    // Display options
    bool m_showGrid;
    bool m_showPianoKeys;
    int m_colorMode;  // 0=track, 1=velocity, 2=pitch
    
    // Viewport for optimized rendering
    QRect m_viewportRect;
    
    // Active notes (currently playing): note -> trackIndex
    QMap<int, int> m_activeNotes;
    
    void updateNoteRange();
    void updateActiveNotes();
    void recalculateSize();
    QColor getTrackColor(int trackIndex) const;
    QColor getNoteColor(int trackIndex, int midiNote, int velocity) const;
    void drawPianoKeys(QPainter &p, int pianoKeyWidth, int viewTop, int viewBottom);
    void drawGrid(QPainter &p, int startTick, int endTick, int ppq, int offsetX, int viewTop, int viewBottom);
    
signals:
    void optionsChanged();
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
    void updateViewportRect();
    
    // Scale controls
    void onZoomInTime();
    void onZoomOutTime();
    void onZoomInPitch();
    void onZoomOutPitch();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

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
