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
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

#include <note_naga_engine/nn_utils.h>
#include "../nn_gui_utils.h"

// ===================== TrackPreviewCanvas =====================

TrackPreviewCanvas::TrackPreviewCanvas(QWidget *parent)
    : QWidget(parent), m_sequence(nullptr), m_currentTick(0),
      m_timeWindowSeconds(5.0), m_noteHeight(8),
      m_lowestNote(21), m_highestNote(108), m_pixelsPerTick(0.05),
      m_showGrid(true), m_showPianoKeys(true), m_colorMode(0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
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
    updateActiveNotes();
    update();
}

void TrackPreviewCanvas::setViewportRect(const QRect &rect)
{
    m_viewportRect = rect;
}

void TrackPreviewCanvas::updateActiveNotes()
{
    m_activeNotes.clear();
    if (!m_sequence) return;
    
    const auto &tracks = m_sequence->getTracks();
    for (const auto *track : tracks) {
        for (const auto &note : track->getNotes()) {
            if (!note.start.has_value()) continue;
            
            int noteStart = note.start.value();
            int noteDuration = note.length.has_value() ? note.length.value() : 100;
            int noteEnd = noteStart + noteDuration;
            
            // Note is active if current tick is within its duration
            if (m_currentTick >= noteStart && m_currentTick < noteEnd) {
                m_activeNotes.insert(note.note);
            }
        }
    }
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

QColor TrackPreviewCanvas::getNoteColor(int trackIndex, int midiNote, int velocity) const
{
    switch (m_colorMode) {
        case 1: {  // Velocity-based coloring
            // Blue (low) -> Green (mid) -> Red (high)
            float t = velocity / 127.0f;
            if (t < 0.5f) {
                float s = t * 2.0f;
                return QColor::fromRgbF(0.0f, s, 1.0f - s);
            } else {
                float s = (t - 0.5f) * 2.0f;
                return QColor::fromRgbF(s, 1.0f - s, 0.0f);
            }
        }
        case 2: {  // Pitch-based coloring (chromatic rainbow)
            int noteInOctave = midiNote % 12;
            float hue = noteInOctave / 12.0f;
            return QColor::fromHsvF(hue, 0.8f, 0.9f);
        }
        default:  // Track-based coloring
            return getTrackColor(trackIndex);
    }
}

void TrackPreviewCanvas::setShowGrid(bool show)
{
    if (m_showGrid != show) {
        m_showGrid = show;
        update();
        emit optionsChanged();
    }
}

void TrackPreviewCanvas::setShowPianoKeys(bool show)
{
    if (m_showPianoKeys != show) {
        m_showPianoKeys = show;
        update();
        emit optionsChanged();
    }
}

void TrackPreviewCanvas::setColorMode(int mode)
{
    if (m_colorMode != mode) {
        m_colorMode = mode;
        update();
        emit optionsChanged();
    }
}

void TrackPreviewCanvas::resetZoom()
{
    m_timeWindowSeconds = 5.0;
    m_noteHeight = 8;
    recalculateSize();
    update();
    emit optionsChanged();
}

void TrackPreviewCanvas::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #2a2d35;
            border: 1px solid #3a3d45;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px;
            color: #e0e0e0;
        }
        QMenu::item:selected {
            background-color: #3a3d45;
        }
        QMenu::separator {
            height: 1px;
            background: #3a3d45;
            margin: 4px 8px;
        }
    )");
    
    // Display options
    QAction *gridAction = menu.addAction(tr("Show Grid"));
    gridAction->setCheckable(true);
    gridAction->setChecked(m_showGrid);
    connect(gridAction, &QAction::toggled, this, &TrackPreviewCanvas::setShowGrid);
    
    QAction *pianoAction = menu.addAction(tr("Show Piano Keys"));
    pianoAction->setCheckable(true);
    pianoAction->setChecked(m_showPianoKeys);
    connect(pianoAction, &QAction::toggled, this, &TrackPreviewCanvas::setShowPianoKeys);
    
    menu.addSeparator();
    
    // Color mode submenu
    QMenu *colorMenu = menu.addMenu(tr("Color Mode"));
    QActionGroup *colorGroup = new QActionGroup(colorMenu);
    
    QAction *trackColorAction = colorMenu->addAction(tr("By Track"));
    trackColorAction->setCheckable(true);
    trackColorAction->setChecked(m_colorMode == 0);
    trackColorAction->setData(0);
    colorGroup->addAction(trackColorAction);
    
    QAction *velocityColorAction = colorMenu->addAction(tr("By Velocity"));
    velocityColorAction->setCheckable(true);
    velocityColorAction->setChecked(m_colorMode == 1);
    velocityColorAction->setData(1);
    colorGroup->addAction(velocityColorAction);
    
    QAction *pitchColorAction = colorMenu->addAction(tr("By Pitch"));
    pitchColorAction->setCheckable(true);
    pitchColorAction->setChecked(m_colorMode == 2);
    pitchColorAction->setData(2);
    colorGroup->addAction(pitchColorAction);
    
    connect(colorGroup, &QActionGroup::triggered, [this](QAction *action) {
        setColorMode(action->data().toInt());
    });
    
    menu.addSeparator();
    
    QAction *resetAction = menu.addAction(tr("Reset Zoom"));
    connect(resetAction, &QAction::triggered, this, &TrackPreviewCanvas::resetZoom);
    
    menu.exec(event->globalPos());
}

