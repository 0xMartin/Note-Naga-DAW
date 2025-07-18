#pragma once

#include <QWidget>
#include <QTimer>
#include <QFont>
#include <QColor>
#include <QMap>
#include <QSet>
#include <vector>
#include <optional>
#include <QMouseEvent>
#include "../core/shared.h"
#include "../core/app_context.h"
#include "../core/mixer.h"

class MidiKeyboardRuler : public QWidget {
    Q_OBJECT
public:
    explicit MidiKeyboardRuler(AppContext *ctx, Mixer *mixer, int viewer_row_height = 16, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void play_note_signal(MidiNote note, int track_id);
    void stop_note_signal(MidiNote note, int track_id);

public slots:
    void on_play_note(const MidiNote &note, int track_id);
    void set_vertical_scroll_slot(float v, float row_height);
    void clear_highlights_slot();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    AppContext *ctx;
    Mixer *mixer;

    int viewer_row_height;
    int verticalScroll;

    MidiNote pressed_note;
    int hovered_note;

    QFont font;
    QFont c_key_font;

    QMap<int, QColor> key_highlights;
    QMap<int, QTimer*> highlight_timers;

    QColor bg_color;
    QColor white_key_color;
    QColor black_key_color;
    QColor white_key_line_color;
    QColor black_key_line_color;
    QColor hover_color;
    QColor press_color;
    QColor c_key_label_color;

    std::vector<int> white_keys() const;
    std::vector<int> black_keys() const;
    std::optional<int> note_at_pos(const QPoint& pos) const;
    void highlight_key(int note, const QColor &color, int timeout);
    void _remove_highlight(int note);
    void clear_highlights();

    static bool is_black_key(int midi_note);
    static bool is_white_key(int midi_note);
    static constexpr int WHITE_ORDER[7] = {0, 2, 4, 5, 7, 9, 11};
    static constexpr int BLACK_ORDER[5] = {1, 3, 6, 8, 10};
    static constexpr double WHITE_HEIGHT[12] = {1.5, 0.0, 2.0, 0.0, 1.5, 1.5, 0.0, 2.0, 0.0, 2.0, 0.0, 1.5};
};