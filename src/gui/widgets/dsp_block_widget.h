#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <vector>
#include <memory>

#include <note_naga_engine/core/dsp_block_base.h>
#include "../components/audio_dial.h"
#include "../components/audio_dial_centered.h"
#include "../components/audio_vertical_slider.h"
#include "../components/vertical_label.h"
#include "../components/audio_dial_grid_widget.h"

class DSPBlockWidget : public QFrame {
    Q_OBJECT
public:
    explicit DSPBlockWidget(NoteNagaDSPBlockBase* block, QWidget* parent = nullptr);

    NoteNagaDSPBlockBase* block() const { return block_; }
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void moveLeftRequested(DSPBlockWidget* widget);
    void moveRightRequested(DSPBlockWidget* widget);
    void deleteRequested(DSPBlockWidget* widget);

private slots:
    void onLeftClicked();
    void onRightClicked();
    void onDeactivateClicked();
    void onDeleteClicked();

private:
    // DSP backend
    NoteNagaDSPBlockBase* block_;

    // Main layout
    QHBoxLayout* mainLayout_;

    // Left side: title label and buttons
    QWidget* leftBar_;
    QVBoxLayout* leftBarLayout_;
    VerticalTitleLabel* titleLabel_;
    QPushButton* leftBtn_;
    QPushButton* rightBtn_;
    QPushButton* deactivateBtn_;
    QPushButton* deleteBtn_;

    // Content area
    QWidget* contentWidget_;
    QVBoxLayout* contentLayout_;

    // Button bar
    QWidget* buttonBar_;
    QHBoxLayout* buttonBarLayout_;
    std::vector<QWidget*> buttonWidgets_;

    QWidget* centerWidget_;
    QHBoxLayout* centerLayout_;

    // Dial grid
    AudioDialGridWidget* dialGridWidget_;
    std::vector<QWidget*> dialWidgets_;

    // Vertical slider stack
    QWidget* vSliderWidget_;
    QHBoxLayout* vSliderLayout_;
    std::vector<AudioVerticalSlider*> vSliderWidgets_;

    void buildUI();
    void updateActivationButton();

    // --- UI build helpers ---
    void buildLeftBar();
    void buildButtonBar();
    void buildDialGrid();
    void buildVSliderStack();
    void buildCenterArea();
};