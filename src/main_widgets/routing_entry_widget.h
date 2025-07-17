#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QColor>
#include <QIcon>
#include "../core/app_context.h"
#include "../core/mixer.h"
#include "../widgets/audio_dial.h"
#include "../widgets/audio_dial_centered.h"

class RoutingEntryWidget : public QFrame {
    Q_OBJECT
public:
    explicit RoutingEntryWidget(TrackRountingEntry& entry, Mixer* mixer, AppContext* ctx, QWidget* parent = nullptr);

    void refresh_style(bool selected);
    
signals:
    void clicked();

protected:
    // For custom selection in parent
    void mousePressEvent(QMouseEvent* event) override;

private:
    TrackRountingEntry& entry;
    Mixer* mixer;
    AppContext* ctx;

    QComboBox* track_combo;
    QComboBox* output_combo;

    AudioDial* channel_dial;
    AudioDial* volume_dial;
    AudioDialCentered* pan_dial;
    AudioDialCentered* offset_dial;

    void _populate_track_combo();
    void _populate_output_combo();
    void _set_combo_selections();

private slots:
    void on_track_info_changed(int track_id);
    void _on_track_changed(int idx);
    void _on_device_changed(int idx);
    void _on_channel_changed(float val);
    void _on_volume_changed(float val);
    void _on_offset_changed(float val);
    void on_global_pan_changed(float value);
};