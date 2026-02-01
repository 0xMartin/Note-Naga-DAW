#include "routing_entry_widget.h"
#include "note_naga_engine/core/types.h"

#include <note_naga_engine/synth/synth_external_midi.h>
#include <note_naga_engine/synth/synth_fluidsynth.h>

#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>

RoutingEntryWidget::RoutingEntryWidget(NoteNagaEngine *engine_,
                                       NoteNagaRoutingEntry *entry_,
                                       QWidget *parent)
    : QFrame(parent), entry(entry_), engine(engine_), indicator_led(nullptr)
{
    setupUI();

    NoteNagaMidiSeq *parent_seq = entry->track->getParent();
    // ComboBox initialization
    populateTrackComboBox(parent_seq);
    updateOutputLabel();

    // Connect to track info changed (refresh combos)
    connect(parent_seq, &NoteNagaMidiSeq::trackListChanged, this,
            [this, parent_seq]()
            { onTrackMetadataChanged(parent_seq); });
    
    // Connect to track metadata changes (name changes)
    connect(parent_seq, &NoteNagaMidiSeq::trackMetadataChanged, this,
            [this, parent_seq](NoteNagaTrack*, const std::string&)
            { onTrackMetadataChanged(parent_seq); });
}

void RoutingEntryWidget::setupUI()
{
    setObjectName("RoutingEntryWidget");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Main Layout
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // ----- Side Panel -----
    QWidget *sidePanel = new QWidget(this);
    sidePanel->setFixedWidth(30);
    sidePanel->setObjectName("RoutingEntrySidePanel");

    QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 2, 0, 2);
    sideLayout->setSpacing(2);

    // Index Label
    int index;
    for (index = 0; index < engine->getMixer()->getRoutingEntries().size();
         ++index)
    {
        if (entry == &engine->getMixer()->getRoutingEntries()[index])
        {
            break;
        }
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

    // ----- Central UI -----
    // Combo column
    QVBoxLayout *combo_col = new QVBoxLayout();
    combo_col->setContentsMargins(0, 5, 0, 0);
    combo_col->setSpacing(2);

    // Track row
    QHBoxLayout *track_row = new QHBoxLayout();
    track_row->setContentsMargins(0, 0, 0, 0);
    track_row->setSpacing(4);
    QLabel *track_icon = new QLabel();
    track_icon->setFixedSize(18, 18);
    track_icon->setAlignment(Qt::AlignCenter);
    track_icon->setObjectName("RoutingTrackIcon");
    track_icon->setStyleSheet(
        "QLabel#RoutingTrackIcon { min-width: 18px; max-width: "
        "18px; min-height: 18px; max-height: 18px;"
        "border-radius: 3px; background: transparent; }");
    track_icon->setPixmap(QIcon(":/icons/track.svg").pixmap(16, 16));
    track_row->addWidget(track_icon, Qt::AlignVCenter);

    track_combo = new QComboBox();
    track_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    track_combo->setMinimumWidth(120);
    track_combo->setMaximumWidth(350);
    track_combo->view()->setMinimumWidth(250);  // Ensure popup is wide enough
    track_combo->setStyleSheet(
        "QComboBox { background: #232731; color: #e8e8e8; border: 1px solid #494d56; "
        "border-radius: 4px; padding: 3px 8px; font-size: 12px; }"
        "QComboBox:hover { border-color: #5a8fd4; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox::down-arrow { image: url(:/icons/arrow-down.svg); width: 12px; height: 12px; }"
        "QComboBox QAbstractItemView { background: #2a2e38; color: #e8e8e8; "
        "selection-background-color: #3477c0; border: 1px solid #494d56; "
        "padding: 4px; min-width: 250px; }"
    );
    connect(track_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RoutingEntryWidget::onTrackChanged);
    track_row->addWidget(track_combo, 1);
    combo_col->addLayout(track_row);

    // Device/Output row - now using a label instead of combobox
    QHBoxLayout *device_row = new QHBoxLayout();
    device_row->setContentsMargins(0, 0, 0, 0);
    device_row->setSpacing(4);
    QLabel *device_icon = new QLabel();
    device_icon->setFixedSize(18, 18);
    device_icon->setAlignment(Qt::AlignCenter);
    device_icon->setObjectName("RoutingDeviceIcon");
    device_icon->setStyleSheet(
        "QLabel#RoutingDeviceIcon { min-width: 18px; max-width: "
        "18px; min-height: 18px; max-height: 18px;"
        "border-radius: 3px; background: transparent; }");
    device_icon->setPixmap(QIcon(":/icons/route.svg").pixmap(16, 16));
    device_row->addWidget(device_icon, Qt::AlignVCenter);

    // Output label instead of combo box
    output_label = new QLabel();
    output_label->setMinimumWidth(120);
    output_label->setMaximumWidth(350);
    output_label->setStyleSheet(
        "QLabel { color: #79b8ff; font-weight: bold; font-size: 12px; "
        "background: #232731; border: 1px solid #494d56; border-radius: 4px; "
        "padding: 3px 8px; }"
    );
    output_label->setToolTip("Output synthesizer for this routing entry");
    device_row->addWidget(output_label, 1);
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
    layout->addStretch(0);
    layout->addLayout(dials_layout);
    setLayout(layout);
}

