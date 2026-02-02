#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QPushButton>
#include <QComboBox>
#include <QRubberBand>
#include <QLabel>
#include <vector>

#include "midi_editor_types.h"

class NoteNagaEngine;
class NoteNagaMidiSeq;
class NoteNagaTrack;
class MidiEditorContextMenu;
class MidiEditorNoteHandler;

/**
 * @brief The MidiEditorWidget class provides a graphical interface for editing MIDI
 * sequences. It allows users to visualize and manipulate MIDI notes, tracks, and
 * sequences.
 */
class MidiEditorWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit MidiEditorWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~MidiEditorWidget();

    // --- Accessors for helper classes ---
    NoteNagaEngine* getEngine() const { return engine; }
    NoteNagaMidiSeq* getSequence() const { return last_seq; }
    QGraphicsScene* getScene() const { return scene; }
    MidiEditorConfig* getConfig() { return &config; }
    const MidiEditorColors& colors() const { return m_colors; }
    
    // --- Public interface ---
    QWidget* getTitleWidget() const { return title_widget; }
    std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> getSelectedNotes() const;
    bool hasSelection() const;

    QSize sizeHint() const override { return QSize(content_width, content_height); }
    QSize minimumSizeHint() const override { return QSize(320, 100); }

    // --- Coordinate conversion (public for helper classes) ---
    int sceneXToTick(qreal x) const;
    int sceneYToNote(qreal y) const;
    qreal tickToSceneX(int tick) const;
    qreal noteToSceneY(int note) const;
    int snapTickToGrid(int tick) const;
    int snapTickToGridNearest(int tick) const;
    int getGridStepTicks() const;
    QPen getNotePen(const NoteNagaTrack *track, bool is_active_track, bool is_selected_note) const;
    NoteDuration getNoteDuration() const;
    
    // --- Track refresh (called by helper classes) ---
    void refreshTrack(NoteNagaTrack *track);
    
    /**
     * @brief Get the maximum tick value based on the scrollable content width
     * @return Maximum tick value for the timeline overview
     */
    int getMaxTickFromContent() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void positionSelected(int tick);
    void horizontalScrollChanged(int value);
    void verticalScrollChanged(int value);
    void followModeChanged(MidiEditorFollowMode mode);
    void timeScaleChanged(double scale);
    void keyHeightChanged(int height);
    void loopingChanged(bool enabled);
    void notesModified();
    void selectionChanged();
    void contentSizeChanged(int maxTick);

public slots:
    void setTimeScale(double scale);
    void setKeyHeight(int h);

private slots:
    void refreshAll();
    void refreshMarker();
    void refreshSequence(NoteNagaMidiSeq *seq);
    void currentTickChanged(int tick);
    void selectFollowMode(MidiEditorFollowMode mode);
    void enableLooping(bool enabled);
    void onPlaybackStopped();
    
    // Context menu actions
    void onColorModeChanged(NoteColorMode mode);
    void onDeleteNotes();
    void onDuplicateNotes();
    void onSelectAll();
    void onInvertSelection();
    void onQuantize();
    void onTransposeUp();
    void onTransposeDown();
    void onTransposeOctaveUp();
    void onTransposeOctaveDown();
    void onSetVelocity(int velocity);

private:
    // --- Engine & data ---
    NoteNagaEngine *engine;
    NoteNagaMidiSeq *last_seq = nullptr;

    // --- Configuration ---
    MidiEditorConfig config;
    MidiEditorColors m_colors;
    int content_width;
    int content_height;

    // --- Helper classes ---
    MidiEditorContextMenu *m_contextMenu;
    MidiEditorNoteHandler *m_noteHandler;
    
    // --- Mouse state ---
    QRubberBand *rubberBand = nullptr;
    QPoint rubberBandOrigin;
    bool m_isDragging = false;
    QPointF m_clickStartPos;
    bool m_hadSelectionBeforeClick = false;

    // --- Active notes tracking ---
    QMap<int, int> m_activeNotes;
    QMap<int, int> m_lastActiveNotes;

    // --- UI controls ---
    QWidget *title_widget;
    QPushButton *btn_follow_center;
    QPushButton *btn_follow_left;
    QPushButton *btn_follow_step;
    QPushButton *btn_follow_none;
    QPushButton *btn_looping;
    QComboBox *combo_note_duration;
    QComboBox *combo_grid_resolution;

    // --- Graphics scene & items ---
    QGraphicsScene *scene;
    QGraphicsLineItem *marker_line = nullptr;
    std::vector<QGraphicsLineItem *> grid_lines;
    std::vector<QGraphicsLineItem *> bar_grid_lines;
    std::vector<QGraphicsSimpleTextItem *> bar_grid_labels;
    std::vector<QGraphicsRectItem *> row_backgrounds;
    
    // --- Legend widget ---
    QWidget *m_legendWidget = nullptr;
    void updateLegendVisibility();

    // --- Setup ---
    void initTitleUI();
    void setupConnections();

    // --- Scene update ---
    void recalculateContentSize();
    void updateScene();
    void updateGrid();
    void updateBarGrid();
    void updateAllNotes();
    void updateTrackNotes(NoteNagaTrack *track);
    void updateActiveNotes();
    void updateRowHighlights();
    void clearScene();

    void drawNote(const NN_Note_t &note, const NoteNagaTrack *track, bool is_selected,
                  bool is_drum, int x, int y, int w, int h);
    QColor getNoteColor(const NN_Note_t &note, const NoteNagaTrack *track) const;
};
