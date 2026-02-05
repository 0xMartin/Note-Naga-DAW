#pragma once

#include <QMainWindow>
#include <QMap>
#include <QString>
#include <QSplitter>
#include <QScrollBar>
#include <QTimer>

#include <note_naga_engine/note_naga_engine.h>
#include "section_interface.h"

class AdvancedDockWidget;
class ArrangementLayerManager;
class ArrangementTimelineWidget;
class ArrangementResourcePanel;
class ArrangementTimelineRuler;

/**
 * @brief ArrangementSection provides the Arrangement/Composition view with:
 *        - Layer manager (left panel) - manage arrangement tracks
 *        - Timeline with clips (center) - main editing area
 *        - Resource panel (right) - list of MIDI sequences for drag & drop
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

private:
    NoteNagaEngine *m_engine;
    
    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;
    
    // Content widgets
    ArrangementLayerManager *m_layerManager;
    ArrangementTimelineWidget *m_timeline;
    ArrangementResourcePanel *m_resourcePanel;
    ArrangementTimelineRuler *m_timelineRuler;
    QScrollBar *m_timelineScrollBar;
    
    // State
    bool m_layoutInitialized = false;
    QTimer *m_meterUpdateTimer = nullptr;
    
    void setupDockLayout();
    void connectSignals();
    void updateScrollBarRange();
    
protected:
    void showEvent(QShowEvent *event) override;
};
