#include "dsp_block_widget.h"
#include "../nn_gui_utils.h"
#include <QButtonGroup>
#include <QIcon>
#include <QResizeEvent>
#include <QSpacerItem>
#include <cmath>

static constexpr int VSLIDER_WIDTH = 36;
static constexpr int MAIN_PADDING = 4;
static constexpr int TITLE_BAR_WIDTH = 36;

DSPBlockWidget::DSPBlockWidget(NoteNagaDSPBlockBase *block, QWidget *parent)
    : QFrame(parent), block_(block)
{
    setObjectName("DSPBlockWidget");
    setStyleSheet(R"(
        QFrame#DSPBlockWidget {
            background-color: #32353b;
            border: 1px solid #19191f;
            border-radius: 6px;
        }
        QWidget#LeftBar {
            background: #2b2f37;
            border-top-left-radius:6px;
            border-bottom-left-radius:6px;
        }
    )");
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    buildUi();
}

void DSPBlockWidget::buildUi() {
    mainLayout_ = new QHBoxLayout(this);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout_->setSpacing(0);

    // Left side: vertical label + buttons at the bottom
    leftBar_ = new QWidget(this);
    leftBar_->setObjectName("LeftBar");
    leftBar_->setFixedWidth(TITLE_BAR_WIDTH);
    leftBarLayout_ = new QVBoxLayout(leftBar_);
    leftBarLayout_->setContentsMargins(0, 8, 0, 8);
    leftBarLayout_->setSpacing(6);

    titleLabel_ = new VerticalTitleLabel(QString::fromStdString(block_->getBlockName()), leftBar_);
    QFont f = font();
    f.setBold(true);
    f.setPointSize(12);
    titleLabel_->setFont(f);
    leftBarLayout_->addWidget(titleLabel_, 0);

    leftBarLayout_->addStretch(1);

    auto addCenteredButton = [&](QPushButton* btn) {
        QWidget* wrapper = new QWidget(leftBar_);
        wrapper->setStyleSheet("QWidget { background: transparent; }");
        QHBoxLayout* hbox = new QHBoxLayout(wrapper);
        hbox->setContentsMargins(0,0,0,0);
        hbox->addStretch(1);
        hbox->addWidget(btn);
        hbox->addStretch(1);
        leftBarLayout_->addWidget(wrapper, 0);
    };

    leftBtn_ = create_small_button(":/icons/left.svg", "Move block left", "leftBtn", 20, leftBar_);
    connect(leftBtn_, &QPushButton::clicked, this, &DSPBlockWidget::onLeftClicked);
    addCenteredButton(leftBtn_);

    rightBtn_ = create_small_button(":/icons/right.svg", "Move block right", "rightBtn", 20, leftBar_);
    connect(rightBtn_, &QPushButton::clicked, this, &DSPBlockWidget::onRightClicked);
    addCenteredButton(rightBtn_);

    deactivateBtn_ = create_small_button(":/icons/active.svg",
                                         block_->isActive() ? "Deactivate block" : "Activate block",
                                         "deactivateBtn", 20, leftBar_);
    deactivateBtn_->setCheckable(true);
    connect(deactivateBtn_, &QPushButton::clicked, this, &DSPBlockWidget::onDeactivateClicked);
    addCenteredButton(deactivateBtn_);

    deleteBtn_ = create_small_button(":/icons/close.svg", "Delete block", "deleteBtn", 20, leftBar_);
    connect(deleteBtn_, &QPushButton::clicked, this, &DSPBlockWidget::onDeleteClicked);
    addCenteredButton(deleteBtn_);

    mainLayout_->addWidget(leftBar_, 0);

    // Content (right side)
    contentWidget_ = new QWidget(this);
    contentLayout_ = new QVBoxLayout(contentWidget_);
    contentLayout_->setContentsMargins(MAIN_PADDING, MAIN_PADDING, MAIN_PADDING, MAIN_PADDING);
    contentLayout_->setSpacing(8);

    // Button bar at top
    buttonBar_ = new QWidget(contentWidget_);
    buttonBarLayout_ = new QHBoxLayout(buttonBar_);
    buttonBarLayout_->setContentsMargins(4, 0, 4, 0);
    buttonBarLayout_->setSpacing(2);

    auto params = block_->getParamDescriptors();
    buttonWidgets_.clear();
    dialWidgets_.clear();
    vSliderWidgets_.clear();

    int buttonCount = 0;
    for (size_t i = 0; i < params.size(); ++i) {
        const auto &desc = params[i];
        QWidget *control = nullptr;
        switch (desc.control_type) {
        case DSControlType::PushButton: {
            auto *btn = create_small_button(":/icons/custom_btn.svg", QString::fromStdString(desc.name),
                                            desc.name.c_str(), 24, buttonBar_);
            connect(btn, &QPushButton::clicked, this, [this, i]() {
                block_->setParamValue(i, 1.0f);
            });
            buttonBarLayout_->addWidget(btn);
            control = btn;
            ++buttonCount;
            break;
        }
        case DSControlType::ToogleButton: {
            auto *btn = create_small_button(":/icons/toggle_btn.svg", QString::fromStdString(desc.name),
                                            desc.name.c_str(), 24, buttonBar_);
            btn->setCheckable(true);
            btn->setChecked(block_->getParamValue(i) > 0.5f);
            connect(btn, &QPushButton::clicked, this, [this, btn, i]() {
                bool checked = btn->isChecked();
                block_->setParamValue(i, checked ? 1.0f : 0.0f);
            });
            buttonBarLayout_->addWidget(btn);
            control = btn;
            ++buttonCount;
            break;
        }
        default:
            break;
        }
        if (control) buttonWidgets_.push_back(control);
    }
    buttonBarLayout_->addStretch(1);
    buttonBar_->setVisible(buttonCount > 0);
    if (buttonCount > 0) contentLayout_->addWidget(buttonBar_);

    // Center area (dial grid + vslider bar side by side)
    centerWidget_ = new QWidget(contentWidget_);
    centerLayout_ = new QHBoxLayout(centerWidget_);
    centerLayout_->setContentsMargins(0, 0, 0, 0);
    centerLayout_->setSpacing(8);

    // Dial grid (AudioDialGridWidget)
    dialGridWidget_ = new AudioDialGridWidget(centerWidget_);
    dialGridWidget_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    for (size_t i = 0; i < params.size(); ++i) {
        const auto &desc = params[i];
        QWidget *control = nullptr;
        if (desc.control_type == DSControlType::Dial ||
            desc.control_type == DSControlType::DialCentered) {
            float value = block_->getParamValue(i);
            if (desc.control_type == DSControlType::Dial) {
                auto *dial = new AudioDial(dialGridWidget_);
                dial->setFixedSize(40, 60);
                dial->setRange(desc.min_value, desc.max_value);
                dial->setValue(value);
                dial->setDefaultValue(desc.default_value);
                dial->setLabel(QString::fromStdString(desc.name));
                dial->setGradient(QColor("#6cb0ff"), QColor("#ae6cff"));
                dial->showLabel(true);
                dial->showValue(true);
                dial->setValueDecimals(2);
                connect(dial, &AudioDial::valueChanged, this,
                        [this, i](float val) { block_->setParamValue(i, val); });
                control = dial;
            } else {
                auto *dial = new AudioDialCentered(dialGridWidget_);
                dial->setFixedSize(40, 60);
                dial->setRange(desc.min_value, desc.max_value);
                dial->setValue(value);
                dial->setDefaultValue(desc.default_value);
                dial->setLabel(QString::fromStdString(desc.name));
                dial->setGradient(QColor("#6cb0ff"), QColor("#ae6cff"));
                dial->showLabel(true);
                dial->showValue(true);
                dial->setValueDecimals(2);
                connect(dial, &AudioDialCentered::valueChanged, this,
                        [this, i](float val) { block_->setParamValue(i, val); });
                control = dial;
            }
            dialWidgets_.push_back(control);
        }
    }
    dialGridWidget_->setDials(dialWidgets_);
    bool showDialGrid = !dialWidgets_.empty();

    // VSlider bar
    vSliderWidget_ = new QWidget(centerWidget_);
    vSliderWidget_->setObjectName("VSliderWidget");
    vSliderWidget_->setFixedWidth(VSLIDER_WIDTH);
    vSliderWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    vSliderLayout_ = new QHBoxLayout(vSliderWidget_);
    vSliderLayout_->setContentsMargins(0, 0, 0, 0);
    vSliderLayout_->setSpacing(2);

    for (size_t i = 0; i < params.size(); ++i) {
        const auto &desc = params[i];
        if (desc.control_type == DSControlType::SliderVertical) {
            float value = block_->getParamValue(i);
            auto *slider = new AudioVerticalSlider(vSliderWidget_);
            slider->setRange(desc.min_value, desc.max_value);
            slider->setDefaultValue(desc.default_value);
            slider->setValue(value);
            slider->setLabelText(QString::fromStdString(desc.name));
            slider->setLabelVisible(true);
            slider->setValueVisible(true);
            slider->setValueDecimals(2);
            slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            slider->setFixedWidth(VSLIDER_WIDTH);
            connect(slider, &AudioVerticalSlider::valueChanged, this,
                    [this, i](float val) { block_->setParamValue(i, val); });
            vSliderLayout_->addWidget(slider);
            vSliderWidgets_.push_back(slider);
        }
    }
    bool showVSliderStack = !vSliderWidgets_.empty();

    dialGridWidget_->setVisible(showDialGrid);
    vSliderWidget_->setVisible(showVSliderStack);

    centerLayout_->addWidget(dialGridWidget_, 1);
    centerLayout_->addWidget(vSliderWidget_, 0);

    centerWidget_->setVisible(showDialGrid || showVSliderStack);
    if (showDialGrid || showVSliderStack) contentLayout_->addWidget(centerWidget_, 1);

    contentWidget_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);

    mainLayout_->addWidget(contentWidget_, 1);

    updateActivationButton();
}

