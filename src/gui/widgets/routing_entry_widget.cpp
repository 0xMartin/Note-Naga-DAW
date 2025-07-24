#include "routing_entry_widget.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>

RoutingEntryWidget::RoutingEntryWidget(NoteNagaEngine *engine_,
                                       NoteNagaRoutingEntry *entry_, QWidget *parent)
    : QFrame(parent), entry(entry_), engine(engine_), indicator_led(nullptr) {
    setupUI();

    // ComboBox initialization
    populateTrackComboBox(entry->track);
    populateOutputComboBox();
    setComboBoxSelections();

    // Connect to track info changed (refresh combos)
    connect(entry->track, &NoteNagaTrack::metadataChanged, this,
            &RoutingEntryWidget::onTrackMetadataChanged);

    // Style
    refresh_style(false);
}

void RoutingEntryWidget::setupUI() {
    setObjectName("RoutingEntryWidget");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Main Layout
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    // ----- Side Panel -----
    QWidget *sidePanel = new QWidget(this);
    sidePanel->setFixedWidth(30);
    sidePanel->setObjectName("RoutingEntrySidePanel");

    QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 4, 0, 4);
    sideLayout->setSpacing(4);

    // Index Label
    int index;
    for (index = 0; index < engine->getMixer()->getRoutingEntries().size(); ++index) {
        if (entry == &engine->getMixer()->getRoutingEntries()[index]) { break; }
    }
    QLabel *indexLabel = new QLabel(QString::number(index + 1));
    indexLabel->setAlignment(Qt::AlignCenter);
    indexLabel->setObjectName("RoutingEntryIndexLabel");
    indexLabel->setStyleSheet(
        "QLabel#RoutingEntryIndexLabel { color: #b0b4b8; font-size: 13px; }");
    sideLayout->addWidget(indexLabel, 0, Qt::AlignHCenter);

    // LED Indicator
    indicator_led = new IndicatorLedWidget(Qt::red, sidePanel);
    indicator_led->setFixedSize(16, 16);
    sideLayout->addWidget(indicator_led, 0, Qt::AlignHCenter);

    sideLayout->addStretch(1);
    sidePanel->setLayout(sideLayout);

    // Side panel style (darker, no border, rounded left)
    sidePanel->setStyleSheet("QWidget#RoutingEntrySidePanel {"
                             "background: #292b2e;"
                             "border-top-left-radius: 8px;"
                             "border-bottom-left-radius: 8px;"
                             "}");

    // ----- Central UI -----
    // Combo column
    QVBoxLayout *combo_col = new QVBoxLayout();
    combo_col->setContentsMargins(0, 0, 0, 0);
    combo_col->setSpacing(2);

    // Track row
    QHBoxLayout *track_row = new QHBoxLayout();
    track_row->setContentsMargins(0, 0, 0, 0);
    track_row->setSpacing(4);
    QLabel *track_icon = new QLabel();
    track_icon->setFixedSize(18, 18);
    track_icon->setAlignment(Qt::AlignCenter);
    track_icon->setObjectName("RoutingTrackIcon");
    track_icon->setStyleSheet("QLabel#RoutingTrackIcon { min-width: 18px; max-width: "
                              "18px; min-height: 18px; max-height: 18px;"
                              "border-radius: 3px; background: transparent; }");
    track_icon->setPixmap(QIcon(":/icons/track.svg").pixmap(16, 16));
    track_row->addWidget(track_icon, Qt::AlignVCenter);

    track_combo = new QComboBox();
    connect(track_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RoutingEntryWidget::onTrackChanged);
    track_row->addWidget(track_combo, 1);
    combo_col->addLayout(track_row);

    // Device row
    QHBoxLayout *device_row = new QHBoxLayout();
    device_row->setContentsMargins(0, 0, 0, 0);
    device_row->setSpacing(4);
    QLabel *device_icon = new QLabel();
    device_icon->setFixedSize(18, 18);
    device_icon->setAlignment(Qt::AlignCenter);
    device_icon->setObjectName("RoutingDeviceIcon");
    device_icon->setStyleSheet("QLabel#RoutingDeviceIcon { min-width: 18px; max-width: "
                               "18px; min-height: 18px; max-height: 18px;"
                               "border-radius: 3px; background: transparent; }");
    device_icon->setPixmap(QIcon(":/icons/route.svg").pixmap(16, 16));
    device_row->addWidget(device_icon, Qt::AlignVCenter);

    output_combo = new QComboBox();
    connect(output_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RoutingEntryWidget::onDeviceChanged);
    device_row->addWidget(output_combo, 1);
    combo_col->addLayout(device_row);

    // --- Dials ---
    QHBoxLayout *dials_layout = new QHBoxLayout();
    dials_layout->setSpacing(4);
    dials_layout->setContentsMargins(0, 0, 0, 0);

    // Channel
    channel_dial = new AudioDial();
    channel_dial->setLabel("Ch");
    channel_dial->setRange(1, 16);
    channel_dial->setValue(entry->channel + 1);
    channel_dial->setDefaultValue(1);
    channel_dial->showValue(true);
    channel_dial->setValueDecimals(0);
    connect(channel_dial, &AudioDial::valueChanged, this,
            &RoutingEntryWidget::onChannelChanged);
    dials_layout->addWidget(channel_dial);

    // Volume
    volume_dial = new AudioDial();
    volume_dial->setLabel("Vol");
    volume_dial->setRange(0, 100);
    volume_dial->setValue(entry->volume * 100);
    volume_dial->setDefaultValue(100);
    volume_dial->setValueDecimals(1);
    volume_dial->setValuePostfix(" %");
    volume_dial->showValue(true);
    connect(volume_dial, &AudioDial::valueChanged, this,
            &RoutingEntryWidget::onVolumeChanged);
    dials_layout->addWidget(volume_dial);

    // Pan
    pan_dial = new AudioDialCentered();
    pan_dial->setLabel("Pan");
    pan_dial->setRange(-1.0, 1.0);
    pan_dial->setValueDecimals(2);
    pan_dial->setValue(entry->pan);
    pan_dial->setDefaultValue(0.0);
    connect(pan_dial, &AudioDialCentered::valueChanged, this,
            &RoutingEntryWidget::onGlobalPanChanged);
    dials_layout->addWidget(pan_dial);

    // Note offset
    offset_dial = new AudioDialCentered();
    offset_dial->setLabel("Offset");
    offset_dial->setRange(-12, 12);
    offset_dial->setValue(entry->note_offset);
    offset_dial->showValue(true);
    offset_dial->setValueDecimals(0);
    offset_dial->setDefaultValue(0);
    connect(offset_dial, &AudioDialCentered::valueChanged, this,
            &RoutingEntryWidget::onOffsetChanged);
    dials_layout->addWidget(offset_dial);

    // Layout order: [panel][combo][stretch][dials]
    layout->addWidget(sidePanel);
    layout->addLayout(combo_col);
    layout->addLayout(dials_layout);
    setLayout(layout);
}

