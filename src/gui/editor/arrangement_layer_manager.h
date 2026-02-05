#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMenu>

class NoteNagaEngine;
class NoteNagaArrangementTrack;

/**
 * @brief ArrangementLayerManager manages the list of arrangement tracks (layers).
 * 
 * Features:
 * - Add/remove/rename tracks
 * - Drag to reorder tracks
 * - Mute/solo per track
 * - Color selection
 */
class ArrangementLayerManager : public QWidget {
    Q_OBJECT

public:
    explicit ArrangementLayerManager(NoteNagaEngine *engine, QWidget *parent = nullptr);

    /**
     * @brief Refreshes the track list from the arrangement.
     */
    void refreshFromArrangement();

    /**
     * @brief Gets the currently selected track.
     * @return Pointer to selected track or nullptr.
     */
    NoteNagaArrangementTrack* getSelectedTrack() const;

signals:
    /**
     * @brief Emitted when a track is selected.
     * @param track Pointer to the selected track.
     */
    void trackSelected(NoteNagaArrangementTrack* track);

    /**
     * @brief Emitted when tracks are reordered.
     */
    void tracksReordered();

    /**
     * @brief Emitted when a track's properties change.
     * @param track The modified track.
     */
    void trackModified(NoteNagaArrangementTrack* track);

private slots:
    void onAddTrack();
    void onRemoveTrack();
    void onRenameTrack();
    void onTrackSelectionChanged();
    void onTrackDoubleClicked(QListWidgetItem* item);
    void showContextMenu(const QPoint& pos);

private:
    NoteNagaEngine *m_engine;
    
    // UI Components
    QListWidget *m_trackList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    
    void initUI();
    void updateTrackItem(QListWidgetItem* item, NoteNagaArrangementTrack* track);
};
