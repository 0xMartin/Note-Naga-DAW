#include "track_mixer_widget.h"

#include <note_naga_engine/core/note_naga_synthesizer.h>
#include <note_naga_engine/core/types.h>

#include <QIcon>
#include <QInputDialog>

#include "../nn_gui_utils.h"

TrackMixerWidget::TrackMixerWidget(NoteNagaEngine *engine_, QWidget *parent)
    : QWidget(parent), engine(engine_), selected_entry_index(-1),
      current_synth_name(TRACK_ROUTING_ENTRY_ANY_DEVICE) {
    setObjectName("TrackMixerWidget");
    
    // Connect to engine signals
    connect(engine->getMixer(), &NoteNagaMixer::noteOutSignal, this,
            &TrackMixerWidget::handlePlayingNote);
    connect(engine->getMixer(), &NoteNagaMixer::routingEntryStackChanged, this,
            &TrackMixerWidget::refresh_routing_table);
    
    // Connect to synthesizer signals
    connect(engine, &NoteNagaEngine::synthAdded, this,
            &TrackMixerWidget::onSynthesizerAdded);
    connect(engine, &NoteNagaEngine::synthRemoved, this,
            &TrackMixerWidget::onSynthesizerRemoved);
    connect(engine, &NoteNagaEngine::synthUpdated, this,
            &TrackMixerWidget::onSynthesizerUpdated);

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
    // Title widget is now empty (Mixer Settings moved to Project Section)
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
        "border-radius: 8px; padding: 2px 0px 0px 0px; }");
    QHBoxLayout *controls_layout = new QHBoxLayout(controls_frame);
    controls_layout->setContentsMargins(5, 0, 5, 0);

    dial_min = new AudioDial();
    dial_min->setLabel("Note Min");
    dial_min->setRange(0, 127);
    dial_min->setValue(engine->getMixer()->getMasterMinNote());
    dial_min->setDefaultValue(0);
    dial_min->showValue(true);
    dial_min->setValueDecimals(0);
    connect(dial_min, &AudioDial::valueChanged, this,
            &TrackMixerWidget::onMinNoteChanged);

    dial_max = new AudioDial();
    dial_max->setLabel("Note Max");
    dial_max->setRange(0, 127);
    dial_max->setValue(engine->getMixer()->getMasterMaxNote());
    dial_max->setDefaultValue(127);
    dial_max->showValue(true);
    dial_max->setValueDecimals(0);
    connect(dial_max, &AudioDial::valueChanged, this,
            &TrackMixerWidget::onMaxNoteChanged);

    dial_offset = new AudioDialCentered();
    dial_offset->setLabel("Offset");
    dial_offset->setRange(-24, 24);
    dial_offset->setValue(engine->getMixer()->getMasterNoteOffset());
    dial_offset->setDefaultValue(0);
    dial_offset->showValue(true);
    dial_offset->setValueDecimals(0);
    connect(dial_offset, &AudioDialCentered::valueChanged, this,
            &TrackMixerWidget::onGlobalOffsetChanged);

    dial_vol = new AudioDial();
    dial_vol->setLabel("Volume");
    dial_vol->setRange(0, 100);
    dial_vol->setValueDecimals(1);
    dial_vol->setValue(engine->getMixer()->getMasterVolume() * 100);
    dial_vol->setDefaultValue(100);
    dial_vol->setValuePostfix(" %");
    dial_vol->showValue(true);
    connect(dial_vol, &AudioDial::valueChanged, this,
            &TrackMixerWidget::onGlobalVolumeChanged);

    dial_pan = new AudioDialCentered();
    dial_pan->setLabel("Pan");
    dial_pan->setRange(-1.0, 1.0);
    dial_pan->setValueDecimals(2);
    dial_pan->setValue(engine->getMixer()->getMasterPan());
    dial_pan->setDefaultValue(0.0);
    connect(dial_pan, &AudioDialCentered::valueChanged, this,
            &TrackMixerWidget::onGlobalPanChanged);

    controls_layout->addWidget(dial_min, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_max, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_offset, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_vol, 0, Qt::AlignVCenter);
    controls_layout->addWidget(dial_pan, 0, Qt::AlignVCenter);

    main_layout->addWidget(controls_frame);
    main_layout->addSpacing(5);
    
    // Synthesizer selector frame
    QFrame *synth_selector_frame = new QFrame();
    synth_selector_frame->setObjectName("SynthSelectorFrame");
    synth_selector_frame->setStyleSheet(
        "QFrame#SynthSelectorFrame { background: #3c424e; border: 1px solid #282b32; }");
    QHBoxLayout *synth_selector_layout = new QHBoxLayout(synth_selector_frame);
    synth_selector_layout->setContentsMargins(12, 5, 12, 5);
    
    QLabel *synth_label = new QLabel("Selected Synth");
    synth_label->setStyleSheet("font-size: 15px; font-weight: bold; color: #79b8ff;");
    synth_selector_layout->addWidget(synth_label, 0, Qt::AlignLeft);
    
    synth_selector = new QComboBox();
    synth_selector->setMinimumWidth(180);
    synth_selector->setMaximumWidth(300);
    synth_selector->setStyleSheet(
        "QComboBox { background: #232731; color: #79b8ff; font-weight: bold; "
        "border-radius: 5px; padding: 3px 8px; }");
    
    synth_selector_layout->addStretch(1);
    synth_selector_layout->addWidget(synth_selector, 0, Qt::AlignRight);
    
    main_layout->addWidget(synth_selector_frame);
    
    // Channel Output section (now with the channel output label and volume bars together)
    QFrame *channel_output_frame = new QFrame();
    channel_output_frame->setObjectName("MixerSectionLabelFrame");
    channel_output_frame->setStyleSheet(
        "QFrame#MixerSectionLabelFrame { background: #3c424e; border: 1px solid #282b32; "
        "margin-bottom: 0px; }");
    main_layout->addWidget(channel_output_frame);

    // MultiChannelVolumeBar for each device - only create these once
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
    
    // Set default visible channel volume bar
    if (!outputs.empty() && !channel_volume_bars.empty()) {
        current_channel_device = QString::fromStdString(outputs[0]);
        if (channel_volume_bars.contains(current_channel_device)) {
            channel_volume_bars[current_channel_device]->setVisible(true);
        }
    }
    
    // Fill the synthesizer selector with available synthesizers
    updateSynthesizerSelector();
    
    // Connect the selector to the handler
    connect(synth_selector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackMixerWidget::onSynthesizerSelectionChanged);
    
    // Routing Table section
    QFrame *routing_label_controls_frame = new QFrame();
    routing_label_controls_frame->setObjectName("RoutingLabelControlsFrame");
    routing_label_controls_frame->setStyleSheet(
        "QFrame#RoutingLabelControlsFrame { background: #3c424e; border: 1px solid #282b32; }");
    QHBoxLayout *routing_label_controls_layout =
        new QHBoxLayout(routing_label_controls_frame);
    routing_label_controls_layout->setContentsMargins(12, 5, 12, 5);
    routing_label_controls_layout->setSpacing(0);

    QLabel *routing_label = new QLabel("Routing Table");
    routing_label->setStyleSheet("font-size: 15px; font-weight: bold; color: #79b8ff;");
    routing_label_controls_layout->addWidget(routing_label, 0, Qt::AlignLeft);

    routing_label_controls_layout->addStretch(1);

    QPushButton *btn_add = create_small_button(":/icons/add.svg", "Add new routing entry",
                                               "RoutingAddButton");
    connect(btn_add, &QPushButton::clicked, this, &TrackMixerWidget::onAddEntry);

    QPushButton *btn_remove = create_small_button(
        ":/icons/remove.svg", "Remove selected routing entry", "RoutingRemoveButton");
    connect(btn_remove, &QPushButton::clicked, this,
            &TrackMixerWidget::onRemoveSelectedEntry);

    QPushButton *btn_reassign = create_small_button(
        ":/icons/reassign.svg", "Reassign selected entry to a different synth", "RoutingReassignButton");
    connect(btn_reassign, &QPushButton::clicked, this, &TrackMixerWidget::onReassignSynth);

    QPushButton *btn_clear = create_small_button(
        ":/icons/clear.svg", "Clear all routing entries", "RoutingClearButton");
    connect(btn_clear, &QPushButton::clicked, this,
            &TrackMixerWidget::onClearRoutingTable);

    QPushButton *btn_default = create_small_button(
        ":/icons/reload.svg", "Set default routing (one entry per track, Fluidsynth)",
        "RoutingDefaultButton");
    connect(btn_default, &QPushButton::clicked, this,
            &TrackMixerWidget::onDefaultEntries);

    QPushButton *btn_max_volume =
        create_small_button(":/icons/sound-on.svg", "Toggle max volume for all tracks",
                            "MaxVolumeAllTracksButton");
    connect(btn_max_volume, &QPushButton::clicked, this,
            &TrackMixerWidget::onMaxVolumeAllTracks);

    QPushButton *btn_min_volume =
        create_small_button(":/icons/sound-off.svg", "Set min volume for all tracks",
                            "MinVolumeAllTracksButton");
    connect(btn_min_volume, &QPushButton::clicked, this,
            &TrackMixerWidget::onMinVolumeAllTracks);

    routing_label_controls_layout->addWidget(btn_add, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_remove, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_reassign, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_clear, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_default, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_max_volume, 0, Qt::AlignRight);
    routing_label_controls_layout->addWidget(btn_min_volume, 0, Qt::AlignRight);

    main_layout->addWidget(routing_label_controls_frame);

    // Routing entries scroll area
    routing_scroll = new QScrollArea(this);
    routing_scroll->setWidgetResizable(true);
    routing_scroll->setFrameShape(QFrame::NoFrame);
    routing_scroll->setMinimumHeight(250);
    routing_scroll->setStyleSheet(
        "QScrollArea { background: transparent; padding: 0px; border: none; }");
    main_layout->addWidget(routing_scroll, 1);

    routing_entries_container = new QWidget();
    routing_entries_layout = new QVBoxLayout(routing_entries_container);
    routing_entries_layout->setContentsMargins(0, 0, 0, 0);
    routing_entries_layout->setSpacing(0);
    routing_entries_layout->addStretch(1);
    routing_scroll->setWidget(routing_entries_container);

    setStyleSheet("QWidget#TrackMixerWidget { background: transparent; border: none; "
                  "padding: 0px; }");

    refresh_routing_table();
}

