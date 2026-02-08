#include "dsp_engine_widget.h"

#include <note_naga_engine/dsp/dsp_factory.h>
#include "../nn_gui_utils.h"
#include "../dialogs/dsp_block_chooser_dialog.h"
#include <QContextMenuEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpacerItem>
#include <QTimer>
#include <QVBoxLayout>

DSPEngineWidget::DSPEngineWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), engine(engine), title_widget(nullptr), dsp_layout(nullptr) {
    
    // Ensure widget fills available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Connect to runtime data for sequence/track changes (synth per track)
#ifndef QT_DEACTIVATED
    if (auto* runtimeData = engine->getRuntimeData()) {
        connect(runtimeData, &NoteNagaRuntimeData::activeSequenceChanged, 
                this, [this](NoteNagaMidiSeq*) { 
                    updateSynthesizerSelector(); 
                    refreshDSPWidgets();  // Refresh DSP blocks after project load
                });
        connect(runtimeData, &NoteNagaRuntimeData::activeSequenceTrackListChanged,
                this, [this](NoteNagaMidiSeq*) { 
                    updateSynthesizerSelector(); 
                    refreshDSPWidgets();  // Refresh DSP blocks when tracks change
                });
    }
#endif

    initTitleUI();
    initUI();
}

void DSPEngineWidget::initTitleUI() {
    if (this->title_widget) return;
    this->title_widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(title_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create vertical combobox for synth selection
    synth_selector = new VerticalComboBox();
    synth_selector->setToolTip("Select synthesizer for DSP effects");
    
    // Fill synth selector
    updateSynthesizerSelector();
    
    // Connect synth selector
    connect(synth_selector, &VerticalComboBox::currentIndexChanged,
            this, &DSPEngineWidget::onSynthesizerSelected);
    
    // Add to layout
    layout->addWidget(synth_selector);
    layout->addSpacing(10);

    btn_add = create_small_button(":/icons/add.svg", "Add DSP module", "btn_add");
    btn_clear = create_small_button(":/icons/clear.svg", "Remove all DSP modules", "btn_clear");
    btn_enable = create_small_button(":/icons/active.svg", "Enable / Disable DSP", "btn_enable");
    btn_enable->setCheckable(true);

    layout->addWidget(btn_add, 0, Qt::AlignBottom | Qt::AlignHCenter);
    layout->addWidget(btn_clear, 0, Qt::AlignBottom | Qt::AlignHCenter);
    layout->addWidget(btn_enable, 0, Qt::AlignBottom | Qt::AlignHCenter);

    connect(btn_add, &QPushButton::clicked, this, &DSPEngineWidget::addDSPClicked);
    connect(btn_clear, &QPushButton::clicked, this, &DSPEngineWidget::removeAllDSPClicked);
    connect(btn_enable, &QPushButton::clicked, this, &DSPEngineWidget::toggleDSPEnabled);
}

void DSPEngineWidget::updateSynthesizerSelector() {
    if (!engine) return;
    
    synth_selector->blockSignals(true);
    
    // Remember current selection by data pointer
    void* current_data = nullptr;
    if (synth_selector->currentIndex() >= 0) {
        QVariant data = synth_selector->itemData(synth_selector->currentIndex());
        if (data.isValid()) {
            current_data = data.value<void*>();
        }
    }
    
    // Clear and add "Master" as first option
    synth_selector->clear();
    synth_selector->addItem("Master", QVariant::fromValue(nullptr));
    
    // Add tracks from active sequence (format: "id : track name")
    auto* runtimeData = engine->getRuntimeData();
    if (!runtimeData) {
        synth_selector->setCurrentIndex(0);
        synth_selector->blockSignals(false);
        return;
    }
    
    if (auto* seq = runtimeData->getActiveSequence()) {
        for (auto* track : seq->getTracks()) {
            if (!track || track->isTempoTrack()) continue;
            
            INoteNagaSoftSynth* softSynth = track->getSoftSynth();
            if (!softSynth) continue;
            
            QString displayName = QString("%1 : %2")
                .arg(track->getId() + 1)
                .arg(QString::fromStdString(track->getName()));
            synth_selector->addItem(displayName, QVariant::fromValue(static_cast<void*>(softSynth)));
        }
    }
    
    // Restore selection if possible (by data pointer)
    int restore_idx = 0;
    if (current_data) {
        for (int i = 0; i < synth_selector->count(); ++i) {
            QVariant data = synth_selector->itemData(i);
            if (data.isValid() && data.value<void*>() == current_data) {
                restore_idx = i;
                break;
            }
        }
    }
    synth_selector->setCurrentIndex(restore_idx);
    
    synth_selector->blockSignals(false);
}

void DSPEngineWidget::onSynthesizerSelected(int index) {
    if (index < 0) return;
    
    // Get the selected synth
    if (index == 0) {
        current_synth = nullptr; // Master
    } else {
        QVariant data = synth_selector->itemData(index);
        if (data.isValid()) {
            current_synth = static_cast<INoteNagaSoftSynth*>(data.value<void*>());
        }
    }
    
    // Update UI
    refreshDSPWidgets();
}

void DSPEngineWidget::clearDSPWidgets() {
    // Remove all widgets
    for (auto widget : dsp_widgets) {
        dsp_layout->removeWidget(widget);
        widget->deleteLater();
    }
    dsp_widgets.clear();
}

void DSPEngineWidget::refreshDSPWidgets() {
    // Clear current widgets
    clearDSPWidgets();
    
    if (!engine) return;
    
    // Check if DSP engine is still valid (may be null during shutdown)
    NoteNagaDSPEngine* dspEngine = engine->getDSPEngine();
    if (!dspEngine) return;
    
    // Sync DSP enabled button state from engine
    bool dspEnabled = dspEngine->isDSPEnabled();
    btn_enable->setChecked(!dspEnabled);  // Checked means disabled
    btn_enable->setIcon(QIcon(dspEnabled ? ":/icons/active.svg" : ":/icons/inactive.svg"));
    
    // Get current DSP blocks (for master or synth)
    std::vector<NoteNagaDSPBlockBase*> blocks;
    if (current_synth == nullptr) {
        // Master blocks
        blocks = dspEngine->getDSPBlocks();
    } else {
        // Synth-specific blocks
        blocks = dspEngine->getSynthDSPBlocks(current_synth);
    }
    
    // Create widgets for each block
    for (auto block : blocks) {
        DSPBlockWidget *dsp_widget = new DSPBlockWidget(block);
        dsp_widgets.push_back(dsp_widget);
        dsp_layout->insertWidget(dsp_layout->count() - 1, dsp_widget);
        
        // Delete handler
        connect(dsp_widget, &DSPBlockWidget::deleteRequested, this, [this, dsp_widget, block]() {
            if (current_synth == nullptr) {
                engine->getDSPEngine()->removeDSPBlock(block);
            } else {
                engine->getDSPEngine()->removeSynthDSPBlock(current_synth, block);
            }
            dsp_layout->removeWidget(dsp_widget);
            dsp_widgets.erase(std::remove(dsp_widgets.begin(), dsp_widgets.end(), dsp_widget), dsp_widgets.end());
            dsp_widget->deleteLater();
        });

        // Move left/right handlers
        connect(dsp_widget, &DSPBlockWidget::moveLeftRequested, this, [this, dsp_widget, block]() {
            int idx = std::distance(dsp_widgets.begin(), std::find(dsp_widgets.begin(), dsp_widgets.end(), dsp_widget));
            if (idx > 0) {
                if (current_synth == nullptr) {
                    engine->getDSPEngine()->reorderDSPBlock(idx, idx-1);
                } else {
                    engine->getDSPEngine()->reorderSynthDSPBlock(current_synth, idx, idx-1);
                }
                dsp_widgets.erase(dsp_widgets.begin() + idx);
                dsp_widgets.insert(dsp_widgets.begin() + idx-1, dsp_widget);
                dsp_layout->removeWidget(dsp_widget);
                dsp_layout->insertWidget(idx-1, dsp_widget);
            }
        });
        
        connect(dsp_widget, &DSPBlockWidget::moveRightRequested, this, [this, dsp_widget, block]() {
            int idx = std::distance(dsp_widgets.begin(), std::find(dsp_widgets.begin(), dsp_widgets.end(), dsp_widget));
            if (idx < int(dsp_widgets.size())-1) {
                if (current_synth == nullptr) {
                    engine->getDSPEngine()->reorderDSPBlock(idx, idx+1);
                } else {
                    engine->getDSPEngine()->reorderSynthDSPBlock(current_synth, idx, idx+1);
                }
                dsp_widgets.erase(dsp_widgets.begin() + idx);
                dsp_widgets.insert(dsp_widgets.begin() + idx+1, dsp_widget);
                dsp_layout->removeWidget(dsp_widget);
                dsp_layout->insertWidget(idx+1, dsp_widget);
            }
        });
    }
}

void DSPEngineWidget::initUI() {
    QHBoxLayout *main_layout = new QHBoxLayout(this);
    setLayout(main_layout);
    main_layout->setContentsMargins(5, 2, 5, 2);
    main_layout->setSpacing(8);

    // Horizontal scroll area for DSP modules (stacked from right)
    QWidget *dsp_container = new QWidget();
    dsp_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    dsp_layout = new QHBoxLayout(dsp_container);
    dsp_layout->setContentsMargins(0, 0, 0, 0);
    dsp_layout->setSpacing(8);

    // DSP widgets will be added to the right, so insert from right
    dsp_layout->addStretch(1); // left side: always spacer/stretch

    QScrollArea *dsp_scroll_area = new QScrollArea();
    dsp_scroll_area->setWidgetResizable(true);
    dsp_scroll_area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    dsp_scroll_area->setFrameShape(QFrame::NoFrame);
    dsp_scroll_area->setStyleSheet(
        "QScrollArea { background: transparent; padding: 0px; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }");
    dsp_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    dsp_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dsp_scroll_area->setWidget(dsp_container);

    main_layout->addWidget(dsp_scroll_area, 2); // give more space to DSP modules

    // Right info panel with volume bar
    QFrame *info_panel = new QFrame();
    info_panel->setObjectName("InfoPanel");
    info_panel->setStyleSheet("QFrame#InfoPanel { background: #2F3139; border: 1px solid #494d56; "
                              "border-radius: 8px; padding: 2px 0px 0px 0px; }");
    info_panel->setFixedWidth(130);

    QVBoxLayout *info_layout = new QVBoxLayout(info_panel);
    info_layout->setContentsMargins(4, 4, 4, 4);
    info_layout->setSpacing(8);

    // Output label nahoře, zarovnaný na střed
    QLabel *lbl_info = new QLabel("Output");
    lbl_info->setAlignment(Qt::AlignCenter);
    lbl_info->setStyleSheet("font-size: 13px; color: #ddd; font-weight: bold;");
    info_layout->addWidget(lbl_info);

    QWidget *center_section = new QWidget(info_panel);
    center_section->setStyleSheet("background: transparent;");
    QHBoxLayout *center_layout = new QHBoxLayout(center_section);
    center_layout->setContentsMargins(0, 0, 0, 0);
    center_layout->setSpacing(6);
    center_layout->addStretch(1);

    AudioVerticalSlider *volume_slider = new AudioVerticalSlider(center_section);
    volume_slider->setRange(0, 100.0f);
    volume_slider->setValue(100.0f);
    volume_slider->setValueDecimals(0);
    volume_slider->setLabelText("Vol");
    volume_slider->setFixedWidth(35);
    volume_slider->setValuePostfix(" %");
    volume_slider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    connect(volume_slider, &AudioVerticalSlider::valueChanged, this, [this](int value) {
        if (engine) { engine->getDSPEngine()->setOutputVolume(value / 100.0f); }
    });
    center_layout->addWidget(volume_slider, 0, Qt::AlignLeft);

    StereoVolumeBarWidget *volume_bar = new StereoVolumeBarWidget(center_section);
    volume_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    center_layout->addWidget(volume_bar, 1);

    info_layout->addWidget(center_section, 1);

    main_layout->addWidget(info_panel, 0);

    // Timer pro aktualizaci hodnoty
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, volume_bar]() {
        if (engine) {
            if (engine->getDSPEngine() == nullptr) return;
            auto dbs = engine->getDSPEngine()->getCurrentVolumeDb();
            volume_bar->setVolumesDb(dbs.first, dbs.second);
        }
    });
    timer->start(50);
    
    // Initialize with current DSP blocks (Master)
    refreshDSPWidgets();
}

