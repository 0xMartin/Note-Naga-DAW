#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <memory>
#include <QMap>
#include "../core/app_context.h"
#include "../core/shared.h"

class MidiEditorWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit MidiEditorWidget(AppContext* ctx, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    double get_time_scale() const { return time_scale; }
    int get_key_height() const { return key_height; }

signals:
    void set_play_position_signal(int tick);

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
    void recalculate_content_size();
    void _reload_notes();
    void set_time_scale(double scale);
    void set_key_height(int h);
    void update_scene_items();
    void update_grid();
    void update_bar_grid();
    void update_notes();
    void draw_note(const MidiNote& note, const Track& track, bool is_selected, bool is_drum, int x, int y, int w, int h);
    void update_marker();
    void clear_scene();

    AppContext* ctx;
    bool has_file;

    double time_scale;
    int key_height;
    int _content_width;
    int _content_height;
    int ppq;
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
};