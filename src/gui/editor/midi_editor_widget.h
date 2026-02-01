#pragma once

#include <note_naga_engine/note_naga_engine.h>
#include <QColor>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QPushButton>
#include <QMap>
#include <QRubberBand>
#include <QComboBox>
#include <QLabel>
#include <QSet>
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
    bool looping;
    MidiEditorFollowMode follow_mode;
};

/** Note duration values for the note size combo box */
enum class NoteDuration {
    Whole,      // 1/1
    Half,       // 1/2
    Quarter,    // 1/4
    Eighth,     // 1/8
    Sixteenth,  // 1/16
    ThirtySecond // 1/32
};

/** Snap/grid resolution for note quantization */
enum class GridResolution {
    Whole,      // 1/1
    Half,       // 1/2
    Quarter,    // 1/4
    Eighth,     // 1/8
    Sixteenth,  // 1/16
    ThirtySecond, // 1/32
    Off         // No snap
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
    ~MidiEditorWidget();

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

    /**
     * @brief Gets the currently selected notes with their track pointers.
     * @return Vector of pairs containing track pointer and note data.
     */
    std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> getSelectedNotes() const;
    
    /**
     * @brief Checks if there are any selected notes.
     * @return True if at least one note is selected.
     */
    bool hasSelection() const { return !selectedNotes.isEmpty(); }

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
    QColor selection_color;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    NoteNagaEngine *engine;
    NoteNagaMidiSeq *last_seq = nullptr; /// Last sequence being edited

    MidiEditorConfig config; /// Configuration for the MIDI editor
    int content_width;  /// Width of the content area
    int content_height; /// Height of the content area

    // --- Note selection and manipulation ---
    struct NoteGraphics {
        QGraphicsItem *item;                // Grafický objekt noty
        QGraphicsSimpleTextItem *label;     // Textový popisek noty
        NN_Note_t note;                     // Data noty
        NoteNagaTrack *track;               // Stopa, ke které nota patří
    };
    QMap<int, std::vector<NoteGraphics>> note_items;  // Všechny grafické noty
    QList<NoteGraphics*> selectedNotes;               // Vybrané noty
    
    // Drag state
    enum class DragMode {
        None,       // Žádná operace tažení
        Select,     // Výběr pomocí rubber band
        Move,       // Přesun noty
        Resize      // Změna velikosti noty
    };
    
    DragMode dragMode = DragMode::None;
    QPointF dragStartPos;    // Pozice, kde začalo tažení
    QPointF lastDragPos;     // Poslední pozice při tažení
    QRubberBand *rubberBand = nullptr;  // Obdélník pro výběr více not
    QPoint rubberBandOrigin; // Počáteční bod rubber bandu
    
    // Edge detection constants
    const int resizeEdgeMargin = 5; // pixely od okraje pro detekci změny velikosti
    
    // --- Active notes tracking (for row highlighting) ---
    QMap<int, int> m_activeNotes; // note -> trackIndex for active notes
    QMap<int, int> m_lastActiveNotes; // previous state for comparison

    // --- UI controls ---
    QWidget *title_widget;
    QPushButton *btn_follow_center;
    QPushButton *btn_follow_left;
    QPushButton *btn_follow_step;
    QPushButton *btn_follow_none;
    QPushButton *btn_looping;
    QComboBox *combo_note_duration;
    QComboBox *combo_grid_resolution;

    // --- Graphics scene and items ---
    QGraphicsScene *scene;

    QGraphicsLineItem *marker_line = nullptr;
    std::vector<QGraphicsLineItem *> grid_lines;
    std::vector<QGraphicsLineItem *> bar_grid_lines;
    std::vector<QGraphicsSimpleTextItem *> bar_grid_labels;
    std::vector<QGraphicsRectItem *> row_backgrounds; // Row background items for highlighting

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
    void updateActiveNotes();
    void updateRowHighlights();
    
    // --- Note Editing Functions ---
    void selectNote(NoteGraphics *noteGraphics, bool clearPrevious = true);
    void deselectNote(NoteGraphics *noteGraphics);
    void clearSelection();
    void selectNotesInRect(const QRectF &rect);
    
    NoteGraphics* findNoteUnderCursor(const QPointF &scenePos);
    bool isNoteEdge(NoteGraphics *note, const QPointF &scenePos);
    
    void addNewNote(const QPointF &scenePos);
    void moveSelectedNotes(const QPointF &delta);
    void resizeSelectedNotes(const QPointF &delta);
    void applyNoteChanges();
    
    // --- Coordinate conversion helpers ---
    QMap<NoteGraphics*, NN_Note_t> dragStartNoteStates; 
    int sceneXToTick(qreal x) const;
    int sceneYToNote(qreal y) const;
    qreal tickToSceneX(int tick) const;
    qreal noteToSceneY(int note) const;
    QRectF getRealNoteRect(const NoteGraphics *ng) const;
    int snapTickToGrid(int tick) const;
    int snapTickToGridNearest(int tick) const;
    int getGridStepTicks() const;
    QPen getNotePen(const NoteNagaTrack *track, bool is_active_track, bool is_selected_note) const;

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

    /**
     * @brief Signal emitted when the looping state changes.
     * @param enabled True if looping is enabled, false otherwise.
     */
    void loopingChanged(bool enabled);

    /**
     * @brief Signal emitted when notes are modified.
     */
    void notesModified();

    /**
     * @brief Signal emitted when note selection changes.
     */
    void selectionChanged();

public slots:
    /**
     * @brief Sets the current time scale for the MIDI editor.
     * @param scale The new time scale.
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
    void enableLooping(bool enabled);
};