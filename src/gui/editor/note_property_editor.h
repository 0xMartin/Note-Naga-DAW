#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QContextMenuEvent>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>

class MidiEditorWidget;

/**
 * @brief NotePropertyEditor provides an interactive visual editor for note properties.
 *        Displays velocity, pan and other properties as editable bars/lanes.
 *        Placed below the main MIDI editor with splitter support.
 */
class NotePropertyEditor : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Property types that can be edited
     */
    enum class PropertyType {
        Velocity,
        Pan,
        // Future: Pitch bend, CC, etc.
    };

    explicit NotePropertyEditor(NoteNagaEngine *engine, MidiEditorWidget *midiEditor, QWidget *parent = nullptr);
    ~NotePropertyEditor();

    /**
     * @brief Set the active property type to display/edit
     */
    void setPropertyType(PropertyType type);
    PropertyType propertyType() const { return m_propertyType; }

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
     * @brief Called when selection changes in MIDI editor
     */
    void onSelectionChanged();

    /**
     * @brief Called when notes are modified
     */
    void onNotesChanged();
    
    /**
     * @brief Called when the active track changes in the sequence
     */
    void onActiveTrackChanged(NoteNagaTrack *track);
    
    /**
     * @brief Called when the active sequence changes
     */
    void onSequenceChanged(NoteNagaMidiSeq *seq);
    
    /**
     * @brief Called when track metadata (color, visibility, name) changes
     */
    void onTrackMetadataChanged(NoteNagaTrack *track, const std::string &param);

signals:
    /**
     * @brief Emitted when a note property is modified (during drag - for data sync)
     */
    void notePropertyChanged(NoteNagaTrack *track, int noteIndex, int newValue);
    
    /**
     * @brief Emitted when note property editing is complete (on mouse release - for UI refresh)
     */
    void notePropertyEditFinished(NoteNagaTrack *track);

    /**
     * @brief Emitted when expanded state changes
     */
    void expandedChanged(bool expanded);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    NoteNagaEngine *m_engine;
    MidiEditorWidget *m_midiEditor;
    
    // UI Components
    QPushButton *m_toggleButton;
    QPushButton *m_velocityButton;
    QPushButton *m_panButton;
    QLabel *m_valueLabel;
    QLabel *m_trackNameLabel;
    
    // Active track
    NoteNagaTrack *m_activeTrack;
    QColor m_trackColor;
    
    // State
    PropertyType m_propertyType;
    bool m_expanded;
    double m_timeScale;
    int m_horizontalScroll;
    int m_leftMargin;  // Space for property selector buttons
    int m_currentTick;  // Current playback position in ticks
    
    // Editing state
    bool m_isDragging;
    QPoint m_dragStartPos;
    int m_dragStartValue;
    bool m_hasSelection;  // True if any notes are selected in MIDI editor
    bool m_isSnapping;    // True when value is snapped to neighbor/common value
    int m_snapValue;      // The value being snapped to (-1 if not snapping)
    bool m_proportionalEdit;  // True when editing multiple selected notes proportionally (Shift held)
    std::vector<std::pair<int, int>> m_selectedBarsStartValues;  // Index in m_noteBars -> original value
    
    // Note data cache for rendering
    struct NoteBar {
        int x;
        int width;
        int value;       // 0-127
        bool selected;
        NoteNagaTrack *track;
        int noteIndex;
        NN_Note_t note;
    };
    std::vector<NoteBar> m_noteBars;
    NoteBar *m_hoveredBar;
    NoteBar *m_editingBar;
    NoteBar *m_contextMenuBar;  // Bar being targeted by context menu
    
    // Drawing helpers
    void rebuildNoteBars();
    void drawVelocityLane(QPainter &p);
    void drawPanLane(QPainter &p);
    void drawGridLines(QPainter &p);
    NoteBar* hitTest(const QPoint &pos);
    int valueFromY(int y) const;
    int yFromValue(int value) const;
    
    // Colors
    QColor m_backgroundColor;
    QColor m_gridColor;
    QColor m_barColor;
    QColor m_barSelectedColor;
    QColor m_barHoverColor;
    QColor m_textColor;
    
    void setupUI();
    void updatePropertyButtons();
    void updateActiveTrack();
    void updateTrackColorStyles();
    
    // Context menu handlers
    void onSetValueTriggered();
    void onSnapToPreviousTriggered();
    void onSnapToNextTriggered();
    void onSnapToAverageTriggered();
    int findNeighborValue(NoteBar *bar, int direction);
    void applyValueToContextBar(int value);
};
