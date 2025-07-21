#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>
#include <QPropertyAnimation>

#include <note_naga_engine.h>
#include "../components/animated_time_label.h"

class MidiControlBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit MidiControlBarWidget(NoteNagaEngine* engine, QWidget* parent = nullptr);

    void set_playing(bool is_playing);

signals:
    void toggle_play_signal();
    void goto_start_signal();
    void goto_end_signal();
    void tempo_changed_signal(int tempo);
    void metronome_toggled_signal(bool state);

public slots:
    void update_values();
    void set_playing_slot(bool is_playing);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    NoteNagaEngine* engine;
    
    int ppq;
    int tempo;
    int max_tick;
    bool metronome_on;

    QLabel* tempo_label;
    QLabel* tempo_icon;
    AnimatedTimeLabel* time_label;
    QPushButton* play_btn;
    QPushButton* to_start_btn;
    QPushButton* to_end_btn;
    QPushButton* metronome_btn;

    void _init_ui();
    void edit_tempo(QMouseEvent* event);
    static QString format_time(double sec);

private slots:
    void metronome_btn_clicked();
};