void TrackMixerWidget::updateSynthesizerSelector() {
    // Save current selection if any
    QString currentSelection;
    if (synth_selector->currentIndex() >= 0) {
        currentSelection = synth_selector->currentText();
    }
    
    synth_selector->blockSignals(true);
    synth_selector->clear();
    
    // Add synthesizers to the selector
    auto synthesizers = engine->getSynthesizers();
    for (NoteNagaSynthesizer* synth : synthesizers) {
        QString displayName = QString::fromStdString(synth->getName());
        
        synth_selector->addItem(displayName, QVariant::fromValue(static_cast<void*>(synth)));
    }
    
    synth_selector->addItem(TRACK_ROUTING_ENTRY_ANY_DEVICE);
    
    // Restore previous selection if it still exists
    if (!currentSelection.isEmpty()) {
        int index = synth_selector->findText(currentSelection);
        if (index >= 0) {
            synth_selector->setCurrentIndex(index);
        }
    }
    
    // After restoring the selection, update current_synth_name
    int idx = synth_selector->currentIndex();
    if (idx >= 0) {
        if (synth_selector->itemText(idx) == TRACK_ROUTING_ENTRY_ANY_DEVICE) {
            current_synth_name = TRACK_ROUTING_ENTRY_ANY_DEVICE;
        } else if (void* synthPtr = synth_selector->itemData(idx).value<void*>()) {
            NoteNagaSynthesizer* synth = static_cast<NoteNagaSynthesizer*>(synthPtr);
            current_synth_name = synth->getName();
        }
    }
    
    synth_selector->blockSignals(false);
    
    // Clear the layout of old volume bars
    for (auto it = channel_volume_bars.begin(); it != channel_volume_bars.end();) {
        // Hide and remove the widget
        it.value()->setVisible(false);
        it.value()->deleteLater();
        it = channel_volume_bars.erase(it);
    }
    
    // Create new volume bars for all available synthesizers
    for (NoteNagaSynthesizer* synth : synthesizers) {
        QString synthName = QString::fromStdString(synth->getName());
        MultiChannelVolumeBar *bar = new MultiChannelVolumeBar(16);
        bar->setMinimumHeight(90);
        bar->setMaximumHeight(120);
        bar->setRange(0, 127);
        bar->setVisible(false);
        channel_volume_bars[synthName] = bar;
        
        // Add to layout
        QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(layout());
        if (mainLayout) {
            // Add the bar just after the "Channel Output" frame
            // Find position to insert (after channel output frame but before routing table)
            int insertPos = 0;
            for (int i = 0; i < mainLayout->count(); i++) {
                QLayoutItem* item = mainLayout->itemAt(i);
                if (item->widget() && item->widget()->objectName() == "MixerSectionLabelFrame") {
                    insertPos = i + 1;
                    break;
                }
            }
            mainLayout->insertWidget(insertPos, bar);
        }
    }
    
    // Update the currently visible bar based on selection
    onSynthesizerSelectionChanged(synth_selector->currentIndex());
}

