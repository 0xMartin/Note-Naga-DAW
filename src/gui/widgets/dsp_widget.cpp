#include "dsp_widget.h"

#include "../nn_gui_utils.h"
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

DSPWidget::DSPWidget(NoteNagaEngine *engine, QWidget *parent) : QWidget(parent) {
    this->engine = engine;
    this->title_widget = nullptr;
    initTitleUI();
    initUI();
}

void DSPWidget::initTitleUI() {
    // Vertikální panel s tlačítky vlevo
    if (this->title_widget) return;
    this->title_widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(title_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QPushButton *btn_add = create_small_button(":/icons/add.svg", "Add DSP module", "btn_add");
    QPushButton *btn_remove =
        create_small_button(":/icons/remove.svg", "Remove selected DSP", "btn_remove");
    QPushButton *btn_clear =
        create_small_button(":/icons/clear.svg", "Remove all DSP modules", "btn_clear");

    layout->addWidget(btn_add, 0, Qt::AlignBottom | Qt::AlignHCenter);
    layout->addWidget(btn_remove, 0, Qt::AlignBottom | Qt::AlignHCenter);
    layout->addWidget(btn_clear, 0, Qt::AlignBottom | Qt::AlignHCenter);

    connect(btn_add, &QPushButton::clicked, this, &DSPWidget::addDSPClicked);
    connect(btn_remove, &QPushButton::clicked, this, &DSPWidget::removeDSPClicked);
    connect(btn_clear, &QPushButton::clicked, this, &DSPWidget::removeAllDSPClicked);
}

void DSPWidget::initUI() {
    QHBoxLayout *main_layout = new QHBoxLayout(this);
    setLayout(main_layout);
    main_layout->setContentsMargins(5, 2, 5, 2);
    main_layout->setSpacing(0);

    // Horizontal scroll area for DSP modules
    /////////////////////////////////////////////////////////////////////////////////////////
    QWidget *dsp_container = new QWidget();
    dsp_layout = new QHBoxLayout(dsp_container);
    dsp_layout->setContentsMargins(0, 0, 0, 0);
    dsp_layout->setSpacing(8);
    dsp_layout->addStretch(1);

    QScrollArea *dsp_scroll_area = new QScrollArea();
    dsp_scroll_area->setWidgetResizable(true);
    dsp_scroll_area->setFrameShape(QFrame::NoFrame);
    dsp_scroll_area->setStyleSheet(
        "QScrollArea { background: transparent; padding: 0px; border: none; }");
    dsp_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    dsp_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dsp_scroll_area->setWidget(dsp_container);

    main_layout->addWidget(dsp_scroll_area, 1);

    // Right info panel with volume bar
    /////////////////////////////////////////////////////////////////////////////////////////
    QFrame *info_panel = new QFrame();
    info_panel->setObjectName("InfoPanel");
    info_panel->setStyleSheet(
        "QFrame#InfoPanel { background: #2F3139; border: 1px solid #494d56; "
        "border-radius: 8px; padding: 2px 0px 0px 0px; }");
    info_panel->setFixedWidth(120);

    QVBoxLayout *info_layout = new QVBoxLayout(info_panel);
    info_layout->setContentsMargins(4, 4, 4, 4);
    info_layout->setSpacing(8);

    // Output label nahoře, zarovnaný na střed
    QLabel *lbl_info = new QLabel("Output");
    lbl_info->setAlignment(Qt::AlignCenter);
    lbl_info->setStyleSheet("font-size: 13px; color: #ccc;");
    info_layout->addWidget(lbl_info);

    // Horizontální sekce: slider vlevo, volume bar vpravo
    QWidget *center_section = new QWidget(info_panel);
    center_section->setStyleSheet("background: transparent;");
    QHBoxLayout *center_layout = new QHBoxLayout(center_section);
    center_layout->setContentsMargins(0, 0, 0, 0);
    center_layout->setSpacing(6);

    // Audio slider 
    AudioVerticalSlider *volume_slider = new AudioVerticalSlider(center_section);
    volume_slider->setRange(0, 100.0f);
    volume_slider->setValue(100.0f);
    volume_slider->setLabelText("Vol");
    volume_slider->setFixedWidth(30); 
    volume_slider->setValuePostfix(" %");
    volume_slider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    connect(volume_slider, &AudioVerticalSlider::valueChanged, this, [this](int value) {
        if (engine) {
            engine->getDSPEngine()->setOutputVolume(value / 100.0f);
        }
    });
    center_layout->addWidget(volume_slider, 0, Qt::AlignLeft);

    // Stereo volume bar 
    StereoVolumeBarWidget *volume_bar = new StereoVolumeBarWidget(center_section);
    volume_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    center_layout->addWidget(volume_bar, 1, Qt::AlignVCenter);

    // Přidat center_section do hlavního vertikálního layoutu
    info_layout->addWidget(center_section, 1);

    main_layout->addWidget(info_panel, 0);
    setLayout(main_layout);

    // Timer pro aktualizaci hodnoty
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, volume_bar]() {
        if (engine) {
            auto dbs = engine->getDSPEngine()->getCurrentVolumeDb();
            volume_bar->setVolumesDb(dbs.first, dbs.second);
        }
    });
    timer->start(50); // obnovovat každých 50ms
}