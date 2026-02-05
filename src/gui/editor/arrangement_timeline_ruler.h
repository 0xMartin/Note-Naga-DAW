#ifndef ARRANGEMENT_TIMELINE_RULER_H
#define ARRANGEMENT_TIMELINE_RULER_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class NoteNagaEngine;

/**
 * @brief Time ruler for the Arrangement timeline
 * 
 * Displays time markers in bars:beats format or seconds/minutes.
 * Supports click-to-seek and time signature display.
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

    // Preferred height
    QSize sizeHint() const override { return QSize(100, 28); }
    int heightForWidth(int) const override { return 28; }

signals:
    void seekRequested(int64_t tick);
    void zoomChanged(double ppTick);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int64_t xToTick(int x) const;
    int tickToX(int64_t tick) const;
    QString formatTickLabel(int64_t tick) const;
    int64_t getTicksPerBar() const;
    int64_t getTicksPerBeat() const;

    NoteNagaEngine *m_engine;
    
    double m_pixelsPerTick = 0.1;      // Zoom level
    int m_horizontalOffset = 0;         // Scroll offset in pixels
    int64_t m_playheadTick = 0;
    
    TimeFormat m_timeFormat = BarsBeats;
    int m_timeSignatureNumerator = 4;
    int m_timeSignatureDenominator = 4;
    int m_ppq = 480;  // Pulses per quarter note
    
    bool m_isDragging = false;
    
    // Hover state for click-to-seek visual feedback
    bool m_isHovered = false;
    int m_hoverX = -1;
};

#endif // ARRANGEMENT_TIMELINE_RULER_H
