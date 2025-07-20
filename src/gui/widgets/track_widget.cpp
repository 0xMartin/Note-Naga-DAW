#include "track_widget.h"

#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>

#include "../icons.h"
#include "../dialogs/instrument_selector_dialog.h"

TrackWidget::TrackWidget(NoteNagaEngine *engine_, NoteNagaTrack* track_, QWidget *parent)
    : QFrame(parent), engine(engine_), track(track_)
{
    connect(track, &NoteNagaTrack::meta_changed_signal, this, &TrackWidget::_update_track_info);
    setObjectName("TrackWidget");

    QHBoxLayout *main_hbox = new QHBoxLayout(this);
    main_hbox->setContentsMargins(2, 2, 2, 2);
    main_hbox->setSpacing(4);

    instrument_btn = new QPushButton();
    instrument_btn->setObjectName("InstrumentButton");
    instrument_btn->setFlat(true);
    instrument_btn->setCursor(Qt::PointingHandCursor);
    instrument_btn->setIconSize(QSize(32, 32));
    connect(instrument_btn, &QPushButton::clicked, this, &TrackWidget::_on_instrument_btn_clicked);
    main_hbox->addWidget(instrument_btn, 0, Qt::AlignVCenter);

    QFrame *right_frame = new QFrame();
    right_frame->setObjectName("TrackWidgetContent");
    QVBoxLayout *right_layout = new QVBoxLayout(right_frame);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(3);
    main_hbox->addWidget(right_frame, 1);

    QFrame *header = new QFrame();
    header->setObjectName("TrackWidgetHeader");
    QHBoxLayout *header_hbox = new QHBoxLayout(header);
    header_hbox->setContentsMargins(0, 0, 0, 0);
    header_hbox->setSpacing(3);

    index_lbl = new QLabel(QString::number(this->track->get_id() + 1));
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
    _update_track_info(this->track, "");
    refresh_style(false);
    setFocusPolicy(Qt::StrongFocus);
}

void TrackWidget::_update_track_info(NoteNagaTrack* track, const QString& param)
{
    if (this->track != track)
        return;

    name_edit->setText(track->get_name());
    name_edit->setToolTip(track->get_name());

    index_lbl->setText(QString::number(track->get_id() + 1));

    auto instrument = find_instrument_by_index(track->get_instrument().value_or(0));
    if (instrument)
    {
        instrument_btn->setIcon(instrument_icon(instrument->icon));
        instrument_btn->setToolTip(instrument->name);
    }
    else
    {
        instrument_btn->setIcon(instrument_icon("vinyl"));
        instrument_btn->setToolTip("Unknown instrument");
    }

    solo_btn->setChecked(track->is_solo());
    mute_btn->setChecked(track->is_muted());
    invisible_btn->setChecked(!track->is_visible());

    invisible_btn->setIcon(QIcon(invisible_btn->isChecked() ? ":/icons/eye-not-visible.svg" : ":/icons/eye-visible.svg"));
    mute_btn->setIcon(QIcon(mute_btn->isChecked() ? ":/icons/sound-off.svg" : ":/icons/sound-on.svg"));

    volume_bar->setValue(0.0);
    color_btn->setIcon(svg_str_icon(COLOR_SVG_ICON, track->get_color(), 16));
}

void TrackWidget::_toggle_visibility()
{
    track->set_visible(!invisible_btn->isChecked());
}

void TrackWidget::_toggle_solo()
{
    engine->solo_track(track, solo_btn->isChecked());
}

void TrackWidget::_toggle_mute()
{
    engine->mute_track(track, mute_btn->isChecked());
}

void TrackWidget::_choose_color()
{
    QColor col = QColorDialog::getColor(track->get_color(), this, "Select Track Color");
    if (col.isValid())
    {
        track->set_color(col);
    }
}

void TrackWidget::_name_edited()
{
    QString new_name = name_edit->text();
    track->set_name(new_name);
}

void TrackWidget::_on_instrument_btn_clicked()
{
    InstrumentSelectorDialog dlg(this, GM_INSTRUMENTS, instrument_icon, track->get_instrument());
    if (dlg.exec() == QDialog::Accepted)
    {
        int gm_index = dlg.get_selected_gm_index();
        auto instrument = find_instrument_by_index(gm_index);
        if (instrument)
        {
            track->set_instrument(gm_index);
        }
    }
}

void TrackWidget::mousePressEvent(QMouseEvent *event)
{
    emit clicked(this->track->get_id());
    QFrame::mousePressEvent(event);
}

void TrackWidget::refresh_style(bool selected)
{
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