#include "track_mixer_widget.h"
#include <QIcon>

TrackMixerWidget::TrackMixerWidget(AppContext* ctx_, Mixer* mixer_, QWidget* parent)
    : QWidget(parent), ctx(ctx_), mixer(mixer_), selected_entry_index(-1), selected_row(-1)
{
    setObjectName("TrackMixerWidget");
    connect(ctx, &AppContext::mixer_playing_note_signal, this, &TrackMixerWidget::_handle_playing_note);
    connect(mixer, &Mixer::routing_entry_stack_changed_signal, this, &TrackMixerWidget::refresh_routing_table);
    _init_ui();
}

void TrackMixerWidget::_init_ui()
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->setSpacing(0);

    // Header frame
    QFrame* header_frame = new QFrame();
    header_frame->setObjectName("MixerHeaderFrame");
    header_frame->setStyleSheet(
        "QFrame#MixerHeaderFrame { background: #353a44; border-radius: 9px; margin-bottom: 8px; }"
    );
    QHBoxLayout* header_layout = new QHBoxLayout(header_frame);
    header_layout->setContentsMargins(10, 5, 10, 5);
    header_layout->setSpacing(12);

    QLabel* header_icon = new QLabel();
    header_icon->setPixmap(QIcon(":/icons/mixer.svg").pixmap(23, 23));
    header_icon->setFixedSize(23, 23);

    QLabel* title = new QLabel("Track Mixer");
    title->setStyleSheet("font-size: 20px; font-weight: bold; color: #79b8ff; letter-spacing: 1.2px;");
    header_layout->addWidget(header_icon, 0, Qt::AlignVCenter);
    header_layout->addWidget(title, 0, Qt::AlignVCenter);
    header_layout->addStretch(1);

    main_layout->addWidget(header_frame);

    // Controls frame
    QFrame* controls_frame = new QFrame();
    controls_frame->setObjectName("MixerControlsFrame");
    controls_frame->setStyleSheet(
        "QFrame#MixerControlsFrame { background: #2F3139; border: 1px solid #494d56; border-radius: 10px; padding: 10px 0px 8px 0px; }"
    );
    QHBoxLayout* controls_layout = new QHBoxLayout(controls_frame);
    controls_layout->setContentsMargins(5, 0, 5, 0);

    dial_min = new AudioDial();
    dial_min->setLabel("Note Min");
    dial_min->setRange(0, 127);
    dial_min->setValue(mixer->master_min_note);
    dial_min->setDefaultValue(0);
    dial_min->showValue(true);
    dial_min->setValueDecimals(0);
    connect(dial_min, &AudioDial::valueChanged, this, &TrackMixerWidget::on_min_note_changed);

    dial_max = new AudioDial();
    dial_max->setLabel("Note Max");
    dial_max->setRange(0, 127);
    dial_max->setValue(mixer->master_max_note);
    dial_max->setDefaultValue(127);
    dial_max->showValue(true);
    dial_max->setValueDecimals(0);
    connect(dial_max, &AudioDial::valueChanged, this, &TrackMixerWidget::on_max_note_changed);

    dial_offset = new AudioDialCentered();
    dial_offset->setLabel("Offset");
    dial_offset->setRange(-24, 24);
    dial_offset->setValue(mixer->master_note_offset);
    dial_offset->setDefaultValue(0);
    dial_offset->showValue(true);
    dial_offset->setValueDecimals(0);
    connect(dial_offset, &AudioDialCentered::valueChanged, this, &TrackMixerWidget::on_global_offset_changed);

    dial_vol = new AudioDial();
    dial_vol->setLabel("Volume");
    dial_vol->setRange(0, 100);
    dial_vol->setValueDecimals(1);
    dial_vol->setValue(mixer->master_volume * 100);
    dial_vol->setDefaultValue(100);
    dial_vol->setValuePostfix(" %");
    dial_vol->showValue(true);
    connect(dial_vol, &AudioDial::valueChanged, this, &TrackMixerWidget::on_global_volume_changed);

    dial_pan = new AudioDialCentered();
    dial_pan->setLabel("Pan");
    dial_pan->setRange(-1.0, 1.0);
    dial_pan->setValueDecimals(2);
    dial_pan->setValue(mixer->master_pan);
    dial_pan->setDefaultValue(0.0);
    connect(dial_pan, &AudioDialCentered::valueChanged, this, &TrackMixerWidget::on_global_pan_changed);

    controls_layout->addWidget(dial_min, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_max, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_offset, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_vol, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_pan, 0, Qt::AlignVCenter);

    main_layout->addWidget(controls_frame);

    main_layout->addSpacing(10);

    // Channel Output section with device selector
    QFrame* channel_output_frame = new QFrame();
    channel_output_frame->setObjectName("MixerSectionLabelFrame");
    channel_output_frame->setStyleSheet(
        "QFrame#MixerSectionLabelFrame { background: #353a44; border-radius: 8px; margin-bottom: 0px; }"
    );
    QHBoxLayout* channel_output_label_layout = new QHBoxLayout(channel_output_frame);
    channel_output_label_layout->setContentsMargins(12, 5, 12, 5);

    QLabel* channel_output_label = new QLabel("Channel Output");
    channel_output_label->setStyleSheet("font-size: 15px; font-weight: bold; color: #79b8ff;");
    channel_output_label_layout->addWidget(channel_output_label, 0, Qt::AlignLeft);

    device_selector = new QComboBox();
    device_selector->setMinimumWidth(130);
    device_selector->setMaximumWidth(220);
    device_selector->setStyleSheet(
        "QComboBox { background: #232731; color: #79b8ff; font-weight: bold; border-radius: 5px; padding: 3px 8px; }"
    );
    channel_output_label_layout->addStretch(1);
    channel_output_label_layout->addWidget(device_selector, 0, Qt::AlignRight);

    main_layout->addWidget(channel_output_frame);
    main_layout->addSpacing(6);

    // MultiChannelVolumeBar for each device
    QVector<QString> outputs = mixer->get_available_outputs();
    for (const QString& dev : outputs) {
        MultiChannelVolumeBar* bar = new MultiChannelVolumeBar(16);
        bar->setMinimumHeight(90);
        bar->setMaximumHeight(120);
        bar->setRange(0, 1);
        bar->setVisible(false);
        channel_volume_bars[dev] = bar;
        main_layout->addWidget(bar);
    }
    if (!outputs.isEmpty()) {
        device_selector->addItems(outputs.toList());
        current_channel_device = outputs[0];
        channel_volume_bars[current_channel_device]->setVisible(true);
    }
    connect(device_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int idx) {
        QString selected = device_selector->itemText(idx);
        for (auto it = channel_volume_bars.begin(); it != channel_volume_bars.end(); ++it) {
            it.value()->setVisible(false);
        }
        if (channel_volume_bars.contains(selected)) {
            channel_volume_bars[selected]->setVisible(true);
            current_channel_device = selected;
        }
    });

    main_layout->addSpacing(18);

    // Routing Table section
    QFrame* routing_label_controls_frame = new QFrame();
    routing_label_controls_frame->setObjectName("RoutingLabelControlsFrame");
    routing_label_controls_frame->setStyleSheet("QFrame#RoutingLabelControlsFrame { background: #353a44; border-radius: 8px; }");
    QHBoxLayout* routing_label_controls_layout = new QHBoxLayout(routing_label_controls_frame);
    routing_label_controls_layout->setContentsMargins(12, 5, 12, 5);
    routing_label_controls_layout->setSpacing(10);

    QLabel* routing_label = new QLabel("Routing Table");
    routing_label->setStyleSheet("font-size: 15px; font-weight: bold; color: #79b8ff;");
    routing_label_controls_layout->addWidget(routing_label, 0, Qt::AlignLeft);

    routing_label_controls_layout->addStretch(1);

    auto make_btn = [](const QString& iconPath, const QString& tooltip, const char* objname) -> QPushButton* {
        QPushButton* btn = new QPushButton();
        btn->setObjectName(objname);
        btn->setIcon(QIcon(iconPath));
        btn->setToolTip(tooltip);
        btn->setFlat(true);
        btn->setFixedSize(26,26);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 6px; }"
            "QPushButton:hover { background: #3477c0; color: #fff; }"
        );
        return btn;
    };

    QPushButton* btn_add = make_btn(":/icons/add.svg", "Add new routing entry", "RoutingAddButton");
    connect(btn_add, &QPushButton::clicked, this, &TrackMixerWidget::_on_add_entry);

    QPushButton* btn_remove = make_btn(":/icons/remove.svg", "Remove selected routing entry", "RoutingRemoveButton");
    connect(btn_remove, &QPushButton::clicked, this, &TrackMixerWidget::_on_remove_selected_entry);

    QPushButton* btn_clear = make_btn(":/icons/clear.svg", "Clear all routing entries", "RoutingClearButton");
    connect(btn_clear, &QPushButton::clicked, this, &TrackMixerWidget::_on_clear_routing_table);

    QPushButton* btn_default = make_btn(":/icons/reload.svg", "Set default routing (one entry per track, Fluidsynth)", "RoutingDefaultButton");
    connect(btn_default, &QPushButton::clicked, this, &TrackMixerWidget::_on_default_entries);

    routing_label_controls_layout->addWidget(btn_add, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_remove, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_clear, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_default, 0, Qt::AlignRight);

    main_layout->addWidget(routing_label_controls_frame);
    main_layout->addSpacing(6);

    // Routing entries scroll area
    routing_scroll = new QScrollArea(this);
    routing_scroll->setWidgetResizable(true);
    routing_scroll->setFrameShape(QFrame::NoFrame);
    routing_scroll->setMinimumHeight(250);
    routing_scroll->setStyleSheet("QScrollArea { background: transparent; padding: 0px; }");
    main_layout->addWidget(routing_scroll, 1);

    routing_entries_container = new QWidget();
    routing_entries_layout = new QVBoxLayout(routing_entries_container);
    routing_entries_layout->setContentsMargins(3, 3, 3, 3);
    routing_entries_layout->setSpacing(4);
    routing_entries_layout->addStretch(1);
    routing_scroll->setWidget(routing_entries_container);

    setStyleSheet(
        "QWidget#TrackMixerWidget { background: transparent; border: none; padding: 0px; }"
        "QPushButton { min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px; padding: 0px; }"
    );

    refresh_routing_table();
}

