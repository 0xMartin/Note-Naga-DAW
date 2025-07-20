#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <memory>
#include <QMap>
#include <QColor>
#include <vector>

#include "../../note_naga_engine/note_naga_engine.h"

class MidiEditorWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit MidiEditorWidget(NoteNagaEngine* engine, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    double get_time_scale() const { return time_scale; }
    int get_key_height() const { return key_height; }

signals:
    void set_position_signal(int tick);

public slots:
    void repaint_slot();
    void set_time_scale_slot(double scale);
    void set_key_height_slot(int h);
    void update_marker_slot();
    void on_viewport_changed();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    NoteNagaEngine* engine;

    double time_scale;
    int key_height;
    int content_width;
    int content_height;
    int tact_subdiv; // number of grid subdivisions per bar

    QGraphicsScene* scene;

    struct NoteGraphics {
        QGraphicsItem* item;
        QGraphicsSimpleTextItem* label;
    };
    QMap<int, std::vector<NoteGraphics>> note_items;

    QGraphicsLineItem* marker_line = nullptr;
    std::vector<QGraphicsLineItem*> grid_lines;
    std::vector<QGraphicsLineItem*> bar_grid_lines;
    std::vector<QGraphicsSimpleTextItem*> bar_grid_labels;

    QColor bg_color;
    QColor fg_color;
    QColor line_color;
    QColor subline_color;
    QColor grid_bar_color;
    QColor grid_row_color1;
    QColor grid_row_color2;
    QColor grid_bar_label_color;
    QColor grid_subdiv_color;

    void recalculate_content_size();
    void set_time_scale(double scale);
    void set_key_height(int h);

    void update_scene_items(NoteNagaMIDISeq *seq = nullptr);
    void update_grid(const NoteNagaMIDISeq *seq);
    void update_bar_grid(const NoteNagaMIDISeq *seq);
    void update_notes(const NoteNagaMIDISeq *seq);
    void update_marker(const NoteNagaMIDISeq *seq);

    void draw_note(const NoteNagaNote& note, const NoteNagaTrack* track, bool is_selected, bool is_drum, int x, int y, int w, int h);
    void clear_scene();
};