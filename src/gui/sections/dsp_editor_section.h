#pragma once

#include <QWidget>
#include <QMainWindow>
#include <QMap>
#include <QString>

#include <note_naga_engine/note_naga_engine.h>
#include "section_interface.h"

class AdvancedDockWidget;
class DSPEngineWidget;
class SpectrumAnalyzer;
class PanAnalyzer;
class TrackPreviewWidget;
class MidiControlBarWidget;

/**
 * @brief DSPEditorSection provides the DSP Editor section layout with:
 *        - Track preview (top right)
 *        - Spectrum analyzer (top left)
 *        - DSP widget (bottom)
 *        - Control bar for playback
 *        All components wrapped in AdvancedDockWidget.
 */
class DSPEditorSection : public QMainWindow, public ISection {
    Q_OBJECT
public:
    explicit DSPEditorSection(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~DSPEditorSection();

    // ISection interface
    void onSectionActivated() override;
    void onSectionDeactivated() override;
    
    /**
     * @brief Get control bar widget for signal connections
     */
    MidiControlBarWidget *getControlBar() const { return m_controlBar; }
    
    /**
     * @brief Get DSP engine widget
     */
    DSPEngineWidget *getDSPEngineWidget() const { return m_dspWidget; }
    
    /**
     * @brief Refresh DSP widgets after project load
     */
    void refreshDSPWidgets();

private:
    NoteNagaEngine *m_engine;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Content widgets
    DSPEngineWidget *m_dspWidget;
    SpectrumAnalyzer *m_spectrumAnalyzer;
    PanAnalyzer *m_panAnalyzer;
    TrackPreviewWidget *m_trackPreview;
    MidiControlBarWidget *m_controlBar;
    
    void setupDockLayout();
};
