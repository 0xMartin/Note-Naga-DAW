#pragma once

#include <QMainWindow>
#include <QMap>
#include <QString>
#include <QSplitter>
#include <QScrollBar>
#include <QTimer>
#include <QPushButton>

#include <note_naga_engine/note_naga_engine.h>
#include "section_interface.h"

class AdvancedDockWidget;
class ArrangementLayerManager;
class ArrangementTimelineWidget;
class ArrangementTrackHeadersWidget;
class ArrangementResourcePanel;
class ArrangementTimelineRuler;
class ArrangementMinimapWidget;
class ArrangementTempoTrackEditor;
class UndoManager;

// Track header width constant (must match arrangement_timeline_widget.h)
static const int TRACK_HEADER_WIDTH = 160;

/**
 * @brief ArrangementSection provides the Arrangement/Composition view with:
 *        - Layer manager (left panel) - manage arrangement tracks
 *        - Timeline with clips (center) - main editing area
 *        - Resource panel (right) - list of MIDI sequences for drag & drop
 *        - Minimap (bottom) - overview of entire arrangement
 *        All components wrapped in AdvancedDockWidget.
 */
class ArrangementSection : public QMainWindow, public ISection {
    Q_OBJECT
public:
    explicit ArrangementSection(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~ArrangementSection();

    // ISection interface
    void onSectionActivated() override;
    void onSectionDeactivated() override;

    // Access to widgets
    ArrangementLayerManager* getLayerManager() const { return m_layerManager; }
    ArrangementTimelineWidget* getTimeline() const { return m_timeline; }
    ArrangementResourcePanel* getResourcePanel() const { return m_resourcePanel; }
    UndoManager* getUndoManager() const { return m_undoManager; }

    /**
     * @brief Resets the dock layout to default
     */
    void resetLayout();

signals:
    /// Emitted when user wants to edit a sequence in MIDI editor
    void switchToMidiEditor(int sequenceIndex);

public slots:
    void showHideDock(const QString &name, bool checked);
    void onArrangementChanged();
    void onPlaybackPositionChanged(int tick);
    
    /// Scroll to a specific tick (used by go to start/end)
    void scrollToTick(int64_t tick);
    
    /// Force refresh of minimap (clips changed, etc.)
    void refreshMinimap();
    
    /// Set the project file path (for audio recording)
    void setProjectFilePath(const QString &path);

private:
    NoteNagaEngine *m_engine;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Content widgets
    ArrangementLayerManager *m_layerManager;
    ArrangementTimelineWidget *m_timeline;
    ArrangementTrackHeadersWidget *m_trackHeaders;
    ArrangementResourcePanel *m_resourcePanel;
    ArrangementTimelineRuler *m_timelineRuler;
    ArrangementMinimapWidget *m_minimap;
    ArrangementTempoTrackEditor *m_tempoTrackEditor;
    QScrollBar *m_timelineScrollBar;
    QWidget *m_timelineTitleWidget = nullptr;
    
    // Splitter and helper widgets
    QSplitter *m_headerTimelineSplitter = nullptr;
    QSplitter *m_mainVerticalSplitter = nullptr;
    QWidget *m_headerCorner = nullptr;
    QWidget *m_scrollbarSpacer = nullptr;
    QWidget *m_minimapSpacer = nullptr;
    
    // State
    bool m_layoutInitialized = false;
    QTimer *m_meterUpdateTimer = nullptr;
    bool m_autoScrollEnabled = true;
    
    // Undo/Redo
    UndoManager *m_undoManager = nullptr;
    QPushButton *m_btnUndo = nullptr;
    QPushButton *m_btnRedo = nullptr;
    
    void setupDockLayout();
    void connectSignals();
    void updateScrollBarRange();
    void updateMinimapVisibleRange();
    void autoScrollToPlayhead(int tick);
    QWidget* createTimelineTitleWidget();
    void updateUndoRedoButtons();

protected:
    void showEvent(QShowEvent *event) override;
};
