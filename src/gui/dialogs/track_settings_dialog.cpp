#include "track_settings_dialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QDir>
#include <QStyle>
#include <QGridLayout>

#include <note_naga_engine/synth/synth_fluidsynth.h>
#include "instrument_selector_dialog.h"

TrackSettingsDialog::TrackSettingsDialog(QWidget *parent, NoteNagaTrack *track,
                                         std::function<QIcon(QString)> iconProvider)
    : QDialog(parent), m_track(track), m_fluidSynth(nullptr), 
      m_iconProvider(iconProvider), m_soundFontValid(false)
{
    setWindowTitle(tr("Track Settings - %1").arg(QString::fromStdString(track->getName())));
    setMinimumWidth(450);
    setModal(true);

    // Get the FluidSynth instance
    m_fluidSynth = dynamic_cast<NoteNagaSynthFluidSynth*>(track->getSynth());
    
    // Initialize pending values
    m_pendingColor = track->getColor();
    m_pendingInstrument = track->getInstrument();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // Style for group boxes
    QString groupBoxStyle = R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #3a3d45;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 4px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
    )";

    // =========================================================================
    // Track Info Section
    // =========================================================================
    QGroupBox *infoGroup = new QGroupBox(tr("Track Info"));
    infoGroup->setStyleSheet(groupBoxStyle);
    QGridLayout *infoLayout = new QGridLayout(infoGroup);
    infoLayout->setSpacing(8);
    infoLayout->setContentsMargins(12, 16, 12, 12);

    // Name
    QLabel *nameLabel = new QLabel(tr("Name:"));
    infoLayout->addWidget(nameLabel, 0, 0);
    
    m_nameEdit = new QLineEdit(QString::fromStdString(track->getName()));
    m_nameEdit->setStyleSheet(R"(
        QLineEdit {
            background: #1e2028;
            border: 1px solid #3a3d45;
            border-radius: 4px;
            padding: 6px 10px;
            color: #e0e0e0;
        }
    )");
    infoLayout->addWidget(m_nameEdit, 0, 1, 1, 2);

    // Color
    QLabel *colorLabel = new QLabel(tr("Color:"));
    infoLayout->addWidget(colorLabel, 1, 0);
    
    m_colorBtn = new QPushButton();
    m_colorBtn->setFixedSize(80, 28);
    m_colorBtn->setCursor(Qt::PointingHandCursor);
    m_colorBtn->setToolTip(tr("Click to change track color"));
    connect(m_colorBtn, &QPushButton::clicked, this, &TrackSettingsDialog::onColorClicked);
    updateColorButton();
    infoLayout->addWidget(m_colorBtn, 1, 1, Qt::AlignLeft);

    // Instrument
    QLabel *instrLabel = new QLabel(tr("Instrument:"));
    infoLayout->addWidget(instrLabel, 2, 0);
    
    m_instrumentBtn = new QPushButton();
    m_instrumentBtn->setMinimumWidth(180);
    m_instrumentBtn->setCursor(Qt::PointingHandCursor);
    m_instrumentBtn->setStyleSheet(R"(
        QPushButton {
            background: #1e2028;
            border: 1px solid #3a3d45;
            border-radius: 4px;
            padding: 6px 12px;
            color: #e0e0e0;
            text-align: left;
        }
        QPushButton:hover {
            background: #2a2d38;
        }
    )");
    connect(m_instrumentBtn, &QPushButton::clicked, this, &TrackSettingsDialog::onInstrumentClicked);
    updateInstrumentButton();
    infoLayout->addWidget(m_instrumentBtn, 2, 1, 1, 2);

    // Checkboxes row
    QHBoxLayout *checkRow = new QHBoxLayout();
    checkRow->setSpacing(16);
    
    m_muteCheck = new QCheckBox(tr("Mute"));
    m_muteCheck->setChecked(track->isMuted());
    checkRow->addWidget(m_muteCheck);
    
    m_soloCheck = new QCheckBox(tr("Solo"));
    m_soloCheck->setChecked(track->isSolo());
    checkRow->addWidget(m_soloCheck);
    
    m_visibleCheck = new QCheckBox(tr("Visible"));
    m_visibleCheck->setChecked(track->isVisible());
    checkRow->addWidget(m_visibleCheck);
    
    checkRow->addStretch();
    infoLayout->addLayout(checkRow, 3, 0, 1, 3);

    mainLayout->addWidget(infoGroup);

    // =========================================================================
    // Track Type Section
    // =========================================================================
    QGroupBox *trackTypeGroup = new QGroupBox(tr("Track Type"));
    trackTypeGroup->setStyleSheet(groupBoxStyle);
    QHBoxLayout *typeLayout = new QHBoxLayout(trackTypeGroup);
    typeLayout->setContentsMargins(12, 16, 12, 12);
    typeLayout->setSpacing(12);

    QLabel *typeLabel = new QLabel(tr("Type:"));
    typeLayout->addWidget(typeLabel);

    m_trackTypeCombo = new QComboBox();
    m_trackTypeCombo->addItem(tr("Melodic Instrument"), 0);
    m_trackTypeCombo->addItem(tr("Drums / Percussion"), 9);
    m_trackTypeCombo->setStyleSheet(R"(
        QComboBox {
            background: #1e2028;
            border: 1px solid #3a3d45;
            border-radius: 4px;
            padding: 6px 10px;
            color: #e0e0e0;
            min-width: 160px;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 6px solid #808080;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background: #1e2028;
            border: 1px solid #3a3d45;
            selection-background-color: #3477c0;
        }
    )");

    std::optional<int> currentChannel = m_track->getChannel();
    if (currentChannel.has_value() && *currentChannel == 9) {
        m_trackTypeCombo->setCurrentIndex(1);
    } else {
        m_trackTypeCombo->setCurrentIndex(0);
    }

    connect(m_trackTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackSettingsDialog::onTrackTypeChanged);
    typeLayout->addWidget(m_trackTypeCombo, 1);

    mainLayout->addWidget(trackTypeGroup);

    // =========================================================================
    // SoundFont Section
    // =========================================================================
    QGroupBox *soundFontGroup = new QGroupBox(tr("SoundFont"));
    soundFontGroup->setStyleSheet(groupBoxStyle);
    QVBoxLayout *sfLayout = new QVBoxLayout(soundFontGroup);
    sfLayout->setContentsMargins(12, 16, 12, 12);
    sfLayout->setSpacing(8);

    QHBoxLayout *pathRow = new QHBoxLayout();
    pathRow->setSpacing(8);

    m_soundFontPath = new QLineEdit();
    m_soundFontPath->setReadOnly(true);
    m_soundFontPath->setPlaceholderText(tr("No SoundFont selected"));
    m_soundFontPath->setStyleSheet(R"(
        QLineEdit {
            background: #1e2028;
            border: 1px solid #3a3d45;
            border-radius: 4px;
            padding: 6px 10px;
            color: #c0c0c0;
        }
    )");
    pathRow->addWidget(m_soundFontPath, 1);

    m_browseBtn = new QPushButton(tr("Browse..."));
    m_browseBtn->setStyleSheet(R"(
        QPushButton {
            background: #3a4050;
            border: 1px solid #4a5060;
            border-radius: 4px;
            padding: 6px 16px;
            color: #e0e0e0;
        }
        QPushButton:hover { background: #4a5060; }
        QPushButton:pressed { background: #2a3040; }
    )");
    connect(m_browseBtn, &QPushButton::clicked, this, &TrackSettingsDialog::onBrowseSoundFont);
    pathRow->addWidget(m_browseBtn);

    sfLayout->addLayout(pathRow);

    QHBoxLayout *statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);

    m_statusIcon = new QLabel();
    m_statusIcon->setFixedSize(16, 16);
    statusRow->addWidget(m_statusIcon);

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #a0a0a0;");
    statusRow->addWidget(m_statusLabel, 1);

    sfLayout->addLayout(statusRow);

    if (!m_fluidSynth) {
        m_soundFontPath->setEnabled(false);
        m_browseBtn->setEnabled(false);
        m_statusLabel->setText(tr("Track does not use FluidSynth"));
        m_statusLabel->setStyleSheet("color: #ff8080;");
    } else {
        QString currentPath = QString::fromStdString(m_fluidSynth->getSoundFontPath());
        if (!currentPath.isEmpty()) {
            m_soundFontPath->setText(currentPath);
            m_pendingSoundFont = currentPath;
        }
        m_soundFontValid = m_fluidSynth->isValid();
        updateSoundFontStatus();
    }

    mainLayout->addWidget(soundFontGroup);

    // =========================================================================
    // Dialog Buttons
    // =========================================================================
    mainLayout->addStretch();

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(12);
    buttonRow->addStretch();

    m_applyBtn = new QPushButton(tr("Apply"));
    m_applyBtn->setStyleSheet(R"(
        QPushButton {
            background: #2a6030;
            border: 1px solid #40a050;
            border-radius: 4px;
            padding: 8px 24px;
            color: #90d090;
            font-weight: bold;
        }
        QPushButton:hover { background: #306838; }
        QPushButton:pressed { background: #205028; }
    )");
    connect(m_applyBtn, &QPushButton::clicked, this, &TrackSettingsDialog::onApply);
    buttonRow->addWidget(m_applyBtn);

    m_closeBtn = new QPushButton(tr("Close"));
    m_closeBtn->setStyleSheet(R"(
        QPushButton {
            background: #3a4050;
            border: 1px solid #4a5060;
            border-radius: 4px;
            padding: 8px 24px;
            color: #e0e0e0;
        }
        QPushButton:hover { background: #4a5060; }
        QPushButton:pressed { background: #2a3040; }
    )");
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(m_closeBtn);

    mainLayout->addLayout(buttonRow);
    setLayout(mainLayout);
}

void TrackSettingsDialog::onBrowseSoundFont()
{
    QString currentPath = m_soundFontPath->text();
    QString startDir = currentPath.isEmpty() ? QDir::homePath() : QFileInfo(currentPath).absolutePath();

    QString sfPath = QFileDialog::getOpenFileName(
        this, tr("Select SoundFont"), startDir,
        tr("SoundFont Files (*.sf2 *.sf3 *.dls);;All Files (*)")
    );

    if (!sfPath.isEmpty()) {
        m_soundFontPath->setText(sfPath);
        m_pendingSoundFont = sfPath;
        
        if (m_fluidSynth) {
            bool success = m_fluidSynth->setSoundFont(sfPath.toStdString());
            m_soundFontValid = success;
            updateSoundFontStatus();
        }
    }
}

void TrackSettingsDialog::onTrackTypeChanged(int index)
{
    Q_UNUSED(index);
}

void TrackSettingsDialog::onColorClicked()
{
    QColor col = QColorDialog::getColor(m_pendingColor.toQColor(), this, tr("Select Track Color"));
    if (col.isValid()) {
        m_pendingColor = NN_Color_t::fromQColor(col);
        updateColorButton();
    }
}

void TrackSettingsDialog::onInstrumentClicked()
{
    if (!m_iconProvider) return;
    
    InstrumentSelectorDialog dlg(this, GM_INSTRUMENTS, m_iconProvider, m_pendingInstrument);
    if (dlg.exec() == QDialog::Accepted) {
        m_pendingInstrument = dlg.getSelectedGMIndex();
        updateInstrumentButton();
    }
}

void TrackSettingsDialog::onApply()
{
    // Apply name
    m_track->setName(m_nameEdit->text().toStdString());
    
    // Apply color
    m_track->setColor(m_pendingColor);
    
    // Apply instrument
    if (m_pendingInstrument.has_value()) {
        m_track->setInstrument(*m_pendingInstrument);
    }
    
    // Apply mute/solo/visible
    m_track->setMuted(m_muteCheck->isChecked());
    m_track->setSolo(m_soloCheck->isChecked());
    m_track->setVisible(m_visibleCheck->isChecked());
    
    // Apply track type (MIDI channel)
    int channelValue = m_trackTypeCombo->currentData().toInt();
    if (channelValue == 9) {
        m_track->setChannel(9);
    } else {
        int ch = m_track->getId() % 16;
        if (ch == 9) ch = 0;
        m_track->setChannel(ch);
    }

    updateSoundFontStatus();
    
    // Emit signal so widget can refresh
    emit trackSettingsChanged();
    
    // Update window title with new name
    setWindowTitle(tr("Track Settings - %1").arg(m_nameEdit->text()));
}

void TrackSettingsDialog::updateSoundFontStatus()
{
    if (!m_fluidSynth) return;

    if (m_fluidSynth->isLoading()) {
        m_statusIcon->setStyleSheet("background: #ffa000; border-radius: 8px;");
        m_statusLabel->setText(tr("Loading SoundFont..."));
        m_statusLabel->setStyleSheet("color: #ffa000;");
        m_soundFontValid = false;
    } else if (m_fluidSynth->isValid()) {
        m_statusIcon->setStyleSheet("background: #40a050; border-radius: 8px;");
        QString sfName = QFileInfo(m_soundFontPath->text()).fileName();
        m_statusLabel->setText(tr("Loaded: %1").arg(sfName));
        m_statusLabel->setStyleSheet("color: #90d090;");
        m_soundFontValid = true;
    } else {
        m_statusIcon->setStyleSheet("background: #e04040; border-radius: 8px;");
        QString error = QString::fromStdString(m_fluidSynth->getLastError());
        if (error.isEmpty()) error = tr("SoundFont not loaded");
        m_statusLabel->setText(error);
        m_statusLabel->setStyleSheet("color: #ff8080;");
        m_soundFontValid = false;
    }
}

void TrackSettingsDialog::updateColorButton()
{
    QColor col = m_pendingColor.toQColor();
    m_colorBtn->setStyleSheet(QString(R"(
        QPushButton {
            background: %1;
            border: 2px solid %2;
            border-radius: 4px;
        }
        QPushButton:hover {
            border: 2px solid #ffffff;
        }
    )").arg(col.name(), col.darker(130).name()));
}

void TrackSettingsDialog::updateInstrumentButton()
{
    auto instrument = nn_find_instrument_by_index(m_pendingInstrument.value_or(0));
    if (instrument) {
        if (m_iconProvider) {
            m_instrumentBtn->setIcon(m_iconProvider(QString::fromStdString(instrument->icon)));
        }
        m_instrumentBtn->setText(QString::fromStdString(instrument->name));
    } else {
        m_instrumentBtn->setText(tr("Unknown"));
    }
}
