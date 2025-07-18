#include "track_widget.h"

#include "../core/shared.h"
#include "../core/icons.h"
#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>

TrackWidget::TrackWidget(int track_id_, AppContext* ctx_, Mixer *mixer_, QWidget* parent)
    : QFrame(parent), ctx(ctx_), mixer(mixer_), track_id(track_id_)
{
    connect(ctx, &AppContext::track_meta_changed_signal, this, &TrackWidget::_update_track_info);
    setObjectName("TrackWidget");

    QHBoxLayout* main_hbox = new QHBoxLayout(this);
    main_hbox->setContentsMargins(2, 2, 2, 2);
    main_hbox->setSpacing(4);

    instrument_btn = new QPushButton();
    instrument_btn->setObjectName("InstrumentButton");
    instrument_btn->setFlat(true);
    instrument_btn->setCursor(Qt::PointingHandCursor);
    instrument_btn->setIconSize(QSize(32, 32));
    connect(instrument_btn, &QPushButton::clicked, this, &TrackWidget::_on_instrument_btn_clicked);
    main_hbox->addWidget(instrument_btn, 0, Qt::AlignVCenter);

    QFrame* right_frame = new QFrame();
    right_frame->setObjectName("TrackWidgetContent");
    QVBoxLayout* right_layout = new QVBoxLayout(right_frame);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(3);
    main_hbox->addWidget(right_frame, 1);

    QFrame* header = new QFrame();
    header->setObjectName("TrackWidgetHeader");
    QHBoxLayout* header_hbox = new QHBoxLayout(header);
    header_hbox->setContentsMargins(0, 0, 0, 0);
    header_hbox->setSpacing(3);

    index_lbl = new QLabel(QString::number(track_id + 1));
    index_lbl->setObjectName("TrackWidgetIndex");
    index_lbl->setAlignment(Qt::AlignCenter);
    index_lbl->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    header_hbox->addWidget(index_lbl, 0);

    name_edit = new QLineEdit("Track Name");
    name_edit->setObjectName("TrackWidgetName");
    name_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    name_edit->setFrame(false);
    name_edit->setStyleSheet("background: transparent; color: #fff; border: none; font-weight: bold; font-size: 12px;");
    connect(name_edit, &QLineEdit::editingFinished, this, &TrackWidget::_name_edited);
    header_hbox->addWidget(name_edit, 1);

    color_btn = new QPushButton();
    color_btn->setObjectName("ColorButton");
    color_btn->setToolTip("Change Track Color");
    color_btn->setFlat(true);
    color_btn->setCursor(Qt::PointingHandCursor);
    color_btn->setIconSize(QSize(16, 16));
    connect(color_btn, &QPushButton::clicked, this, &TrackWidget::_choose_color);
    header_hbox->addWidget(color_btn, 0);

    invisible_btn = new QPushButton();
    invisible_btn->setObjectName("VisibilityButton");
    invisible_btn->setCheckable(true);
    invisible_btn->setToolTip("Toggle Track Visibility");
    invisible_btn->setFlat(true);
    invisible_btn->setCursor(Qt::PointingHandCursor);
    invisible_btn->setIconSize(QSize(16, 16));
    connect(invisible_btn, &QPushButton::clicked, this, &TrackWidget::_toggle_visibility);
    header_hbox->addWidget(invisible_btn, 0);

    solo_btn = new QPushButton();
    solo_btn->setObjectName("SoloButton");
    solo_btn->setCheckable(true);
    solo_btn->setToolTip("Toggle Solo Mode");
    solo_btn->setIcon(QIcon(":/icons/solo.svg"));
    solo_btn->setFlat(true);
    solo_btn->setCursor(Qt::PointingHandCursor);
    solo_btn->setIconSize(QSize(16, 16));
    connect(solo_btn, &QPushButton::clicked, this, &TrackWidget::_toggle_solo);
    header_hbox->addWidget(solo_btn, 0);

    mute_btn = new QPushButton();
    mute_btn->setObjectName("MuteButton");
    mute_btn->setCheckable(true);
    mute_btn->setToolTip("Toggle Track Mute/Play");
    mute_btn->setFlat(true);
    mute_btn->setCursor(Qt::PointingHandCursor);
    mute_btn->setIconSize(QSize(16, 16));
    connect(mute_btn, &QPushButton::clicked, this, &TrackWidget::_toggle_mute);
    header_hbox->addWidget(mute_btn, 0);

    right_layout->addWidget(header);

    volume_bar = new VolumeBar(0.0);
    volume_bar->setObjectName("VolumeBar");
    right_layout->addWidget(volume_bar);

    setLayout(main_hbox);
    _update_track_info(this->track_id);
    refresh_style(false);
    setFocusPolicy(Qt::StrongFocus);
}

