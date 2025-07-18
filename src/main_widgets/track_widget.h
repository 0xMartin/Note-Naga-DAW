#pragma once

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QColorDialog>
#include <QSizePolicy>
#include <QLineEdit>
#include <QDialog>
#include <QIcon>
#include <QColor>
#include <QString>

#include "../core/app_context.h"
#include "../core/mixer.h"
#include "../widgets/volume_bar.h"
#include "../dialogs/instrument_selector_dialog.h"
#include "../core/shared.h"
#include "../core/icons.h"

class TrackWidget : public QFrame {
    Q_OBJECT
public:
    explicit TrackWidget(int track_id, AppContext* ctx, Mixer *mixer, QWidget* parent = nullptr);

    int get_track_id() const { return track_id; }
    VolumeBar* get_volume_bar() const { return volume_bar; }

    void refresh_style(bool selected);

signals:
    void instrument_changed_signal(int track_id, int instrument_index);
    void visibility_changed_signal(int track_id, bool visible);
    void muted_changed_signal(int track_id, bool muted);
    void color_changed_signal(int track_id, QColor color);
    void name_changed_signal(int track_id, QString new_name);
    void clicked(int track_id);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void _update_track_info(int track_id);
    void _toggle_visibility();
    void _toggle_solo();
    void _toggle_mute();
    void _choose_color();
    void _name_edited();
    void _on_instrument_btn_clicked();

private:
    AppContext *ctx;
    Mixer *mixer;
    int track_id;

    QPushButton* instrument_btn;
    QLabel* index_lbl;
    QLineEdit* name_edit;
    QPushButton* color_btn;
    QPushButton* invisible_btn;
    QPushButton* solo_btn;
    QPushButton* mute_btn;
    VolumeBar* volume_bar;
};