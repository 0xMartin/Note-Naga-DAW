#include "track_widget.h"

#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>

#include "../nn_gui_utils.h"
#include "../dialogs/instrument_selector_dialog.h"
#include <note_naga_engine/synth/synth_fluidsynth.h>

TrackWidget::TrackWidget(NoteNagaEngine *engine_, NoteNagaTrack* track_, QWidget *parent)
    : QFrame(parent), engine(engine_), track(track_), m_isTempoTrackLayout(false),
      m_normalContent(nullptr), m_tempoContent(nullptr), tempo_active_btn(nullptr),
      m_volumeDial(nullptr), m_panDial(nullptr), m_stereoMeter(nullptr),
      m_dialsWidget(nullptr), m_leftPanel(nullptr), m_selected(false), m_darkerBg(false)
{
    connect(track, &NoteNagaTrack::metadataChanged, this, &TrackWidget::updateTrackInfo);
    setObjectName("TrackWidget");
    setFixedHeight(64);  // Height to fit larger dials with value text

    QHBoxLayout *main_hbox = new QHBoxLayout(this);
    main_hbox->setContentsMargins(0, 0, 4, 0);
    main_hbox->setSpacing(0);

    // =========================================================================
    // Column 1: TrackInfoPanel - dark bg with rounded right corners
    //           Contains: Left colored panel + name/buttons
    // =========================================================================
    m_normalContent = new QWidget();
    m_normalContent->setObjectName("TrackInfoPanel");
    m_normalContent->setFixedWidth(170);
    // Background color is set dynamically in refreshStyle() for selection support
    QHBoxLayout *info_outer_layout = new QHBoxLayout(m_normalContent);
    info_outer_layout->setContentsMargins(0, 0, 0, 0);
    info_outer_layout->setSpacing(0);

    // Left panel with instrument icon + track number (inside TrackInfoPanel)
    m_leftPanel = new QWidget();
    m_leftPanel->setObjectName("TrackLeftPanel");
    m_leftPanel->setFixedWidth(52);
    QHBoxLayout *left_layout = new QHBoxLayout(m_leftPanel);
    left_layout->setSpacing(0);

    // Instrument button (left column) - larger area for icon
    instrument_btn = new QPushButton();
    instrument_btn->setObjectName("InstrumentButton");
    instrument_btn->setFlat(true);
    instrument_btn->setCursor(Qt::PointingHandCursor);
    connect(instrument_btn, &QPushButton::clicked, this, &TrackWidget::instrumentSelect);
    left_layout->addWidget(instrument_btn, 1, Qt::AlignCenter);

    // Track number button (right column) - small, for color selection only
    index_btn = new QPushButton(QString::number(this->track->getId() + 1));
    index_btn->setObjectName("TrackIndexButton");
    index_btn->setCursor(Qt::PointingHandCursor);
    index_btn->setToolTip("Click to change track color");
    connect(index_btn, &QPushButton::clicked, this, &TrackWidget::colorSelect);
    left_layout->addWidget(index_btn, 0, Qt::AlignRight | Qt::AlignVCenter);

    info_outer_layout->addWidget(m_leftPanel, 0);

    // Right side: name and buttons
    QWidget *info_right = new QWidget();
    info_right->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *info_layout = new QVBoxLayout(info_right);
    info_layout->setContentsMargins(8, 6, 6, 6);
    info_layout->setSpacing(4);

    // Top row: [Name] [Unsaved indicator]
    QHBoxLayout *top_row = new QHBoxLayout();
    top_row->setContentsMargins(0, 0, 0, 0);
    top_row->setSpacing(4);

    name_edit = new QLineEdit("Track Name");
    name_edit->setObjectName("TrackWidgetName");
    name_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    name_edit->setFixedHeight(22);
    name_edit->setFrame(false);
    name_edit->setStyleSheet("background: transparent; color: #fff; border: none; font-weight: bold; font-size: 12px; padding-left: 0px;");
    connect(name_edit, &QLineEdit::editingFinished, this, &TrackWidget::onNameEdited);
    connect(name_edit, &QLineEdit::textChanged, this, &TrackWidget::onNameTextChanged);
    top_row->addWidget(name_edit, 1);

    unsaved_indicator = new QLabel();
    unsaved_indicator->setObjectName("UnsavedIndicator");
    unsaved_indicator->setFixedSize(8, 8);
    unsaved_indicator->setStyleSheet("QLabel#UnsavedIndicator { background-color: transparent; border-radius: 4px; }");
    unsaved_indicator->setToolTip("Track name not yet applied (press Enter to apply)");
    // Use style instead of visibility to avoid layout shift
    top_row->addWidget(unsaved_indicator, 0);

    info_layout->addLayout(top_row);

    // Bottom row: [Mute] [Solo] [Visibility] [SoloView] [Synth] - with spacing
    QHBoxLayout *btn_row = new QHBoxLayout();
    btn_row->setContentsMargins(0, 0, 0, 0);
    btn_row->setSpacing(6);  // Good spacing between buttons

    mute_btn = create_small_button(":/icons/sound-on.svg", "Toggle Track Mute/Play", "MuteButton", 16);
    mute_btn->setCheckable(true);
    mute_btn->setFixedSize(24, 24);
    connect(mute_btn, &QPushButton::clicked, this, &TrackWidget::onToggleMute);
    btn_row->addWidget(mute_btn);

    solo_btn = create_small_button(":/icons/solo.svg", "Toggle Solo Mode", "SoloButton", 16);
    solo_btn->setCheckable(true);
    solo_btn->setFixedSize(24, 24);
    connect(solo_btn, &QPushButton::clicked, this, &TrackWidget::onToggleSolo);
    btn_row->addWidget(solo_btn);

    invisible_btn = create_small_button(":/icons/eye-visible.svg", "Toggle Track Visibility", "InvisibleButton", 16);
    invisible_btn->setCheckable(true);
    invisible_btn->setFixedSize(24, 24);
    connect(invisible_btn, &QPushButton::clicked, this, &TrackWidget::onToggleVisibility);
    btn_row->addWidget(invisible_btn);

    solo_view_btn = create_small_button(":/icons/solo-view.svg", "Solo View - Show only this track", "SoloViewButton", 16);
    solo_view_btn->setCheckable(true);
    solo_view_btn->setFixedSize(24, 24);
    solo_view_btn->setToolTip("Solo View: Show only this track in editor");
    connect(solo_view_btn, &QPushButton::clicked, this, &TrackWidget::onToggleSoloView);
    btn_row->addWidget(solo_view_btn);

    synth_btn = create_small_button(":/icons/settings.svg", "Configure Track Synthesizer (SoundFont)", "SynthButton", 16);
    synth_btn->setFixedSize(24, 24);
    synth_btn->setToolTip("Configure SoundFont for this track");
    connect(synth_btn, &QPushButton::clicked, this, &TrackWidget::onSynthClicked);
    btn_row->addWidget(synth_btn);

    btn_row->addStretch();
    info_layout->addLayout(btn_row);

    info_outer_layout->addWidget(info_right, 1);

    main_hbox->addWidget(m_normalContent, 0);

    // =========================================================================
    // Column 3: Dials (Vol + Pan) side by side - larger size with value display
    // =========================================================================
    m_dialsWidget = new QWidget();
    m_dialsWidget->setObjectName("TrackDialsWidget");
    m_dialsWidget->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *dials_layout = new QHBoxLayout(m_dialsWidget);
    dials_layout->setContentsMargins(0, 0, 0, 0);
    dials_layout->setSpacing(0);

    m_volumeDial = new AudioDialCentered();
    m_volumeDial->setFixedSize(46, 54);
    m_volumeDial->setRange(-24.0f, 6.0f);
    m_volumeDial->setDefaultValue(0.0f);
    m_volumeDial->setValue(track->getAudioVolumeDb());
    m_volumeDial->setLabel("Vol");
    m_volumeDial->setValuePostfix("dB");
    m_volumeDial->setValueDecimals(0);
    m_volumeDial->showLabel(true);
    m_volumeDial->showValue(true);
    connect(m_volumeDial, &AudioDialCentered::valueChanged, this, &TrackWidget::onVolumeDialChanged);
    dials_layout->addWidget(m_volumeDial);

    m_panDial = new AudioDialCentered();
    m_panDial->setFixedSize(46, 54);
    m_panDial->setRange(-64.0f, 64.0f);
    m_panDial->setDefaultValue(0.0f);
    m_panDial->setValue(static_cast<float>(track->getMidiPanOffset()));
    m_panDial->setLabel("Pan");
    m_panDial->setValueDecimals(0);
    m_panDial->showLabel(true);
    m_panDial->showValue(true);
    connect(m_panDial, &AudioDialCentered::valueChanged, this, &TrackWidget::onPanDialChanged);
    dials_layout->addWidget(m_panDial);

    main_hbox->addWidget(m_dialsWidget, 0);

    // =========================================================================
    // Column 4: Stereo meter (expands horizontally)
    // =========================================================================
    m_stereoMeter = new TrackStereoMeter();
    m_stereoMeter->setMinimumWidth(60);
    m_stereoMeter->setFixedHeight(56);
    main_hbox->addWidget(m_stereoMeter, 1);  // stretch factor 1 = expands

    // =========================================================================
    // Tempo track special content (initially hidden)
    // =========================================================================
    m_tempoContent = new QWidget();
    m_tempoContent->setVisible(false);
    m_tempoContent->setAttribute(Qt::WA_TranslucentBackground);
    m_tempoContent->setStyleSheet("background: transparent;");
    QHBoxLayout *tempo_layout = new QHBoxLayout(m_tempoContent);
    tempo_layout->setContentsMargins(4, 6, 0, 6);
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

void TrackWidget::updateIndexButtonStyle()
{
    bool isTempoTrack = track->isTempoTrack();
    
    if (isTempoTrack) {
        // White text for tempo track on dark background
        QString style = R"(
            QPushButton#TrackIndexButton {
                background: transparent;
                border: none;
                color: #ffffff;
                font-weight: bold;
                font-size: 10px;
                padding: 2px 4px;
                min-width: 16px;
                max-width: 24px;
                min-height: 14px;
                max-height: 18px;
            }
        )";
        index_btn->setStyleSheet(style);
        return;
    }
    
    NN_Color_t color = track->getColor();
    QColor bgColor = color.toQColor();
    
    // Calculate luminance to determine text/hover colors
    double luminance = nn_yiq_luminance(color);
    QString textColor = luminance > 128.0 ? "#000000" : "#ffffff";
    // Hover: if dark background, lighten; if light, darken
    QString hoverColor = luminance > 128.0 ? bgColor.darker(120).name() : bgColor.lighter(140).name();
    
    QString style = QString(R"(
        QPushButton#TrackIndexButton {
            background: transparent;
            border: none;
            color: %1;
            font-weight: bold;
            font-size: 10px;
            padding: 2px 4px;
            min-width: 16px;
            max-width: 24px;
            min-height: 14px;
            max-height: 18px;
        }
        QPushButton#TrackIndexButton:hover {
            background: %2;
            border-radius: 3px;
        }
    )").arg(textColor)
      .arg(hoverColor);
    
    index_btn->setStyleSheet(style);
}

