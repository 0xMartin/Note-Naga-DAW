#include "midi_control_bar_widget.h"

#include <QIcon>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPalette>
#include <QPropertyAnimation>
#include <cmath>

MidiControlBarWidget::MidiControlBarWidget(NoteNagaEngine *engine_, QWidget *parent)
    : QFrame(parent), engine(engine_) {

    this->ppq = 0;
    this->tempo = 0;
    this->max_tick = 0;

    // Initialize UI components
    this->setObjectName("MidiControlBarWidget");
    initUI();

    // midi sequence change signals
    connect(this->engine->getProject(), &NoteNagaRuntimeData::activeSequenceChanged, this,
            [this](NoteNagaMidiSeq *seq) {
                this->ppq = seq->getPPQ();
                this->tempo = seq->getTempo();
                this->max_tick = seq->getMaxTick();
                this->progress_bar->setMidiSequence(seq);
                this->updateProgressBar();
                this->updateBPM();
            });
    connect(this->engine->getProject(), &NoteNagaRuntimeData::sequenceMetadataChanged, this,
            [this](NoteNagaMidiSeq *seq, const std::string &param) {
                this->ppq = seq->getPPQ();
                this->tempo = seq->getTempo();
                this->max_tick = seq->getMaxTick();
                this->progress_bar->setMidiSequence(seq);
                this->updateProgressBar();
                this->updateBPM();
            });
    // current tick changed signal
    connect(this->engine->getProject(), &NoteNagaRuntimeData::currentTickChanged, this,
            [this]() { this->updateProgressBar(); });
    // Playback worker start/stop signals
    connect(this->engine, &NoteNagaEngine::playbackStarted, this,
            [this]() { this->setPlaying(true); });
    connect(this->engine, &NoteNagaEngine::playbackStopped, this,
            [this]() { this->setPlaying(false); });
}

void MidiControlBarWidget::initUI() {
    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(6, 3, 6, 3);
    hbox->setSpacing(8);

    //////////////////////////////////////////////////////////////////////////////////////////
    // main playback control buttons
    QStringList btnNames = { "toStartBtn", "playToggleBtn", "toEndBtn" };
    QList<QIcon> btnIcons = {
        QIcon(":/icons/media-backward-end.svg"),
        QIcon(":/icons/play.svg"),
        QIcon(":/icons/media-forward-end.svg")
    };
    QStringList btnTips = { "Go to start", "Play/Stop", "Go to end" };

    playback_btn_group = new ButtonGroupWidget(btnNames, btnIcons, btnTips, QSize(21,21), false, this);
    connect(playback_btn_group, &ButtonGroupWidget::buttonClicked, this, [this](const QString& btn){
        if (btn == "toStartBtn") goToStart();
        else if (btn == "playToggleBtn") playToggled();
        else if (btn == "toEndBtn") goToEnd();
    });
    playback_btn_group->button("playToggleBtn")->setCheckable(true);
    hbox->addWidget(playback_btn_group);

    //////////////////////////////////////////////////////////////////////////////////////////
    // Midi progress bar
    progress_bar = new MidiSequenceProgressBar(this);
    progress_bar->setObjectName("midiProgressBar");
    connect(progress_bar, &MidiSequenceProgressBar::positionPressed, this,
            &MidiControlBarWidget::onProgressBarPositionPressed);
    connect(progress_bar, &MidiSequenceProgressBar::positionDragged, this,
            &MidiControlBarWidget::onProgressBarPositionDragged);
    connect(progress_bar, &MidiSequenceProgressBar::positionReleased, this,
            &MidiControlBarWidget::onProgressBarPositionReleased);
    hbox->addWidget(progress_bar);

    //////////////////////////////////////////////////////////////////////////////////////////
    // Tempo label with icon
    QHBoxLayout *tempo_hbox = new QHBoxLayout();
    tempo_hbox->setContentsMargins(0, 0, 0, 0);
    tempo_hbox->setSpacing(2);

    // Metronome button
    metronome_btn = new QPushButton();
    metronome_btn->setObjectName("metronomeBtn");
    metronome_btn->setCheckable(true);
    metronome_btn->setChecked(this->engine->isMetronomeEnabled());
    metronome_btn->setIcon(QIcon(":/icons/tempo.svg"));
    metronome_btn->setIconSize(QSize(21, 21));
    metronome_btn->setCursor(Qt::PointingHandCursor);
    connect(metronome_btn, &QPushButton::clicked, this,
            &MidiControlBarWidget::metronomeBtnClicked);
    tempo_hbox->addWidget(metronome_btn);

    tempo_label = new QLabel();
    tempo_label->setObjectName("tempoLabel");
    tempo_label->setCursor(Qt::PointingHandCursor);
    tempo_hbox->addWidget(tempo_label);
    tempo_label->installEventFilter(this);

    QWidget *tempo_widget = new QWidget();
    tempo_widget->setObjectName("tempoWidget");
    tempo_widget->setLayout(tempo_hbox);
    hbox->addWidget(tempo_widget);

    //////////////////////////////////////////////////////////////////////////////////////////
    // styles
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet(R"(
        QFrame#MidiControlBarWidget {
            border-top: 1px solid #19191f;
            background-color: #373942;
        }
        QWidget#tempoWidget { 
            background: transparent; 
        }
        QPushButton#metronomeBtn {
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
            padding: 0px;
            border-radius: 8px;
            background: transparent;
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
            min-height: 32px;
            max-height: 32px;    
        }   
        QWidget#progress_bar {
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
}