void TrackMixerWidget::on_min_note_changed(float value) {
    mixer->master_min_note = int(value);
}
void TrackMixerWidget::on_max_note_changed(float value) {
    mixer->master_max_note = int(value);
}
void TrackMixerWidget::on_global_offset_changed(float value) {
    mixer->master_note_offset = int(value);
}
void TrackMixerWidget::on_global_volume_changed(float value) {
    mixer->master_volume = float(value / 100.0f);
}
void TrackMixerWidget::on_global_pan_changed(float value) {
    mixer->master_pan = value;
}

void TrackMixerWidget::set_channel_output_value(const QString& device, int channel_idx, float value, int time_ms) {
    if (channel_volume_bars.contains(device)) {
        channel_volume_bars[device]->setValue(channel_idx, value, time_ms);
    }
}

void TrackMixerWidget::refresh_routing_table() {
    QVBoxLayout* layout = routing_entries_layout;
    selected_entry_index = -1;
    // Remove all widgets except last stretch
    for (int i = layout->count() - 2; i >= 0; --i) {
        QLayoutItem* item = layout->itemAt(i);
        QWidget* widget = item->widget();
        if (widget) {
            widget->setParent(nullptr);
            widget->deleteLater();
            layout->removeWidget(widget);
        } else {
            layout->removeItem(item);
        }
    }
    entry_widgets.clear();
    for (int idx = 0; idx < mixer->get_routing_entries().size(); ++idx) {
        RoutingEntryWidget* widget = new RoutingEntryWidget(mixer->get_routing_entries()[idx], mixer, ctx);
        widget->installEventFilter(this);
        widget->setMouseTracking(true);
        connect(widget, &RoutingEntryWidget::clicked, this, [this, idx]() {
            this->_update_entry_selection(idx);
        });
        layout->insertWidget(layout->count() - 1, widget);
        entry_widgets.push_back(widget);
    }
}

