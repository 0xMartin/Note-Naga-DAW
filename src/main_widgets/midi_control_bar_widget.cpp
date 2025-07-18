#include "midi_control_bar_widget.h"

#include <QInputDialog>
#include <QIcon>
#include <QPropertyAnimation>
#include <QPalette>
#include <QMouseEvent>
#include <cmath>

MidiControlBarWidget::MidiControlBarWidget(AppContext* ctx_, QWidget* parent)
    : QWidget(parent), ctx(ctx_)
{
    _init_ui();
}

void MidiControlBarWidget::_init_ui()
{
    setStyleSheet(R"(
        QPushButton#playToggleBtn, QPushButton#toStartBtn, QPushButton#toEndBtn, QPushButton#metronomeBtn {
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
            padding: 0px;
            border-radius: 8px;
            background: transparent;
        }
        QPushButton#metronomeBtn {
            background: #253a4c;
            border: 1.6px solid #4866a0;
        }
        QPushButton#metronomeBtn:checked {
            background: #3477c0;
            border: 1.9px solid #79b8ff;
        }
        QPushButton#metronomeBtn:hover {
            background: #29528c;
            border: 1.9px solid #79b8ff;
        }
        QLabel#tempoLabel {
            color: #eee;
            border: 1px solid #232731;
            border-radius: 5px;
            min-width: 68px;
            font-size: 13px;
            background-color: #30343a;
            margin-right: 15px;
            min-height: 32px;
            max-height: 32px;    
        }   
        QLabel#tempoIcon {
            margin-right: 3px;
        }
        QLabel#timeLabel {
            min-height: 32px;
            max-height: 32px;    
        }
    )");

    QHBoxLayout* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(7, 2, 7, 2);
    hbox->setSpacing(9);

    // Start button
    to_start_btn = new QPushButton();
    to_start_btn->setObjectName("toStartBtn");
    to_start_btn->setIcon(QIcon(":/icons/media-backward-end.svg"));
    to_start_btn->setIconSize(QSize(21, 21));
    to_start_btn->setCursor(Qt::PointingHandCursor);
    connect(to_start_btn, &QPushButton::clicked, this, &MidiControlBarWidget::goto_start_signal);
    hbox->addWidget(to_start_btn);

    // Play/stop toggle button
    play_btn = new QPushButton();
    play_btn->setObjectName("playToggleBtn");
    play_btn->setIcon(QIcon(":/icons/play.svg"));
    play_btn->setIconSize(QSize(21, 21));
    play_btn->setCursor(Qt::PointingHandCursor);
    connect(play_btn, &QPushButton::clicked, this, &MidiControlBarWidget::toggle_play_signal);
    hbox->addWidget(play_btn);

    // End button
    to_end_btn = new QPushButton();
    to_end_btn->setObjectName("toEndBtn");
    to_end_btn->setIcon(QIcon(":/icons/media-forward-end.svg"));
    to_end_btn->setIconSize(QSize(21, 21));
    to_end_btn->setCursor(Qt::PointingHandCursor);
    connect(to_end_btn, &QPushButton::clicked, this, &MidiControlBarWidget::goto_end_signal);
    hbox->addWidget(to_end_btn);

    hbox->addSpacing(20);

    // Tempo label with icon
    QHBoxLayout* tempo_hbox = new QHBoxLayout();
    tempo_hbox->setContentsMargins(0,0,0,0);
    tempo_hbox->setSpacing(2);
    
    // Metronome button
    metronome_btn = new QPushButton();
    metronome_btn->setObjectName("metronomeBtn");
    metronome_btn->setCheckable(true);
    metronome_btn->setIcon(QIcon(":/icons/tempo.svg"));
    metronome_btn->setIconSize(QSize(21, 21));
    metronome_btn->setCursor(Qt::PointingHandCursor);
    connect(metronome_btn, &QPushButton::clicked, this, &MidiControlBarWidget::metronome_btn_clicked);
    tempo_hbox->addWidget(metronome_btn);

    tempo_label = new QLabel();
    tempo_label->setObjectName("tempoLabel");
    tempo_label->setCursor(Qt::PointingHandCursor);
    tempo_hbox->addWidget(tempo_label);

    QWidget* tempo_widget = new QWidget();
    tempo_widget->setLayout(tempo_hbox);
    hbox->addWidget(tempo_widget);

    hbox->addSpacing(18);

    // Time label - animated
    time_label = new AnimatedTimeLabel(this);
    time_label->setObjectName("timeLabel");
    hbox->addWidget(time_label);

    hbox->addStretch(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    tempo_label->installEventFilter(this);
}

void MidiControlBarWidget::update_times(int cur_tick, int max_tick, int tempo, int ppq) {
    double us_per_tick = double(tempo) / double(ppq);
    double total_sec = double(max_tick) * us_per_tick / 1'000'000.0;
    double cur_sec = double(cur_tick) * us_per_tick / 1'000'000.0;
    time_label->setText(QString("%1 / %2").arg(format_time(cur_sec), format_time(total_sec)));
    time_label->animateTick();
    double bpm = tempo ? (60'000'000.0 / double(tempo)) : 0.0;
    tempo_label->setText(QString("%1 BPM").arg(bpm, 0, 'f', 2));
}

void MidiControlBarWidget::set_playing(bool is_playing) {
    if (is_playing) {
        play_btn->setIcon(QIcon(":/icons/stop.svg"));
    } else {
        play_btn->setIcon(QIcon(":/icons/play.svg"));
    }
}

void MidiControlBarWidget::set_playing_slot(bool is_playing) {
    set_playing(is_playing);
}

void MidiControlBarWidget::edit_tempo(QMouseEvent* event) {
    double cur_bpm = ctx->tempo ? (60'000'000.0 / double(ctx->tempo)) : 120.0;
    bool ok = false;
    double bpm = QInputDialog::getDouble(this, "Change Tempo", "New Tempo (BPM):", cur_bpm, 5, 500, 2, &ok);
    if (ok) {
        ctx->tempo = int(60'000'000.0 / bpm);
        update_times(ctx->current_tick, ctx->max_tick, ctx->tempo, ctx->ppq);
        emit tempo_changed_signal(ctx->tempo);
    }
}

bool MidiControlBarWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == tempo_label && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        edit_tempo(mouseEvent);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void MidiControlBarWidget::metronome_btn_clicked() {
    metronome_on = metronome_btn->isChecked();
    emit metronome_toggled_signal(metronome_on);
}

QString MidiControlBarWidget::format_time(double sec) {
    if (sec < 0) sec = 0;
    int m = int(sec / 60);
    int s = int(std::fmod(sec, 60));
    int ms = int((sec - int(sec)) * 1000);
    return QString("%1:%2.%3").arg(m).arg(s, 2, 10, QChar('0')).arg(ms, 3, 10, QChar('0'));
}