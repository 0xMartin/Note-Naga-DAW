#pragma once

#include <QColor>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QMap>
#include <memory>
#include <note_naga_engine/note_naga_engine.h>
#include <vector>

class MidiEditorWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit MidiEditorWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    double getTimeScale() const { return time_scale; }
    int getKeyHeight() const { return key_height; }

signals:
    void positionSelected(int tick);

public slots:
    // Refreshe používají vždy uloženou sekvenci
    void refreshAll();
    void refreshMarker();
    void refreshTrack(NoteNagaTrack *track);
    void refreshSequence(NoteNagaMidiSeq *seq);

    void setTimeScale(double scale);
    void setKeyHeight(int h);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    NoteNagaEngine *engine;
    NoteNagaMidiSeq *last_seq = nullptr; /// Last sequence being edited

    double time_scale; /// < 1.0 = zoomed in, > 1.0 = zoomed out
    int key_height; /// Height of each key in pixels
    int content_width; /// Width of the content area
    int content_height; /// Height of the content area
    int tact_subdiv; /// Number of subdivisions per tact

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

    // --- Colors ---
    QColor bg_color, fg_color, line_color, subline_color, grid_bar_color, grid_row_color1,
        grid_row_color2, grid_bar_label_color, grid_subdiv_color;

    // --- Setup & Helpers ---
    void setupConnections();
    void setSequence(NoteNagaMidiSeq *seq);

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
};