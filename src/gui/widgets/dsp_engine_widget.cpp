#include "dsp_engine_widget.h"

#include <note_naga_engine/dsp/dsp_factory.h>
#include "../nn_gui_utils.h"
#include "../dialogs/dsp_block_chooser_dialog.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpacerItem>
#include <QTimer>
#include <QVBoxLayout>

DSPEngineWidget::DSPEngineWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), engine(engine), title_widget(nullptr), dsp_layout(nullptr) {
    initTitleUI();
    initUI();
}

void DSPEngineWidget::initTitleUI() {
    if (this->title_widget) return;
    this->title_widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(title_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

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

void DSPEngineWidget::initUI() {
    QHBoxLayout *main_layout = new QHBoxLayout(this);
    setLayout(main_layout);
    main_layout->setContentsMargins(5, 2, 5, 2);
    main_layout->setSpacing(8);

    // Horizontal scroll area for DSP modules (stacked from right)
    QWidget *dsp_container = new QWidget();
    dsp_layout = new QHBoxLayout(dsp_container);
    dsp_layout->setContentsMargins(0, 0, 0, 2);
    dsp_layout->setSpacing(8);

    // DSP widgets will be added to the right, so insert from right
    dsp_layout->addStretch(1); // left side: always spacer/stretch

    QScrollArea *dsp_scroll_area = new QScrollArea();
    dsp_scroll_area->setWidgetResizable(true);
    dsp_scroll_area->setFrameShape(QFrame::NoFrame);
    dsp_scroll_area->setStyleSheet(
        "QScrollArea { background: transparent; padding: 0px; border: none; }");
    dsp_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    dsp_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dsp_scroll_area->setWidget(dsp_container);

    main_layout->addWidget(dsp_scroll_area, 2); // give more space to DSP modules

    // Right info panel with volume bar
    QFrame *info_panel = new QFrame();
    info_panel->setObjectName("InfoPanel");
    info_panel->setStyleSheet("QFrame#InfoPanel { background: #2F3139; border: 1px solid #494d56; "
                              "border-radius: 8px; padding: 2px 0px 0px 0px; }");
    info_panel->setFixedWidth(320);

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

    spectrum_analyzer = new SpectrumAnalyzer(this->engine->getSpectrumAnalyzer(), center_section);
    spectrum_analyzer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    center_layout->addWidget(spectrum_analyzer, 1);

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
            auto dbs = engine->getDSPEngine()->getCurrentVolumeDb();
            volume_bar->setVolumesDb(dbs.first, dbs.second);
        }
    });
    timer->start(50);
}

void DSPEngineWidget::addDSPClicked() {
    DSPBlockChooserDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    const DSPBlockFactoryEntry* selected = dlg.selectedFactory();
    if (!selected || !selected->create) return;

    NoteNagaDSPBlockBase *new_block = selected->create();
    if (!new_block) return;

    engine->getDSPEngine()->addDSPBlock(new_block);

    DSPBlockWidget *dsp_widget = new DSPBlockWidget(new_block);
    dsp_widgets.push_back(dsp_widget);
    dsp_layout->insertWidget(dsp_layout->count() - 1, dsp_widget);

    connect(dsp_widget, &DSPBlockWidget::deleteRequested, this, [this, dsp_widget, new_block]() {
        engine->getDSPEngine()->removeDSPBlock(new_block);
        dsp_layout->removeWidget(dsp_widget);
        dsp_widgets.erase(std::remove(dsp_widgets.begin(), dsp_widgets.end(), dsp_widget), dsp_widgets.end());
        dsp_widget->deleteLater();
        delete new_block;
    });

    // Move left/right signály (beze změny)
    connect(dsp_widget, &DSPBlockWidget::moveLeftRequested, this, [this, dsp_widget, new_block]() {
        int idx = std::distance(dsp_widgets.begin(), std::find(dsp_widgets.begin(), dsp_widgets.end(), dsp_widget));
        if (idx > 0) {
            engine->getDSPEngine()->reorderDSPBlock(idx, idx-1);
            dsp_widgets.erase(dsp_widgets.begin() + idx);
            dsp_widgets.insert(dsp_widgets.begin() + idx-1, dsp_widget);
            dsp_layout->removeWidget(dsp_widget);
            dsp_layout->insertWidget(idx-1, dsp_widget);
        }
    });
    connect(dsp_widget, &DSPBlockWidget::moveRightRequested, this, [this, dsp_widget, new_block]() {
        int idx = std::distance(dsp_widgets.begin(), std::find(dsp_widgets.begin(), dsp_widgets.end(), dsp_widget));
        if (idx < int(dsp_widgets.size())-1) {
            engine->getDSPEngine()->reorderDSPBlock(idx, idx+1);
            dsp_widgets.erase(dsp_widgets.begin() + idx);
            dsp_widgets.insert(dsp_widgets.begin() + idx+1, dsp_widget);
            dsp_layout->removeWidget(dsp_widget);
            dsp_layout->insertWidget(idx+1, dsp_widget);
        }
    });
}

void DSPEngineWidget::removeAllDSPClicked() {
    for (auto *dsp_widget : dsp_widgets) {
        engine->getDSPEngine()->removeDSPBlock(dsp_widget->block());
        dsp_layout->removeWidget(dsp_widget);
        dsp_widget->deleteLater();
        delete dsp_widget->block();
    }
    dsp_widgets.clear();
}

void DSPEngineWidget::toggleDSPEnabled() {
    bool enabled = !btn_enable->isChecked();
    btn_enable->setIcon(QIcon(enabled ? ":/icons/active.svg" : ":/icons/inactive.svg"));
    if (engine) {
        engine->getDSPEngine()->setEnableDSP(enabled);
    }
}
