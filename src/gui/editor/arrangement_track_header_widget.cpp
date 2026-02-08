#include "arrangement_track_header_widget.h"
#include "../components/track_stereo_meter.h"
#include "../components/audio_horizontal_slider.h"
#include "../components/audio_dial_centered.h"
#include "../nn_gui_utils.h"

#include <note_naga_engine/core/types.h>

#include <QPainter>
#include <QMouseEvent>
#include <QColorDialog>
#include <cmath>

ArrangementTrackHeaderWidget::ArrangementTrackHeaderWidget(NoteNagaArrangementTrack *track, int trackIndex, QWidget *parent)
    : QWidget(parent), m_track(track), m_trackIndex(trackIndex)
{
    setupUI();
    updateFromTrack();
}

void ArrangementTrackHeaderWidget::setupUI()
{
    setFixedHeight(80);  // Reduced height for smaller controls
    setMinimumWidth(200);
    
    // Main horizontal layout: left column (name+controls) + right column (pan dial centered)
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(6, 3, 4, 3);
    mainLayout->setSpacing(2);
    
    // LEFT COLUMN: everything except pan dial
    QVBoxLayout *leftColumn = new QVBoxLayout();
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(2);
    
    // Top row: editable name + buttons on same line
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(2);
    
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setFrame(false);
    m_nameEdit->setStyleSheet(
        "QLineEdit { "
        "  background: transparent; "
        "  color: #cccccc; "
        "  border: none; "
        "  font-size: 11px; "
        "  padding: 0px; "
        "}"
        "QLineEdit:focus { "
        "  background: #2a2a35; "
        "  border: 1px solid #4a4a55; "
        "  border-radius: 2px; "
        "}"
    );
    m_nameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_nameEdit->setFixedHeight(16);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &ArrangementTrackHeaderWidget::onNameEditFinished);
    topRow->addWidget(m_nameEdit);
    
    // Button size (reduced by 30% from 14 to 10)
    const int BTN_SIZE = 10;
    
    // Color button
    m_colorButton = new QPushButton(this);
    m_colorButton->setFixedSize(BTN_SIZE, BTN_SIZE);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    m_colorButton->setToolTip(tr("Click to change track color"));
    connect(m_colorButton, &QPushButton::clicked, this, [this]() {
        emit colorChangeRequested(m_trackIndex);
    });
    topRow->addWidget(m_colorButton);
    
    // Mute button
    m_muteButton = create_small_button(":/icons/sound-on.svg", tr("Toggle Track Mute/Play"), "MuteButton", 8, this);
    m_muteButton->setCheckable(true);
    m_muteButton->setFixedSize(BTN_SIZE, BTN_SIZE);
    connect(m_muteButton, &QPushButton::clicked, this, [this]() {
        if (m_track) {
            m_track->setMuted(!m_track->isMuted());
            updateButtonStyles();
            emit muteToggled(m_trackIndex);
        }
    });
    topRow->addWidget(m_muteButton);
    
    // Solo button
    m_soloButton = create_small_button(":/icons/solo.svg", tr("Toggle Solo Mode"), "SoloButton", 8, this);
    m_soloButton->setCheckable(true);
    m_soloButton->setFixedSize(BTN_SIZE, BTN_SIZE);
    connect(m_soloButton, &QPushButton::clicked, this, [this]() {
        if (m_track) {
            m_track->setSolo(!m_track->isSolo());
            updateButtonStyles();
            emit soloToggled(m_trackIndex);
        }
    });
    topRow->addWidget(m_soloButton);
    
    leftColumn->addLayout(topRow);
    
    // Volume slider (horizontal) - using dB scale
    // Range: -60 dB to +6 dB (0 dB = 1.0 linear)
    m_volumeSlider = new AudioHorizontalSlider(this);
    m_volumeSlider->setRange(-60.0f, 6.0f);
    m_volumeSlider->setValue(0.0f);  // 0 dB = unity gain
    m_volumeSlider->setDefaultValue(0.0f);
    m_volumeSlider->setValuePostfix(" dB");
    m_volumeSlider->setLabelText("Vol");
    m_volumeSlider->setLabelVisible(false);
    m_volumeSlider->setValueVisible(true);
    m_volumeSlider->setValueDecimals(1);
    m_volumeSlider->setMinimumHeight(11);
    m_volumeSlider->setToolTip(tr("Volume in dB (Right-click to reset to 0 dB)"));
    connect(m_volumeSlider, &AudioHorizontalSlider::valueChanged, this, [this](float dB) {
        if (m_track) {
            // Convert dB to linear: linear = 10^(dB/20)
            float linear = (dB <= -60.0f) ? 0.0f : std::pow(10.0f, dB / 20.0f);
            m_track->setVolume(linear);
            emit volumeChanged(m_trackIndex, linear);
        }
    });
    leftColumn->addWidget(m_volumeSlider);
    
    // Stereo meter below slider
    m_stereoMeter = new TrackStereoMeter(this, -70, 0);
    m_stereoMeter->setFixedHeight(18);
    leftColumn->addWidget(m_stereoMeter);
    
    leftColumn->addStretch();
    mainLayout->addLayout(leftColumn, 1);
    
    // RIGHT COLUMN: Pan dial centered vertically (default size 40x60)
    QVBoxLayout *rightColumn = new QVBoxLayout();
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->addStretch();
    
    m_panDial = new AudioDialCentered(this);
    // Use default minimum size (40x60) - no setFixedSize needed
    m_panDial->setRange(-100.0f, 100.0f);
    m_panDial->setValue(0.0f);
    m_panDial->setDefaultValue(0.0f);
    m_panDial->showLabel(false);
    m_panDial->setLabel("Pan");
    m_panDial->showValue(true);
    m_panDial->setToolTip(tr("Pan (Right-click to center)"));
    m_panDial->setValuePrefix("");
    m_panDial->setValuePostfix("");
    m_panDial->setValueDecimals(0);
    m_panDial->bg_color = QColor("#2a2f35");
    m_panDial->arc_bg_color = QColor("#1e1e20");
    connect(m_panDial, &AudioDialCentered::valueChanged, this, [this](float pan) {
        if (m_track) {
            m_track->setPan(pan / 100.0f);  // Convert from -100..100 to -1..1
            emit panChanged(m_trackIndex, pan / 100.0f);
        }
    });
    rightColumn->addWidget(m_panDial, 0, Qt::AlignCenter);
    
    rightColumn->addStretch();
    mainLayout->addLayout(rightColumn);
    
    updateButtonStyles();
}

