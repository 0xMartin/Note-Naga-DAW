#ifndef ARRANGEMENT_TIMELINE_RULER_H
#define ARRANGEMENT_TIMELINE_RULER_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class NoteNagaEngine;
class QContextMenuEvent;

/**
 * @brief Time ruler for the Arrangement timeline
 * 
 * Displays time markers in bars:beats format or seconds/minutes.
 * Supports click-to-seek, loop region, and time signature display.
 * Now a standalone widget that aligns with ArrangementTimelineWidget.
 */
class ArrangementTimelineRuler : public QWidget
{
    Q_OBJECT

public:
    enum TimeFormat {
        BarsBeats,
        Seconds
    };

    explicit ArrangementTimelineRuler(NoteNagaEngine *engine, QWidget *parent = nullptr);

    // Zoom & scroll
    void setPixelsPerTick(double ppTick);
    double getPixelsPerTick() const { return m_pixelsPerTick; }
    void setHorizontalOffset(int offset);
    int getHorizontalOffset() const { return m_horizontalOffset; }

    // Playhead position
    void setPlayheadTick(int64_t tick);
    int64_t getPlayheadTick() const { return m_playheadTick; }

    // Time format
    void setTimeFormat(TimeFormat format);
    TimeFormat getTimeFormat() const { return m_timeFormat; }

    // Time signature
    void setTimeSignature(int numerator, int denominator);
    
    // Loop region
    void setLoopRegion(int64_t startTick, int64_t endTick);
    void setLoopEnabled(bool enabled);
    bool isLoopEnabled() const { return m_loopEnabled; }
    int64_t getLoopStartTick() const { return m_loopStartTick; }
    int64_t getLoopEndTick() const { return m_loopEndTick; }
    
    // Grid info for timeline
    int64_t getTicksPerBar() const;
    int64_t getTicksPerBeat() const;

    // Preferred height
    QSize sizeHint() const override { return QSize(100, 28); }
    int heightForWidth(int) const override { return 28; }

signals:
    void seekRequested(int64_t tick);
    void zoomChanged(double ppTick);
    void loopRegionChanged(int64_t startTick, int64_t endTick);
    void loopEnabledChanged(bool enabled);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    int64_t xToTick(int x) const;
    int tickToX(int64_t tick) const;
    QString formatTickLabel(int64_t tick) const;

    NoteNagaEngine *m_engine;
    
    double m_pixelsPerTick = 0.1;      // Zoom level
    int m_horizontalOffset = 0;         // Scroll offset in pixels
    int64_t m_playheadTick = 0;
    
    TimeFormat m_timeFormat = BarsBeats;
    int m_timeSignatureNumerator = 4;
    int m_timeSignatureDenominator = 4;
    int m_ppq = 480;  // Pulses per quarter note
    
    // Loop region
    int64_t m_loopStartTick = 0;
    int64_t m_loopEndTick = 0;
    bool m_loopEnabled = false;
    
    bool m_isDragging = false;
    
    // Loop region dragging
    enum DragMode { DragNone, DragSeek, DragLoopStart, DragLoopEnd, DragLoopBody };
    DragMode m_dragMode = DragNone;
    int64_t m_dragStartLoopStart = 0;
    int64_t m_dragStartLoopEnd = 0;
    int m_dragStartX = 0;
    
    // Hover state for click-to-seek visual feedback
    bool m_isHovered = false;
    int m_hoverX = -1;
};

#endif // ARRANGEMENT_TIMELINE_RULER_H
