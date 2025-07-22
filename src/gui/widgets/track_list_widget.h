#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QColor>
#include <vector>
#include <memory>

#include <note_naga_engine/note_naga_engine.h>
#include "track_widget.h"

/**
 * @brief The TrackListWidget class provides a widget to display and manage a list of tracks in the NoteNaga application.
 */
class TrackListWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a TrackListWidget for managing tracks in the NoteNaga application.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param parent Parent widget.
     */
    explicit TrackListWidget(NoteNagaEngine* engine, QWidget* parent = nullptr);

private slots:
    void initUI();
    void reloadTracks(NoteNagaMidiSeq *seq);
    void handlePlayingNote(const NN_Note_t& note);

private:
    NoteNagaEngine* engine;

    int selected_row;
    std::vector<TrackWidget*> track_widgets;

    QScrollArea* scroll_area;
    QWidget* container;
    QVBoxLayout* vbox;

    void updateSelection(NoteNagaMidiSeq *sequence, int widget_idx);
};