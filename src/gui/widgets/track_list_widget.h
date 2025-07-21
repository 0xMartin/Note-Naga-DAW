#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QColor>
#include <vector>
#include <memory>

#include <note_naga_engine.h>
#include "track_widget.h"

// Moderní list widget s TrackWidgety ve scrollovací oblasti.
class TrackListWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrackListWidget(NoteNagaEngine* engine, QWidget* parent = nullptr);

private slots:
    void _init_ui();
    void _reload_tracks(NoteNagaMIDISeq *seq);
    void _handle_playing_note(const NoteNagaNote& note);

private:
    void _update_selection(NoteNagaMIDISeq *sequence, int widget_idx);

    NoteNagaEngine* engine;

    int selected_row;
    std::vector<TrackWidget*> track_widgets;

    QScrollArea* scroll_area;
    QWidget* container;
    QVBoxLayout* vbox;
};