void DSPEngineWidget::addDSPClicked() {
    DSPBlockChooserDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    const DSPBlockFactoryEntry* selected = dlg.selectedFactory();
    if (!selected || !selected->create) return;

    NoteNagaDSPBlockBase *new_block = selected->create();
    if (!new_block) return;

    // Add to engine
    if (current_synth == nullptr) {
        engine->getDSPEngine()->addDSPBlock(new_block);
    } else {
        engine->getDSPEngine()->addSynthDSPBlock(current_synth, new_block);
    }
    refreshDSPWidgets();
}

void DSPEngineWidget::removeAllDSPClicked() {
    if (dsp_widgets.empty()) return;
    
    // Remove all blocks from engine
    for (auto *dsp_widget : dsp_widgets) {
        NoteNagaDSPBlockBase *block = dsp_widget->block();
        if (current_synth == nullptr) {
            engine->getDSPEngine()->removeDSPBlock(block);
        } else {
            engine->getDSPEngine()->removeSynthDSPBlock(current_synth, block);
        }
    }
    refreshDSPWidgets();
}

void DSPEngineWidget::toggleDSPEnabled() {
    bool enabled = !btn_enable->isChecked();
    btn_enable->setIcon(QIcon(enabled ? ":/icons/active.svg" : ":/icons/inactive.svg"));
    if (engine) {
        engine->getDSPEngine()->setEnableDSP(enabled);
    }
}

void DSPEngineWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    
    // Add DSP Effect submenu
    QMenu *addMenu = menu.addMenu(QIcon(":/icons/add.svg"), "Add DSP Effect");
    
    // Get all available DSP factories
    const auto &factories = DSPBlockFactory::allBlocks();
    for (const auto &factory : factories) {
        QAction *action = addMenu->addAction(QString::fromStdString(factory.name));
        connect(action, &QAction::triggered, this, [this, &factory]() {
            if (!factory.create) return;
            NoteNagaDSPBlockBase *new_block = factory.create();
            if (!new_block) return;
            
            // Add to engine
            if (current_synth == nullptr) {
                engine->getDSPEngine()->addDSPBlock(new_block);
            } else {
                engine->getDSPEngine()->addSynthDSPBlock(current_synth, new_block);
            }
            refreshDSPWidgets();
        });
    }
    
    menu.addSeparator();
    
    // Enable/Disable DSP
    NoteNagaDSPEngine* dspEng = engine ? engine->getDSPEngine() : nullptr;
    bool dspEnabled = dspEng ? dspEng->isDSPEnabled() : true;
    QAction *toggleAction = menu.addAction(
        QIcon(dspEnabled ? ":/icons/inactive.svg" : ":/icons/active.svg"),
        dspEnabled ? "Disable DSP Processing" : "Enable DSP Processing"
    );
    connect(toggleAction, &QAction::triggered, this, [this, dspEnabled]() {
        if (engine) {
            engine->getDSPEngine()->setEnableDSP(!dspEnabled);
            btn_enable->setChecked(dspEnabled);
            btn_enable->setIcon(QIcon(!dspEnabled ? ":/icons/active.svg" : ":/icons/inactive.svg"));
        }
    });
    
    menu.addSeparator();
    
    // Remove all DSP effects
    QAction *removeAllAction = menu.addAction(QIcon(":/icons/clear.svg"), "Remove All DSP Effects");
    removeAllAction->setEnabled(!dsp_widgets.empty());
    connect(removeAllAction, &QAction::triggered, this, [this]() {
        if (dsp_widgets.empty()) return;
        
        if (QMessageBox::question(this, "Remove All DSP Effects",
                                  "Are you sure you want to remove all DSP effects?",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) == QMessageBox::Yes) {
            removeAllDSPClicked();
        }
    });
    
    menu.addSeparator();
    
    // Reset output volume
    QAction *resetVolumeAction = menu.addAction(QIcon(":/icons/reload.svg"), "Reset Output Volume");
    connect(resetVolumeAction, &QAction::triggered, this, [this]() {
        if (engine) {
            engine->getDSPEngine()->setOutputVolume(1.0f);
        }
    });
    
    // Target selection submenu
    menu.addSeparator();
    QMenu *targetMenu = menu.addMenu(QIcon(":/icons/route.svg"), "DSP Target");
    
    // Master option
    QAction *masterAction = targetMenu->addAction("Master");
    masterAction->setCheckable(true);
    masterAction->setChecked(current_synth == nullptr);
    connect(masterAction, &QAction::triggered, this, [this]() {
        synth_selector->setCurrentIndex(0);
    });
    
    // Add available synthesizers from combo box (tracks)
    int synthCount = synth_selector->count();  // All items including Master
    for (int i = 1; i < synthCount; ++i) {
        QString synthName = synth_selector->itemText(i);
        QAction *synthAction = targetMenu->addAction(synthName);
        synthAction->setCheckable(true);
        
        // Check if this is the current selection
        void *synthPtr = synth_selector->itemData(i).value<void*>();
        synthAction->setChecked(current_synth == static_cast<INoteNagaSoftSynth*>(synthPtr));
        
        connect(synthAction, &QAction::triggered, this, [this, i]() {
            synth_selector->setCurrentIndex(i);
        });
    }
    
    menu.exec(event->globalPos());
}