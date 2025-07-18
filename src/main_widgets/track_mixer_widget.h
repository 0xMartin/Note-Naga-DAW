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

#include "../core/app_context.h"
#include "../core/mixer.h"
#include "../core/shared.h"
#include "../widgets/audio_dial.h"
#include "../widgets/audio_dial_centered.h"
#include "../widgets/multi_channel_volume_bar.h"
#include "routing_entry_widget.h"

class TrackMixerWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrackMixerWidget(AppContext* ctx, Mixer* mixer, QWidget* parent = nullptr);

    void refresh_routing_table();

public slots:
    void on_min_note_changed(float value);
    void on_max_note_changed(float value);
    void on_global_offset_changed(float value);
    void on_global_volume_changed(float value);
    void on_global_pan_changed(float value);

private slots:
    void _on_add_entry();
    void _on_remove_selected_entry();
    void _on_clear_routing_table();
    void _on_default_entries();
    void _handle_playing_note(const MidiNote& note, const QString& device_name, int channel);

private:
    void set_channel_output_value(const QString& device, int channel_idx, float value, int time_ms = -1);
    void _init_ui();
    void _update_entry_selection(int idx);

    AppContext* ctx;
    Mixer* mixer;
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
};
