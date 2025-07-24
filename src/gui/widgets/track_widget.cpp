#include "track_widget.h"

#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>

#include "../nn_gui_utils.h"
#include "../dialogs/instrument_selector_dialog.h"

TrackWidget::TrackWidget(NoteNagaEngine *engine_, NoteNagaTrack* track_, QWidget *parent)
    : QFrame(parent), engine(engine_), track(track_)
{
    connect(track, &NoteNagaTrack::metadataChanged, this, &TrackWidget::updateTrackInfo);
    setObjectName("TrackWidget");

    QHBoxLayout *main_hbox = new QHBoxLayout(this);
    main_hbox->setContentsMargins(0, 0, 0, 0);
    main_hbox->setSpacing(0);

    instrument_btn = new QPushButton();
    instrument_btn->setObjectName("InstrumentButton");
    instrument_btn->setFlat(true);
    instrument_btn->setCursor(Qt::PointingHandCursor);
    instrument_btn->setIconSize(QSize(32, 32));
    connect(instrument_btn, &QPushButton::clicked, this, &TrackWidget::instrumentSelect);
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

    index_lbl = new QLabel(QString::number(this->track->getId() + 1));
    index_lbl->setObjectName("TrackWidgetIndex");
    index_lbl->setAlignment(Qt::AlignCenter);
    index_lbl->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    header_hbox->addWidget(index_lbl, 0);

    name_edit = new QLineEdit("Track Name");
    name_edit->setObjectName("TrackWidgetName");
    name_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    name_edit->setFrame(false);
    name_edit->setStyleSheet("background: transparent; color: #fff; border: none; font-weight: bold; font-size: 12px;");
    connect(name_edit, &QLineEdit::editingFinished, this, &TrackWidget::onNameEdited);
    header_hbox->addWidget(name_edit, 1);

    color_btn = create_small_button(":/icons/color.svg", "Change Track Color", "ColorButton", 18);
    connect(color_btn, &QPushButton::clicked, this, &TrackWidget::colorSelect);
    header_hbox->addWidget(color_btn, 0);

    invisible_btn = create_small_button(":/icons/eye-visible.svg", "Toggle Track Visibility", "InvisibleButton", 18);
    invisible_btn->setCheckable(true);
    connect(invisible_btn, &QPushButton::clicked, this, &TrackWidget::onToggleVisibility);
    header_hbox->addWidget(invisible_btn, 0);

    solo_btn = create_small_button(":/icons/solo.svg", "Toggle Solo Mode", "SoloButton", 18);
    solo_btn->setCheckable(true);
    connect(solo_btn, &QPushButton::clicked, this, &TrackWidget::onToggleSolo);
    header_hbox->addWidget(solo_btn, 0);

    mute_btn = create_small_button(":/icons/sound-on.svg", "Toggle Track Mute/Play", "MuteButton", 18);
    mute_btn->setCheckable(true);
    connect(mute_btn, &QPushButton::clicked, this, &TrackWidget::onToggleMute);
    header_hbox->addWidget(mute_btn, 0);

    right_layout->addWidget(header);

    volume_bar = new VolumeBar(0.0);
    volume_bar->setRange(0, 127);
    volume_bar->setObjectName("VolumeBar");
    right_layout->addWidget(volume_bar);

    setLayout(main_hbox);
    updateTrackInfo(this->track, "");
    setFocusPolicy(Qt::StrongFocus);
}

void TrackWidget::updateTrackInfo(NoteNagaTrack* track, const std::string &param)
{
    if (this->track != track)
        return;

    name_edit->setText(QString::fromStdString(track->getName()));
    name_edit->setToolTip(QString::fromStdString(track->getName()));

    index_lbl->setText(QString::number(track->getId() + 1));

    auto instrument = nn_find_instrument_by_index(track->getInstrument().value_or(0));
    if (instrument)
    {
        instrument_btn->setIcon(instrument_icon(QString::fromStdString(instrument->icon)));
        instrument_btn->setToolTip(QString::fromStdString(instrument->name));
    }
    else
    {
        instrument_btn->setIcon(instrument_icon("vinyl"));
        instrument_btn->setToolTip("Unknown instrument");
    }

    solo_btn->setChecked(track->isSolo());
    mute_btn->setChecked(track->isMuted());
    invisible_btn->setChecked(!track->isVisible());

    invisible_btn->setIcon(QIcon(invisible_btn->isChecked() ? ":/icons/eye-not-visible.svg" : ":/icons/eye-visible.svg"));
    mute_btn->setIcon(QIcon(mute_btn->isChecked() ? ":/icons/sound-off.svg" : ":/icons/sound-on.svg"));

    volume_bar->setValue(0.0);
    color_btn->setIcon(svg_str_icon(COLOR_SVG_ICON, track->getColor().toQColor(), 16));
}

void TrackWidget::onToggleVisibility()
{
    track->setVisible(!invisible_btn->isChecked());
}

void TrackWidget::onToggleSolo()
{
    engine->soloTrack(track, solo_btn->isChecked());
}

void TrackWidget::onToggleMute()
{
    engine->muteTrack(track, mute_btn->isChecked());
}

void TrackWidget::colorSelect()
{
    QColor col = QColorDialog::getColor(track->getColor().toQColor(), this, "Select Track Color");
    if (col.isValid())
    {
        track->setColor(NN_Color_t::fromQColor(col));
    }
}

void TrackWidget::onNameEdited()
{
    QString new_name = name_edit->text();
    track->setName(new_name.toStdString());
}

void TrackWidget::instrumentSelect()
{
    InstrumentSelectorDialog dlg(this, GM_INSTRUMENTS, instrument_icon, track->getInstrument());
    if (dlg.exec() == QDialog::Accepted)
    {
        int gm_index = dlg.getSelectedGMIndex();
        auto instrument = nn_find_instrument_by_index(gm_index);
        if (instrument)
        {
            track->setInstrument(gm_index);
        }
    }
}

void TrackWidget::mousePressEvent(QMouseEvent *event)
{
    emit clicked(this->track->getId());
    QFrame::mousePressEvent(event);
}

void TrackWidget::refreshStyle(bool selected, bool darker_bg)
{
    QString base_style = R"(
        QFrame#TrackWidget {
            background: %1;
            border: 1px solid %2;
            padding: 2px;
        }
        QFrame#TrackWidgetContent {
            background: transparent;
        }
        QFrame#TrackWidgetHeader {
            background: transparent;
        }
        QLabel#TrackWidgetIndex {
            background: #304060;
            border-radius: 5px;
            color: #fff;
            font-weight: bold;
            font-size: 11px;
            min-width: 18px; max-width: 18px;
            padding: 1px 3px;
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

    QString bg = darker_bg ? "#282930" : "#2F3139";
    QString style = selected ? base_style.arg("#273a51", "#3477c0")
                             : base_style.arg(bg, "#494d56");
    setStyleSheet(style);
    update();
}