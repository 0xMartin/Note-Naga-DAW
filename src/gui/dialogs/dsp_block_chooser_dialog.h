#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <note_naga_engine/dsp/dsp_factory.h>

/**
 * @brief Dialog for choosing a DSP block type (categorized, with description and icons).
 */
class DSPBlockChooserDialog : public QDialog {
    Q_OBJECT
public:
    explicit DSPBlockChooserDialog(QWidget* parent = nullptr);

    const DSPBlockFactoryEntry* selectedFactory() const {
        return selected_factory_;
    }

private:
    QTreeWidget* tree_;
    QLabel* desc_label_;
    const DSPBlockFactoryEntry* selected_factory_;

    void fillTree();
};