void TrackWidget::_update_track_info(int track_id) {
    if (this->track_id != track_id) return;

    auto track_info = ctx->get_track_by_id(track_id);
    if (track_info) {
        name_edit->setText(track_info->name);
        name_edit->setToolTip(track_info->name);

        index_lbl->setText(QString::number(track_id + 1));

        auto instrument = find_instrument_by_index(track_info->instrument.value_or(0));
        if (instrument) {
            instrument_btn->setIcon(instrument_icon(instrument->icon));
            instrument_btn->setToolTip(instrument->name);
        } else {
            instrument_btn->setIcon(instrument_icon("vinyl"));
            instrument_btn->setToolTip("Unknown instrument");
        }

        solo_btn->setChecked(track_info->solo);
        mute_btn->setChecked(track_info->muted);
        invisible_btn->setChecked(!track_info->visible);

        invisible_btn->setIcon(QIcon(invisible_btn->isChecked() ? ":/icons/eye-not-visible.svg" : ":/icons/eye-visible.svg"));
        mute_btn->setIcon(QIcon(mute_btn->isChecked() ? ":/icons/sound-off.svg" : ":/icons/sound-on.svg"));

        volume_bar->setValue(0.0);
        color_btn->setIcon(svg_str_icon(COLOR_SVG_ICON, track_info->color, 16));
    }
}

void TrackWidget::_toggle_visibility() {
    auto track_info = ctx->get_track_by_id(track_id);
    if (!track_info) return;
    track_info->visible = !invisible_btn->isChecked();
    emit visibility_changed_signal(track_id, track_info->visible);
    emit ctx->track_meta_changed_signal(track_id);
}

void TrackWidget::_toggle_solo() {
    mixer->solo_track(track_id, solo_btn->isChecked());
}

void TrackWidget::_toggle_mute() {
    mixer->mute_track(track_id, mute_btn->isChecked());
}

void TrackWidget::_choose_color() {
    auto track = ctx->get_track_by_id(track_id);
    if (!track) return;
    QColor col = QColorDialog::getColor(track->color, this, "Select Track Color");
    if (col.isValid()) {
        track->color = col;
        emit color_changed_signal(track_id, col);
        emit ctx->track_meta_changed_signal(track_id);
    }
}

void TrackWidget::_name_edited() {
    auto track = ctx->get_track_by_id(track_id);
    if (!track) return;
    QString new_name = name_edit->text();
    name_edit->setToolTip(new_name);
    if (new_name != track->name) {
        track->name = new_name;
        emit name_changed_signal(track_id, new_name);
        emit ctx->track_meta_changed_signal(track_id);
    }
}

void TrackWidget::_on_instrument_btn_clicked() {
    auto track_info = ctx->get_track_by_id(track_id);
    InstrumentSelectorDialog dlg(this, GM_INSTRUMENTS, instrument_icon, track_info->instrument);
    if (dlg.exec() == QDialog::Accepted) {
        int gm_index = dlg.get_selected_gm_index();
        auto instrument = find_instrument_by_index(gm_index);
        if (instrument) {
            track_info->instrument = gm_index;
            emit ctx->track_meta_changed_signal(track_id);
            emit instrument_changed_signal(track_id, gm_index);
        }
    }
}

void TrackWidget::mousePressEvent(QMouseEvent* event) {
    emit clicked(track_id);
    QFrame::mousePressEvent(event);
}

void TrackWidget::refresh_style(bool selected) {
    QString base_style = R"(
        QFrame#TrackWidget {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 2px;
        }
        QFrame#TrackWidgetContent {
            background: transparent;
        }
        QFrame#TrackWidgetHeader {
            background: transparent;
            border-radius: 0px;
        }
        QLabel#TrackWidgetIndex {
            background: #304060;
            color: #fff;
            font-weight: bold;
            font-size: 11px;
            min-width: 18px; max-width: 18px;
            border-radius: 5px;
            padding: 1px 3px;
        }
        QPushButton#ColorButton,
        QPushButton#VisibilityButton,
        QPushButton#MuteButton,
        QPushButton#SoloButton {
            min-width: 16px;
            max-width: 16px;
            min-height: 16px;
            max-height: 16px;
            padding: 0px;
            border: none;
            background: transparent;
        }
        QPushButton#MuteButton:checked,
        QPushButton#VisibilityButton:checked,
        QPushButton#SoloButton:checked {
            background: #3477c0;
            border: 1px solid #79b8ff;
        }
        QPushButton#InstrumentButton {
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
            padding: 0px 8px;
            border: none;
            background: transparent;
        }
    )";
    QString style = selected
        ? base_style.arg("#273a51", "#3477c0")
        : base_style.arg("#2F3139", "#494d56");
    setStyleSheet(style);
    update();
}