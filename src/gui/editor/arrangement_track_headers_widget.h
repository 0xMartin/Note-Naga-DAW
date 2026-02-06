#ifndef ARRANGEMENT_TRACK_HEADERS_WIDGET_H
#define ARRANGEMENT_TRACK_HEADERS_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QList>

class NoteNagaEngine;
class NoteNagaArrangementTrack;
class ArrangementTrackHeaderWidget;
class TrackStereoMeter;

/**
 * @brief Container widget for all track headers in the arrangement view.
 * Provides a vertically scrollable list of track header widgets.
 * Supports drag-to-reorder tracks.
 */
class ArrangementTrackHeadersWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArrangementTrackHeadersWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);
    
    void setEngine(NoteNagaEngine *engine);
    NoteNagaEngine* getEngine() const { return m_engine; }
    
    void refreshFromArrangement();
    void setVerticalOffset(int offset);
    int getVerticalOffset() const { return m_verticalOffset; }
    
    void setTrackHeight(int height);
    int getTrackHeight() const { return m_trackHeight; }
    
    void setSelectedTrack(int trackIndex);
    int getSelectedTrack() const { return m_selectedTrackIndex; }
    
    void updateTrackMeters();
    
    // Get total content height
    int contentHeight() const;

signals:
    void trackMuteToggled(int trackIndex);
    void trackSoloToggled(int trackIndex);
    void trackColorChangeRequested(int trackIndex);
    void trackSelected(int trackIndex);
    void trackNameChanged(int trackIndex, const QString &newName);
    void deleteTrackRequested(int trackIndex);
    void addTrackRequested();
    void tracksReordered(int fromIndex, int toIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    NoteNagaEngine *m_engine = nullptr;
    QList<ArrangementTrackHeaderWidget*> m_headerWidgets;
    
    int m_trackHeight = 80;
    int m_verticalOffset = 0;
    int m_selectedTrackIndex = -1;
    
    // Drag-to-reorder state
    bool m_isDraggingTrack = false;
    int m_dragSourceIndex = -1;
    int m_dragTargetIndex = -1;
    QPoint m_dragStartPos;
    int m_dragOffsetY = 0;
    
    void updateHeaderPositions();
    void clearHeaders();
    int trackIndexAtY(int y) const;
};

#endif // ARRANGEMENT_TRACK_HEADERS_WIDGET_H
