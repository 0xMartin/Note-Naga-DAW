#include "instrument_selector_dialog.h"

InstrumentSelectorDialog::InstrumentSelectorDialog(
    QWidget *parent, const std::vector<NN_GMInstrument_t> &gm_instruments,
    std::function<QIcon(QString)> icon_provider, std::optional<int> selected_gm_index)
    : QDialog(parent), gm_instruments(gm_instruments), icon_provider(icon_provider),
      selected_gm_index(selected_gm_index) {
    setWindowTitle("Instrument Selector");
    setMinimumWidth(580);
    setMinimumHeight(340);
    setWindowModality(Qt::WindowModal);

    groups = groupInstruments(gm_instruments);
    int idx = 0;
    for (const auto &key : groups.keys())
        icon_to_group_index[key] = idx++;

    if (selected_gm_index.has_value())
        selected_group = findGroupByGMIndex(selected_gm_index.value());
    else
        selected_group = groups.keys().isEmpty() ? "" : groups.keys().first();

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(12);

    // --- Left panel (groups) ---
    QVBoxLayout *left_panel = new QVBoxLayout();
    left_panel->setSpacing(2);

    QLabel *title = new QLabel("🎹 Choose Instrument");
    title->setFont(QFont("Segoe UI", 12, QFont::Bold));
    title->setStyleSheet("color: #7eb8f9; margin-bottom: 3px; padding-bottom: 3px; "
                         "border-bottom: 1.5px solid #263e54; letter-spacing: 1.5px;");
    left_panel->addWidget(title, 0, Qt::AlignHCenter);

    group_scroll = new QScrollArea();
    group_scroll->setWidgetResizable(true);
    QWidget *group_list_widget = new QWidget();
    group_grid = new QGridLayout(group_list_widget);
    group_grid->setAlignment(Qt::AlignTop);
    group_grid->setHorizontalSpacing(8);
    group_grid->setVerticalSpacing(8);
    group_scroll->setWidget(group_list_widget);
    left_panel->addWidget(group_scroll, 1);

    layout->addLayout(left_panel, 5);

    // --- Right panel (variants) ---
    QVBoxLayout *right_panel = new QVBoxLayout();
    right_panel->setSpacing(2);

    variant_title = new QLabel("🎶 Variant");
    variant_title->setFont(QFont("Segoe UI", 12, QFont::Bold));
    variant_title->setStyleSheet(
        "color: #7eb8f9; margin-bottom: 3px; padding-bottom: 3px; border-bottom: 1.5px "
        "solid #263e54; letter-spacing: 1.2px;");
    right_panel->addWidget(variant_title, 0, Qt::AlignHCenter);

    variant_scroll = new QScrollArea();
    variant_scroll->setWidgetResizable(true);
    QWidget *variant_list_widget = new QWidget();
    variant_vbox = new QVBoxLayout(variant_list_widget);
    variant_vbox->setAlignment(Qt::AlignTop);
    variant_scroll->setWidget(variant_list_widget);
    right_panel->addWidget(variant_scroll, 1);

    QHBoxLayout *btn_box = new QHBoxLayout();
    btn_box->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding));
    QPushButton *cancel_btn = new QPushButton("Cancel");
    cancel_btn->setFont(QFont("Segoe UI", 10, QFont::Bold));
    cancel_btn->setStyleSheet("background: #20242a; color: #7eb8f9; border: 1px solid "
                              "#263e54; border-radius: 6px; padding: 4px 20px;"
                              "QPushButton:hover { background: #232b38; color: #a6d2ff; "
                              "border: 1.5px solid #3477c0; }");
    connect(cancel_btn, &QPushButton::clicked, this, &InstrumentSelectorDialog::reject);
    btn_box->addWidget(cancel_btn);
    btn_box->setContentsMargins(0, 6, 0, 0);
    right_panel->addLayout(btn_box);

    layout->addLayout(right_panel, 3);

    populateGroups();
    selectGroup(selected_group, true);
}

int InstrumentSelectorDialog::getSelectedGMIndex() const {
    return this->selected_gm_index.value_or(-1);
}

QMap<QString, std::vector<NN_GMInstrument_t>> InstrumentSelectorDialog::groupInstruments(
    const std::vector<NN_GMInstrument_t> &gm_instruments) {
    QMap<QString, std::vector<NN_GMInstrument_t>> groups;
    for (const auto &instr : gm_instruments) {
        groups[QString::fromStdString(instr.icon)].push_back(instr);
    }
    return groups;
}