void TrackPreviewCanvas::drawPianoKeys(QPainter &p, int pianoKeyWidth, int viewTop, int viewBottom)
{
    // Calculate visible note range based on viewport
    int firstVisibleNote = m_highestNote - (viewBottom / m_noteHeight);
    int lastVisibleNote = m_highestNote - (viewTop / m_noteHeight) + 1;
    firstVisibleNote = qMax(m_lowestNote, firstVisibleNote);
    lastVisibleNote = qMin(m_highestNote, lastVisibleNote);
    
    for (int note = firstVisibleNote; note <= lastVisibleNote; ++note) {
        int noteIndex = m_highestNote - note;
        int y = noteIndex * m_noteHeight;
        
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || 
                           noteInOctave == 8 || noteInOctave == 10);
        
        // Check if this note is currently being played
        bool isActive = m_activeNotes.contains(note);
        
        // Piano key background - highlight active notes
        QColor keyColor;
        if (isActive) {
            keyColor = QColor(80, 120, 180);  // Blue highlight for active
        } else {
            keyColor = isBlackKey ? QColor(35, 35, 40) : QColor(55, 55, 60);
        }
        p.fillRect(0, y, pianoKeyWidth, m_noteHeight, keyColor);
        
        // Key border
        p.setPen(QColor(25, 25, 30));
        p.drawLine(0, y + m_noteHeight - 1, pianoKeyWidth, y + m_noteHeight - 1);
        
        // Note name for C notes (only if height allows)
        if (noteInOctave == 0 && m_noteHeight >= 10) {
            int octave = (note / 12) - 1;
            p.setPen(isActive ? QColor(255, 255, 255) : QColor(140, 140, 145));
            QFont font = p.font();
            font.setPixelSize(qMin(m_noteHeight - 2, 11));
            font.setBold(true);
            p.setFont(font);
            p.drawText(QRect(2, y, pianoKeyWidth - 4, m_noteHeight), 
                       Qt::AlignVCenter | Qt::AlignLeft, 
                       QString("C%1").arg(octave));
        }
    }
    
    // Right border of piano keys
    p.setPen(QColor(60, 60, 65));
    p.drawLine(pianoKeyWidth - 1, viewTop, pianoKeyWidth - 1, viewBottom);
}

void TrackPreviewCanvas::drawGrid(QPainter &p, int startTick, int endTick, int ppq, int offsetX, int viewTop, int viewBottom)
{
    // Calculate measure interval (only draw measure lines, not every beat)
    int beatsPerMeasure = 4;  // Assume 4/4 time
    int ticksPerBeat = ppq;
    int ticksPerMeasure = ticksPerBeat * beatsPerMeasure;
    
    // Find first visible measure
    int firstMeasure = (startTick / ticksPerMeasure) * ticksPerMeasure;
    if (firstMeasure < startTick) firstMeasure += ticksPerMeasure;
    
    p.setRenderHint(QPainter::Antialiasing, false);
    
    // Draw only measure lines (not every beat) for cleaner look
    for (int tick = firstMeasure; tick <= endTick; tick += ticksPerMeasure) {
        int x = offsetX + static_cast<int>((tick - startTick) * m_pixelsPerTick);
        p.setPen(QPen(QColor(55, 55, 60), 1));
        p.drawLine(x, viewTop, x, viewBottom);
    }
    
    p.setRenderHint(QPainter::Antialiasing, true);
}

void TrackPreviewCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    
    // Get viewport bounds for optimized rendering
    int viewTop = m_viewportRect.isValid() ? m_viewportRect.top() : 0;
    int viewBottom = m_viewportRect.isValid() ? m_viewportRect.bottom() : height();
    int viewHeight = viewBottom - viewTop;
    
    // Background - only fill visible area
    p.fillRect(0, viewTop, width(), viewHeight, QColor(20, 20, 26));
    
    if (!m_sequence) {
        p.setPen(QColor(100, 100, 105));
        p.drawText(rect(), Qt::AlignCenter, tr("No sequence loaded"));
        return;
    }
    
    const auto &tracks = m_sequence->getTracks();
    if (tracks.empty()) {
        p.setPen(QColor(100, 100, 105));
        p.drawText(rect(), Qt::AlignCenter, tr("No tracks"));
        return;
    }
    
    // Calculate time window in ticks
    int ppq = m_sequence->getPPQ();
    double tempo = m_sequence->getTempo();
    int windowTicksHalf = nn_seconds_to_ticks(m_timeWindowSeconds, ppq, tempo);
    
    // Piano keys width
    int pianoKeyWidth = m_showPianoKeys ? 32 : 0;
    int rollWidth = width() - pianoKeyWidth;
    
    // Recalculate pixels per tick based on current width
    int windowTicks = windowTicksHalf * 2;
    if (windowTicks > 0 && rollWidth > 0) {
        m_pixelsPerTick = static_cast<double>(rollWidth) / static_cast<double>(windowTicks);
    }
    
    // Visible tick range (current position ± window)
    int startTick = m_currentTick - windowTicksHalf;
    int endTick = m_currentTick + windowTicksHalf;
    
    // Calculate visible note range based on viewport
    int firstVisibleNote = m_highestNote - (viewBottom / m_noteHeight) - 1;
    int lastVisibleNote = m_highestNote - (viewTop / m_noteHeight) + 1;
    firstVisibleNote = qMax(m_lowestNote, firstVisibleNote);
    lastVisibleNote = qMin(m_highestNote, lastVisibleNote);
    
    // Draw piano keys (left side) - only visible rows
    if (m_showPianoKeys) {
        drawPianoKeys(p, pianoKeyWidth, viewTop, viewBottom);
    }
    
    // Set clipping for piano roll area
    p.setClipRect(pianoKeyWidth, viewTop, rollWidth, viewHeight);
    
    // Draw piano roll background rows - only visible ones
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int note = firstVisibleNote; note <= lastVisibleNote; ++note) {
        int noteIndex = m_highestNote - note;
        int y = noteIndex * m_noteHeight;
        
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || 
                           noteInOctave == 8 || noteInOctave == 10);
        
        QColor bgColor = isBlackKey ? QColor(20, 20, 26) : QColor(28, 28, 34);
        p.fillRect(pianoKeyWidth, y, rollWidth, m_noteHeight, bgColor);
        
        // Draw octave lines (C notes only)
        if (noteInOctave == 0) {
            p.setPen(QPen(QColor(50, 50, 56), 1));
            p.drawLine(pianoKeyWidth, y, width(), y);
        }
    }
    
    // Draw grid lines - only visible area
    if (m_showGrid) {
        drawGrid(p, startTick, endTick, ppq, pianoKeyWidth, viewTop, viewBottom);
    }
    
    // Collect only visible notes
    struct NoteData {
        int trackIdx;
        int noteStart;
        int noteDuration;
        int midiNote;
        int velocity;
    };
    std::vector<NoteData> visibleNotes;
    visibleNotes.reserve(256);  // Pre-allocate for performance
    
    for (int trackIdx = 0; trackIdx < static_cast<int>(tracks.size()); ++trackIdx) {
        NoteNagaTrack *track = tracks[trackIdx];
        for (const auto &note : track->getNotes()) {
            if (!note.start.has_value()) continue;
            
            int noteStart = note.start.value();
            int noteDuration = note.length.has_value() ? note.length.value() : 100;
            int noteEnd = noteStart + noteDuration;
            int midiNote = note.note;
            
            // Skip notes outside visible time range
            if (noteEnd < startTick || noteStart > endTick) continue;
            
            // Skip notes outside visible pitch range
            if (midiNote < firstVisibleNote || midiNote > lastVisibleNote) continue;
            
            int vel = note.velocity.has_value() ? note.velocity.value() : 90;
            visibleNotes.push_back({trackIdx, noteStart, noteDuration, midiNote, vel});
        }
    }
    
    // Sort by start time
    std::sort(visibleNotes.begin(), visibleNotes.end(), 
              [](const NoteData &a, const NoteData &b) { return a.noteStart < b.noteStart; });
    
    // Draw notes with optimized rendering
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    
    for (const auto &nd : visibleNotes) {
        int noteX = pianoKeyWidth + static_cast<int>((nd.noteStart - startTick) * m_pixelsPerTick);
        int noteWidth = qMax(3, static_cast<int>(nd.noteDuration * m_pixelsPerTick));
        
        // Clip to view area
        if (noteX < pianoKeyWidth) {
            noteWidth -= (pianoKeyWidth - noteX);
            noteX = pianoKeyWidth;
        }
        if (noteX + noteWidth > width()) {
            noteWidth = width() - noteX;
        }
        if (noteWidth <= 0) continue;
        
        int noteIndex = m_highestNote - nd.midiNote;
        int noteY = noteIndex * m_noteHeight;
        
        // Get note color
        QColor baseColor = getNoteColor(nd.trackIdx, nd.midiNote, nd.velocity);
        int brightness = 95 + (nd.velocity * 25 / 127);
        QColor noteColor = baseColor.lighter(brightness);
        
        // Calculate note rectangle
        int padding = qMax(1, m_noteHeight / 5);
        int noteH = m_noteHeight - padding * 2;
        int noteYPadded = noteY + padding;
        
        // Simple gradient for 3D effect
        QLinearGradient noteGrad(0, noteYPadded, 0, noteYPadded + noteH);
        noteGrad.setColorAt(0, noteColor.lighter(110));
        noteGrad.setColorAt(1, noteColor.darker(110));
        
        p.setBrush(noteGrad);
        int cornerRadius = qMin(2, noteH / 3);
        p.drawRoundedRect(noteX, noteYPadded, noteWidth, noteH, cornerRadius, cornerRadius);
        
        // Border
        p.setPen(QPen(noteColor.darker(130), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(noteX, noteYPadded, noteWidth, noteH, cornerRadius, cornerRadius);
        p.setPen(Qt::NoPen);
    }
    
    // Remove clipping for playhead overlay
    p.setClipping(false);
    
    // Playhead X position (center of roll area)
    int playheadX = pianoKeyWidth + rollWidth / 2;
    
    // Draw playhead line - full height
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(playheadX, 0, playheadX, height());
    
    // Draw playhead triangle at viewport top (always visible)
    int triangleY = viewTop;
    QPainterPath triangle;
    triangle.moveTo(playheadX - 6, triangleY);
    triangle.lineTo(playheadX + 6, triangleY);
    triangle.lineTo(playheadX, triangleY + 10);
    triangle.closeSubpath();
    
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 80, 80));
    p.drawPath(triangle);
    
    // Draw time display at viewport bottom (always visible)
    double currentSeconds = nn_ticks_to_seconds(m_currentTick, ppq, tempo);
    QString currentTimeStr = QString("%1:%2.%3")
        .arg(static_cast<int>(currentSeconds) / 60, 1, 10)
        .arg(static_cast<int>(currentSeconds) % 60, 2, 10, QChar('0'))
        .arg(static_cast<int>(currentSeconds * 10) % 10);
    
    QFont timeFont = p.font();
    timeFont.setPixelSize(11);
    timeFont.setBold(true);
    p.setFont(timeFont);
    
    QFontMetrics fm(timeFont);
    int textWidth = fm.horizontalAdvance(currentTimeStr) + 12;
    
    int timeY = viewBottom - 20;
    QRect timeRect(playheadX - textWidth / 2, timeY, textWidth, 18);
    
    p.setBrush(QColor(30, 30, 36, 230));
    p.drawRoundedRect(timeRect, 4, 4);
    
    p.setPen(QColor(220, 220, 225));
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
    
    // Connect scroll area to update viewport rect
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, 
            this, &TrackPreviewWidget::updateViewportRect);
    
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

void TrackPreviewWidget::updateViewportRect()
{
    QRect viewportRect = m_scrollArea->viewport()->rect();
    viewportRect.translate(0, m_scrollArea->verticalScrollBar()->value());
    m_canvas->setViewportRect(viewportRect);
}

void TrackPreviewWidget::onSequenceChanged(NoteNagaMidiSeq *seq)
{
    m_canvas->setSequence(seq);
    updateViewportRect();
}

void TrackPreviewWidget::onTickChanged(int tick)
{
    updateViewportRect();
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