void ArrangementTrackHeaderWidget::setTrack(NoteNagaArrangementTrack *track)
{
    m_track = track;
    updateFromTrack();
}

void ArrangementTrackHeaderWidget::updateFromTrack()
{
    if (!m_track) {
        m_nameEdit->setText(tr("Track %1").arg(m_trackIndex + 1));
        return;
    }
    
    QString name = QString::fromStdString(m_track->getName());
    if (name.isEmpty()) {
        name = tr("Track %1").arg(m_trackIndex + 1);
    }
    m_nameEdit->setText(name);
    
    // Sync volume slider (convert from linear 0..1+ to dB)
    if (m_volumeSlider) {
        m_volumeSlider->blockSignals(true);
        float linear = m_track->getVolume();
        float dB = (linear <= 0.0f) ? -60.0f : 20.0f * std::log10(linear);
        dB = std::clamp(dB, -60.0f, 6.0f);
        m_volumeSlider->setValue(dB);
        m_volumeSlider->blockSignals(false);
    }
    
    // Sync pan dial (convert from -1..1 to -100..100)
    if (m_panDial) {
        m_panDial->blockSignals(true);
        m_panDial->setValue(m_track->getPan() * 100.0f);
        m_panDial->blockSignals(false);
    }
    
    updateButtonStyles();
    update();
}

void ArrangementTrackHeaderWidget::updateButtonStyles()
{
    if (!m_track) return;
    
    QColor trackColor = m_track->getColor().toQColor();
    const int BTN_SIZE = 18;
    
    // Color button - square with rounded corners, shows track color
    m_colorButton->setStyleSheet(QString(
        "QPushButton { "
        "  background-color: %1; "
        "  border: 1px solid #555555; "
        "  border-radius: 4px; "
        "  padding: 0; "
        "  margin: 0; "
        "  min-width: %2px; max-width: %2px; "
        "  min-height: %2px; max-height: %2px; "
        "}"
        "QPushButton:hover { border: 1px solid #888888; }"
    ).arg(trackColor.name()).arg(BTN_SIZE));
    
    // Mute button - checkable style with forced square size
    bool muted = m_track->isMuted();
    m_muteButton->setChecked(muted);
    m_muteButton->setStyleSheet(QString(
        "QPushButton { "
        "  background-color: %1; "
        "  border: 1px solid #444444; "
        "  border-radius: 4px; "
        "  padding: 2px; "
        "  margin: 0; "
        "  min-width: %3px; max-width: %3px; "
        "  min-height: %3px; max-height: %3px; "
        "}"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:checked { background-color: #ef4444; }"
        "QPushButton:checked:hover { background-color: #f87171; }"
    ).arg(muted ? "#ef4444" : "#3a3a42")
     .arg(muted ? "#f87171" : "#4a4a52")
     .arg(BTN_SIZE));
    
    // Solo button - checkable style with forced square size
    bool solo = m_track->isSolo();
    m_soloButton->setChecked(solo);
    m_soloButton->setStyleSheet(QString(
        "QPushButton { "
        "  background-color: %1; "
        "  border: 1px solid #444444; "
        "  border-radius: 4px; "
        "  padding: 2px; "
        "  margin: 0; "
        "  min-width: %3px; max-width: %3px; "
        "  min-height: %3px; max-height: %3px; "
        "}"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:checked { background-color: #22c55e; }"
        "QPushButton:checked:hover { background-color: #4ade80; }"
    ).arg(solo ? "#22c55e" : "#3a3a42")
     .arg(solo ? "#4ade80" : "#4a4a52")
     .arg(BTN_SIZE));
}

void ArrangementTrackHeaderWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void ArrangementTrackHeaderWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Background
    if (m_selected) {
        painter.fillRect(rect(), QColor("#32323d"));
    } else {
        painter.fillRect(rect(), QColor("#26262c"));
    }
    
    // Track color indicator on the left
    if (m_track) {
        QColor trackColor = m_track->getColor().toQColor();
        painter.fillRect(0, 0, 4, height(), trackColor);
    }
    
    // Bottom separator line
    painter.setPen(QColor("#3a3a42"));
    painter.drawLine(0, height() - 1, width(), height() - 1);
    
    // Right separator line
    painter.drawLine(width() - 1, 0, width() - 1, height());
}

void ArrangementTrackHeaderWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit trackSelected(m_trackIndex);
    }
    QWidget::mousePressEvent(event);
}

void ArrangementTrackHeaderWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Focus the name edit for editing
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ArrangementTrackHeaderWidget::onNameEditFinished()
{
    if (!m_track) return;
    
    QString newName = m_nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        // Restore default name if empty
        newName = tr("Track %1").arg(m_trackIndex + 1);
        m_nameEdit->setText(newName);
    }
    
    // Update the track name if changed
    QString currentName = QString::fromStdString(m_track->getName());
    if (newName != currentName) {
        m_track->setName(newName.toStdString());
        emit nameChanged(m_trackIndex, newName);
    }
}