void RoutingEntryWidget::populateTrackComboBox(NoteNagaMidiSeq *seq)
{
    if (!seq)
        return;

    track_combo->blockSignals(true);
    track_combo->clear();

    // Naplníme combo box aktuálně platnými tracky
    const std::vector<NoteNagaTrack *> &tracks = seq->getTracks();
    for (size_t idx = 0; idx < tracks.size(); ++idx)
    {
        auto &tr = tracks[idx];
        QString name = QString("%1: %2").arg(tr->getId() + 1).arg(tr->getName());
        track_combo->addItem(name, tr->getId());
    }

    // Získáme track, který si widget "pamatuje"
    NoteNagaTrack *entry_track = entry->track;

    // Ověříme, že track, který si pamatujeme,
    // stále existuje v aktuálním seznamu tracků.
    bool track_is_still_valid = false;
    if (entry_track)
    { // Nejprve zkontrolujeme, zda vůbec nějaký máme
        for (const auto &valid_track : tracks)
        {
            if (valid_track == entry_track)
            {
                track_is_still_valid = true;
                break;
            }
        }
    }

    // Pouze pokud je náš track stále platný, pokusíme se ho nastavit
    if (track_is_still_valid)
    {
        // Toto je teď bezpečné, protože víme, že entry_track
        // je platný ukazatel ze seznamu 'tracks'.
        int current_track_index = track_combo->findData(entry_track->getId());
        if (current_track_index >= 0)
            track_combo->setCurrentIndex(current_track_index);
    }

    track_combo->blockSignals(false);
}

void RoutingEntryWidget::updateOutputLabel()
{
    // Update the label to show the currently assigned output
    QString outputText = QString::fromStdString(entry->output);

    output_label->setText(outputText);
}

void RoutingEntryWidget::onTrackMetadataChanged(NoteNagaMidiSeq *seq)
{
    populateTrackComboBox(seq);
    updateOutputLabel();
}

void RoutingEntryWidget::onTrackChanged(int idx)
{
    int new_track_id = track_combo->itemData(idx).toInt();

    NoteNagaTrack *track = entry->track;
    if (!track)
        return;
    NoteNagaMidiSeq *seq = track->getParent();
    if (!seq)
        return;

    NoteNagaTrack *new_track = seq->getTrackById(new_track_id);
    if (!new_track)
        return;

    entry->track = new_track;
}

void RoutingEntryWidget::onChannelChanged(float val)
{
    entry->channel = int(val - 1);
}

void RoutingEntryWidget::onVolumeChanged(float val)
{
    entry->volume = float(val / 100.0f);
}

void RoutingEntryWidget::onOffsetChanged(float val)
{
    entry->note_offset = int(val);
}

void RoutingEntryWidget::onGlobalPanChanged(float value) { entry->pan = value; }

void RoutingEntryWidget::refreshStyle(bool selected, bool darker_bg)
{
    QString base_style = R"(
        QFrame#RoutingEntryWidget {
            background: %1;
            border: 1px solid %2;
        }
        QWidget#RoutingEntrySidePanel {
            background: %3;
        }
    )";
    QString bg = darker_bg ? "#282930" : "#2F3139";
    QString bg_side = darker_bg ? "#202224" : "#292b2e";
    QString style = selected ? base_style.arg("#273a51", "#3477c0", "#1e2e46")
                             : base_style.arg(bg, "#494d56", bg_side);
    setStyleSheet(style);
    update();
}

// Optional: You can override mousePressEvent if you want to handle selection in
// parent
void RoutingEntryWidget::mousePressEvent(QMouseEvent *event)
{
    emit clicked();
    QFrame::mousePressEvent(event);
}

void RoutingEntryWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    
    // Duplicate entry
    QAction *duplicateAction = menu.addAction(QIcon(":/icons/copy.svg"), "Duplicate Entry");
    connect(duplicateAction, &QAction::triggered, this, [this]() {
        NoteNagaRoutingEntry newEntry = *entry;  // Copy the entry
        engine->getMixer()->addRoutingEntry(newEntry);
    });
    
    menu.addSeparator();
    
    // Reassign to different synth
    QMenu *reassignMenu = menu.addMenu(QIcon(":/icons/route.svg"), "Reassign to Synth");
    auto synthesizers = engine->getSynthesizers();
    for (NoteNagaSynthesizer *synth : synthesizers) {
        QString synthName = QString::fromStdString(synth->getName());
        QAction *synthAction = reassignMenu->addAction(synthName);
        synthAction->setCheckable(true);
        synthAction->setChecked(entry->output == synth->getName());
        connect(synthAction, &QAction::triggered, this, [this, synth]() {
            entry->output = synth->getName();
            updateOutputLabel();
            engine->getMixer()->routingEntryStackChanged();
        });
    }
    
    menu.addSeparator();
    
    // Reset to defaults
    QAction *resetAction = menu.addAction(QIcon(":/icons/reload.svg"), "Reset to Defaults");
    connect(resetAction, &QAction::triggered, this, [this]() {
        entry->channel = 0;
        entry->volume = 1.0f;
        entry->pan = 0.0f;
        entry->note_offset = 0;
        channel_dial->setValue(1);
        volume_dial->setValue(100);
        pan_dial->setValue(0.0);
        offset_dial->setValue(0);
    });
    
    menu.addSeparator();
    
    // Remove entry
    QAction *removeAction = menu.addAction(QIcon(":/icons/remove.svg"), "Remove Entry");
    connect(removeAction, &QAction::triggered, this, [this]() {
        // Find the index of this entry in the mixer
        auto &entries = engine->getMixer()->getRoutingEntries();
        for (size_t i = 0; i < entries.size(); ++i) {
            if (&entries[i] == entry) {
                engine->getMixer()->removeRoutingEntry(static_cast<int>(i));
                break;
            }
        }
    });
    
    menu.exec(event->globalPos());
}