void MidiControlBarWidget::updateBPM() {
    NoteNagaRuntimeData *project = this->engine->getProject();
    // update bmp label
    double bpm = project->getTempo() ? (60'000'000.0 / double(project->getTempo())) : 0.0;
    tempo_label->setText(QString("%1 BPM").arg(bpm, 0, 'f', 2));

    // update progress bar total time
    progress_bar->updateMaxTime();
}

void MidiControlBarWidget::updateProgressBar() {
    NoteNagaRuntimeData *project = this->engine->getProject();
    double us_per_tick = double(this->tempo) / double(this->ppq);
    double cur_sec = double(project->getCurrentTick()) * us_per_tick / 1'000'000.0;
    progress_bar->setCurrentTime(cur_sec);
}

void MidiControlBarWidget::setPlaying(bool is_playing) {
    QPushButton *play_btn = playback_btn_group->button("playToggleBtn");
    if (!play_btn) return;
    if (is_playing) {
        play_btn->setIcon(QIcon(":/icons/stop.svg"));
        play_btn->setChecked(true);
    } else {
        play_btn->setIcon(QIcon(":/icons/play.svg"));
        play_btn->setChecked(false);
    }
    play_btn->update();
}

void MidiControlBarWidget::editTempo(QMouseEvent *event) {
    NoteNagaMidiSeq *seq = this->engine->getProject()->getActiveSequence();
    if (!seq) return;

    double cur_bpm = seq->getTempo() ? (60'000'000.0 / double(seq->getTempo())) : 120.0;
    bool ok = false;
    double bpm = QInputDialog::getDouble(this, "Change Tempo",
                                         "New Tempo (BPM):", cur_bpm, 5, 500, 2, &ok);
    if (ok) {
        seq->setTempo(60'000'000.0 / bpm);
        updateProgressBar();
        engine->changeTempo(seq->getTempo());
        emit tempoChanged(seq->getTempo());
    }
}

bool MidiControlBarWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == tempo_label && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        editTempo(mouseEvent);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void MidiControlBarWidget::metronomeBtnClicked() {
    bool metronome_on = metronome_btn->isChecked();
    this->engine->enableMetronome(metronome_on);
    emit metronomeToggled(metronome_on);
}

void MidiControlBarWidget::onProgressBarPositionPressed(float seconds) {
    NoteNagaRuntimeData *project = this->engine->getProject();
    if (!project) return;

    // Calculate tick position based on seconds
    int tick_position = nn_seconds_to_ticks(seconds, this->ppq, this->tempo);

    // Set the current tick to the calculated position
    was_playing = engine->isPlaying();
    if (was_playing) engine->stopPlayback();
    project->setCurrentTick(tick_position);

    // Update the progress bar and other UI elements
    updateProgressBar();

    emit playPositionChanged(seconds, tick_position);
}

void MidiControlBarWidget::onProgressBarPositionDragged(float seconds) {
    NoteNagaRuntimeData *project = this->engine->getProject();
    if (!project) return;

    // Calculate tick position based on seconds
    int tick_position = nn_seconds_to_ticks(seconds, this->ppq, this->tempo);

    // Set the current tick to the calculated position
    project->setCurrentTick(tick_position);

    // Update the progress bar and other UI elements
    updateProgressBar();

    emit playPositionChanged(seconds, tick_position);
}

void MidiControlBarWidget::onProgressBarPositionReleased(float seconds) {
    if (was_playing) engine->startPlayback();
}

QString MidiControlBarWidget::format_time(double sec) {
    if (sec < 0) sec = 0;
    int m = int(sec / 60);
    int s = int(std::fmod(sec, 60));
    int ms = int((sec - int(sec)) * 1000);
    return QString("%1:%2.%3")
        .arg(m)
        .arg(s, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}