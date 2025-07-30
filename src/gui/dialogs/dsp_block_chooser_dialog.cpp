#include "dsp_block_chooser_dialog.h"
#include <QIcon>
#include <QHBoxLayout>

struct DSPBlockMeta {
    QString category;
    QString desc;
    QString icon;
};

static QMap<QString, DSPBlockMeta> dspBlockMeta = {
    { "Gain",         { "Utility",    "Controls the signal volume.", "icons/audio-signal.svg" } },
    { "Pan",          { "Utility",    "Stereo panning of the signal.", "icons/device.svg" } },
    { "Single EQ",    { "Filter",     "Single-band equalizer.", "icons/mixer.svg" } },
    { "Multi Band EQ",{ "Filter",     "Multi-band equalizer.", "icons/mixer.svg" } },
    { "Filter",       { "Filter",     "Lowpass/Highpass/Bandpass filter.", "icons/mixer.svg" } },
    { "Compressor",   { "Dynamics",   "Reduces dynamic range.", "icons/sound-on.svg" } },
    { "Limiter",      { "Dynamics",   "Limits peaks above a threshold.", "icons/sound-on.svg" } },
    { "Noise Gate",   { "Dynamics",   "Silences audio below threshold.", "icons/sound-off.svg" } },
    { "Bitcrusher",   { "Distortion", "Digital lo-fi distortion.", "icons/device.svg" } },
    { "Saturator",    { "Distortion", "Analog-style saturation/soft clipping.", "icons/device.svg" } },
    { "Exciter",      { "Distortion", "Adds brightness and harmonics.", "icons/device.svg" } },
    { "Delay",        { "Effect",     "Classic delay/echo effect.", "icons/loop.svg" } },
    { "Reverb",       { "Effect",     "Room/space simulation (reverb).", "icons/loop.svg" } },
    { "Chorus",       { "Effect",     "Thickens sound with modulated delay.", "icons/solo.svg" } },
    { "Flanger",      { "Effect",     "Jet/space effect with short modulated delay.", "icons/solo.svg" } },
    { "Phaser",       { "Effect",     "Sweeping filter/phasing effect.", "icons/solo.svg" } },
    { "Tremolo",      { "Effect",     "Amplitude modulation (tremolo).", "icons/tempo.svg" } },
    { "Stereo Imager",{ "Stereo",     "Controls stereo width using mid/side processing.", "icons/left.svg" } },
};

void DSPBlockChooserDialog::fillTree() {
    QMap<QString, QTreeWidgetItem*> categoryItems;

    for (const auto& entry : DSPBlockFactory::allBlocks()) {
        DSPBlockMeta meta = dspBlockMeta.value(QString::fromStdString(entry.name), {"Other", "", ""});
        QTreeWidgetItem* categoryItem = categoryItems.value(meta.category, nullptr);
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(tree_);
            categoryItem->setText(0, meta.category);
            categoryItems[meta.category] = categoryItem;
        }
        QTreeWidgetItem* item = new QTreeWidgetItem(categoryItem);
        item->setText(0, QString::fromStdString(entry.name));
        if (!meta.icon.isEmpty()) {
            item->setIcon(0, QIcon(":/"+meta.icon));
        }
        item->setData(0, Qt::UserRole, QVariant(QString::fromStdString(entry.name)));
    }
    tree_->expandAll();
}

DSPBlockChooserDialog::DSPBlockChooserDialog(QWidget *parent)
    : QDialog(parent), selected_factory_(nullptr)
{
    setWindowTitle("Add DSP Block");
    setMinimumSize(360, 420);

    QVBoxLayout *layout = new QVBoxLayout(this);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    fillTree();
    layout->addWidget(tree_);

    desc_label_ = new QLabel(this);
    desc_label_->setWordWrap(true);
    desc_label_->setStyleSheet("color: #ccc; font-size: 11px; margin: 3px 2px;");
    desc_label_->setMinimumHeight(32);
    layout->addWidget(desc_label_);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnOk = new QPushButton("Add", this);
    QPushButton *btnCancel = new QPushButton("Cancel", this);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    layout->addLayout(btnLayout);

    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
        QString name = item ? item->data(0, Qt::UserRole).toString() : "";
        DSPBlockMeta meta = dspBlockMeta.value(name, {"", "Select a block.", ""});
        desc_label_->setText(meta.desc);
    });

    connect(btnOk, &QPushButton::clicked, this, [this]() {
        QTreeWidgetItem* item = tree_->currentItem();
        if (item && item->parent()) {
            QString name = item->data(0, Qt::UserRole).toString();
            for (const auto& entry : DSPBlockFactory::allBlocks()) {
                if (QString::fromStdString(entry.name) == name) {
                    selected_factory_ = &entry;
                    accept();
                    return;
                }
            }
        }
    });
    connect(btnCancel, &QPushButton::clicked, this, [this]() { reject(); });

    // Select first block by default
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        if (tree_->topLevelItem(i)->childCount() > 0) {
            tree_->setCurrentItem(tree_->topLevelItem(i)->child(0));
            break;
        }
    }
}