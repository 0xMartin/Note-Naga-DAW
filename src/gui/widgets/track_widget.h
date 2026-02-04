#pragma once

#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>

#include "../components/audio_dial_centered.h"
#include "../components/track_stereo_meter.h"
#include <note_naga_engine/note_naga_engine.h>

/**
 * @brief TrackWidget is a GUI widget representing a single track in the NoteNaga
 * engine. It displays track information, allows interaction with the track's
 * properties, and provides controls for manipulating the track.
 */
class TrackWidget : public QFrame {
    Q_OBJECT
public:
    /**
     * @brief Constructs a TrackWidget for a specific NoteNagaTrack.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param track Pointer to the NoteNagaTrack to display.
     * @param parent Parent widget.
     */
    explicit TrackWidget(NoteNagaEngine *engine, NoteNagaTrack *track,
                         QWidget *parent = nullptr);

    /**
     * @brief Get the associated NoteNagaTrack.
     * @return Pointer to the NoteNagaTrack.
     */
    NoteNagaTrack *getTrack() const { return this->track; }

    /**
     * @brief Get the stereo meter associated with this track.
     * @return Pointer to the TrackStereoMeter.
     */
    TrackStereoMeter *getStereoMeter() const { return m_stereoMeter; }

    /**
     * @brief Set the checked state of the solo view button.
     * @param checked True to check, false to uncheck.
     */
    void setSoloViewChecked(bool checked) { solo_view_btn->setChecked(checked); }

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    NoteNagaTrack *track;
    NoteNagaEngine *engine;

    QPushButton *instrument_btn;
    QPushButton *index_btn;           ///< Track number button (also color picker)
    QLineEdit *name_edit;
    QLabel *unsaved_indicator;        ///< Dot indicator showing unsaved track name
    QPushButton *invisible_btn;
    QPushButton *solo_view_btn;       ///< Toggle solo view (show only this track)
    QPushButton *solo_btn;
    QPushButton *mute_btn;
    QPushButton *synth_btn;           ///< Synth configuration button
    QPushButton *tempo_active_btn;    ///< Toggle for tempo track activation
    TrackStereoMeter *m_stereoMeter;  ///< Stereo level meter
    
    // Per-track synth control dials
    AudioDialCentered *m_volumeDial;          ///< Audio volume in dB (-24 to +6)
    AudioDialCentered *m_panDial;             ///< MIDI pan offset (-64 to +64)
    QWidget *m_dialsWidget;                   ///< Container for dials (hidden for tempo track)
    QWidget *m_leftPanel;                     ///< Left panel with index + instrument (inside TrackInfoPanel)
    
    // For tempo track special layout
    QWidget *m_tempoLeftPanel;                ///< Separate left panel for tempo track (always visible)
    QPushButton *m_tempoInstrumentBtn;        ///< Tempo track instrument button
    QPushButton *m_tempoIndexBtn;             ///< Tempo track index button
    QWidget *m_normalContent;  ///< Container for normal track controls
    QWidget *m_tempoContent;   ///< Container for tempo track controls
    bool m_isTempoTrackLayout; ///< Track if currently showing tempo layout
    
    // Selection state for styling
    bool m_selected = false;   ///< Whether this track is currently selected
    bool m_darkerBg = false;   ///< Whether using darker background (alternating rows)

    void updateIndexButtonStyle();  ///< Update track number button style based on track color
    void updateLeftPanelStyle();    ///< Update left panel style based on track color
    void updateSynthButtonIcon();   ///< Update synth button icon based on SoundFont status

/*******************************************************************************************************/
// Signal and Slots
/*******************************************************************************************************/
public slots:
    /**
     * @brief Refreshes the widget's style based on the track's state.
     * @param selected True if the track is selected, false otherwise.
     * @param darker_bg True if the background should be darker, false otherwise.
     */
    void refreshStyle(bool selected, bool darker_bg);

signals:
    /**
     * @brief Signal emitted when the track is clicked.
     * @param track_id ID of the clicked track.
     */
    void clicked(int track_id);
    
    /**
     * @brief Signal emitted when solo view is toggled for this track.
     * @param track Pointer to the track.
     * @param enabled True to show only this track, false to show all.
     */
    void soloViewToggled(NoteNagaTrack *track, bool enabled);
    
private slots:
    void updateTrackInfo(NoteNagaTrack *track, const std::string &param);
    void onToggleVisibility();
    void onToggleSoloView();
    void onToggleSolo();
    void onToggleMute();
    void onToggleTempoActive();
    void onSynthClicked();
    void onNameEdited();
    void onNameTextChanged(const QString &text);
    void colorSelect();
    void instrumentSelect();
    void onVolumeDialChanged(float value);
    void onPanDialChanged(float value);
    
private:
    void setupTempoTrackLayout();
    void setupNormalTrackLayout();
};
