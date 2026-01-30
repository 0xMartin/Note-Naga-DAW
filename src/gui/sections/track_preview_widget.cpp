#include "track_preview_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <algorithm>

#include <note_naga_engine/nn_utils.h>
#include "../nn_gui_utils.h"

// ===================== TrackPreviewCanvas =====================

TrackPreviewCanvas::TrackPreviewCanvas(QWidget *parent)
    : QWidget(parent), m_sequence(nullptr), m_currentTick(0),
      m_timeWindowSeconds(5.0), m_noteHeight(6),
      m_lowestNote(21), m_highestNote(108), m_pixelsPerTick(0.05)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TrackPreviewCanvas::setSequence(NoteNagaMidiSeq *seq)
{
    m_sequence = seq;
    updateNoteRange();
    recalculateSize();
    update();
}

void TrackPreviewCanvas::setCurrentTick(int tick)
{
    m_currentTick = tick;
    update();
}

void TrackPreviewCanvas::setTimeWindowSeconds(double seconds)
{
    m_timeWindowSeconds = seconds;
    recalculateSize();
    update();
}

void TrackPreviewCanvas::setNoteHeight(int height)
{
    m_noteHeight = height;
    recalculateSize();
    update();
}

void TrackPreviewCanvas::updateNoteRange()
{
    if (!m_sequence) {
        m_lowestNote = 21;
        m_highestNote = 108;
        return;
    }
    
    int minNote = 127;
    int maxNote = 0;
    
    for (const auto *track : m_sequence->getTracks()) {
        for (const auto &note : track->getNotes()) {
            int key = note.note;
            minNote = qMin(minNote, key);
            maxNote = qMax(maxNote, key);
        }
    }
    
    // Add some padding
    m_lowestNote = qMax(0, minNote - 3);
    m_highestNote = qMin(127, maxNote + 3);
    
    // Ensure reasonable range
    if (m_highestNote - m_lowestNote < 12) {
        int center = (m_lowestNote + m_highestNote) / 2;
        m_lowestNote = qMax(0, center - 6);
        m_highestNote = qMin(127, center + 6);
    }
}

void TrackPreviewCanvas::recalculateSize()
{
    int noteRange = m_highestNote - m_lowestNote + 1;
    int totalHeight = noteRange * m_noteHeight;
    setFixedHeight(totalHeight);
    
    if (m_sequence) {
        int ppq = m_sequence->getPPQ();
        double tempo = m_sequence->getTempo();
        int windowTicks = nn_seconds_to_ticks(m_timeWindowSeconds * 2, ppq, tempo);
        if (windowTicks > 0) {
            m_pixelsPerTick = (double)width() / (double)windowTicks;
        }
    }
}

QColor TrackPreviewCanvas::getTrackColor(int trackIndex) const
{
    static const QColor defaultColors[] = {
        QColor(76, 175, 80),   // Green
        QColor(33, 150, 243),  // Blue
        QColor(255, 152, 0),   // Orange
        QColor(156, 39, 176),  // Purple
        QColor(244, 67, 54),   // Red
        QColor(0, 188, 212),   // Cyan
        QColor(255, 235, 59),  // Yellow
        QColor(121, 85, 72),   // Brown
    };
    
    if (m_sequence && trackIndex < (int)m_sequence->getTracks().size()) {
        QColor trackColor = m_sequence->getTracks()[trackIndex]->getColor().toQColor();
        // If track color is black or nearly black, use default color
        if (trackColor.lightness() < 10) {
            return defaultColors[trackIndex % 8];
        }
        return trackColor;
    }
    
    return defaultColors[trackIndex % 8];
}

void TrackPreviewCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    // Background
    p.fillRect(rect(), QColor(25, 25, 30));
    
    if (!m_sequence) {
        p.setPen(QColor(100, 100, 100));
        p.drawText(rect(), Qt::AlignCenter, tr("No sequence loaded"));
        return;
    }
    
    const auto &tracks = m_sequence->getTracks();
    if (tracks.empty()) {
        p.setPen(QColor(100, 100, 100));
        p.drawText(rect(), Qt::AlignCenter, tr("No tracks"));
        return;
    }
    
    // Calculate time window in ticks
    int ppq = m_sequence->getPPQ();
    double tempo = m_sequence->getTempo();
    int windowTicksHalf = nn_seconds_to_ticks(m_timeWindowSeconds, ppq, tempo);
    
    // Recalculate pixels per tick based on current width
    int windowTicks = windowTicksHalf * 2;
    if (windowTicks > 0) {
        m_pixelsPerTick = (double)width() / (double)windowTicks;
    }
    
    // Visible tick range (current position ± window)
    int startTick = m_currentTick - windowTicksHalf;
    int endTick = m_currentTick + windowTicksHalf;
    
    int noteRange = m_highestNote - m_lowestNote + 1;
    
    // Draw piano roll background - each row has exactly m_noteHeight pixels
    for (int note = m_lowestNote; note <= m_highestNote; ++note) {
        int noteIndex = m_highestNote - note;
        int y = noteIndex * m_noteHeight;
        
        // Black keys have darker background
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || 
                           noteInOctave == 8 || noteInOctave == 10);
        
        QColor bgColor = isBlackKey ? QColor(20, 20, 25) : QColor(30, 30, 35);
        p.fillRect(0, y, width(), m_noteHeight, bgColor);
        
        // Draw octave lines (C notes)
        if (noteInOctave == 0) {
            p.setPen(QColor(50, 50, 55));
            p.drawLine(0, y, width(), y);
        }
    }
    
    // Draw all notes from all tracks (merged)
    p.setPen(Qt::NoPen);
    for (int trackIdx = 0; trackIdx < (int)tracks.size(); ++trackIdx) {
        NoteNagaTrack *track = tracks[trackIdx];
        QColor noteColor = getTrackColor(trackIdx);
        
        for (const auto &note : track->getNotes()) {
            if (!note.start.has_value()) continue;
            
            int noteStart = note.start.value();
            int noteDuration = note.length.has_value() ? note.length.value() : 100;
            int noteEnd = noteStart + noteDuration;
            int midiNote = note.note;
            
            // Skip notes outside visible range
            if (noteEnd < startTick || noteStart > endTick) continue;
            if (midiNote < m_lowestNote || midiNote > m_highestNote) continue;
            
            // Calculate X position relative to viewport
            int noteX = (int)((noteStart - startTick) * m_pixelsPerTick);
            int noteWidth = qMax(2, (int)(noteDuration * m_pixelsPerTick));
            
            // Clip to view area
            if (noteX < 0) {
                noteWidth += noteX;
                noteX = 0;
            }
            if (noteX + noteWidth > width()) {
                noteWidth = width() - noteX;
            }
            if (noteWidth <= 0) continue;
            
            // Note Y position - each note has exactly m_noteHeight pixels
            int noteIndex = m_highestNote - midiNote;
            int noteY = noteIndex * m_noteHeight;
            
            // Velocity-based brightness (keep color recognizable)
            int vel = note.velocity.has_value() ? note.velocity.value() : 90;
            QColor c = noteColor;
            // Brightness range: 90 (low velocity) to 130 (high velocity)
            int brightness = 90 + (vel * 40 / 127);
            c = c.lighter(brightness);
            
            // Draw note with slight padding
            int padding = qMax(1, m_noteHeight / 6);
            p.setBrush(c);
            p.drawRoundedRect(noteX, noteY + padding, noteWidth, m_noteHeight - padding * 2, 2, 2);
            
            // Note border
            p.setPen(QPen(c.darker(130), 1));
            p.drawRoundedRect(noteX, noteY + padding, noteWidth, m_noteHeight - padding * 2, 2, 2);
            p.setPen(Qt::NoPen);
        }
    }
    
    // Draw playhead (always in center)
    int playheadX = width() / 2;
    
    // Playhead line
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(playheadX, 0, playheadX, height());
    
    // Playhead triangle at top
    QPainterPath triangle;
    triangle.moveTo(playheadX - 5, 0);
    triangle.lineTo(playheadX + 5, 0);
    triangle.lineTo(playheadX, 8);
    triangle.closeSubpath();
    p.fillPath(triangle, QColor(255, 80, 80));
    
    // Draw time info at bottom
    p.setPen(QColor(120, 120, 125));
    QFont timeFont = p.font();
    timeFont.setPointSize(9);
    p.setFont(timeFont);
    
    double currentSeconds = nn_ticks_to_seconds(m_currentTick, ppq, tempo);
    QString currentTimeStr = QString("%1:%2.%3")
        .arg((int)currentSeconds / 60, 1, 10)
        .arg((int)currentSeconds % 60, 2, 10, QChar('0'))
        .arg((int)(currentSeconds * 10) % 10);
    
    QRect timeRect(playheadX - 30, height() - 18, 60, 16);
    p.fillRect(timeRect, QColor(30, 30, 35, 200));
    p.drawText(timeRect, Qt::AlignCenter, currentTimeStr);
}

// ===================== TrackPreviewWidget =====================