void TrackMixerWidget::onSynthesizerSelectionChanged(int index) {
    if (index < 0) return;
    
    // Get the selected synthesizer name
    QString selectedSynth;
    if (synth_selector->itemText(index) == TRACK_ROUTING_ENTRY_ANY_DEVICE) {
        selectedSynth = TRACK_ROUTING_ENTRY_ANY_DEVICE;
        current_synth_name = TRACK_ROUTING_ENTRY_ANY_DEVICE;
    } else if (void* synthPtr = synth_selector->itemData(index).value<void*>()) {
        NoteNagaSynthesizer* synth = static_cast<NoteNagaSynthesizer*>(synthPtr);
        selectedSynth = QString::fromStdString(synth->getName());
        current_synth_name = synth->getName();
    }
    
    // Update channel volume bars visibility
    for (auto it = channel_volume_bars.begin(); it != channel_volume_bars.end(); ++it) {
        it.value()->setVisible(it.key() == selectedSynth);
        if (it.key() == selectedSynth) {
            current_channel_device = selectedSynth;
        }
    }
    
    // Important: Refresh the routing table to show only entries for this synth
    refresh_routing_table();
}

void TrackMixerWidget::onMinNoteChanged(float value) {
    engine->getMixer()->setMasterMinNote(int(value));
}
void TrackMixerWidget::onMaxNoteChanged(float value) {
    engine->getMixer()->setMasterMaxNote(int(value));
}
void TrackMixerWidget::onGlobalOffsetChanged(float value) {
    engine->getMixer()->setMasterNoteOffset(int(value));
}
void TrackMixerWidget::onGlobalVolumeChanged(float value) {
    engine->getMixer()->setMasterVolume(float(value / 100.0f));
}
void TrackMixerWidget::onGlobalPanChanged(float value) {
    engine->getMixer()->setMasterPan(value);
}

