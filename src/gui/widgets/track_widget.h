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

#include <note_naga_engine/note_naga_engine.h>
#include "../components/volume_bar.h"

class TrackWidget : public QFrame {
    Q_OBJECT
public:
    explicit TrackWidget(NoteNagaEngine* engine, NoteNagaTrack* track, QWidget* parent = nullptr);

    NoteNagaTrack* get_track() const { return this->track; }
    VolumeBar* get_volume_bar() const { return volume_bar; }

    void refresh_style(bool selected);

signals:
    void clicked(int track_id);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void _update_track_info(NoteNagaTrack* track, const std::string &param);
    void _toggle_visibility();
    void _toggle_solo();
    void _toggle_mute();
    void _choose_color();
    void _name_edited();
    void _on_instrument_btn_clicked();

private:
    NoteNagaTrack* track;
    NoteNagaEngine* engine;

    QPushButton* instrument_btn;
    QLabel* index_lbl;
    QLineEdit* name_edit;
    QPushButton* color_btn;
    QPushButton* invisible_btn;
    QPushButton* solo_btn;
    QPushButton* mute_btn;
    VolumeBar* volume_bar;
};