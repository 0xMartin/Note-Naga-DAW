#pragma once

#include <QColor>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QPushButton>
#include <QMap>
#include <memory>
#include <note_naga_engine/note_naga_engine.h>
#include <vector>

/** MIDI editor follow modes */
enum class MidiEditorFollowMode {
    None,
    LeftSideIsCurrent,
    CenterIsCurrent,
    StepByStep
};

/** Configuration for the MIDI editor */
struct MidiEditorConfig {
    double time_scale; 
    int key_height;    
    int tact_subdiv;
    MidiEditorFollowMode follow_mode;
};

/**
 * @brief The MidiEditorWidget class provides a graphical interface for editing MIDI
 * sequences. It allows users to visualize and manipulate MIDI notes, tracks, and
 * sequences.
 */
class MidiEditorWidget : public QGraphicsView {
    Q_OBJECT
public:
    /**
     * @brief Constructs a MidiEditorWidget for editing MIDI sequences.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param parent Parent widget.
     */
    explicit MidiEditorWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);

    /**
     * @brief Gets the title widget that will be inserted into the dock title bar.
     * @return Pointer to the title widget.
     */
    QWidget *getTitleWidget() const { return this->title_widget; }

    /**
     * @brief Gets the current configuration of the MIDI editor.
     * @return Reference to the MidiEditorConfig.
     */
    MidiEditorConfig *getConfig() { return &config; }

    // Size hints for the widget
    QSize sizeHint() const override { return QSize(content_width, content_height); }
    QSize minimumSizeHint() const override { return QSize(320, 100); }

    // Colors
    QColor bg_color;
    QColor fg_color;
    QColor line_color;
    QColor subline_color;
    QColor grid_bar_color;
    QColor grid_row_color1;
    QColor grid_row_color2;
    QColor grid_bar_label_color;
    QColor grid_subdiv_color;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    NoteNagaEngine *engine;
    NoteNagaMidiSeq *last_seq = nullptr; /// Last sequence being edited

    MidiEditorConfig config; /// Configuration for the MIDI editor
    int content_width;  /// Width of the content area
    int content_height; /// Height of the content area

    // --- UI controls ---
    QWidget *title_widget;
    QPushButton *btn_follow_center;
    QPushButton *btn_follow_left;
    QPushButton *btn_follow_step;
    QPushButton *btn_follow_none;

    // --- Graphics scene and items ---
    QGraphicsScene *scene;

    struct NoteGraphics {
        QGraphicsItem *item;
        QGraphicsSimpleTextItem *label;
    };
    QMap<int, std::vector<NoteGraphics>> note_items;

    QGraphicsLineItem *marker_line = nullptr;
    std::vector<QGraphicsLineItem *> grid_lines;
    std::vector<QGraphicsLineItem *> bar_grid_lines;
    std::vector<QGraphicsSimpleTextItem *> bar_grid_labels;

    // --- Setup & Helpers ---
    void initTitleUI();
    void setupConnections();

    // --- UI/Scene Update ---
    void recalculateContentSize();
    void updateScene();
    void updateGrid();
    void updateBarGrid();
    void updateAllNotes();
    void updateTrackNotes(NoteNagaTrack *track);

    void drawNote(const NN_Note_t &note, const NoteNagaTrack *track, bool is_selected,
                  bool is_drum, int x, int y, int w, int h);

    void clearScene();
    void clearNotes();
    void clearTrackNotes(int track_id);

/*******************************************************************************************************/
// Signal and Slots
/*******************************************************************************************************/

signals:
    /**
     * @brief Signal emitted when position is selected in editor.
     * @param tick The selected position in ticks.
     */
    void positionSelected(int tick);

    /**
     * @brief Signal emitted when the current tick position changes.
     * @param current_tick The new current tick position.
     */
    void horizontalScrollChanged(int value);

    /**
     * @brief Signal emitted when the vertical scroll position changes.
     * @param value The new vertical scroll position.
     */
    void verticalScrollChanged(int value);

    /**
     * @brief Signal emitted when the follow mode changes.
     * @param mode The new follow mode.
     */
    void followModeChanged(MidiEditorFollowMode mode);

    /**
     * @brief Signal emitted when the time scale changes.
     * @param scale The new time scale.
     */
    void timeScaleChanged(double scale);

    /**
     * @brief Signal emitted when the key height changes.
     * @param height The new key height in pixels.
     */
    void keyHeightChanged(int height);

public slots:
    /**
     * @brief Sets the current MIDI sequence to edit.
     * @param seq Pointer to the NoteNagaMidiSeq to edit.
     */
    void setTimeScale(double scale);

    /**
     * @brief Sets the height of each key in pixels.
     * @param h Height in pixels.
     */
    void setKeyHeight(int h);

private slots:
    // Refreshes the UI elements based on the current state of the MIDI sequence
    void refreshAll();
    void refreshMarker();
    void refreshTrack(NoteNagaTrack *track);
    void refreshSequence(NoteNagaMidiSeq *seq);
    void currentTickChanged(int tick);

    // Control methods
    void selectFollowMode(MidiEditorFollowMode mode);
};