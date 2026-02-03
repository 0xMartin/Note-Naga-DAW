#include "track_widget.h"

#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>

#include "../nn_gui_utils.h"
#include "../dialogs/instrument_selector_dialog.h"

TrackWidget::TrackWidget(NoteNagaEngine *engine_, NoteNagaTrack* track_, QWidget *parent)
    : QFrame(parent), engine(engine_), track(track_), m_isTempoTrackLayout(false),
      m_normalContent(nullptr), m_tempoContent(nullptr), tempo_active_btn(nullptr)
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

    // Normal track content
    m_normalContent = new QWidget();
    QVBoxLayout *right_layout = new QVBoxLayout(m_normalContent);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(3);
    main_hbox->addWidget(m_normalContent, 1);

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
    connect(name_edit, &QLineEdit::textChanged, this, &TrackWidget::onNameTextChanged);
    header_hbox->addWidget(name_edit, 1);

    // Unsaved indicator dot - shows when track name hasn't been applied yet
    unsaved_indicator = new QLabel();
    unsaved_indicator->setObjectName("UnsavedIndicator");
    unsaved_indicator->setFixedSize(8, 8);
    unsaved_indicator->setStyleSheet(
        "QLabel#UnsavedIndicator { background-color: #ff9900; border-radius: 4px; }");
    unsaved_indicator->setToolTip("Track name not yet applied (press Enter to apply)");
    unsaved_indicator->setVisible(false);
    header_hbox->addWidget(unsaved_indicator, 0);

    color_btn = create_small_button("", "Change Track Color", "ColorButton", 18);
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
    
    // Tempo track special content (initially hidden)
    m_tempoContent = new QWidget();
    m_tempoContent->setVisible(false);
    QHBoxLayout *tempo_layout = new QHBoxLayout(m_tempoContent);
    tempo_layout->setContentsMargins(4, 4, 4, 4);
    tempo_layout->setSpacing(8);
    
    QLabel *tempo_label = new QLabel("Tempo Track");
    tempo_label->setStyleSheet("color: #ff8c3c; font-weight: bold; font-size: 13px;");
    tempo_layout->addWidget(tempo_label, 1);
    
    tempo_active_btn = new QPushButton("Active");
    tempo_active_btn->setObjectName("TempoActiveBtn");
    tempo_active_btn->setCheckable(true);
    tempo_active_btn->setChecked(true);
    tempo_active_btn->setToolTip("Toggle tempo track - when inactive, fixed BPM is used");
    tempo_active_btn->setStyleSheet(R"(
        QPushButton#TempoActiveBtn {
            background: #304060;
            border: 1px solid #3477c0;
            border-radius: 4px;
            color: #8ab4d8;
            font-size: 11px;
            font-weight: bold;
            padding: 4px 12px;
        }
        QPushButton#TempoActiveBtn:hover {
            background: #3a5070;
        }
        QPushButton#TempoActiveBtn:checked {
            background: #2a6030;
            border-color: #40a050;
            color: #90d090;
        }
        QPushButton#TempoActiveBtn:checked:hover {
            background: #306838;
        }
    )");
    connect(tempo_active_btn, &QPushButton::clicked, this, &TrackWidget::onToggleTempoActive);
    tempo_layout->addWidget(tempo_active_btn, 0);
    
    main_hbox->addWidget(m_tempoContent, 1);

    setLayout(main_hbox);
    updateTrackInfo(this->track, "");
    setFocusPolicy(Qt::StrongFocus);
}

void TrackWidget::updateTrackInfo(NoteNagaTrack* track, const std::string &param)
{
    if (this->track != track)
        return;

    // Handle tempo track specially - switch to simplified layout
    bool isTempoTrack = track->isTempoTrack();
    if (isTempoTrack != m_isTempoTrackLayout) {
        m_isTempoTrackLayout = isTempoTrack;
        m_normalContent->setVisible(!isTempoTrack);
        m_tempoContent->setVisible(isTempoTrack);
    }
    
    if (isTempoTrack) {
        instrument_btn->setIcon(QIcon(":/icons/tempo.svg"));
        instrument_btn->setToolTip("Tempo Track - Controls dynamic tempo changes");
        instrument_btn->setEnabled(false);
        
        // Update tempo active button state
        if (tempo_active_btn) {
            tempo_active_btn->setChecked(track->isTempoTrackActive());
            tempo_active_btn->setText(track->isTempoTrackActive() ? "Active" : "Inactive");
        }
        return;
    }
    
    // Normal track handling below
    instrument_btn->setEnabled(true);

    // Block signals to prevent triggering onNameTextChanged
    name_edit->blockSignals(true);
    name_edit->setText(QString::fromStdString(track->getName()));
    name_edit->blockSignals(false);
    name_edit->setToolTip(QString::fromStdString(track->getName()));
    
    // Hide unsaved indicator since the name matches the track
    unsaved_indicator->setVisible(false);

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

void TrackWidget::onToggleTempoActive()
{
    if (!track->isTempoTrack() || !tempo_active_btn) return;
    
    bool active = tempo_active_btn->isChecked();
    track->setTempoTrackActive(active);
    tempo_active_btn->setText(active ? "Active" : "Inactive");
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
    // Hide the unsaved indicator since the name has been applied
    unsaved_indicator->setVisible(false);
}

void TrackWidget::onNameTextChanged(const QString &text)
{
    // Show the unsaved indicator if the text differs from the track's name
    QString currentTrackName = QString::fromStdString(track->getName());
    unsaved_indicator->setVisible(text != currentTrackName);
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