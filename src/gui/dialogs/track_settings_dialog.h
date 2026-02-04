#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileInfo>
#include <QCheckBox>
#include <functional>

#include <note_naga_engine/note_naga_engine.h>

class NoteNagaSynthFluidSynth;

/**
 * @brief Dialog for configuring track settings.
 *        Allows editing name, color, mute/solo, instrument, SoundFont, track type.
 */
class TrackSettingsDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Constructor for TrackSettingsDialog.
     * @param parent Parent widget.
     * @param track The track to configure.
     * @param iconProvider Function to provide instrument icons.
     */
    explicit TrackSettingsDialog(QWidget *parent, NoteNagaTrack *track,
                                 std::function<QIcon(QString)> iconProvider = nullptr);

    /**
     * @brief Check if SoundFont was loaded successfully after dialog closed.
     * @return True if SoundFont is valid.
     */
    bool isSoundFontValid() const { return m_soundFontValid; }

signals:
    /**
     * @brief Emitted when settings are applied and track needs refresh.
     */
    void trackSettingsChanged();

private slots:
    void onBrowseSoundFont();
    void onTrackTypeChanged(int index);
    void onColorClicked();
    void onInstrumentClicked();
    void onApply();

private:
    void updateSoundFontStatus();
    void updateColorButton();
    void updateInstrumentButton();

    NoteNagaTrack *m_track;
    NoteNagaSynthFluidSynth *m_fluidSynth;
    std::function<QIcon(QString)> m_iconProvider;
    
    // Track info section
    QLineEdit *m_nameEdit;
    QPushButton *m_colorBtn;
    QPushButton *m_instrumentBtn;
    QCheckBox *m_muteCheck;
    QCheckBox *m_soloCheck;
    QCheckBox *m_visibleCheck;
    
    // SoundFont section
    QLineEdit *m_soundFontPath;
    QPushButton *m_browseBtn;
    QLabel *m_statusLabel;
    QLabel *m_statusIcon;
    
    // Track type section
    QComboBox *m_trackTypeCombo;
    
    // Dialog buttons
    QPushButton *m_applyBtn;
    QPushButton *m_closeBtn;
    
    bool m_soundFontValid;
    QString m_pendingSoundFont;
    NN_Color_t m_pendingColor;
    std::optional<int> m_pendingInstrument;
};
