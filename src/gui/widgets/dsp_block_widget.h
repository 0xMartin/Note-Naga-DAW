#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <vector>
#include <memory>

#include <note_naga_engine/core/dsp_block_base.h>
#include "../components/audio_dial.h"
#include "../components/audio_dial_centered.h"
#include "../components/audio_vertical_slider.h"

/**
 * @brief Widget for controlling a DSP block (auto-generates UI based on block parameters).
 */
class DSPBlockWidget : public QFrame {
    Q_OBJECT
public:
    explicit DSPBlockWidget(NoteNagaDSPBlockBase* block, QWidget* parent = nullptr);

    NoteNagaDSPBlockBase* block() const { return block_; }

signals:
    void deleteRequested(DSPBlockWidget* widget);

private slots:
    void onDeactivateClicked();
    void onDeleteClicked();

private:
    void buildUi();
    void updateActivationButton();

    NoteNagaDSPBlockBase* block_;

    QVBoxLayout* mainLayout_;
    QFrame* topBar_;
    QPushButton* deactivateBtn_;
    QPushButton* deleteBtn_;

    struct ParamWidget {
        QWidget* control = nullptr;
        QString uiType;
    };
    std::vector<ParamWidget> paramWidgets_;
};