void TrackMixerWidget::_on_add_entry() {
    mixer->add_routing_entry();
}

void TrackMixerWidget::_on_remove_selected_entry() {
    if (selected_entry_index >= 0) {
        mixer->remove_routing_entry(selected_entry_index);
    } else {
        QMessageBox::warning(this, "No Entry Selected", "Please select a routing entry to remove.", QMessageBox::Ok);
    }
}

void TrackMixerWidget::_on_clear_routing_table() {
    auto reply = QMessageBox::question(
        this, "Clear Routing Table", "Are you sure you want to clear all routing entries?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );
    if (reply == QMessageBox::Yes) {
        mixer->clear_routing_table();
    }
}

void TrackMixerWidget::_on_default_entries() {
    auto reply = QMessageBox::question(
        this, "Set Default Routing", "This will clear all routing entries and set default routing. Continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );
    if (reply == QMessageBox::Yes) {
        mixer->clear_routing_table();
        mixer->create_default_routing();
    }
}

void TrackMixerWidget::_handle_playing_note(const MidiNote& note, const QString& device_name, int channel) {
    int time_ms = int(note_time_ms(note, ctx->ppq, ctx->tempo));
    if (note.velocity.has_value() && note.velocity.value() > 0) {
        set_channel_output_value(device_name, channel, note.velocity.value() / 127.0f, time_ms);
    }
}

void TrackMixerWidget::_update_entry_selection(int idx) {
    selected_row = idx;
    for (size_t i = 0; i < entry_widgets.size(); ++i) {
        entry_widgets[i]->refresh_style(int(i) == idx);
        if (int(i) == idx) {
            selected_entry_index = int(i);
        }
    }
}