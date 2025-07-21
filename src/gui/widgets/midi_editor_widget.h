#pragma once

#include <QColor>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QMap>
#include <memory>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>

// MidiEditorWidget: přehledněji rozdělená implementace, optimalizace pro signál
// track_meta_changed_signal

class MidiEditorWidget : public QGraphicsView {
    Q_OBJECT
  public:
    explicit MidiEditorWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);

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
    void reload_all(); // pro active_sequence_changed_signal
    void reload_track(NoteNagaTrack *track,
                      const std::string &param); // pro track_meta_changed_signal

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

  private:
    void setup_connections();

    // Výpočty a přepočty
    void recalculate_content_size(NoteNagaMidiSeq *seq = nullptr);
    void set_time_scale(double scale);
    void set_key_height(int h);

    // Vykreslování (rozděleno)
    void update_scene_items(NoteNagaMidiSeq *seq = nullptr);
    void update_grid(const NoteNagaMidiSeq *seq);
    void update_bar_grid(const NoteNagaMidiSeq *seq);
    void update_all_notes(const NoteNagaMidiSeq *seq); // všechny tracky
    void update_one_track_notes(const NoteNagaTrack *track,
                                const NoteNagaMidiSeq *seq); // pouze jeden track
    void update_marker(const NoteNagaMidiSeq *seq);

    void draw_note(const NoteNagaNote &note, const NoteNagaTrack *track, bool is_selected,
                   bool is_drum, int x, int y, int w, int h);
    void clear_scene();
    void clear_notes();
    void clear_track_notes(int track_id);

    // UI data
    NoteNagaEngine *engine;
    double time_scale;
    int key_height;
    int content_width;
    int content_height;
    int tact_subdiv;

    QGraphicsScene *scene;

    struct NoteGraphics {
        QGraphicsItem *item;
        QGraphicsSimpleTextItem *label;
    };
    QMap<int, std::vector<NoteGraphics>> note_items; // track_id -> notes

    QGraphicsLineItem *marker_line = nullptr;
    std::vector<QGraphicsLineItem *> grid_lines;
    std::vector<QGraphicsLineItem *> bar_grid_lines;
    std::vector<QGraphicsSimpleTextItem *> bar_grid_labels;

    QColor bg_color, fg_color, line_color, subline_color, grid_bar_color, grid_row_color1,
        grid_row_color2, grid_bar_label_color, grid_subdiv_color;
};