void TrackMixerWidget::setChannelOutputValue(const std::string &device, int channel_idx,
                                             float value, int time_ms) {
    QString device_name = QString::fromStdString(device);
    if (channel_volume_bars.contains(device_name)) {
        if (channel_volume_bars[device_name]->isVisible())
            channel_volume_bars[device_name]->setValue(channel_idx, value, time_ms);
    }
}

void TrackMixerWidget::refresh_routing_table() {
    QVBoxLayout *layout = routing_entries_layout;
    if (layout == nullptr) return;
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

    // Get all routing entries
    std::vector<NoteNagaRoutingEntry> &entries = engine->getMixer()->getRoutingEntries();
    
    // Filter entries based on currently selected synthesizer
    int filteredIdx = 0;
    for (int idx = 0; idx < entries.size(); ++idx) {
        NoteNagaRoutingEntry &entry = entries[idx];
        
        // Skip entries that don't match the current synthesizer
        // Show all entries if "Any" is selected, or if the output matches current synth
        if (current_synth_name != TRACK_ROUTING_ENTRY_ANY_DEVICE && 
            entry.output != current_synth_name) {
            continue;
        }
        
        RoutingEntryWidget *widget = new RoutingEntryWidget(engine, &entry);
        widget->installEventFilter(this);
        widget->setMouseTracking(true);
        widget->refreshStyle(false, filteredIdx % 2 == 0);
        
        // Use original index for selection, not filtered index
        int originalIdx = idx;
        connect(widget, &RoutingEntryWidget::clicked, this,
                [this, originalIdx]() { this->updateEntrySelection(originalIdx); });
                
        layout->insertWidget(layout->count() - 1, widget);
        entry_widgets.push_back(widget);
        filteredIdx++;
    }
}

void TrackMixerWidget::onSynthesizerAdded(NoteNagaSynthesizer* synth) {
    updateSynthesizerSelector();
    refresh_routing_table();
}

void TrackMixerWidget::onSynthesizerRemoved(NoteNagaSynthesizer* synth) {
    updateSynthesizerSelector();
    refresh_routing_table();
}

void TrackMixerWidget::onSynthesizerUpdated(NoteNagaSynthesizer* synth) {
    updateSynthesizerSelector();
    refresh_routing_table();
}

