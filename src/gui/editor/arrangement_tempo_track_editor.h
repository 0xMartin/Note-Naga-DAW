#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QPointer>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>

class ArrangementTimelineWidget;

/**
 * @brief ArrangementTempoTrackEditor provides an interactive visual editor for 
 *        arrangement tempo events. Displays tempo curve with clickable/draggable 
 *        tempo points. Placed above the arrangement timeline.
 */
class ArrangementTempoTrackEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ArrangementTempoTrackEditor(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~ArrangementTempoTrackEditor();

    /**
     * @brief Set the timeline widget to sync with
     */
    void setTimelineWidget(ArrangementTimelineWidget *timeline);

    /**
     * @brief Toggle visibility with animation
     */
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

    /**
     * @brief Get the collapse/expand button for external use
     */
    QPushButton* getToggleButton() const { return m_toggleButton; }

    /**
     * @brief Sync horizontal scroll and zoom with timeline
     */
    void setHorizontalOffset(int offset);
    void setPixelsPerTick(double ppTick);
    
    /**
     * @brief Set header width to match track headers splitter position
     */
    void setHeaderWidth(int width);

    /**
     * @brief Update current playback position
     */
    void setPlayheadTick(int64_t tick);

public slots:
    /**
     * @brief Refresh the tempo display
     */
    void refresh();

    /**
     * @brief Called when tempo events change
     */
    void onTempoEventsChanged();
    
    /**
     * @brief Called when current tempo changes during playback
     */
    void onCurrentTempoChanged(double bpm);
    
    /**
     * @brief Import tempo track from active MIDI sequence
     */
    void importTempoFromSequence();

signals:
    /**
     * @brief Emitted when expanded state changes
     */
    void expandedChanged(bool expanded);

    /**
     * @brief Emitted when a tempo event is modified
     */
    void tempoEventChanged(int tick, double bpm);
    
    /**
     * @brief Emitted when visibility should change (tempo track exists/removed)
     */
    void visibilityChanged(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    // Coordinate conversion
    int tickToX(int tick) const;
    int xToTick(int x) const;
    double bpmToY(double bpm) const;
    double yToBpm(int y) const;

    // Hit testing
    int hitTestTempoEvent(const QPoint &pos) const;

    // Drawing
    void drawBackground(QPainter &painter);
    void drawGrid(QPainter &painter);
    void drawTempoCurve(QPainter &painter);
    void drawTempoPoints(QPainter &painter);
    void drawPlayhead(QPainter &painter);
    void drawCurrentBPM(QPainter &painter);
    void drawHeaderLabel(QPainter &painter);
    void updateActiveIndicator();
    void updateVisibility();

    NoteNagaEngine *m_engine;
    ArrangementTimelineWidget *m_timeline = nullptr;
    
    // View state
    bool m_expanded = true;
    int m_horizontalOffset = 0;
    double m_pixelsPerTick = 0.1;
    int64_t m_playheadTick = 0;
    double m_currentBpm = 120.0;

    // UI controls
    QPushButton *m_toggleButton = nullptr;
    QPushButton *m_importButton = nullptr;
    QLabel *m_bpmLabel = nullptr;
    QLabel *m_activeIndicator = nullptr;

    // BPM range for display
    static constexpr double MIN_BPM = 20.0;
    static constexpr double MAX_BPM = 300.0;
    static constexpr int DEFAULT_HEADER_WIDTH = 164;  // Default, can be changed by setHeaderWidth
    static constexpr int PREFERRED_HEIGHT = 60;
    
    int m_headerWidth = DEFAULT_HEADER_WIDTH;  // Dynamic header width

    // Interaction state
    int m_dragEventIndex = -1;
    bool m_isDragging = false;
    QPoint m_dragStartPos;
    double m_dragStartBpm = 0.0;
    int m_dragStartTick = 0;
    int m_hoveredEventIndex = -1;
};
