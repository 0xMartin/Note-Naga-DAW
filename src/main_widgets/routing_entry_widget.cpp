#include "routing_entry_widget.h"

RoutingEntryWidget::RoutingEntryWidget(TrackRountingEntry& entry_, Mixer* mixer_, AppContext* ctx_, QWidget* parent)
    : QFrame(parent), entry(entry_), mixer(mixer_), ctx(ctx_)
{
    setObjectName("RoutingEntryWidget");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Layout
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 1, 2, 1);
    layout->setSpacing(5);

    // Combo column
    QVBoxLayout* combo_col = new QVBoxLayout();
    combo_col->setContentsMargins(0, 0, 0, 0);
    combo_col->setSpacing(2);

    // Track row
    QHBoxLayout* track_row = new QHBoxLayout();
    track_row->setContentsMargins(0, 0, 0, 0);
    track_row->setSpacing(4);
    QLabel* track_icon = new QLabel();
    track_icon->setFixedSize(18, 18);
    track_icon->setAlignment(Qt::AlignCenter);
    track_icon->setObjectName("RoutingTrackIcon");
    track_icon->setStyleSheet(
        "QLabel#RoutingTrackIcon { min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px;"
        "border-radius: 3px; background: transparent; }"
    );
    track_icon->setPixmap(QIcon(":/icons/track.svg").pixmap(16, 16));
    track_row->addWidget(track_icon, Qt::AlignVCenter);

    track_combo = new QComboBox();
    track_combo->setFixedWidth(120);
    connect(track_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RoutingEntryWidget::_on_track_changed);
    track_row->addWidget(track_combo);
    combo_col->addLayout(track_row);

    // Device row
    QHBoxLayout* device_row = new QHBoxLayout();
    device_row->setContentsMargins(0, 0, 0, 0);
    device_row->setSpacing(4);
    QLabel* device_icon = new QLabel();
    device_icon->setFixedSize(18, 18);
    device_icon->setAlignment(Qt::AlignCenter);
    device_icon->setObjectName("RoutingDeviceIcon");
    device_icon->setStyleSheet(
        "QLabel#RoutingDeviceIcon { min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px;"
        "border-radius: 3px; background: transparent; }"
    );
    device_icon->setPixmap(QIcon(":/icons/route.svg").pixmap(16, 16));
    device_row->addWidget(device_icon, Qt::AlignVCenter);

    output_combo = new QComboBox();
    output_combo->setFixedWidth(120);
    connect(output_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RoutingEntryWidget::_on_device_changed);
    device_row->addWidget(output_combo);
    combo_col->addLayout(device_row);

    // --- Dials ---
    QHBoxLayout* dials_layout = new QHBoxLayout();
    dials_layout->setSpacing(4);
    dials_layout->setContentsMargins(0, 0, 0, 0);

    // Channel
    channel_dial = new AudioDial();
    channel_dial->setLabel("Ch");
    channel_dial->setRange(1, 16);
    channel_dial->setValue(entry.channel + 1);
    channel_dial->setDefaultValue(1);
    channel_dial->showValue(true);
    channel_dial->setValueDecimals(0);
    connect(channel_dial, &AudioDial::valueChanged, this, &RoutingEntryWidget::_on_channel_changed);
    dials_layout->addWidget(channel_dial);

    // Volume
    volume_dial = new AudioDial();
    volume_dial->setLabel("Vol");
    volume_dial->setRange(0, 100);
    volume_dial->setValue(entry.volume * 100);
    volume_dial->setDefaultValue(100);
    volume_dial->setValueDecimals(1);
    volume_dial->setValuePostfix(" %");
    volume_dial->showValue(true);
    connect(volume_dial, &AudioDial::valueChanged, this, &RoutingEntryWidget::_on_volume_changed);
    dials_layout->addWidget(volume_dial);

    // Pan
    pan_dial = new AudioDialCentered();
    pan_dial->setLabel("Pan");
    pan_dial->setRange(-1.0, 1.0);
    pan_dial->setValueDecimals(2);
    pan_dial->setValue(entry.pan);
    pan_dial->setDefaultValue(0.0);
    connect(pan_dial, &AudioDialCentered::valueChanged, this, &RoutingEntryWidget::on_global_pan_changed);
    dials_layout->addWidget(pan_dial);

    // Note offset
    offset_dial = new AudioDialCentered();
    offset_dial->setLabel("Offset");
    offset_dial->setRange(-12, 12);
    offset_dial->setValue(entry.note_offset);
    offset_dial->showValue(true);
    offset_dial->setValueDecimals(0);
    offset_dial->setDefaultValue(0);
    connect(offset_dial, &AudioDialCentered::valueChanged, this, &RoutingEntryWidget::_on_offset_changed);
    dials_layout->addWidget(offset_dial);

    // Main layout
    layout->addLayout(combo_col);
    layout->addStretch(1);
    layout->addLayout(dials_layout);
    setLayout(layout);

    // ComboBox initialization
    _populate_track_combo();
    _populate_output_combo();
    _set_combo_selections();

    // Connect to track info changed (refresh combos)
    connect(ctx, &AppContext::track_meta_changed_signal, this, &RoutingEntryWidget::on_track_info_changed);

    // Style
    refresh_style(false);
}

