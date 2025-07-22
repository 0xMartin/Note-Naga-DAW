#pragma once

#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

#include "../components/audio_dial.h"
#include "../components/audio_dial_centered.h"
#include <note_naga_engine/note_naga_engine.h>

/**
 * @brief Widget for displaying and editing a routing entry in the NoteNaga app.
 */
class RoutingEntryWidget : public QFrame {
    Q_OBJECT
public:
    /**
     * @brief Constructs a RoutingEntryWidget.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param entry Pointer to the NoteNagaRoutingEntry to display/edit.
     * @param parent Parent widget (default is nullptr).
     */
    explicit RoutingEntryWidget(NoteNagaEngine *engine, NoteNagaRoutingEntry *entry,
                                QWidget *parent = nullptr);

public slots:
    /**
     * @brief Refreshes the style of the widget based on selection.
     * @param selected True if the widget is selected, false otherwise.
     */
    void refresh_style(bool selected);

signals:
    /**
     * @brief Signal emitted when the routing entry is clicked.
     */
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    NoteNagaRoutingEntry *entry;
    NoteNagaEngine *engine;

    QComboBox *track_combo;
    QComboBox *output_combo;

    AudioDial *channel_dial;
    AudioDial *volume_dial;
    AudioDialCentered *pan_dial;
    AudioDialCentered *offset_dial;

    void populateTrackComboBox(NoteNagaTrack *track);
    void populateOutputComboBox();
    void setComboBoxSelections();

private slots:
    void onTrackMetadataChanged(NoteNagaTrack *track);
    void onTrackChanged(int idx);
    void onDeviceChanged(int idx);
    void onChannelChanged(float val);
    void onVolumeChanged(float val);
    void onOffsetChanged(float val);
    void onGlobalPanChanged(float value);
};