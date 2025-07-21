#pragma once

#include <QWidget>
#include <QFont>
#include <QColor>
#include <QRect>
#include <QPen>

#include <note_naga_engine/note_naga_engine.h>

class MidiTactRuler : public QWidget {
    Q_OBJECT
public:
    explicit MidiTactRuler(NoteNagaEngine *engine, QWidget* parent = nullptr);

    void set_time_scale(double time_scale);

signals:
    void set_horizontal_scroll(int tick);

public slots:
    void set_tick_position(int val);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    NoteNagaEngine *engine;

    double time_scale;
    int horizontalScroll;
    QFont font;

    // Colors
    QColor bg_color;
    QColor fg_color;
    QColor subline_color;
    QColor tact_bg_color;
    QColor tact_line_color;
};