void RoutingEntryWidget::populateTrackComboBox(NoteNagaTrack *track) {
    track_combo->blockSignals(true);
    track_combo->clear();

    if (!track) return;
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq) return;

    // populate track combo with all tracks from the sequence
    const std::vector<NoteNagaTrack *> &tracks = seq->getTracks();
    for (size_t idx = 0; idx < tracks.size(); ++idx) {
        auto &tr = tracks[idx];
        QString name = QString("%1: %2").arg(idx + 1).arg(tr->getName());
        track_combo->addItem(name, tr->getId());
    }

    // select current track of entry
    NoteNagaTrack *entry_track = entry->track;
    if (entry_track) {
        int current_track_index = track_combo->findData(entry_track->getId());
        if (current_track_index >= 0) track_combo->setCurrentIndex(current_track_index);
        track_combo->blockSignals(false);
    }
}

void RoutingEntryWidget::populateOutputComboBox() {
    output_combo->blockSignals(true);
    output_combo->clear();
    for (const std::string &out : engine->getMixer()->getAvailableOutputs()) {
        output_combo->addItem(QString::fromStdString(out));
    }
    output_combo->addItem(TRACK_ROUTING_ENTRY_ANY_DEVICE);
    int idx = output_combo->findText(QString::fromStdString(entry->output));
    if (idx >= 0) output_combo->setCurrentIndex(idx);
    output_combo->blockSignals(false);
}

void RoutingEntryWidget::setComboBoxSelections() {
    if (entry->track) {
        int track_idx = track_combo->findData(entry->track->getId());
        if (track_idx >= 0) track_combo->setCurrentIndex(track_idx);
    }

    int dev_idx = output_combo->findText(QString::fromStdString(entry->output));
    if (dev_idx >= 0) output_combo->setCurrentIndex(dev_idx);
}

void RoutingEntryWidget::onTrackMetadataChanged(NoteNagaTrack *track) {
    populateTrackComboBox(track);
    populateOutputComboBox();
}

void RoutingEntryWidget::onTrackChanged(int idx) {
    int new_track_id = track_combo->itemData(idx).toInt();

    NoteNagaTrack *track = entry->track;
    if (!track) return;
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq) return;

    NoteNagaTrack *new_track = seq->getTrackById(new_track_id);
    if (!new_track) return;

    entry->track = new_track;
}

void RoutingEntryWidget::onDeviceChanged(int idx) {
    QString new_device = output_combo->currentText();
    entry->output = new_device.toStdString();
}

void RoutingEntryWidget::onChannelChanged(float val) { entry->channel = int(val - 1); }

void RoutingEntryWidget::onVolumeChanged(float val) {
    entry->volume = float(val / 100.0f);
}

void RoutingEntryWidget::onOffsetChanged(float val) { entry->note_offset = int(val); }

void RoutingEntryWidget::onGlobalPanChanged(float value) { entry->pan = value; }

void RoutingEntryWidget::refresh_style(bool selected) {
    QString base_style = R"(
        QFrame#RoutingEntryWidget {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 2px;
        }
    )";
    QString style = selected ? base_style.arg("#273a51", "#3477c0")
                             : base_style.arg("#2F3139", "#494d56");
    setStyleSheet(style);
    update();
}

// Optional: You can override mousePressEvent if you want to handle selection in parent
void RoutingEntryWidget::mousePressEvent(QMouseEvent *event) {
    emit clicked();
    QFrame::mousePressEvent(event);
}