#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QMenu>
#include <QPointer>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>

class MidiEditorWidget;

/**
 * @brief TempoTrackEditor provides an interactive visual editor for tempo events.
 *        Displays tempo curve with clickable/draggable tempo points.
 *        Allows adding, editing, and removing tempo changes.
 */
class TempoTrackEditor : public QWidget
{
    Q_OBJECT

public:
    explicit TempoTrackEditor(NoteNagaEngine *engine, MidiEditorWidget *midiEditor, QWidget *parent = nullptr);
    ~TempoTrackEditor();

    /**
     * @brief Set the tempo track to edit
     */
    void setTempoTrack(NoteNagaTrack *track);
    NoteNagaTrack* tempoTrack() const { return m_tempoTrack.data(); }

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
     * @brief Sync horizontal scroll with main MIDI editor
     */
    void setHorizontalScroll(int value);

    /**
     * @brief Sync time scale with main MIDI editor
     */
    void setTimeScale(double scale);

    /**
     * @brief Update current playback position
     */
    void setCurrentTick(int tick);

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

signals:
    /**
     * @brief Emitted when expanded state changes
     */
    void expandedChanged(bool expanded);

    /**
     * @brief Emitted when a tempo event is modified
     */
    void tempoEventChanged(int tick, double bpm);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    NoteNagaEngine *m_engine;
    MidiEditorWidget *m_midiEditor;
    QPointer<NoteNagaTrack> m_tempoTrack;
    
    // UI Components
    QPushButton *m_toggleButton;
    QLabel *m_valueLabel;
    QLabel *m_titleLabel;
    
    // State
    bool m_expanded;
    double m_timeScale;
    int m_horizontalScroll;
    int m_leftMargin;
    int m_currentTick;  ///< Current playback position in ticks
    double m_currentDisplayBPM;  ///< Current BPM for realtime display
    
    // Tempo range (configurable)
    double m_minBPM = 20.0;
    double m_maxBPM = 300.0;
    
    // Editing state
    bool m_isDragging;
    QPoint m_dragStartPos;
    int m_draggedEventIndex;
    double m_dragStartBPM;
    int m_dragStartTick;
    
    // Tempo point data cache for rendering
    struct TempoPoint {
        int x;              // Screen X position
        int y;              // Screen Y position
        int tick;
        double bpm;
        TempoInterpolation interpolation;
        bool hovered;
        bool selected;
        int eventIndex;
    };
    std::vector<TempoPoint> m_tempoPoints;
    TempoPoint *m_hoveredPoint;
    TempoPoint *m_contextMenuPoint;
    
    // Colors
    QColor m_backgroundColor;
    QColor m_gridColor;
    QColor m_curveColor;
    QColor m_pointColor;
    QColor m_pointHoverColor;
    QColor m_pointSelectedColor;
    QColor m_textColor;
    
    void setupUI();
    void rebuildTempoPoints();
    void drawGrid(QPainter &p);
    void drawTempoCurve(QPainter &p);
    void drawTempoPoints(QPainter &p);
    TempoPoint* hitTest(const QPoint &pos);
    
    int tickFromX(int x) const;
    int xFromTick(int tick) const;
    double bpmFromY(int y) const;
    int yFromBPM(double bpm) const;
    double getBPMAtTick(int tick) const;
    
    // Context menu handlers
    void showAddTempoDialog(int tick);
    void showEditTempoDialog(TempoPoint *point);
    void deleteTempoPoint(TempoPoint *point);
    void toggleInterpolation(TempoPoint *point);
    void showTempoRangeDialog();
    void setAllInterpolation(TempoInterpolation interpolation);
    void resetTempoPoints();
};
