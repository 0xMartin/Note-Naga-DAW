#ifndef ARRANGEMENT_TRACK_HEADER_WIDGET_H
#define ARRANGEMENT_TRACK_HEADER_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

class NoteNagaArrangementTrack;
class TrackStereoMeter;
class AudioHorizontalSlider;
class AudioDialCentered;

/**
 * @brief Widget representing a single track header in the arrangement view.
 * Contains track name, mute/solo buttons, color indicator, volume/pan controls, and stereo meter.
 */
class ArrangementTrackHeaderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArrangementTrackHeaderWidget(NoteNagaArrangementTrack *track, int trackIndex, QWidget *parent = nullptr);
    
    void setTrack(NoteNagaArrangementTrack *track);
    NoteNagaArrangementTrack* getTrack() const { return m_track; }
    int getTrackIndex() const { return m_trackIndex; }
    void setTrackIndex(int index) { m_trackIndex = index; }
    
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    
    void updateFromTrack();
    
    TrackStereoMeter* getStereoMeter() const { return m_stereoMeter; }

signals:
    void muteToggled(int trackIndex);
    void soloToggled(int trackIndex);
    void colorChangeRequested(int trackIndex);
    void trackSelected(int trackIndex);
    void nameChanged(int trackIndex, const QString &newName);
    void volumeChanged(int trackIndex, float volume);
    void panChanged(int trackIndex, float pan);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onNameEditFinished();

private:
    NoteNagaArrangementTrack *m_track = nullptr;
    int m_trackIndex = -1;
    bool m_selected = false;
    
    // UI elements
    QLineEdit *m_nameEdit = nullptr;
    QPushButton *m_muteButton = nullptr;
    QPushButton *m_soloButton = nullptr;
    QPushButton *m_colorButton = nullptr;
    TrackStereoMeter *m_stereoMeter = nullptr;
    AudioHorizontalSlider *m_volumeSlider = nullptr;
    AudioDialCentered *m_panDial = nullptr;
    
    void setupUI();
    void updateButtonStyles();
};

#endif // ARRANGEMENT_TRACK_HEADER_WIDGET_H
