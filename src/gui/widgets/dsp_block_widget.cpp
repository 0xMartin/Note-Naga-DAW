#include "dsp_block_widget.h"

#include "../nn_gui_utils.h"
#include <QDebug>
#include <QIcon>
#include <QSpacerItem>
#include <cmath>

DSPBlockWidget::DSPBlockWidget(NoteNagaDSPBlockBase *block, QWidget *parent)
    : QFrame(parent), block_(block), mainLayout_(nullptr), deactivateBtn_(nullptr),
      deleteBtn_(nullptr) {
    setObjectName("DSPBlockWidget");
    setStyleSheet(R"(
        QFrame#DSPBlockWidget {
            background-color: #32353b;
            border: 1px solid #19191f;
            border-radius: 6px;
        }
    )");
    setMinimumWidth(260);

    buildUi();
}

void DSPBlockWidget::buildUi() {
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout_->setSpacing(0);

    // --- Horní lišta ---
    topBar_ = new QFrame(this);
    topBar_->setObjectName("TopBar");
    topBar_->setStyleSheet("QFrame#TopBar {"
                           "  background: #2b2f37;"
                           "  border: 1px solid #19191f;"
                           "  margin-bottom: 0px; "
                           "  border-top-left-radius:6px;"
                           "  border-top-right-radius:6px;"
                           "}");

    auto *barLayout = new QHBoxLayout(topBar_);
    barLayout->setContentsMargins(12, 2, 12, 2);
    barLayout->setSpacing(8);

    auto *title = new QLabel(QString::fromStdString(block_->getBlockName()), topBar_);
    title->setStyleSheet("font-weight: bold; font-size: 15px; color: #fff;");
    barLayout->addWidget(title);

    barLayout->addStretch();

    deactivateBtn_ = create_small_button(":/icons/active.svg",
                                         block_->isActive() ? "Deactivate block" : "Activate block",
                                         "deactivateBtn", 24, topBar_);
    connect(deactivateBtn_, &QPushButton::clicked, this, &DSPBlockWidget::onDeactivateClicked);
    barLayout->addWidget(deactivateBtn_);

    deleteBtn_ = create_small_button(":/icons/close.svg", "Delete block", "deleteBtn", 24, topBar_);
    connect(deleteBtn_, &QPushButton::clicked, this, &DSPBlockWidget::onDeleteClicked);
    barLayout->addWidget(deleteBtn_);

    mainLayout_->addWidget(topBar_);

    // --- Parametry ---
    QWidget *centerWidget = new QWidget(this);
    auto *centerLayout = new QHBoxLayout(centerWidget);
    centerLayout->setContentsMargins(18, 12, 18, 18);

    auto *leftParamLayout = new QVBoxLayout();
    leftParamLayout->setSpacing(10);
    auto *rightParamLayout = new QVBoxLayout();
    rightParamLayout->setSpacing(10);

    auto params = block_->getParamDescriptors();
    paramWidgets_.resize(params.size());

    for (size_t i = 0; i < params.size(); ++i) {
        const auto &desc = params[i];
        QWidget *control = nullptr;

        // --- UI typ z descriptoru ---
        QString ui_type = desc.uiType.c_str();
        if (desc.type == DSPParamType::Float) {
            float value = block_->getParamValue(i);

            if (ui_type == "dial" || ui_type == DSP_UI_TYPE_DIAL) {
                auto *dial = new AudioDial(this);
                dial->setRange(desc.minValue, desc.maxValue);
                dial->setValue(value);
                dial->setDefaultValue(desc.defaultValue);
                dial->setLabel(QString::fromStdString(desc.name));
                dial->setGradient(QColor("#6cb0ff"), QColor("#ae6cff"));
                dial->showLabel(true);
                dial->showValue(true);
                dial->setValueDecimals(2);
                connect(dial, &AudioDial::valueChanged, this,
                        [this, i](float val) { block_->setParamValue(i, val); });
                control = dial;
                leftParamLayout->addWidget(control);
            } else if (ui_type == DSP_UI_TYPE_DIAL_CENTERED) {
                auto *dial = new AudioDialCentered(this);
                dial->setRange(desc.minValue, desc.maxValue);
                dial->setValue(value);
                dial->setDefaultValue(desc.defaultValue);
                dial->setLabel(QString::fromStdString(desc.name));
                dial->setGradient(QColor("#6cb0ff"), QColor("#ae6cff"));
                dial->showLabel(true);
                dial->showValue(true);
                dial->setValueDecimals(2);
                connect(dial, &AudioDialCentered::valueChanged, this,
                        [this, i](float val) { block_->setParamValue(i, val); });
                control = dial;
                leftParamLayout->addWidget(control);
            } else if (ui_type == DSP_UI_TYPE_SLIDER_VERTICAL) {
                auto *slider = new AudioVerticalSlider(this);
                slider->setRange(int(desc.minValue * 100), int(desc.maxValue * 100));
                slider->setValue(int(value * 100));
                slider->setLabelText(QString::fromStdString(desc.name));
                slider->setLabelVisible(true);
                slider->setValueVisible(true);
                connect(slider, &AudioVerticalSlider::valueChanged, this,
                        [this, i](int val) { block_->setParamValue(i, float(val) / 100.0f); });
                control = slider;
                rightParamLayout->addWidget(control);
            }
        } else if (desc.type == DSPParamType::Int) {
            if (ui_type == DSP_UI_TYPE_SLIDER_VERTICAL) {
                auto *slider = new AudioVerticalSlider(this);
                slider->setRange(int(desc.minValue), int(desc.maxValue));
                slider->setValue(int(block_->getParamValue(i)));
                slider->setLabelText(QString::fromStdString(desc.name));
                slider->setLabelVisible(true);
                slider->setValueVisible(true);
                connect(slider, &AudioVerticalSlider::valueChanged, this,
                        [this, i](int val) { block_->setParamValue(i, float(val)); });
                control = slider;
                rightParamLayout->addWidget(control);
            }
        } else if (desc.type == DSPParamType::Bool) {
            auto *btn = new QPushButton(this);
            btn->setCheckable(true);
            btn->setChecked(block_->getParamValue(i) > 0.5f);
            btn->setText(btn->isChecked() ? "On" : "Off");
            btn->setStyleSheet("QPushButton { background-color: #3e4a5a; color: #fff; }"
                               "QPushButton:checked { background-color: #28ff42; color: #222; }");
            btn->setMinimumHeight(32);
            connect(btn, &QPushButton::clicked, this, [this, btn, i]() {
                bool checked = btn->isChecked();
                block_->setParamValue(i, checked ? 1.0f : 0.0f);
                btn->setText(checked ? "On" : "Off");
            });
            control = btn;
            leftParamLayout->addWidget(control);
        }

        if (control) { paramWidgets_[i] = {control, ui_type}; }
    }

    centerLayout->addLayout(leftParamLayout);
    centerLayout->addStretch();
    centerLayout->addLayout(rightParamLayout);

    mainLayout_->addWidget(centerWidget, 1);

    mainLayout_->addStretch();
    updateActivationButton();
}

void DSPBlockWidget::updateActivationButton() {
    if (!deactivateBtn_) return;
    deactivateBtn_->setIcon(
        QIcon(block_->isActive() ? ":/icons/active.svg" : ":/icons/inactive.svg"));
    deactivateBtn_->setToolTip(block_->isActive() ? "Deactivate block" : "Activate block");
}

void DSPBlockWidget::onDeactivateClicked() {
    block_->setActive(!block_->isActive());
    updateActivationButton();
}

void DSPBlockWidget::onDeleteClicked() { emit deleteRequested(this); }