void RoutingEntryWidget::_populate_track_combo() {
    track_combo->blockSignals(true);
    track_combo->clear();
    for (size_t idx = 0; idx < ctx->tracks.size(); ++idx) {
        auto& tr = ctx->tracks[idx];
        QString name = QString("%1: %2").arg(idx + 1).arg(tr->name);
        track_combo->addItem(name, tr->track_id);
    }
    int current_track_index = track_combo->findData(entry.track_id);
    if (current_track_index >= 0)
        track_combo->setCurrentIndex(current_track_index);
    track_combo->blockSignals(false);
}

void RoutingEntryWidget::_populate_output_combo() {
    output_combo->blockSignals(true);
    output_combo->clear();
    for (const QString& out : mixer->get_available_outputs()) {
        output_combo->addItem(out);
    }
    output_combo->addItem(TRACK_ROUTING_ENTRY_ANY_DEVICE);
    int idx = output_combo->findText(entry.output);
    if (idx >= 0)
        output_combo->setCurrentIndex(idx);
    output_combo->blockSignals(false);
}

void RoutingEntryWidget::_set_combo_selections() {
    int track_idx = track_combo->findData(entry.track_id);
    if (track_idx >= 0)
        track_combo->setCurrentIndex(track_idx);
    int dev_idx = output_combo->findText(entry.output);
    if (dev_idx >= 0)
        output_combo->setCurrentIndex(dev_idx);
}

void RoutingEntryWidget::on_track_info_changed(int /*track_id*/) {
    _populate_track_combo();
    _populate_output_combo();
}

void RoutingEntryWidget::_on_track_changed(int idx) {
    int new_track_id = track_combo->itemData(idx).toInt();
    entry.track_id = new_track_id;
}

void RoutingEntryWidget::_on_device_changed(int idx) {
    QString new_device = output_combo->currentText();
    entry.output = new_device;
}

void RoutingEntryWidget::_on_channel_changed(float val) {
    entry.channel = int(val - 1);
}

void RoutingEntryWidget::_on_volume_changed(float val) {
    entry.volume = float(val / 100.0f);
}

void RoutingEntryWidget::_on_offset_changed(float val) {
    entry.note_offset = int(val);
}

void RoutingEntryWidget::on_global_pan_changed(float value) {
    entry.pan = value;
}

void RoutingEntryWidget::refresh_style(bool selected) {
    QString base_style = R"(
        QFrame#RoutingEntryWidget {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 2px;
        }
    )";
    QString style = selected
        ? base_style.arg("#273a51", "#3477c0")
        : base_style.arg("#2F3139", "#494d56");
    setStyleSheet(style);
    update();
}

// Optional: You can override mousePressEvent if you want to handle selection in parent
void RoutingEntryWidget::mousePressEvent(QMouseEvent* event) {
    emit clicked();
    QFrame::mousePressEvent(event);
}