#pragma once

#include <QWidget>
#include <QMainWindow>
#include <QMap>
#include <QString>

#include <note_naga_engine/note_naga_engine.h>

class AdvancedDockWidget;
class DSPEngineWidget;
class SpectrumAnalyzer;
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
class DSPEditorSection : public QMainWindow {
    Q_OBJECT
public:
    explicit DSPEditorSection(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~DSPEditorSection();

    /**
     * @brief Called when section becomes visible - activates spectrum analyzer
     */
    void onSectionActivated();

    /**
     * @brief Called when section becomes hidden - deactivates spectrum analyzer
     */
    void onSectionDeactivated();
    
    /**
     * @brief Get control bar widget for signal connections
     */
    MidiControlBarWidget *getControlBar() const { return m_controlBar; }

private:
    NoteNagaEngine *m_engine;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Content widgets
    DSPEngineWidget *m_dspWidget;
    SpectrumAnalyzer *m_spectrumAnalyzer;
    TrackPreviewWidget *m_trackPreview;
    MidiControlBarWidget *m_controlBar;
    
    void setupDockLayout();
};
