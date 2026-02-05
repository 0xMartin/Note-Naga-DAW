#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QColor>
#include <QMenu>
#include <QTimer>
#include <vector>

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

    /**
     * @brief Gets the title widget that will be inserted into the dock title bar.
     * @return Pointer to the title widget.
     */
    QWidget *getTitleWidget() const { return this->title_widget; }
    
    /**
     * @brief Returns preferred size hint for dock layout.
     */
    QSize sizeHint() const override { return QSize(280, 300); }

private:
    NoteNagaEngine* engine;

    int selected_row;
    std::vector<TrackWidget*> track_widgets;
    NoteNagaTrack* solo_view_track_ = nullptr; // Track currently in solo view mode

    QWidget *title_widget;
    QScrollArea* scroll_area;
    QWidget* container;
    QVBoxLayout* vbox;
    QTimer* meter_update_timer_;

    void initTitleUI();
    void initUI();
    void updateSelection(NoteNagaMidiSeq *sequence, int widget_idx);
    void showTrackContextMenu(TrackWidget *trackWidget, const QPoint &globalPos);
    void updateTrackMeters();

/*******************************************************************************************************/
// Signal and Slots
/*******************************************************************************************************/
public slots:
    /**
     * @brief Select and scroll to a specific track
     * @param track The track to select
     */
    void selectAndScrollToTrack(NoteNagaTrack *track);

private slots:
    void reloadTracks(NoteNagaMidiSeq *seq);
    void onAddTrack();
    void onAddTempoTrack();
    void onRemoveTrack();
    void onClearTracks();
    void onReloadTracks();
    void onReloadAllSoundFonts();
    void onDuplicateTrack();
    void onMoveTrackUp();
    void onMoveTrackDown();
    
signals:
    /**
     * @brief Signal emitted when solo view is toggled for a track.
     * @param track Pointer to the track.
     * @param enabled True to show only this track, false to show all.
     */
    void soloViewToggled(NoteNagaTrack *track, bool enabled);
};