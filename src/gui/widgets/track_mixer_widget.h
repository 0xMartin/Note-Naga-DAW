#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QMessageBox>
#include <QMap>
#include <QString>
#include <QIcon>

#include <note_naga_engine/note_naga_engine.h>
#include "../components/multi_channel_volume_bar.h"
#include "routing_entry_widget.h"

/**
 * @brief The TrackMixerWidget class provides a user interface for mixing tracks in the NoteNaga engine.
 * It allows users to adjust volume, pan, and other parameters for each track.
 */
class TrackMixerWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a new TrackMixerWidget.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param parent Parent widget (default is nullptr).
     */
    explicit TrackMixerWidget(NoteNagaEngine* engine, QWidget* parent = nullptr);

    /**
     * @brief Gets the title widget that will be inserted into the dock title bar.
     * @return Pointer to the title widget.
     */
    QWidget *getTitleWidget() const { return this->title_widget; }
    
    /**
     * @brief Returns preferred size hint for dock layout.
     */
    QSize sizeHint() const override { return QSize(280, 300); }

private:
    NoteNagaEngine* engine;

    int selected_entry_index;

    std::vector<RoutingEntryWidget*> entry_widgets;

    QWidget *title_widget;

    // Synthesizer selector combobox
    QComboBox* synth_selector;
    QMap<QString, MultiChannelVolumeBar*> channel_volume_bars;
    QString current_channel_device;
    std::string current_synth_name;  // Currently selected synthesizer name

    QVBoxLayout* routing_entries_layout;
    QWidget* routing_entries_container;
    QScrollArea* routing_scroll;

    void initUI();
    void initTitleUI();
    void setChannelOutputValue(const std::string& device, int channel_idx, float value, int time_ms = -1);
    void updateEntrySelection(int idx);
    
    /**
     * @brief Updates the synthesizer selector combobox with available synthesizers
     */
    void updateSynthesizerSelector();

/*******************************************************************************************************/
// Signal and Slots
/*******************************************************************************************************/
public slots:
    /**
     * @brief Refreshes the routing table GUI with active routing entries.
     */
    void refresh_routing_table();
    
    /**
     * @brief Handles synthesizer added event
     * @param synth Pointer to the added synthesizer
     */
    void onSynthesizerAdded(NoteNagaSynthesizer* synth);
    
    /**
     * @brief Handles synthesizer removed event
     * @param synth Pointer to the removed synthesizer
     */
    void onSynthesizerRemoved(NoteNagaSynthesizer* synth);
    
    /**
     * @brief Handles synthesizer updated event
     * @param synth Pointer to the updated synthesizer
     */
    void onSynthesizerUpdated(NoteNagaSynthesizer* synth);

private slots:

    void onAddEntry();
    void onRemoveSelectedEntry();
    void onReassignSynth();
    void onClearRoutingTable();
    void onDefaultEntries();
    void onMaxVolumeAllTracks();
    void onMinVolumeAllTracks();
    void handlePlayingNote(const NN_Note_t& note, const std::string& device_name, int channel);
    
    /**
     * @brief Handles selection change in the synthesizer selector
     * @param index Index of the selected item
     */
    void onSynthesizerSelectionChanged(int index);
};