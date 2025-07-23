#include "track_mixer_widget.h"

#include <QIcon>
#include "../nn_gui_utils.h"

TrackMixerWidget::TrackMixerWidget(NoteNagaEngine *engine_, QWidget *parent)
    : QWidget(parent), engine(engine_), selected_entry_index(-1), selected_row(-1) {
    setObjectName("TrackMixerWidget");
    connect(engine->getMixer(), &NoteNagaMixer::noteOutSignal, this,
            &TrackMixerWidget::handlePlayingNote);
    connect(engine->getMixer(), &NoteNagaMixer::routingEntryStackChanged, this,
            &TrackMixerWidget::refresh_routing_table);
    
    this->title_widget = nullptr;
    initTitleUI();
    initUI();
}

void TrackMixerWidget::initTitleUI() {
    if (this->title_widget) return;
    this->title_widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(title_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QPushButton *btn_settings = create_small_button(
        ":/icons/settings.svg", "Mixer Settings", "MixerSettingsButton");

    layout->addWidget(btn_settings, 0, Qt::AlignRight);
}

void TrackMixerWidget::initUI() {
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->setSpacing(0);

    // Controls frame
    QFrame *controls_frame = new QFrame();
    controls_frame->setObjectName("MixerControlsFrame");
    controls_frame->setStyleSheet(
        "QFrame#MixerControlsFrame { background: #2F3139; border: 1px solid #494d56; "
        "border-radius: 10px; padding: 10px 0px 8px 0px; }");
    QHBoxLayout *controls_layout = new QHBoxLayout(controls_frame);
    controls_layout->setContentsMargins(5, 0, 5, 0);

    dial_min = new AudioDial();
    dial_min->setLabel("Note Min");
    dial_min->setRange(0, 127);
    dial_min->setValue(engine->getMixer()->master_min_note);
    dial_min->setDefaultValue(0);
    dial_min->showValue(true);
    dial_min->setValueDecimals(0);
    connect(dial_min, &AudioDial::valueChanged, this,
            &TrackMixerWidget::onMinNoteChanged);

    dial_max = new AudioDial();
    dial_max->setLabel("Note Max");
    dial_max->setRange(0, 127);
    dial_max->setValue(engine->getMixer()->master_max_note);
    dial_max->setDefaultValue(127);
    dial_max->showValue(true);
    dial_max->setValueDecimals(0);
    connect(dial_max, &AudioDial::valueChanged, this,
            &TrackMixerWidget::onMaxNoteChanged);

    dial_offset = new AudioDialCentered();
    dial_offset->setLabel("Offset");
    dial_offset->setRange(-24, 24);
    dial_offset->setValue(engine->getMixer()->master_note_offset);
    dial_offset->setDefaultValue(0);
    dial_offset->showValue(true);
    dial_offset->setValueDecimals(0);
    connect(dial_offset, &AudioDialCentered::valueChanged, this,
            &TrackMixerWidget::onGlobalOffsetChanged);

    dial_vol = new AudioDial();
    dial_vol->setLabel("Volume");
    dial_vol->setRange(0, 100);
    dial_vol->setValueDecimals(1);
    dial_vol->setValue(engine->getMixer()->master_volume * 100);
    dial_vol->setDefaultValue(100);
    dial_vol->setValuePostfix(" %");
    dial_vol->showValue(true);
    connect(dial_vol, &AudioDial::valueChanged, this,
            &TrackMixerWidget::onGlobalVolumeChanged);

    dial_pan = new AudioDialCentered();
    dial_pan->setLabel("Pan");
    dial_pan->setRange(-1.0, 1.0);
    dial_pan->setValueDecimals(2);
    dial_pan->setValue(engine->getMixer()->master_pan);
    dial_pan->setDefaultValue(0.0);
    connect(dial_pan, &AudioDialCentered::valueChanged, this,
            &TrackMixerWidget::onGlobalPanChanged);

    controls_layout->addWidget(dial_min, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_max, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_offset, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_vol, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_pan, 0, Qt::AlignVCenter);

    main_layout->addWidget(controls_frame);

    main_layout->addSpacing(10);

    // Channel Output section with device selector
    QFrame *channel_output_frame = new QFrame();
    channel_output_frame->setObjectName("MixerSectionLabelFrame");
    channel_output_frame->setStyleSheet(
        "QFrame#MixerSectionLabelFrame { background: #353a44; border-radius: 8px; "
        "margin-bottom: 0px; }");
    QHBoxLayout *channel_output_label_layout = new QHBoxLayout(channel_output_frame);
    channel_output_label_layout->setContentsMargins(12, 5, 12, 5);

    QLabel *channel_output_label = new QLabel("Channel Output");
    channel_output_label->setStyleSheet(
        "font-size: 15px; font-weight: bold; color: #79b8ff;");
    channel_output_label_layout->addWidget(channel_output_label, 0, Qt::AlignLeft);

    device_selector = new QComboBox();
    device_selector->setMinimumWidth(130);
    device_selector->setMaximumWidth(220);
    device_selector->setStyleSheet(
        "QComboBox { background: #232731; color: #79b8ff; font-weight: bold; "
        "border-radius: 5px; padding: 3px 8px; }");
    channel_output_label_layout->addStretch(1);
    channel_output_label_layout->addWidget(device_selector, 0, Qt::AlignRight);

    main_layout->addWidget(channel_output_frame);
    main_layout->addSpacing(6);

    // MultiChannelVolumeBar for each device
    std::vector<std::string> outputs = engine->getMixer()->getAvailableOutputs();
    for (const std::string &dev : outputs) {
        MultiChannelVolumeBar *bar = new MultiChannelVolumeBar(16);
        bar->setMinimumHeight(90);
        bar->setMaximumHeight(120);
        bar->setRange(0, 127);
        bar->setVisible(false);
        channel_volume_bars[QString::fromStdString(dev)] = bar;
        main_layout->addWidget(bar);
    }
    if (!outputs.empty()) {
        QStringList list;
        for (const auto &s : outputs) {
            list << QString::fromStdString(s);
        }
        device_selector->addItems(list);
        current_channel_device = QString::fromStdString(outputs[0]);
        channel_volume_bars[current_channel_device]->setVisible(true);
    }
    connect(device_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [=](int idx) {
                QString selected = device_selector->itemText(idx);
                for (auto it = channel_volume_bars.begin();
                     it != channel_volume_bars.end(); ++it) {
                    it.value()->setVisible(false);
                }
                if (channel_volume_bars.contains(selected)) {
                    channel_volume_bars[selected]->setVisible(true);
                    current_channel_device = selected;
                }
            });

    main_layout->addSpacing(18);

    // Routing Table section
    QFrame *routing_label_controls_frame = new QFrame();
    routing_label_controls_frame->setObjectName("RoutingLabelControlsFrame");
    routing_label_controls_frame->setStyleSheet(
        "QFrame#RoutingLabelControlsFrame { background: #353a44; border-radius: 8px; }");
    QHBoxLayout *routing_label_controls_layout =
        new QHBoxLayout(routing_label_controls_frame);
    routing_label_controls_layout->setContentsMargins(12, 5, 12, 5);
    routing_label_controls_layout->setSpacing(4);

    QLabel *routing_label = new QLabel("Routing Table");
    routing_label->setStyleSheet("font-size: 15px; font-weight: bold; color: #79b8ff;");
    routing_label_controls_layout->addWidget(routing_label, 0, Qt::AlignLeft);

    routing_label_controls_layout->addStretch(1);

    QPushButton *btn_add =
        create_small_button(":/icons/add.svg", "Add new routing entry", "RoutingAddButton");
    connect(btn_add, &QPushButton::clicked, this, &TrackMixerWidget::onAddEntry);

    QPushButton *btn_remove = create_small_button(
        ":/icons/remove.svg", "Remove selected routing entry", "RoutingRemoveButton");
    connect(btn_remove, &QPushButton::clicked, this,
            &TrackMixerWidget::onRemoveSelectedEntry);

    QPushButton *btn_clear =
        create_small_button(":/icons/clear.svg", "Clear all routing entries", "RoutingClearButton");
    connect(btn_clear, &QPushButton::clicked, this,
            &TrackMixerWidget::onClearRoutingTable);

    QPushButton *btn_default = create_small_button(
        ":/icons/reload.svg", "Set default routing (one entry per track, Fluidsynth)",
        "RoutingDefaultButton");
    connect(btn_default, &QPushButton::clicked, this,
            &TrackMixerWidget::onDefaultEntries);

    QPushButton *btn_max_volume = create_small_button(
        ":/icons/sound-on.svg", "Toggle max volume for all tracks",
        "MaxVolumeAllTracksButton");
    btn_max_volume->setCheckable(true);
    connect(btn_max_volume, &QPushButton::clicked, this, &TrackMixerWidget::onMaxVolumeAllTracks);

    QPushButton *btn_min_volume = create_small_button(
        ":/icons/sound-off.svg", "Set min volume for all tracks",
        "MinVolumeAllTracksButton");
    btn_min_volume->setCheckable(true);
    connect(btn_min_volume, &QPushButton::clicked, this, &TrackMixerWidget::onMinVolumeAllTracks);

    QPushButton *btn_output_device = create_small_button(
        ":/icons/device.svg", "Set output device for all tracks",
        "OutputDeviceAllTracksButton");
    btn_output_device->setCheckable(true);
    //connect(btn_output_device, &QPushButton::clicked, this, &TrackMixerWidget::onOutputDeviceAllTracks);

    routing_label_controls_layout->addWidget(btn_add, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_remove, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_clear, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_default, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_max_volume, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_min_volume, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_output_device, 0, Qt::AlignRight);

    main_layout->addWidget(routing_label_controls_frame);
    main_layout->addSpacing(6);

    // Routing entries scroll area
    routing_scroll = new QScrollArea(this);
    routing_scroll->setWidgetResizable(true);
    routing_scroll->setFrameShape(QFrame::NoFrame);
    routing_scroll->setMinimumHeight(250);
    routing_scroll->setStyleSheet(
        "QScrollArea { background: transparent; padding: 0px; }");
    main_layout->addWidget(routing_scroll, 1);

    routing_entries_container = new QWidget();
    routing_entries_layout = new QVBoxLayout(routing_entries_container);
    routing_entries_layout->setContentsMargins(3, 3, 3, 3);
    routing_entries_layout->setSpacing(4);
    routing_entries_layout->addStretch(1);
    routing_scroll->setWidget(routing_entries_container);

    setStyleSheet("QWidget#TrackMixerWidget { background: transparent; border: none; "
                  "padding: 0px; }"
                  "QPushButton { min-width: 24px; max-width: 24px; min-height: 24px; "
                  "max-height: 24px; padding: 0px; }");

    refresh_routing_table();
}

void TrackMixerWidget::onMinNoteChanged(float value) {
    engine->getMixer()->master_min_note = int(value);
}
void TrackMixerWidget::onMaxNoteChanged(float value) {
    engine->getMixer()->master_max_note = int(value);
}
void TrackMixerWidget::onGlobalOffsetChanged(float value) {
    engine->getMixer()->master_note_offset = int(value);
}
void TrackMixerWidget::onGlobalVolumeChanged(float value) {
    engine->getMixer()->master_volume = float(value / 100.0f);
}
void TrackMixerWidget::onGlobalPanChanged(float value) {
    engine->getMixer()->master_pan = value;
}

void TrackMixerWidget::setChannelOutputValue(const std::string &device, int channel_idx,
                                             float value, int time_ms) {
    if (channel_volume_bars.contains(QString::fromStdString(device))) {
        channel_volume_bars[QString::fromStdString(device)]->setValue(channel_idx, value,
                                                                      time_ms);
    }
}

void TrackMixerWidget::refresh_routing_table() {
    QVBoxLayout *layout = routing_entries_layout;
    selected_entry_index = -1;
    // Remove all widgets except last stretch
    for (int i = layout->count() - 2; i >= 0; --i) {
        QLayoutItem *item = layout->itemAt(i);
        QWidget *widget = item->widget();
        if (widget) {
            widget->setParent(nullptr);
            widget->deleteLater();
            layout->removeWidget(widget);
        } else {
            layout->removeItem(item);
        }
    }
    entry_widgets.clear();

    // Populate with current routing entries
    std::vector<NoteNagaRoutingEntry> &entries = engine->getMixer()->getRoutingEntries();
    for (int idx = 0; idx < entries.size(); ++idx) {
        RoutingEntryWidget *widget = new RoutingEntryWidget(engine, &entries[idx]);
        widget->installEventFilter(this);
        widget->setMouseTracking(true);
        connect(widget, &RoutingEntryWidget::clicked, this,
                [this, idx]() { this->updateEntrySelection(idx); });
        layout->insertWidget(layout->count() - 1, widget);
        entry_widgets.push_back(widget);
    }
}

void TrackMixerWidget::onAddEntry() { engine->getMixer()->addRoutingEntry(); }

void TrackMixerWidget::onRemoveSelectedEntry() {
    if (selected_entry_index >= 0) {
        engine->getMixer()->removeRoutingEntry(selected_entry_index);
    } else {
        QMessageBox::warning(this, "No Entry Selected",
                             "Please select a routing entry to remove.", QMessageBox::Ok);
    }
}

void TrackMixerWidget::onClearRoutingTable() {
    auto reply =
        QMessageBox::question(this, "Clear Routing Table",
                              "Are you sure you want to clear all routing entries?",
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) { engine->getMixer()->clearRoutingTable(); }
}

void TrackMixerWidget::onDefaultEntries() {
    auto reply = QMessageBox::question(
        this, "Set Default Routing",
        "This will clear all routing entries and set default routing. Continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        engine->getMixer()->clearRoutingTable();
        engine->getMixer()->createDefaultRouting();
    }
}

void TrackMixerWidget::onMaxVolumeAllTracks() {
    for (auto &entry : engine->getMixer()->getRoutingEntries()) {
        entry.volume = 1.0f;
    }
    refresh_routing_table();
}

void TrackMixerWidget::onMinVolumeAllTracks() {
    for (auto &entry : engine->getMixer()->getRoutingEntries()) {
        entry.volume = 0.0f;
    }
    refresh_routing_table();
}

void TrackMixerWidget::handlePlayingNote(const NN_Note_t &note,
                                         const std::string &device_name, int channel) {
    // channel signalization
    NoteNagaProject *project = engine->getProject();
    int time_ms = int(note_time_ms(note, project->getPPQ(), project->getTempo()));
    if (note.velocity.has_value() && note.velocity.value() > 0) {
        setChannelOutputValue(device_name, channel, note.velocity.value(), time_ms);
    }

    // entry signalization
    NoteNagaTrack *track = note.parent;
    if (!track) return;
    for (size_t i = 0; i < entry_widgets.size(); ++i) {
        RoutingEntryWidget *entry_widget = entry_widgets[i];
        if (!entry_widget) continue;
        NoteNagaRoutingEntry *entry = entry_widget->getRoutingEntry();
        if (!entry) continue;
        if (entry->track == track) {
            entry_widget->getIndicatorLed()->setState(true, false, time_ms);     
        }
    }
}

void TrackMixerWidget::updateEntrySelection(int idx) {
    selected_row = idx;
    for (size_t i = 0; i < entry_widgets.size(); ++i) {
        entry_widgets[i]->refresh_style(int(i) == idx);
        if (int(i) == idx) { selected_entry_index = int(i); }
    }
}