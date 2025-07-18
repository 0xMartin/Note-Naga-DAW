#include "track_list_widget.h"
#include <QMouseEvent>
#include <QDebug>

TrackListWidget::TrackListWidget(AppContext* ctx_, Mixer *mixer_, QWidget* parent)
    : QWidget(parent), ctx(ctx_), mixer(mixer_), selected_row(-1)
{
    _init_ui();
    _reload_tracks();
}

void TrackListWidget::_init_ui()
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

    // Signals
    connect(ctx, &AppContext::midi_file_loaded_signal, this, &TrackListWidget::_reload_tracks);
    connect(ctx, &AppContext::playing_note_signal, this, &TrackListWidget::_handle_playing_note);
}

void TrackListWidget::_reload_tracks()
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

    for (size_t idx = 0; idx < ctx->tracks.size(); ++idx) {
        auto& tr = ctx->tracks[idx];
        TrackWidget* widget = new TrackWidget(tr->track_id, ctx, mixer, container);
        connect(widget, &TrackWidget::visibility_changed_signal, this, &TrackListWidget::visibility_changed_signal);
        connect(widget, &TrackWidget::muted_changed_signal, this, &TrackListWidget::muted_changed_signal);
        connect(widget, &TrackWidget::color_changed_signal, this, &TrackListWidget::color_changed_signal);
        connect(widget, &TrackWidget::instrument_changed_signal, this, &TrackListWidget::instrument_changed_signal);
        connect(widget, &TrackWidget::name_changed_signal, this, &TrackListWidget::name_changed_signal);

        // Selection handling via event filter
        widget->installEventFilter(this);
        widget->setMouseTracking(true);

        // Custom mousePressEvent via subclass or signal (see below)
        connect(widget, &TrackWidget::clicked, this, [this, idx](int /*track_index*/) {
            _update_selection(static_cast<int>(idx));
            emit track_selected_signal(static_cast<int>(idx));
        });

        track_widgets.push_back(widget);
        vbox->addWidget(widget);
    }
    vbox->addStretch();
    _update_selection(track_widgets.empty() ? -1 : 0);
}

void TrackListWidget::_update_selection(int idx)
{
    selected_row = idx;
    for (size_t i = 0; i < track_widgets.size(); ++i) {
        track_widgets[i]->refresh_style(static_cast<int>(i) == idx);
        if (static_cast<int>(i) == idx) {
            ctx->active_track_id = track_widgets[i]->get_track_id();
            if (ctx->active_track_id.has_value())
                emit ctx->selected_track_changed_signal(ctx->active_track_id.value());
        }
    }
}

void TrackListWidget::_handle_playing_note(const MidiNote& note, int track_id)
{
    double time_ms = note_time_ms(note, ctx->ppq, ctx->tempo);
    for (auto* w : track_widgets) {
        if (w->get_track_id() == track_id && note.velocity.has_value() && note.velocity.value() > 0) {
            w->get_volume_bar()->setValue(static_cast<double>(note.velocity.value()) / 127.0, time_ms);
            break;
        }
    }
}