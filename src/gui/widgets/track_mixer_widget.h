#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QMessageBox>
#include <QMap>
#include <QString>
#include <QIcon>

#include <note_naga_engine/note_naga_engine.h>
#include "../components/audio_dial.h"
#include "../components/audio_dial_centered.h"
#include "../components/multi_channel_volume_bar.h"
#include "routing_entry_widget.h"

/**
 * @brief The TrackMixerWidget class provides a user interface for mixing tracks in the NoteNaga engine.
 * It allows users to adjust volume, pan, and other parameters for each track.
 */
class TrackMixerWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a new TrackMixerWidget.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param parent Parent widget (default is nullptr).
     */
    explicit TrackMixerWidget(NoteNagaEngine* engine, QWidget* parent = nullptr);

public slots:
    /**
     * @brief Refreshes the routing table GUI with active routing entries.
     */
    void refresh_routing_table();

private slots:
    void onMinNoteChanged(float value);
    void onMaxNoteChanged(float value);
    void onGlobalOffsetChanged(float value);
    void onGlobalVolumeChanged(float value);
    void onGlobalPanChanged(float value);

    void onAddEntry();
    void onRemoveSelectedEntry();
    void onClearRoutingTable();
    void onDefaultEntries();
    void handlePlayingNote(const NN_Note_t& note, const std::string& device_name, int channel);

private:
    NoteNagaEngine* engine;

    int selected_entry_index;
    int selected_row;

    std::vector<RoutingEntryWidget*> entry_widgets;

    AudioDial* dial_min;
    AudioDial* dial_max;
    AudioDialCentered* dial_offset;
    AudioDial* dial_vol;
    AudioDialCentered* dial_pan;

    QComboBox* device_selector;
    QMap<QString, MultiChannelVolumeBar*> channel_volume_bars;
    QString current_channel_device;

    QVBoxLayout* routing_entries_layout;
    QWidget* routing_entries_container;
    QScrollArea* routing_scroll;

    void initUI();
    void setChannelOutputValue(const std::string& device, int channel_idx, float value, int time_ms = -1);
    void updateEntrySelection(int idx);
};
