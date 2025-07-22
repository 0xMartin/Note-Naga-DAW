#pragma once

#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>

#include "../components/volume_bar.h"
#include <note_naga_engine/note_naga_engine.h>

/**
 * @brief TrackWidget is a GUI widget representing a single track in the NoteNaga
 * engine. It displays track information, allows interaction with the track's
 * properties, and provides controls for manipulating the track.
 */
class TrackWidget : public QFrame {
    Q_OBJECT
public:
    /**
     * @brief Constructs a TrackWidget for a specific NoteNagaTrack.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param track Pointer to the NoteNagaTrack to display.
     * @param parent Parent widget.
     */
    explicit TrackWidget(NoteNagaEngine *engine, NoteNagaTrack *track,
                         QWidget *parent = nullptr);

    /**
     * @brief Get the associated NoteNagaTrack.
     * @return Pointer to the NoteNagaTrack.
     */
    NoteNagaTrack *getTrack() const { return this->track; }

    /**
     * @brief Get the volume bar associated with this track.
     * @return Pointer to the VolumeBar.
     */
    VolumeBar *getVolumeBar() const { return volume_bar; }

public slots:
    /**
     * @brief Refreshes the widget's style based on the track's state.
     * @param selected True if the track is selected, false otherwise.
     */
    void refreshStyle(bool selected);

signals:
    /**
     * @brief Signal emitted when the track is clicked.
     * @param track_id ID of the clicked track.
     */
    void clicked(int track_id);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void updateTrackInfo(NoteNagaTrack *track, const std::string &param);
    void onToggleVisibility();
    void onToggleSolo();
    void onToggleMute();
    void onNameEdited();
    void colorSelect();
    void instrumentSelect();

private:
    NoteNagaTrack *track;
    NoteNagaEngine *engine;

    QPushButton *instrument_btn;
    QLabel *index_lbl;
    QLineEdit *name_edit;
    QPushButton *color_btn;
    QPushButton *invisible_btn;
    QPushButton *solo_btn;
    QPushButton *mute_btn;
    VolumeBar *volume_bar;
};