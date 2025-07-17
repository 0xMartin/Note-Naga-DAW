#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>
#include <QPropertyAnimation>
#include "../core/app_context.h"
#include "../widgets/animated_time_label.h"

class MidiControlBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit MidiControlBarWidget(AppContext* ctx, QWidget* parent = nullptr);

    void update_times(int cur_tick, int max_tick, int tempo, int ppq);
    void set_playing(bool is_playing);

signals:
    void toggle_play_signal();
    void goto_start_signal();
    void goto_end_signal();
    void tempo_changed_signal(int tempo);
    void metronome_toggled_signal(bool state);

public slots:
    void set_playing_slot(bool is_playing);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    AppContext* ctx;
    QLabel* tempo_label;
    QLabel* tempo_icon;
    AnimatedTimeLabel* time_label;
    QPushButton* play_btn;
    QPushButton* to_start_btn;
    QPushButton* to_end_btn;
    QPushButton* metronome_btn;

    bool metronome_on = false;

    void _init_ui();
    void edit_tempo(QMouseEvent* event);
    static QString format_time(double sec);

private slots:
    void metronome_btn_clicked();
};