void TrackMixerWidget::onAddEntry() { 
    // Create entry with the currently selected synthesizer as output
    if (current_synth_name == TRACK_ROUTING_ENTRY_ANY_DEVICE) {
        // If "Any" is selected, use the default behavior
        engine->getMixer()->addRoutingEntry();
    } else {
        NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
        if (!seq) return;
        // Create an entry with the selected synthesizer
        NoteNagaRoutingEntry entry(seq->getActiveTrack(), current_synth_name, 0);
        engine->getMixer()->addRoutingEntry(entry);
    }
}

void TrackMixerWidget::onRemoveSelectedEntry() {
    if (selected_entry_index >= 0) {
        engine->getMixer()->removeRoutingEntry(selected_entry_index);
    } else {
        QMessageBox::warning(this, "No Entry Selected",
                             "Please select a routing entry to remove.", QMessageBox::Ok);
    }
}

void TrackMixerWidget::onReassignSynth() {
    // Zkontrolovat, zda je něco vybráno
    if (selected_entry_index < 0) {
        QMessageBox::warning(this, "No Entry Selected",
                             "Please select a routing entry to reassign.", QMessageBox::Ok);
        return;
    }

    // Získat seznam dostupných syntetizátorů
    auto synthesizers = engine->getSynthesizers();
    QStringList synth_names;
    for (NoteNagaSynthesizer* synth : synthesizers) {
        synth_names << QString::fromStdString(synth->getName());
    }

    if (synth_names.isEmpty()) {
        QMessageBox::information(this, "No Synthesizers",
                                 "There are no available synthesizers to assign to.", QMessageBox::Ok);
        return;
    }

    // Zobrazit dialog pro výběr
    bool ok;
    QString new_synth_name = QInputDialog::getItem(this, "Reassign Synthesizer",
                                                   "Select a new synthesizer for the entry:",
                                                   synth_names, 0, false, &ok);

    // Pokud uživatel potvrdil výběr, provést změnu
    if (ok && !new_synth_name.isEmpty()) {
        // Získat referenci na routovací položku
        NoteNagaRoutingEntry &entry = engine->getMixer()->getRoutingEntries()[selected_entry_index];

        // Změnit její výstup (output)
        entry.output = new_synth_name.toStdString();

        // Tato funkce znovu načte data z mixeru a překreslí seznam
        engine->getMixer()->routingEntryStackChanged();
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
        "This will clear all routing entries for all synths and set default routing. Do you want to continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        engine->getMixer()->clearRoutingTable();
        engine->getMixer()->createDefaultRouting();
    }
}

void TrackMixerWidget::onMaxVolumeAllTracks() {
    for (auto &entry : engine->getMixer()->getRoutingEntries()) {
        // Only apply to entries matching current synthesizer if one is selected
        if (current_synth_name == TRACK_ROUTING_ENTRY_ANY_DEVICE || 
            entry.output == current_synth_name) {
            entry.volume = 1.0f;
        }
    }
    refresh_routing_table();
}

void TrackMixerWidget::onMinVolumeAllTracks() {
    for (auto &entry : engine->getMixer()->getRoutingEntries()) {
        // Only apply to entries matching current synthesizer if one is selected
        if (current_synth_name == TRACK_ROUTING_ENTRY_ANY_DEVICE || 
            entry.output == current_synth_name) {
            entry.volume = 0.0f;
        }
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
    selected_entry_index = idx; // Uložíme si původní, absolutní index. To je v pořádku.

    // Bezpečnostní kontrola, jestli je index platný v hlavním seznamu
    auto& all_entries = engine->getMixer()->getRoutingEntries();
    if (idx < 0 || (size_t)idx >= all_entries.size()) {
        // Pokud je index neplatný, raději odznačíme vše
        for (size_t i = 0; i < entry_widgets.size(); ++i) {
            entry_widgets[i]->refreshStyle(false, i % 2 == 0);
        }
        return;
    }

    // Získáme ukazatel na položku v enginu, která má být skutečně vybrána
    NoteNagaRoutingEntry* target_entry = &all_entries[idx];

    // Nyní projdeme všechny VIDITELNÉ widgety
    for (size_t i = 0; i < entry_widgets.size(); ++i) {
        RoutingEntryWidget* widget = entry_widgets[i];
        
        // A porovnáme, jestli widget ukazuje na stejnou položku, jaká má být vybrána
        if (widget->getRoutingEntry() == target_entry) {
            // Našli jsme správný widget, označíme ho
            widget->refreshStyle(true, i % 2 == 0);
        } else {
            // Toto je jiný widget, odznačíme ho
            widget->refreshStyle(false, i % 2 == 0);
        }
    }
}