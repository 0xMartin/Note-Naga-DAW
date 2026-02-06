#ifndef ARRANGEMENT_MINIMAP_WIDGET_H
#define ARRANGEMENT_MINIMAP_WIDGET_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class NoteNagaEngine;
class NoteNagaArrangement;

/**
 * @brief Mini-map widget showing an overview of the entire arrangement
 * 
 * Displays a compressed view of all tracks and clips with:
 * - Visible area indicator (rectangle)
 * - Playhead position
 * - Loop region
 * - Click-to-navigate functionality
 */
class ArrangementMinimapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArrangementMinimapWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);

    // Synchronization with main timeline
    void setHorizontalOffset(int offset);
    void setVisibleTickRange(int64_t startTick, int64_t endTick);
    void setPlayheadTick(int64_t tick);
    void setLoopRegion(int64_t startTick, int64_t endTick, bool enabled);
    
    // Dimensions
    void setTotalTicks(int64_t ticks);
    
    // Size hints
    QSize sizeHint() const override { return QSize(100, 40); }
    QSize minimumSizeHint() const override { return QSize(50, 30); }

signals:
    void seekRequested(int64_t tick);
    void visibleRangeChangeRequested(int64_t startTick);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    int64_t xToTick(int x) const;
    int tickToX(int64_t tick) const;
    
    NoteNagaEngine *m_engine;
    
    int64_t m_totalTicks = 0;
    int64_t m_visibleStartTick = 0;
    int64_t m_visibleEndTick = 0;
    int64_t m_playheadTick = 0;
    
    // Loop region
    int64_t m_loopStartTick = 0;
    int64_t m_loopEndTick = 0;
    bool m_loopEnabled = false;
    
    // Interaction
    bool m_isDragging = false;
    int m_dragStartX = 0;
    int64_t m_dragStartTick = 0;
};

#endif // ARRANGEMENT_MINIMAP_WIDGET_H
