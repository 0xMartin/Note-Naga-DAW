#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QMap>
#include <QTimer>
#include <note_naga_engine/note_naga_engine.h>
#include "section_interface.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QComboBox;
class QScrollArea;
class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;
class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QCheckBox;
QT_END_NAMESPACE

class AdvancedDockWidget;
class NoteNagaTrack;
class NoteNagaArrangementTrack;

/**
 * @brief Structure to hold external MIDI routing configuration for a track
 */
struct ExternalMidiRouting {
    QString deviceName;     ///< MIDI output device name (empty = disabled)
    int channel = 1;        ///< MIDI channel (1-16)
    bool enabled = false;   ///< Whether external MIDI output is enabled for this track
};

/**
 * @brief Section for configuring external MIDI output routing.
 *        Allows sending MIDI notes to external hardware/software synthesizers.
 */
class ExternalMidiSection : public QMainWindow, public ISection
{
    Q_OBJECT

public:
    explicit ExternalMidiSection(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~ExternalMidiSection();

    // ISection interface
    void onSectionActivated() override;
    void onSectionDeactivated() override;

    /**
     * @brief Get the external MIDI routing for a MIDI track
     * @param track Pointer to the MIDI track
     * @return Routing configuration for the track
     */
    ExternalMidiRouting getRoutingForTrack(NoteNagaTrack* track) const;

    /**
     * @brief Get the external MIDI routing for an arrangement track
     * @param track Pointer to the arrangement track
     * @return Routing configuration for the track
     */
    ExternalMidiRouting getRoutingForArrangementTrack(NoteNagaArrangementTrack* track) const;

    /**
     * @brief Get available MIDI output devices
     * @return List of device names
     */
    QStringList getAvailableDevices() const;

public slots:
    /**
     * @brief Called when playback mode changes (Sequence/Arrangement)
     * @param mode The new playback mode
     */
    void setPlaybackMode(PlaybackMode mode);

    /**
     * @brief Refresh the list of available MIDI devices
     */
    void refreshDevices();

    /**
     * @brief Refresh the track list based on current playback mode
     */
    void refreshTracks();

signals:
    /**
     * @brief Emitted when routing configuration changes
     */
    void routingChanged();

private slots:
    void onDeviceSelected(int index);
    void onDeviceItemClicked(QListWidgetItem* item);
    void onTrackRoutingChanged();
    void onActiveSequenceChanged(NoteNagaMidiSeq* sequence);

private:
    // Engine reference
    NoteNagaEngine *m_engine;
    bool m_sectionActive;

    // Device tracking
    QStringList m_availableDevices;
    QString m_selectedDevice;

    // Track routing maps
    QMap<NoteNagaTrack*, ExternalMidiRouting> m_midiTrackRouting;
    QMap<NoteNagaArrangementTrack*, ExternalMidiRouting> m_arrangementTrackRouting;

    // Dock widgets
    QMap<QString, AdvancedDockWidget*> m_docks;

    // No content placeholder
    QLabel *m_noContentLabel;

    // Device list widgets
    QListWidget *m_deviceList;
    QPushButton *m_refreshBtn;
    QLabel *m_deviceStatusLabel;

    // Track routing widgets container
    QScrollArea *m_trackScrollArea;
    QWidget *m_trackListWidget;
    QVBoxLayout *m_trackListLayout;

    // Track routing row widgets (for dynamic updates)
    struct TrackRoutingRow {
        QWidget* container;
        QLabel* nameLabel;
        QComboBox* deviceCombo;
        QSpinBox* channelSpin;
        QCheckBox* enableCheck;
        NoteNagaTrack* midiTrack;
        NoteNagaArrangementTrack* arrangementTrack;
    };
    QList<TrackRoutingRow> m_trackRows;

    // Setup methods
    void setupUi();
    void setupDockLayout();
    void connectEngineSignals();
    void updateDeviceList();
    void rebuildTrackList();
    void clearTrackList();
    
    // Helper
    bool isArrangementMode() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
};

