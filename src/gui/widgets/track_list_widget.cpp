#include "track_list_widget.h"

#include "../nn_gui_utils.h"
#include <QMouseEvent>

TrackListWidget::TrackListWidget(NoteNagaEngine *engine_, QWidget *parent)
    : QWidget(parent), engine(engine_), selected_row(-1) {
    this->title_widget = nullptr;
    initTitleUI();
    initUI();

    NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
    reloadTracks(seq);

    // Signals
    connect(engine->getProject(), &NoteNagaProject::activeSequenceChanged, this,
            &TrackListWidget::reloadTracks);
    connect(engine->getMixer(), &NoteNagaMixer::noteInSignal, this,
            &TrackListWidget::handlePlayingNote);
}

void TrackListWidget::initTitleUI()
{
    if (this->title_widget) return;
    this->title_widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(title_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QPushButton *btn_add = create_small_button(":/icons/add.svg", "Add new Track", "AddButton");

    QPushButton *btn_remove =
        create_small_button(":/icons/remove.svg", "Remove selected Track", "RemoveButton");

    QPushButton *btn_clear =
        create_small_button(":/icons/clear.svg", "Clear all Tracks", "ClearButton");

    QPushButton *btn_reload =
        create_small_button(":/icons/reload.svg", "Reload Tracks from MIDI", "ReloadButton");

    layout->addWidget(btn_add, 0, Qt::AlignRight);
    layout->addWidget(btn_remove, 0, Qt::AlignRight);
    layout->addWidget(btn_clear, 0, Qt::AlignRight);
    layout->addWidget(btn_reload, 0, Qt::AlignRight);
}

void TrackListWidget::initUI() {
    scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setStyleSheet("QScrollArea { background: transparent; padding: 0px; }");

    container = new QWidget();
    vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(3, 3, 3, 3);
    vbox->setSpacing(4);
    vbox->addStretch(1);

    scroll_area->setWidget(container);

    // --- Layout pro celý widget ---
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->addWidget(scroll_area, 1);
    setLayout(main_layout);
}

void TrackListWidget::reloadTracks(NoteNagaMidiSeq *seq) {
    // Remove all widgets and any existing stretch from the layout
    while (vbox->count() > 0) {
        QLayoutItem *item = vbox->takeAt(vbox->count() - 1);
        QWidget *widget = item->widget();
        if (widget != nullptr) { widget->deleteLater(); }
        delete item;
    }
    track_widgets.clear();

    if (!seq) {
        selected_row = -1;
        return;
    }

    for (size_t idx = 0; idx < seq->getTracks().size(); ++idx) {
        NoteNagaTrack *track = seq->getTracks()[idx];
        if (!track) continue;
        TrackWidget *widget = new TrackWidget(this->engine, track, container);

        // Selection handling via event filter
        widget->installEventFilter(this);
        widget->setMouseTracking(true);

        // Custom mousePressEvent via subclass or signal (see below)
        connect(widget, &TrackWidget::clicked, this,
                [this, seq, idx]() { updateSelection(seq, static_cast<int>(idx)); });

        track_widgets.push_back(widget);
        vbox->addWidget(widget);
    }
    vbox->addStretch();
    updateSelection(seq, track_widgets.empty() ? -1 : 0);
}

void TrackListWidget::updateSelection(NoteNagaMidiSeq *sequence, int widget_idx) {
    selected_row = widget_idx;
    for (size_t i = 0; i < track_widgets.size(); ++i) {
        track_widgets[i]->refreshStyle(static_cast<int>(i) == widget_idx);
        if (static_cast<int>(i) == widget_idx) {
            sequence->setActiveTrack(track_widgets[i]->getTrack());
        }
    }
}

void TrackListWidget::handlePlayingNote(const NN_Note_t &note) {
    NoteNagaTrack *track = note.parent;
    if (!track) return;
    NoteNagaProject *project = engine->getProject();

    double time_ms = note_time_ms(note, project->getPPQ(), project->getTempo());
    for (auto *w : track_widgets) {
        if (w->getTrack() == track && note.velocity.has_value() &&
            note.velocity.value() > 0) {
            w->getVolumeBar()->setValue(static_cast<double>(note.velocity.value()),
                                        time_ms);
            break;
        }
    }
}