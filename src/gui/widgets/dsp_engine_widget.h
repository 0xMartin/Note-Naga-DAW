#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QSpacerItem>
#include <QSlider>
#include <QList>
#include <QComboBox>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>
#include "dsp_block_widget.h"
#include "../components/stereo_volume_bar_widget.h"
#include "../components/audio_vertical_slider.h"

/**
 * @brief DSPWidget provides a user interface for managing DSP modules in the application.
 * It includes a title bar with buttons for adding, removing, and clearing DSP modules,
 * and a scrollable area to display the DSP modules.
 */
class DSPEngineWidget : public QWidget {
    Q_OBJECT
public:
    explicit DSPEngineWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);

    QWidget *getTitleWidget() const { return this->title_widget; }

private:
    NoteNagaEngine * engine;

    std::vector<DSPBlockWidget*> dsp_widgets;

    QWidget *title_widget;
    AudioVerticalSlider *volume_slider;
    StereoVolumeBarWidget* volume_bar;
    QHBoxLayout *dsp_layout;

    QPushButton *btn_add;
    QPushButton *btn_clear;
    QPushButton *btn_enable;
    
    // Combobox to select synthesizer
    QComboBox *synth_selector;
    
    // Currently selected synth (nullptr for master)
    INoteNagaSoftSynth *current_synth = nullptr;

    void initTitleUI();
    void initUI();
    void refreshDSPWidgets();
    void clearDSPWidgets();

private slots:
    void addDSPClicked();
    void removeAllDSPClicked();
    void toggleDSPEnabled();
    void onSynthesizerSelected(int index);
    void onSynthAdded(NoteNagaSynthesizer *synth);
    void onSynthRemoved(NoteNagaSynthesizer *synth);
    void onSynthUpdated(NoteNagaSynthesizer *synth);
    void updateSynthesizerSelector();
};