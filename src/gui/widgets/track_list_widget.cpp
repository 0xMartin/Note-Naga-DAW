#include "track_list_widget.h"
#include <QMouseEvent>
#include <QDebug>

TrackListWidget::TrackListWidget(NoteNagaEngine* engine_, QWidget* parent)
    : QWidget(parent), engine(engine_), selected_row(-1)
{
    initUI();

    NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
    reloadTracks(seq);

    // Signals
    connect(engine->getProject(), &NoteNagaProject::activeSequenceChanged, this, &TrackListWidget::reloadTracks);
    connect(engine->getMixer(), &NoteNagaMixer::noteInSignal, this, &TrackListWidget::handlePlayingNote);
}

void TrackListWidget::initUI()
{
    // --- Moderní světlý header s ikonou a titulkem ---
    QFrame* header_frame = new QFrame();
    header_frame->setObjectName("TrackListHeaderFrame");
    header_frame->setStyleSheet(
        "QFrame#TrackListHeaderFrame { background: #353a44; border-radius: 9px; margin-bottom: 8px; }"
    );
    QHBoxLayout* header_layout = new QHBoxLayout(header_frame);
    header_layout->setContentsMargins(10, 5, 10, 5);
    header_layout->setSpacing(12);

    QLabel* header_icon = new QLabel();
    header_icon->setPixmap(QIcon(":/icons/track.svg").pixmap(23, 23));
    header_icon->setFixedSize(23, 23);

    QLabel* title = new QLabel("Tracks");
    title->setStyleSheet(
        "font-size: 20px; font-weight: bold; color: #79b8ff; letter-spacing: 1.2px;"
    );
    header_layout->addWidget(header_icon, 0, Qt::AlignVCenter);
    header_layout->addWidget(title, 0, Qt::AlignVCenter);
    header_layout->addStretch(1);

    // --- Scrollovací oblast tracků ---
    scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setStyleSheet(
        "QScrollArea { background: transparent; padding: 0px; }"
    );

    container = new QWidget();
    vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(3, 3, 3, 3);
    vbox->setSpacing(4);
    vbox->addStretch(1);

    scroll_area->setWidget(container);

    // --- Layout pro celý widget ---
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->setSpacing(0);
    main_layout->addWidget(header_frame);
    main_layout->addWidget(scroll_area, 1);
    setLayout(main_layout);
}

void TrackListWidget::reloadTracks(NoteNagaMidiSeq *seq)
{
    // Remove all widgets and any existing stretch from the layout
    while (vbox->count() > 0) {
        QLayoutItem* item = vbox->takeAt(vbox->count() - 1);
        QWidget* widget = item->widget();
        if (widget != nullptr) {
            widget->deleteLater();
        }
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
        TrackWidget* widget = new TrackWidget(this->engine, track, container);

        // Selection handling via event filter
        widget->installEventFilter(this);
        widget->setMouseTracking(true);

        // Custom mousePressEvent via subclass or signal (see below)
        connect(widget, &TrackWidget::clicked, this, [this, seq, idx]() {
            updateSelection(seq, static_cast<int>(idx));
        });

        track_widgets.push_back(widget);
        vbox->addWidget(widget);
    }
    vbox->addStretch();
    updateSelection(seq, track_widgets.empty() ? -1 : 0);
}

void TrackListWidget::updateSelection(NoteNagaMidiSeq *sequence, int widget_idx)
{
    selected_row = widget_idx;
    for (size_t i = 0; i < track_widgets.size(); ++i) {
        track_widgets[i]->refreshStyle(static_cast<int>(i) == widget_idx);
        if (static_cast<int>(i) == widget_idx) {
            sequence->setActiveTrack(track_widgets[i]->getTrack());
        }
    }
}

void TrackListWidget::handlePlayingNote(const NoteNagaNote& note)
{
    NoteNagaTrack *track = note.parent;
    if (!track) return;
    NoteNagaProject *project = engine->getProject();

    double time_ms = note_time_ms(note, project->getPPQ(), project->getTempo());
    for (auto* w : track_widgets) {
        if (w->getTrack() == track && note.velocity.has_value() && note.velocity.value() > 0) {
            w->getVolumeBar()->setValue(static_cast<double>(note.velocity.value()), time_ms);
            break;
        }
    }
}