void TrackWidget::updateLeftPanelStyle()
{
    bool isTempoTrack = track->isTempoTrack();
    
    if (isTempoTrack) {
        // Dark background for tempo track (no color), slightly bluer when selected
        QString bg = m_selected ? "#2a3848" : "#252830";
        QString borderColor = m_selected ? "#3a5070" : "#3a3d45";
        QString style = QString(R"(
            QWidget#TrackLeftPanel {
                background: %1;
                border: 1px solid %2;
                border-top-left-radius: 0px;
                border-bottom-left-radius: 0px;
                border-top-right-radius: 8px;
                border-bottom-right-radius: 8px;
            }
        )").arg(bg, borderColor);
        if (m_leftPanel) {
            m_leftPanel->setStyleSheet(style);
            m_leftPanel->setAttribute(Qt::WA_TranslucentBackground, false);
        }
    } else {
        NN_Color_t color = track->getColor();
        QColor bgColor = color.toQColor();
        
        // Calculate border color (slightly darker/lighter based on luminance)
        double luminance = nn_yiq_luminance(color);
        QString borderColor = luminance > 128.0 ? bgColor.darker(130).name() : bgColor.lighter(140).name();
        
        // When selected, blend the track color with blue selection tint
        if (m_selected) {
            // Blend with selection blue (#273a51 ~ rgb(39, 58, 81))
            int r = (bgColor.red() * 2 + 39) / 3;
            int g = (bgColor.green() * 2 + 58) / 3;
            int b = (bgColor.blue() * 2 + 81) / 3;
            bgColor = QColor(r, g, b);
            borderColor = "#3a5070";
        }
        
        QString style = QString(R"(
            QWidget#TrackLeftPanel {
                background: %1;
                border: 1px solid %2;
                border-top-left-radius: 0px;
                border-bottom-left-radius: 0px;
                border-top-right-radius: 8px;
                border-bottom-right-radius: 8px;
            }
        )").arg(bgColor.name(), borderColor);
        
        if (m_leftPanel) {
            m_leftPanel->setStyleSheet(style);
            m_leftPanel->setAttribute(Qt::WA_TranslucentBackground, false);
        }
    }
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
        // Hide dials and stereo meter for tempo track
        if (m_dialsWidget) m_dialsWidget->setVisible(!isTempoTrack);
        if (m_stereoMeter) m_stereoMeter->setVisible(!isTempoTrack);
    }
    
    // Update left panel style (color or dark for tempo)
    updateLeftPanelStyle();
    
    if (isTempoTrack) {
        instrument_btn->setIcon(QIcon(":/icons/tempo.svg"));
        instrument_btn->setToolTip("Tempo Track - Controls dynamic tempo changes");
        instrument_btn->setEnabled(false);
        index_btn->setText(QString::number(track->getId() + 1));
        index_btn->setEnabled(false);
        
        // Update tempo active button state
        if (tempo_active_btn) {
            tempo_active_btn->setChecked(track->isTempoTrackActive());
            tempo_active_btn->setText(track->isTempoTrackActive() ? "Active" : "Inactive");
        }
        return;
    }
    
    // Normal track handling below
    instrument_btn->setEnabled(true);
    index_btn->setEnabled(true);

    // Block signals to prevent triggering onNameTextChanged
    name_edit->blockSignals(true);
    name_edit->setText(QString::fromStdString(track->getName()));
    name_edit->blockSignals(false);
    name_edit->setToolTip(QString::fromStdString(track->getName()));
    
    // Hide unsaved indicator since the name matches the track
    unsaved_indicator->setStyleSheet("QLabel#UnsavedIndicator { background-color: transparent; border-radius: 4px; }");

    index_btn->setText(QString::number(track->getId() + 1));
    updateIndexButtonStyle();

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

    // Update dial values (block signals to prevent feedback loops)
    if (m_volumeDial) {
        m_volumeDial->blockSignals(true);
        m_volumeDial->setValue(track->getAudioVolumeDb());
        m_volumeDial->blockSignals(false);
    }
    if (m_panDial) {
        m_panDial->blockSignals(true);
        m_panDial->setValue(static_cast<float>(track->getMidiPanOffset()));
        m_panDial->blockSignals(false);
    }
}