void DSPBlockWidget::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);
}

void DSPBlockWidget::onLeftClicked() { emit moveLeftRequested(this); }
void DSPBlockWidget::onRightClicked() { emit moveRightRequested(this); }

void DSPBlockWidget::updateActivationButton() {
    if (!deactivateBtn_) return;
    deactivateBtn_->setIcon(
        QIcon(!deactivateBtn_->isChecked() ? ":/icons/active.svg" : ":/icons/inactive.svg"));
    deactivateBtn_->setToolTip(block_->isActive() ? "Deactivate block" : "Activate block");
}

void DSPBlockWidget::onDeactivateClicked() {
    block_->setActive(!deactivateBtn_->isChecked());
    updateActivationButton();
}

void DSPBlockWidget::onDeleteClicked() { emit deleteRequested(this); }

QSize DSPBlockWidget::minimumSizeHint() const {
    int minWidth = TITLE_BAR_WIDTH + 30;

    // DialGridWidget si řídí minWidth dynamicky, tady pro jistotu fallback
    int sliderCount = vSliderWidgets_.size();
    if (sliderCount > 0) {
        int sliderWidth = VSLIDER_WIDTH + 4;
        minWidth += sliderWidth;
    }
    if (dialWidgets_.empty() && sliderCount == 0) minWidth = TITLE_BAR_WIDTH + 120;
    if (buttonBar_ && buttonBar_->isVisible()) {
        minWidth = std::max(minWidth, TITLE_BAR_WIDTH + buttonBar_->sizeHint().width() + 20);
    }
    return QSize(minWidth, QFrame::minimumSizeHint().height());
}