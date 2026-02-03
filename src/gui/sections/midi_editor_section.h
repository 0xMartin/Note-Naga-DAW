#pragma once

#include <QMainWindow>
#include <QMap>
#include <QString>
#include <QSplitter>

#include <note_naga_engine/note_naga_engine.h>
#include "section_interface.h"

class AdvancedDockWidget;
class MidiEditorWidget;
class MidiControlBarWidget;
class MidiTactRuler;
class MidiKeyboardRuler;
class TrackListWidget;
class TrackMixerWidget;
class NotePropertyEditor;
class TempoTrackEditor;
class TimelineOverviewWidget;

/**
 * @brief MidiEditorSection provides the MIDI Editor section layout with:
 *        - Track list (left)
 *        - MIDI editor with rulers + note property editor + control bar (center)
 *        - Track mixer (right)
 *        All components wrapped in AdvancedDockWidget.
 */
class MidiEditorSection : public QMainWindow, public ISection {
    Q_OBJECT
public:
    explicit MidiEditorSection(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~MidiEditorSection();

    // ISection interface
    void onSectionActivated() override;
    void onSectionDeactivated() override;

    // Access to widgets for external signal connections
    MidiEditorWidget* getMidiEditor() const { return m_midiEditor; }
    MidiControlBarWidget* getControlBar() const { return m_controlBar; }
    MidiTactRuler* getTactRuler() const { return m_midiTactRuler; }
    MidiKeyboardRuler* getKeyboardRuler() const { return m_midiKeyboardRuler; }
    TrackListWidget* getTrackList() const { return m_trackListWidget; }
    TrackMixerWidget* getTrackMixer() const { return m_mixerWidget; }
    NotePropertyEditor* getNotePropertyEditor() const { return m_notePropertyEditor; }
    TempoTrackEditor* getTempoTrackEditor() const { return m_tempoTrackEditor; }

    /**
     * @brief Resets the dock layout to default
     */
    void resetLayout();

public slots:
    void showHideDock(const QString &name, bool checked);
    void toggleNotePropertyEditor();
    void onActiveTrackChanged(NoteNagaTrack *track);

private:
    NoteNagaEngine *m_engine;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Content widgets
    MidiEditorWidget *m_midiEditor;
    MidiControlBarWidget *m_controlBar;
    MidiTactRuler *m_midiTactRuler;
    MidiKeyboardRuler *m_midiKeyboardRuler;
    TrackListWidget *m_trackListWidget;
    TrackMixerWidget *m_mixerWidget;
    NotePropertyEditor *m_notePropertyEditor;
    TempoTrackEditor *m_tempoTrackEditor;
    TimelineOverviewWidget *m_timelineOverview;
    
    // Splitter for MIDI editor / Note property editor
    QSplitter *m_editorSplitter;
    QWidget *m_propertyEditorContainer;
    bool m_layoutInitialized = false;
    
    void setupDockLayout();
    void connectSignals();
    
protected:
    void showEvent(QShowEvent *event) override;
};