void TrackWidget::onToggleVisibility()
{
    track->setVisible(!invisible_btn->isChecked());
}

void TrackWidget::onToggleSoloView()
{
    emit soloViewToggled(track, solo_view_btn->isChecked());
}

void TrackWidget::onToggleSolo()
{
    engine->soloTrack(track, solo_btn->isChecked());
}

void TrackWidget::onToggleMute()
{
    engine->muteTrack(track, mute_btn->isChecked());
}

void TrackWidget::onSynthClicked()
{
    // Get the FluidSynth instance from the track
    NoteNagaSynthFluidSynth *fluidSynth = dynamic_cast<NoteNagaSynthFluidSynth*>(track->getSynth());
    if (!fluidSynth) {
        QMessageBox::warning(this, tr("No Synthesizer"),
            tr("This track does not have a FluidSynth synthesizer.\n"
               "Only FluidSynth synthesizers can load SoundFonts."));
        return;
    }

    QString currentSF = QString::fromStdString(fluidSynth->getSoundFontPath());
    QString sfPath = QFileDialog::getOpenFileName(
        this,
        tr("Select SoundFont for %1").arg(QString::fromStdString(track->getName())),
        currentSF.isEmpty() ? QDir::homePath() : QFileInfo(currentSF).absolutePath(),
        tr("SoundFont Files (*.sf2 *.sf3 *.dls);;All Files (*)")
    );
    
    if (!sfPath.isEmpty()) {
        bool success = fluidSynth->setSoundFont(sfPath.toStdString());
        
        if (success) {
            QMessageBox::information(this, tr("SoundFont Loaded"),
                tr("SoundFont successfully loaded:\n%1").arg(QFileInfo(sfPath).fileName()));
        } else {
            QString errorMsg = QString::fromStdString(fluidSynth->getLastError());
            if (errorMsg.isEmpty()) {
                errorMsg = tr("Unknown error");
            }
            QMessageBox::warning(this, tr("SoundFont Load Failed"),
                tr("Failed to load SoundFont:\n%1\n\nError: %2\n\n"
                   "The file may be corrupted or in an unsupported format.")
                .arg(QFileInfo(sfPath).fileName())
                .arg(errorMsg));
        }
    }
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
    unsaved_indicator->setStyleSheet("QLabel#UnsavedIndicator { background-color: transparent; border-radius: 4px; }");
}

