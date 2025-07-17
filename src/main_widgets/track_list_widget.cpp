#include "track_list_widget.h"
#include <QMouseEvent>
#include <QDebug>

TrackListWidget::TrackListWidget(AppContext* ctx_, QWidget* parent)
    : QWidget(parent), ctx(ctx_), selected_row(-1)
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
    header_layout->setContentsMargins(14, 7, 14, 7);
    header_layout->setSpacing(13);

    QLabel* header_icon = new QLabel();
    header_icon->setPixmap(QIcon(":/icons/track.svg").pixmap(32,32));
    header_icon->setFixedSize(36,36);

    QLabel* title = new QLabel("Tracks");
    title->setStyleSheet(
        "font-size: 22px; font-weight: bold; color: #79b8ff; letter-spacing: 1.2px;"
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
    QVBoxLayout* outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(12, 12, 12, 12);
    outer_layout->setSpacing(0);
    outer_layout->addWidget(header_frame);
    outer_layout->addWidget(scroll_area, 1);
    setLayout(outer_layout);

    // Signals
    connect(ctx, &AppContext::midi_file_loaded_signal, this, &TrackListWidget::_reload_tracks);
    connect(ctx, &AppContext::playing_note_signal, this, &TrackListWidget::_handle_playing_note);
}

void TrackListWidget::set_track_visible(int track_index, bool state)
{
    for (auto* w : track_widgets) {
        if (w->get_track_index() == track_index) {
            w->setVisible(state);
            break;
        }
    }
}

void TrackListWidget::set_track_play(int track_index, bool state)
{
    for (auto* w : track_widgets) {
        if (w->get_track_index() == track_index) {
            // If you have a play icon refresh function, call it here
            break;
        }
    }
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
        qDebug() << "TrackListWidget: Adding track" << tr->track_id << "-" << tr->name;
        TrackWidget* widget = new TrackWidget(tr->track_id, ctx, container);
        connect(widget, &TrackWidget::visibility_changed_signal, this, &TrackListWidget::visibility_changed_signal);
        connect(widget, &TrackWidget::playback_changed_signal, this, &TrackListWidget::playback_changed_signal);
        connect(widget, &TrackWidget::color_changed_signal, this, &TrackListWidget::color_changed_signal);
        connect(widget, &TrackWidget::instrument_changed_signal, this, &TrackListWidget::instrument_changed_signal);
        connect(widget, &TrackWidget::name_changed_signal, this, &TrackListWidget::name_changed_signal);

        // Selection handling via event filter
        widget->installEventFilter(this);
        widget->setProperty("_select_idx", static_cast<int>(idx));
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
            ctx->active_track_id = track_widgets[i]->get_track_index();
            if (ctx->active_track_id.has_value())
                emit ctx->selected_track_changed_signal(ctx->active_track_id.value());
        }
    }
}

void TrackListWidget::_handle_playing_note(const MidiNote& note)
{
    double time_ms = note_time_ms(note, ctx->ppq, ctx->tempo);
    for (auto* w : track_widgets) {
        if (w->get_track_index() == note.track && note.velocity.has_value() && note.velocity.value() > 0) {
            w->get_volume_bar()->setValue(static_cast<double>(note.velocity.value()) / 127.0, time_ms);
            break;
        }
    }
}

// To enable selection via click, add this to your TrackWidget class:
// signals:
//     void clicked(int track_index);
// protected:
//     void mousePressEvent(QMouseEvent* event) override {
//         emit clicked(get_track_index());
//         QFrame::mousePressEvent(event);
//     }