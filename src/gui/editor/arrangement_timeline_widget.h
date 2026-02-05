#ifndef ARRANGEMENT_TIMELINE_WIDGET_H
#define ARRANGEMENT_TIMELINE_WIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QSet>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QLineEdit>

class NoteNagaEngine;
class NoteNagaArrangement;
class NoteNagaArrangementTrack;
struct NN_MidiClip_t;
class ArrangementTimelineRuler;

/**
 * @brief Main timeline widget for Arrangement section
 * 
 * Displays arrangement tracks with MIDI clips on a horizontal timeline.
 * Features an integrated track header column on the left that stays fixed
 * while the clip content area scrolls horizontally (like video editing software).
 * 
 * Supports:
 * - Integrated track headers with name, mute/solo, color
 * - Drag & drop clips from resource panel
 * - Move/resize clips on timeline
 * - Multi-selection with batch operations
 * - Snap to grid
 * - Zoom control
 */
class ArrangementTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArrangementTimelineWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);

    // Track header width
    static constexpr int TRACK_HEADER_WIDTH = 160;

    // Zoom & scroll
    void setPixelsPerTick(double ppTick);
    double getPixelsPerTick() const { return m_pixelsPerTick; }
    void setHorizontalOffset(int offset);
    int getHorizontalOffset() const { return m_horizontalOffset; }
    void setVerticalOffset(int offset);
    int getVerticalOffset() const { return m_verticalOffset; }

    // Snap settings
    void setSnapEnabled(bool enabled);
    bool isSnapEnabled() const { return m_snapEnabled; }
    void setSnapResolution(int ticksPerSnap);
    int getSnapResolution() const { return m_snapResolution; }

    // Track dimensions
    void setTrackHeight(int height);
    int getTrackHeight() const { return m_trackHeight; }

    // Playhead
    void setPlayheadTick(int64_t tick);
    int64_t getPlayheadTick() const { return m_playheadTick; }

    // Selection
    QSet<int> getSelectedClipIds() const { return m_selectedClipIds; }
    void clearSelection();
    void selectClip(int clipId, bool addToSelection = false);
    void deleteSelectedClips();

    // Refresh from model
    void refreshFromArrangement();

    // Ruler synchronization
    void setRuler(ArrangementTimelineRuler *ruler);
    
    // Content rect (excluding track headers)
    QRect contentRect() const;

signals:
    void clipSelected(NN_MidiClip_t *clip);
    void clipMoved(int clipId, int64_t newStartTick);
    void clipResized(int clipId, int64_t newDuration);
    void clipDropped(int trackIndex, int64_t tick, int midiSequenceIndex);
    void selectionChanged();
    void seekRequested(int64_t tick);
    void zoomChanged(double ppTick);
    void horizontalOffsetChanged(int offset);
    void trackSelected(int trackIndex);
    void trackMuteToggled(int trackIndex);
    void trackSoloToggled(int trackIndex);

public slots:
    void onSequenceDropped(int midiSequenceIndex, const QPoint &pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Coordinate conversion (now offset by track header width)
    int64_t xToTick(int x) const;
    int tickToX(int64_t tick) const;
    int yToTrackIndex(int y) const;
    int trackIndexToY(int trackIndex) const;
    
    // Check if position is in track header area
    bool isInHeaderArea(const QPoint &pos) const;
    int headerTrackAtY(int y) const;
    
    // Header button hit testing
    enum HeaderButton { NoButton, MuteButton, SoloButton, ColorButton };
    HeaderButton headerButtonAtPos(const QPoint &pos, int &outTrackIndex) const;
    
    // Snapping
    int64_t snapTick(int64_t tick) const;
    
    // Hit testing
    NN_MidiClip_t* clipAtPosition(const QPoint &pos, int &outTrackIndex);
    enum HitZone { NoHit, BodyHit, LeftEdgeHit, RightEdgeHit };
    HitZone hitTestClip(NN_MidiClip_t *clip, int trackIndex, const QPoint &pos);
    
    // Drawing helpers
    void drawTrackHeaders(QPainter &painter);
    void drawTrackLanes(QPainter &painter);
    void drawClips(QPainter &painter);
    void drawPlayhead(QPainter &painter);
    void drawSelectionRect(QPainter &painter);
    void drawDropPreview(QPainter &painter);
    
    // Context menus
    void showTrackContextMenu(int trackIndex, const QPoint &globalPos);
    void showEmptyAreaContextMenu(const QPoint &globalPos);

    NoteNagaEngine *m_engine;
    ArrangementTimelineRuler *m_ruler = nullptr;
    
    // View parameters
    double m_pixelsPerTick = 0.1;
    int m_horizontalOffset = 0;
    int m_verticalOffset = 0;
    int m_trackHeight = 60;
    
    // Snap settings
    bool m_snapEnabled = true;
    int m_snapResolution = 480; // Quarter note at 480 PPQ
    
    // Playhead
    int64_t m_playheadTick = 0;
    
    // Selection
    QSet<int> m_selectedClipIds;
    int m_selectedTrackIndex = -1;
    
    // Interaction state
    enum InteractionMode { None, Selecting, MovingClip, ResizingClipLeft, ResizingClipRight };
    InteractionMode m_interactionMode = None;
    QPoint m_dragStartPos;
    int64_t m_dragStartTick = 0;
    int m_dragClipId = -1;
    int m_dragTrackIndex = -1;
    int64_t m_originalClipStart = 0;
    int64_t m_originalClipDuration = 0;
    QRect m_selectionRect;
    
    // Drop preview
    bool m_showDropPreview = false;
    int m_dropPreviewTrack = -1;
    int64_t m_dropPreviewTick = 0;
    int64_t m_dropPreviewDuration = 0;
    
    // Inline track name editing
    QLineEdit *m_trackNameEdit = nullptr;
    int m_editingTrackIndex = -1;
    
    void startTrackNameEdit(int trackIndex);
    void finishTrackNameEdit();
    void cancelTrackNameEdit();
};

#endif // ARRANGEMENT_TIMELINE_WIDGET_H