void TrackWidget::onNameTextChanged(const QString &text)
{
    // Show the unsaved indicator if the text differs from the track's name
    QString currentTrackName = QString::fromStdString(track->getName());
    bool unsaved = (text != currentTrackName);
    unsaved_indicator->setStyleSheet(unsaved 
        ? "QLabel#UnsavedIndicator { background-color: #ff9900; border-radius: 4px; }" 
        : "QLabel#UnsavedIndicator { background-color: transparent; border-radius: 4px; }");
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

void TrackWidget::onVolumeDialChanged(float value)
{
    track->setAudioVolumeDb(value);
}

void TrackWidget::onPanDialChanged(float value)
{
    track->setMidiPanOffset(static_cast<int>(value));
}

void TrackWidget::mousePressEvent(QMouseEvent *event)
{
    emit clicked(this->track->getId());
    QFrame::mousePressEvent(event);
}

void TrackWidget::refreshStyle(bool selected, bool darker_bg)
{
    m_selected = selected;
    m_darkerBg = darker_bg;
    
    QString bg = darker_bg ? "#282930" : "#2F3139";
    QString selectedBg = "#273a51";
    QString actualBg = selected ? selectedBg : bg;
    QString borderColor = selected ? "#3477c0" : "#232731";
    
    // Darker background for info panel (track name + buttons area)
    QString infoPanelBg = selected ? "#222a38" : "#252830";
    QString infoPanelBorder = selected ? "#3a5070" : "#3a3d45";
    
    QString base_style = QString(R"(
        QFrame#TrackWidget {
            background: %1;
            border: 1px solid %2;
            border-radius: 0px;
            padding: 0px;
        }
        QPushButton#InstrumentButton {
            border: none;
            background: transparent;
            min-width: 36px;
            max-width: 40px;
            min-height: 36px;
            max-height: 44px;
            icon-size: 32px;
        }
        QPushButton#InstrumentButton:hover {
            background: rgba(255, 255, 255, 40);
            border-radius: 4px;
        }
        QWidget#TrackDialsWidget {
            background: transparent;
        }
        QWidget#TrackInfoPanel {
            background: %3;
            border: 1px solid %4;
            border-top-left-radius: 0px;
            border-bottom-left-radius: 0px;
            border-top-right-radius: 8px;
            border-bottom-right-radius: 8px;
        }
    )").arg(actualBg, borderColor, infoPanelBg, infoPanelBorder);
    
    setStyleSheet(base_style);
    
    // Also update left panel and index button styles (pass selection state)
    updateLeftPanelStyle();
    updateIndexButtonStyle();
    
    update();
}