TrackPreviewWidget::TrackPreviewWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine), m_isPlaying(false)
{
    setMinimumHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Create title widget with zoom controls
    m_titleWidget = new QWidget();
    QHBoxLayout *titleLayout = new QHBoxLayout(m_titleWidget);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);
    
    // Add spacer on left side
    titleLayout->addStretch();
    
    // Time zoom buttons (using create_small_button for consistent styling)
    m_btnZoomOutTime = create_small_button(
        ":/icons/zoom-out-horizontal.svg", "Zoom out (time)", "btnZoomOutTime", 20);
    connect(m_btnZoomOutTime, &QPushButton::clicked, this, &TrackPreviewWidget::onZoomOutTime);
    
    m_btnZoomInTime = create_small_button(
        ":/icons/zoom-in-horizontal.svg", "Zoom in (time)", "btnZoomInTime", 20);
    connect(m_btnZoomInTime, &QPushButton::clicked, this, &TrackPreviewWidget::onZoomInTime);
    
    // Pitch zoom buttons
    m_btnZoomOutPitch = create_small_button(
        ":/icons/zoom-out-vertical.svg", "Zoom out (pitch)", "btnZoomOutPitch", 20);
    connect(m_btnZoomOutPitch, &QPushButton::clicked, this, &TrackPreviewWidget::onZoomOutPitch);
    
    m_btnZoomInPitch = create_small_button(
        ":/icons/zoom-in-vertical.svg", "Zoom in (pitch)", "btnZoomInPitch", 20);
    connect(m_btnZoomInPitch, &QPushButton::clicked, this, &TrackPreviewWidget::onZoomInPitch);
    
    titleLayout->addWidget(m_btnZoomOutTime);
    titleLayout->addWidget(m_btnZoomInTime);
    titleLayout->addSpacing(4);
    titleLayout->addWidget(m_btnZoomOutPitch);
    titleLayout->addWidget(m_btnZoomInPitch);
    
    // Main layout with scroll area
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create canvas inside scroll area
    m_canvas = new TrackPreviewCanvas(this);
    
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_canvas);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet("QScrollArea { background: #191920; border: none; }");
    
    mainLayout->addWidget(m_scrollArea);
    
    // Connect signals
    connect(m_engine->getProject(), &NoteNagaProject::activeSequenceChanged,
            this, &TrackPreviewWidget::onSequenceChanged);
    connect(m_engine->getProject(), &NoteNagaProject::currentTickChanged,
            this, &TrackPreviewWidget::onTickChanged);
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::currentTickChanged,
            this, &TrackPreviewWidget::onTickChanged);
    connect(m_engine->getPlaybackWorker(), &NoteNagaPlaybackWorker::playingStateChanged,
            this, &TrackPreviewWidget::onPlayingStateChanged);
    
    // Initialize with current sequence if available
    onSequenceChanged(m_engine->getProject()->getActiveSequence());
}

TrackPreviewWidget::~TrackPreviewWidget()
{
}

void TrackPreviewWidget::onSequenceChanged(NoteNagaMidiSeq *seq)
{
    m_canvas->setSequence(seq);
}

void TrackPreviewWidget::onTickChanged(int tick)
{
    m_canvas->setCurrentTick(tick);
}

void TrackPreviewWidget::onPlayingStateChanged(bool playing)
{
    m_isPlaying = playing;
}

void TrackPreviewWidget::onZoomInTime()
{
    double current = m_canvas->getTimeWindowSeconds();
    m_canvas->setTimeWindowSeconds(qMax(1.0, current * 0.7));
}

void TrackPreviewWidget::onZoomOutTime()
{
    double current = m_canvas->getTimeWindowSeconds();
    m_canvas->setTimeWindowSeconds(qMin(30.0, current * 1.4));
}

void TrackPreviewWidget::onZoomInPitch()
{
    int current = m_canvas->getNoteHeight();
    int newHeight = qMin(24, current + 2);
    if (newHeight == current) return;
    
    // Get current center position as ratio
    QScrollBar *vbar = m_scrollArea->verticalScrollBar();
    int maxVal = vbar->maximum();
    double centerRatio = maxVal > 0 ? (vbar->value() + m_scrollArea->viewport()->height() / 2.0) / (double)(maxVal + m_scrollArea->viewport()->height()) : 0.5;
    
    m_canvas->setNoteHeight(newHeight);
    
    // Restore center position
    int newMax = vbar->maximum();
    int newViewportHeight = m_scrollArea->viewport()->height();
    int newValue = (int)(centerRatio * (newMax + newViewportHeight) - newViewportHeight / 2.0);
    vbar->setValue(qMax(0, qMin(newMax, newValue)));
}

void TrackPreviewWidget::onZoomOutPitch()
{
    int current = m_canvas->getNoteHeight();
    int newHeight = qMax(3, current - 2);
    if (newHeight == current) return;
    
    // Get current center position as ratio
    QScrollBar *vbar = m_scrollArea->verticalScrollBar();
    int maxVal = vbar->maximum();
    double centerRatio = maxVal > 0 ? (vbar->value() + m_scrollArea->viewport()->height() / 2.0) / (double)(maxVal + m_scrollArea->viewport()->height()) : 0.5;
    
    m_canvas->setNoteHeight(newHeight);
    
    // Restore center position
    int newMax = vbar->maximum();
    int newViewportHeight = m_scrollArea->viewport()->height();
    int newValue = (int)(centerRatio * (newMax + newViewportHeight) - newViewportHeight / 2.0);
    vbar->setValue(qMax(0, qMin(newMax, newValue)));
}
