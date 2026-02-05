#include "arrangement_track_header_widget.h"
#include "../components/track_stereo_meter.h"
#include "../nn_gui_utils.h"

#include <note_naga_engine/core/types.h>

#include <QPainter>
#include <QMouseEvent>
#include <QColorDialog>

ArrangementTrackHeaderWidget::ArrangementTrackHeaderWidget(NoteNagaArrangementTrack *track, int trackIndex, QWidget *parent)
    : QWidget(parent), m_track(track), m_trackIndex(trackIndex)
{
    setupUI();
    updateFromTrack();
}

void ArrangementTrackHeaderWidget::setupUI()
{
    setFixedHeight(60);
    setMinimumWidth(160);
    
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 4, 4);
    mainLayout->setSpacing(2);
    
    // Top row: name and buttons
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(2);
    
    // Editable track name
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
    
    // Button size for square buttons (1:1 aspect ratio)
    const int BTN_SIZE = 18;
    
    // Color button - square with rounded corners, shows track color
    m_colorButton = new QPushButton(this);
    m_colorButton->setFixedSize(BTN_SIZE, BTN_SIZE);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    m_colorButton->setToolTip(tr("Click to change track color"));
    connect(m_colorButton, &QPushButton::clicked, this, [this]() {
        emit colorChangeRequested(m_trackIndex);
    });
    topRow->addWidget(m_colorButton);
    
    // Mute button - square with icon (same style as TrackWidget)
    m_muteButton = create_small_button(":/icons/sound-on.svg", tr("Toggle Track Mute/Play"), "MuteButton", 14, this);
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
    
    // Solo button - square with icon (same style as TrackWidget)
    m_soloButton = create_small_button(":/icons/solo.svg", tr("Toggle Solo Mode"), "SoloButton", 14, this);
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
    
    mainLayout->addLayout(topRow);
    
    // Spacer to push meter to bottom
    mainLayout->addStretch(1);
    
    // Stereo meter at bottom (50% taller)
    m_stereoMeter = new TrackStereoMeter(this, -70, 0);
    m_stereoMeter->setFixedHeight(21);
    mainLayout->addWidget(m_stereoMeter);
    
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
        painter.fillRect(rect(), QColor("#2a2a35"));
    } else {
        painter.fillRect(rect(), QColor("#1e1e24"));
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
