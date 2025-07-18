#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QColor>
#include <vector>
#include <memory>

#include "track_widget.h"
#include "../core/app_context.h"
#include "../core/mixer.h"
#include "../core/shared.h"

// Moderní list widget s TrackWidgety ve scrollovací oblasti.
class TrackListWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrackListWidget(AppContext* ctx, Mixer *mixer, QWidget* parent = nullptr);

signals:
    void track_selected_signal(int);
    void visibility_changed_signal(int, bool);
    void muted_changed_signal(int, bool);
    void color_changed_signal(int, QColor);
    void instrument_changed_signal(int, int);
    void name_changed_signal(int, QString); // track_index, new_name

private slots:
    void _init_ui();
    void _reload_tracks();
    void _handle_playing_note(const MidiNote& note, int track_id);

private:
    void _update_selection(int idx);

    AppContext *ctx;
    Mixer *mixer;

    int selected_row;
    std::vector<TrackWidget*> track_widgets;

    QScrollArea* scroll_area;
    QWidget* container;
    QVBoxLayout* vbox;
};