QString InstrumentSelectorDialog::findGroupByGMIndex(int gm_index) {
    for (const auto &icon : groups.keys()) {
        for (const auto &instr : groups[icon]) {
            if (instr.index == gm_index) return icon;
        }
    }
    return groups.keys().isEmpty() ? "" : groups.keys().first();
}

void InstrumentSelectorDialog::populateGroups() {
    // Remove old widgets
    QLayoutItem *item;
    while ((item = group_grid->takeAt(group_grid->count() - 1)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
    }

    int icons_per_row = 3;
    int row = 0, col = 0;

    for (auto idx = 0; idx < groups.size(); ++idx) {
        QString icon_name = groups.keys()[idx];
        const auto &instrument_list = groups.value(icon_name);

        QString group_label =
            QString::fromStdString(instrument_list[0].name).section(' ', 0, 0);
        QIcon icon = icon_provider(icon_name);

        QPushButton *btn = new QPushButton();
        btn->setIcon(icon);
        btn->setIconSize(QSize(44, 44));
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setStyleSheet(
            "QPushButton { border: none; background: transparent; padding: 4px; }"
            "QPushButton:checked { background: #263e54; border-radius: 8px; }");
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QVBoxLayout *v = new QVBoxLayout();
        v->setSpacing(2);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(btn, 0, Qt::AlignHCenter);

        QLabel *label = new QLabel(group_label);
        label->setStyleSheet("color: #a6b8c9; font-size: 10.5pt;");
        label->setAlignment(Qt::AlignHCenter);
        v->addWidget(label);

        QFrame *frame = new QFrame();
        frame->setLayout(v);
        group_grid->addWidget(frame, row, col);

        connect(btn, &QPushButton::clicked,
                [this, icon_name]() { selectGroup(icon_name, true); });

        frame->setToolTip(group_label.left(1).toUpper() + group_label.mid(1));
        frame->setProperty("_btn", QVariant::fromValue(btn));

        col++;
        if (col >= icons_per_row) {
            col = 0;
            row++;
        }
    }
}

void InstrumentSelectorDialog::selectGroup(const QString &icon_name,
                                           bool scroll_to_selected) {
    selected_group = icon_name;
    // Set checked state
    for (int i = 0; i < group_grid->count(); ++i) {
        QLayoutItem *item = group_grid->itemAt(i);
        QFrame *frame = qobject_cast<QFrame *>(item->widget());
        if (frame) {
            QPushButton *btn = frame->property("_btn").value<QPushButton *>();
            if (btn) btn->setChecked(groups[icon_name][0].icon == icon_name);
        }
    }
    populateVariants(icon_name);
    if (scroll_to_selected) scrollToSelectedGroup(icon_name);
}

void InstrumentSelectorDialog::scrollToSelectedGroup(const QString &icon_name) {
    for (int i = 0; i < group_grid->count(); ++i) {
        QLayoutItem *item = group_grid->itemAt(i);
        QFrame *frame = qobject_cast<QFrame *>(item->widget());
        if (frame) {
            QPushButton *btn = frame->property("_btn").value<QPushButton *>();
            if (btn && btn->isChecked()) {
                group_scroll->ensureWidgetVisible(frame);
                break;
            }
        }
    }
}

void InstrumentSelectorDialog::populateVariants(const QString &icon_name) {
    // Remove old widgets
    QLayoutItem *item;
    while ((item = variant_vbox->takeAt(variant_vbox->count() - 1)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
    }

    for (const auto &instr : groups[icon_name]) {
        QPushButton *btn = new QPushButton();
        btn->setIcon(icon_provider(QString::fromStdString(instr.icon)));
        btn->setIconSize(QSize(36, 36));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet("QPushButton { border: 1px solid #2b3644; border-radius: 8px; "
                           "text-align: left; padding: 6px 12px; background: #181f27; "
                           "color: #e0e9fa; font-size: 11pt; }"
                           "QPushButton:hover { background: #273a51; border: 1.7px solid "
                           "#3477c0; color: #7eb8f9; }");
        btn->setText(QString::fromStdString(instr.name));
        btn->setToolTip(QString::fromStdString(instr.name));
        connect(btn, &QPushButton::clicked,
                [this, idx = instr.index]() { selectVariant(idx); });

        if (selected_gm_index.has_value() && selected_gm_index.value() == instr.index) {
            btn->setStyleSheet(btn->styleSheet() +
                               "QPushButton { background: #253a4c; border: 1.7px solid "
                               "#5a9be6; color: #7eb8f9; }");
        }
        variant_vbox->addWidget(btn);
    }
}

void InstrumentSelectorDialog::selectVariant(int gm_index) {
    this->selected_gm_index = gm_index;
    emit instrumentSelected(gm_index);
    accept();
}