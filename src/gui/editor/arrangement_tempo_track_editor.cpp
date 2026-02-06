#include "arrangement_tempo_track_editor.h"
#include "arrangement_timeline_widget.h"

#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QHBoxLayout>

ArrangementTempoTrackEditor::ArrangementTempoTrackEditor(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    setMinimumHeight(40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMouseTracking(true);

    // Create toggle button (square 1:1)
    m_toggleButton = new QPushButton("▼", this);
    m_toggleButton->setToolTip(tr("Toggle tempo track visibility"));
    m_toggleButton->setStyleSheet(R"(
        QPushButton {
            background: #333;
            border: 1px solid #555;
            border-radius: 3px;
            color: #ccc;
            font-size: 10px;
            min-width: 18px;
            max-width: 18px;
            min-height: 18px;
            max-height: 18px;
            padding: 0px;
        }
        QPushButton:hover {
            background: #444;
            color: #fff;
        }
    )");
    connect(m_toggleButton, &QPushButton::clicked, this, [this]() {
        setExpanded(!m_expanded);
    });

    // Create BPM label
    m_bpmLabel = new QLabel("120.0", this);
    m_bpmLabel->setStyleSheet("color: #ff9800; font-size: 11px; font-weight: bold;");
    m_bpmLabel->setAlignment(Qt::AlignCenter);
    
    // Create import button
    m_importButton = new QPushButton("⬇", this);
    m_importButton->setToolTip(tr("Import tempo track from active MIDI sequence"));
    m_importButton->setStyleSheet(R"(
        QPushButton {
            background: #2a4a2a;
            border: 1px solid #4a6a4a;
            border-radius: 3px;
            color: #8c8;
            font-size: 10px;
            min-width: 18px;
            max-width: 18px;
            min-height: 18px;
            max-height: 18px;
            padding: 0px;
        }
        QPushButton:hover {
            background: #3a5a3a;
            color: #afa;
        }
    )");
    connect(m_importButton, &QPushButton::clicked, this, &ArrangementTempoTrackEditor::importTempoFromSequence);
    
    // Create active indicator LED
    m_activeIndicator = new QLabel(this);
    m_activeIndicator->setFixedSize(10, 10);
    updateActiveIndicator();

    // Listen to arrangement tempo track changes
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
        if (arrangement) {
            connect(arrangement, &NoteNagaArrangement::tempoTrackChanged, 
                    this, &ArrangementTempoTrackEditor::onTempoEventsChanged);
        }
        
        // Listen for tempo changes during playback
        connect(m_engine->getRuntimeData(), &NoteNagaRuntimeData::currentTempoChanged,
                this, &ArrangementTempoTrackEditor::onCurrentTempoChanged);
    }
    
    // Initial visibility check
    updateVisibility();
}

ArrangementTempoTrackEditor::~ArrangementTempoTrackEditor() = default;

void ArrangementTempoTrackEditor::setTimelineWidget(ArrangementTimelineWidget *timeline)
{
    m_timeline = timeline;
    if (m_timeline) {
        m_pixelsPerTick = m_timeline->getPixelsPerTick();
        m_horizontalOffset = m_timeline->getHorizontalOffset();
    }
}

void ArrangementTempoTrackEditor::setExpanded(bool expanded)
{
    if (m_expanded == expanded) return;
    m_expanded = expanded;
    
    if (m_expanded) {
        setMaximumHeight(16777215);  // QWIDGETSIZE_MAX - allow splitter to resize
        setMinimumHeight(40);
        m_toggleButton->setText("▼");
    } else {
        // Keep minimum height for toggle button visibility
        setMaximumHeight(24);
        setMinimumHeight(24);
        m_toggleButton->setText("▶");
    }
    
    emit expandedChanged(m_expanded);
    update();
}

void ArrangementTempoTrackEditor::setHorizontalOffset(int offset)
{
    m_horizontalOffset = offset;
    update();
}

void ArrangementTempoTrackEditor::setPixelsPerTick(double ppTick)
{
    m_pixelsPerTick = ppTick;
    update();
}

void ArrangementTempoTrackEditor::setHeaderWidth(int width)
{
    m_headerWidth = width;
    update();
}

void ArrangementTempoTrackEditor::setPlayheadTick(int64_t tick)
{
    m_playheadTick = tick;
    update();
}

void ArrangementTempoTrackEditor::refresh()
{
    update();
}

void ArrangementTempoTrackEditor::onTempoEventsChanged()
{
    updateActiveIndicator();
    updateVisibility();
    update();
}

void ArrangementTempoTrackEditor::onCurrentTempoChanged(double bpm)
{
    m_currentBpm = bpm;
    if (m_bpmLabel) {
        m_bpmLabel->setText(QString::number(bpm, 'f', 1));
    }
    update();
}

void ArrangementTempoTrackEditor::importTempoFromSequence()
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaMidiSeq* activeSeq = m_engine->getRuntimeData()->getActiveSequence();
    if (!activeSeq) {
        QMessageBox::warning(this, tr("Import Tempo Track"), 
                             tr("No active MIDI sequence. Please select a sequence first."));
        return;
    }
    
    NoteNagaTrack* seqTempoTrack = activeSeq->getTempoTrack();
    if (!seqTempoTrack || seqTempoTrack->getTempoEvents().empty()) {
        QMessageBox::warning(this, tr("Import Tempo Track"), 
                             tr("The active MIDI sequence does not have a tempo track."));
        return;
    }
    
    const auto& seqEvents = seqTempoTrack->getTempoEvents();
    
    // Ask for confirmation
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, 
        tr("Import Tempo Track"),
        tr("Import tempo track from the active sequence?\n\n"
           "This will replace the current arrangement tempo track with %1 tempo event(s) from the MIDI sequence.")
            .arg(seqEvents.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Import tempo events
    NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    // Create tempo track if it doesn't exist
    if (!arrangement->hasTempoTrack()) {
        arrangement->createTempoTrack(seqEvents.front().bpm);
    }
    
    NoteNagaTrack* arrTempoTrack = arrangement->getTempoTrack();
    if (arrTempoTrack) {
        arrTempoTrack->setTempoEvents(seqEvents);
        arrTempoTrack->setTempoTrackActive(true);
        emit arrangement->tempoTrackChanged();
    }
    
    update();
}

// Coordinate conversion
int ArrangementTempoTrackEditor::tickToX(int tick) const
{
    return m_headerWidth + static_cast<int>(tick * m_pixelsPerTick) - m_horizontalOffset;
}

int ArrangementTempoTrackEditor::xToTick(int x) const
{
    return static_cast<int>((x - m_headerWidth + m_horizontalOffset) / m_pixelsPerTick);
}

double ArrangementTempoTrackEditor::bpmToY(double bpm) const
{
    // Map BPM range to widget height (inverted - higher BPM = higher on screen)
    double normalized = (bpm - MIN_BPM) / (MAX_BPM - MIN_BPM);
    normalized = qBound(0.0, normalized, 1.0);
    int margin = 8;
    int availableHeight = height() - 2 * margin;
    return height() - margin - static_cast<int>(normalized * availableHeight);
}

double ArrangementTempoTrackEditor::yToBpm(int y) const
{
    int margin = 8;
    int availableHeight = height() - 2 * margin;
    double normalized = 1.0 - static_cast<double>(y - margin) / availableHeight;
    normalized = qBound(0.0, normalized, 1.0);
    return MIN_BPM + normalized * (MAX_BPM - MIN_BPM);
}

int ArrangementTempoTrackEditor::hitTestTempoEvent(const QPoint &pos) const
{
    if (!m_engine || !m_engine->getRuntimeData()) return -1;
    
    NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement || !arrangement->hasTempoTrack()) return -1;
    
    NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
    if (!tempoTrack) return -1;
    
    const auto& events = tempoTrack->getTempoEvents();
    const int hitRadius = 8;
    
    for (size_t i = 0; i < events.size(); ++i) {
        int x = tickToX(events[i].tick);
        int y = static_cast<int>(bpmToY(events[i].bpm));
        
        int dx = pos.x() - x;
        int dy = pos.y() - y;
        
        if (dx * dx + dy * dy <= hitRadius * hitRadius) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

void ArrangementTempoTrackEditor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawBackground(painter);
    drawGrid(painter);
    drawTempoCurve(painter);
    drawTempoPoints(painter);
    drawPlayhead(painter);
    drawHeaderLabel(painter);
}

void ArrangementTempoTrackEditor::drawBackground(QPainter &painter)
{
    // Header area (left side matching track headers)
    painter.fillRect(0, 0, m_headerWidth, height(), QColor("#252530"));
    
    // Main area (timeline)
    painter.fillRect(m_headerWidth, 0, width() - m_headerWidth, height(), QColor("#1a1a20"));
    
    // Border between header and timeline
    painter.setPen(QColor("#3a3a42"));
    painter.drawLine(m_headerWidth, 0, m_headerWidth, height());
    painter.drawLine(0, height() - 1, width(), height() - 1);
    
    // Draw info in header area (below controls)
    if (height() > 40) {
        painter.setPen(QColor("#666"));
        painter.setFont(QFont("Arial", 8));
        
        // Count tempo events
        int eventCount = 0;
        if (m_engine && m_engine->getRuntimeData()) {
            NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
            if (arrangement && arrangement->hasTempoTrack()) {
                eventCount = static_cast<int>(arrangement->getTempoTrack()->getTempoEvents().size());
            }
        }
        
        QString info = QString("%1 %2").arg(eventCount).arg(eventCount == 1 ? "event" : "events");
        painter.drawText(QRect(5, height() - 20, m_headerWidth - 10, 16), Qt::AlignLeft, info);
    }
}

void ArrangementTempoTrackEditor::drawGrid(QPainter &painter)
{
    // Draw horizontal BPM reference lines
    painter.setPen(QPen(QColor("#2a2a35"), 1, Qt::DotLine));
    
    QVector<double> bpmLines = {60, 90, 120, 150, 180, 240};
    for (double bpm : bpmLines) {
        int y = static_cast<int>(bpmToY(bpm));
        if (y > 5 && y < height() - 5) {
            painter.drawLine(m_headerWidth, y, width(), y);
        }
    }
}

void ArrangementTempoTrackEditor::drawTempoCurve(QPainter &painter)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement || !arrangement->hasTempoTrack()) return;
    
    NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
    if (!tempoTrack) return;
    
    const auto& events = tempoTrack->getTempoEvents();
    if (events.empty()) return;
    
    // Draw tempo curve
    bool isActive = tempoTrack->isTempoTrackActive();
    QColor curveColor = isActive ? QColor("#ff9800") : QColor("#666666");
    painter.setPen(QPen(curveColor, 2));
    
    for (size_t i = 0; i < events.size(); ++i) {
        int x1 = tickToX(events[i].tick);
        int y1 = static_cast<int>(bpmToY(events[i].bpm));
        
        // Draw curve to next event or end of visible area
        int x2;
        int y2;
        if (i + 1 < events.size()) {
            x2 = tickToX(events[i + 1].tick);
            y2 = static_cast<int>(bpmToY(events[i + 1].bpm));
        } else {
            x2 = width();
            y2 = y1;
        }
        
        if (x2 > m_headerWidth && x1 < width()) {
            x1 = qMax(x1, m_headerWidth);
            
            if (events[i].interpolation == TempoInterpolation::Linear && i + 1 < events.size()) {
                // Linear: direct diagonal line to next point
                painter.drawLine(x1, y1, x2, y2);
            } else {
                // Step: horizontal then vertical
                painter.drawLine(x1, y1, x2, y1);
                if (i + 1 < events.size()) {
                    painter.drawLine(x2, y1, x2, y2);
                }
            }
        }
    }
}

void ArrangementTempoTrackEditor::drawTempoPoints(QPainter &painter)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement || !arrangement->hasTempoTrack()) return;
    
    NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
    if (!tempoTrack) return;
    
    const auto& events = tempoTrack->getTempoEvents();
    bool isActive = tempoTrack->isTempoTrackActive();
    
    for (size_t i = 0; i < events.size(); ++i) {
        int x = tickToX(events[i].tick);
        int y = static_cast<int>(bpmToY(events[i].bpm));
        
        // Skip if not visible
        if (x < m_headerWidth - 10 || x > width() + 10) continue;
        
        // Draw point
        int radius = (static_cast<int>(i) == m_hoveredEventIndex) ? 7 : 5;
        QColor pointColor = isActive ? QColor("#ff9800") : QColor("#666666");
        
        if (static_cast<int>(i) == m_dragEventIndex) {
            pointColor = QColor("#ffcc00");
            radius = 8;
        }
        
        painter.setBrush(pointColor);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(QPoint(x, y), radius, radius);
        
        // Draw BPM value label near point
        if (static_cast<int>(i) == m_hoveredEventIndex || static_cast<int>(i) == m_dragEventIndex) {
            QString label = QString::number(events[i].bpm, 'f', 1);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 9));
            painter.drawText(x + 10, y - 5, label);
        }
        
        // Draw interpolation indicator (icon below point)
        // Check if there's enough space (skip if next point is too close)
        bool hasSpace = true;
        for (size_t j = 0; j < events.size(); ++j) {
            if (j != i) {
                int otherX = tickToX(events[j].tick);
                int dist = std::abs(otherX - x);
                if (dist < 30 && dist > 0) {
                    hasSpace = false;
                    break;
                }
            }
        }
        
        if (hasSpace && y + radius + 20 < height()) {
            int iconY = y + radius + 8;
            int iconSize = 12;
            
            // Draw frame background
            QRect iconRect(x - iconSize/2 - 2, iconY - 2, iconSize + 4, iconSize + 4);
            painter.setPen(QPen(QColor(60, 60, 70), 1));
            painter.setBrush(QColor(40, 43, 50, 200));
            painter.drawRoundedRect(iconRect, 2, 2);
            
            // Draw symbol
            painter.setPen(QPen(pointColor, 2));
            if (events[i].interpolation == TempoInterpolation::Linear) {
                // Diagonal line indicating linear interpolation
                painter.drawLine(x - iconSize/2, iconY + iconSize/2, 
                                x + iconSize/2, iconY - iconSize/2 + 4);
            } else {
                // Step indicator
                painter.drawLine(x - iconSize/2, iconY + iconSize/2, x, iconY + iconSize/2);
                painter.drawLine(x, iconY + iconSize/2, x, iconY);
                painter.drawLine(x, iconY, x + iconSize/2, iconY);
            }
        }
    }
}

void ArrangementTempoTrackEditor::drawPlayhead(QPainter &painter)
{
    int x = tickToX(m_playheadTick);
    if (x < m_headerWidth || x > width()) return;
    
    painter.setPen(QPen(QColor("#00ff00"), 1));
    painter.drawLine(x, 0, x, height());
}

void ArrangementTempoTrackEditor::drawHeaderLabel(QPainter &painter)
{
    // Position toggle button at top-left
    m_toggleButton->move(5, 5);
    m_toggleButton->raise();  // Ensure button is on top
    
    // Position active indicator LED next to toggle button
    m_activeIndicator->move(28, 9);
    
    // Position import button next to LED
    m_importButton->move(45, 5);
    m_importButton->raise();
    
    // Position BPM label at top-right corner of header area
    m_bpmLabel->setGeometry(m_headerWidth - 55, 5, 50, 18);
    
    updateActiveIndicator();
}

void ArrangementTempoTrackEditor::updateActiveIndicator()
{
    bool isActive = false;
    bool hasTempoTrack = false;
    
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
        if (arrangement && arrangement->hasTempoTrack()) {
            hasTempoTrack = true;
            NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
            if (tempoTrack) {
                isActive = tempoTrack->isTempoTrackActive();
            }
        }
    }
    
    if (!hasTempoTrack) {
        m_activeIndicator->setStyleSheet(R"(
            QLabel {
                background-color: #444;
                border-radius: 5px;
            }
        )");
        m_activeIndicator->setToolTip(tr("No tempo track"));
    } else if (isActive) {
        m_activeIndicator->setStyleSheet(R"(
            QLabel {
                background-color: #4CAF50;
                border-radius: 5px;
            }
        )");
        m_activeIndicator->setToolTip(tr("Tempo track active"));
    } else {
        m_activeIndicator->setStyleSheet(R"(
            QLabel {
                background-color: #ff5722;
                border-radius: 5px;
            }
        )");
        m_activeIndicator->setToolTip(tr("Tempo track disabled"));
    }
}

void ArrangementTempoTrackEditor::updateVisibility()
{
    bool hasTempoTrack = false;
    
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
        if (arrangement && arrangement->hasTempoTrack()) {
            hasTempoTrack = true;
        }
    }
    
    bool shouldBeVisible = hasTempoTrack;
    if (isVisible() != shouldBeVisible) {
        setVisible(shouldBeVisible);
        emit visibilityChanged(shouldBeVisible);
    }
}

void ArrangementTempoTrackEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().x() > m_headerWidth) {
        int eventIdx = hitTestTempoEvent(event->pos());
        
        if (eventIdx >= 0) {
            // Start dragging tempo point
            m_dragEventIndex = eventIdx;
            m_isDragging = true;
            m_dragStartPos = event->pos();
            
            NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
            if (arrangement && arrangement->hasTempoTrack()) {
                const auto& events = arrangement->getTempoTrack()->getTempoEvents();
                if (eventIdx < static_cast<int>(events.size())) {
                    m_dragStartBpm = events[eventIdx].bpm;
                    m_dragStartTick = events[eventIdx].tick;
                }
            }
            update();
        } else {
            // Left click on empty area - add new tempo point
            int tick = xToTick(event->pos().x());
            double bpm = yToBpm(event->pos().y());
            bpm = qBound(MIN_BPM, bpm, MAX_BPM);
            
            NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
            if (arrangement) {
                if (!arrangement->hasTempoTrack()) {
                    double projectBpm = 120.0;
                    int projectTempo = m_engine->getRuntimeData()->getTempo();
                    if (projectTempo > 0) {
                        projectBpm = 60'000'000.0 / projectTempo;
                    }
                    arrangement->createTempoTrack(projectBpm);
                }
                
                NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
                if (tempoTrack) {
                    // Snap to grid
                    int snapResolution = 480;
                    tick = (tick / snapResolution) * snapResolution;
                    tick = qMax(1, tick);  // Don't add at tick 0
                    
                    NN_TempoEvent_t newEvent(tick, bpm, TempoInterpolation::Step);
                    tempoTrack->addTempoEvent(newEvent);
                    update();
                }
            }
        }
    }
    
    QWidget::mousePressEvent(event);
}

void ArrangementTempoTrackEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && m_dragEventIndex >= 0) {
        // Drag to change BPM and/or tick position
        double newBpm = yToBpm(event->pos().y());
        newBpm = qBound(MIN_BPM, newBpm, MAX_BPM);
        
        int newTick = xToTick(event->pos().x());
        newTick = qMax(0, newTick);
        
        NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
        if (arrangement && arrangement->hasTempoTrack()) {
            NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
            auto events = tempoTrack->getTempoEvents();
            if (m_dragEventIndex < static_cast<int>(events.size())) {
                // First event (tick 0) cannot be moved horizontally
                if (m_dragEventIndex == 0) {
                    newTick = 0;
                } else {
                    // Constrain tick to be between previous and next events
                    int minTick = 0;
                    int maxTick = INT_MAX;
                    
                    if (m_dragEventIndex > 0) {
                        minTick = events[m_dragEventIndex - 1].tick + 1;
                    }
                    if (m_dragEventIndex + 1 < static_cast<int>(events.size())) {
                        maxTick = events[m_dragEventIndex + 1].tick - 1;
                    }
                    
                    newTick = qBound(minTick, newTick, maxTick);
                }
                
                events[m_dragEventIndex].bpm = newBpm;
                events[m_dragEventIndex].tick = newTick;
                tempoTrack->setTempoEvents(events);
                m_currentBpm = newBpm;
                m_bpmLabel->setText(QString::number(newBpm, 'f', 1));
            }
        }
        
        update();
    } else {
        // Hover detection
        int newHovered = hitTestTempoEvent(event->pos());
        if (newHovered != m_hoveredEventIndex) {
            m_hoveredEventIndex = newHovered;
            setCursor(newHovered >= 0 ? Qt::SizeAllCursor : Qt::ArrowCursor);
            update();
        }
    }
    
    QWidget::mouseMoveEvent(event);
}

void ArrangementTempoTrackEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        m_isDragging = false;
        m_dragEventIndex = -1;
        update();
    }
    
    // Right click on tempo point - delete it (except first point)
    if (event->button() == Qt::RightButton && event->pos().x() > m_headerWidth) {
        int eventIdx = hitTestTempoEvent(event->pos());
        if (eventIdx > 0) {  // Can't delete first event (index 0)
            NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
            if (arrangement && arrangement->hasTempoTrack()) {
                NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
                const auto& events = tempoTrack->getTempoEvents();
                if (eventIdx < static_cast<int>(events.size())) {
                    tempoTrack->removeTempoEventAtTick(events[eventIdx].tick);
                    update();
                    return;  // Don't pass to context menu
                }
            }
        }
    }
    
    QWidget::mouseReleaseEvent(event);
}

void ArrangementTempoTrackEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().x() > m_headerWidth) {
        // Double click to add new tempo event
        int tick = xToTick(event->pos().x());
        double bpm = yToBpm(event->pos().y());
        bpm = qBound(MIN_BPM, bpm, MAX_BPM);
        
        NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
        if (arrangement) {
            if (!arrangement->hasTempoTrack()) {
                // Create tempo track if it doesn't exist
                double projectBpm = 120.0;
                int projectTempo = m_engine->getRuntimeData()->getTempo();
                if (projectTempo > 0) {
                    projectBpm = 60'000'000.0 / projectTempo;
                }
                arrangement->createTempoTrack(projectBpm);
            }
            
            NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
            if (tempoTrack) {
                // Snap to grid
                int snapResolution = 480; // Quarter note
                tick = (tick / snapResolution) * snapResolution;
                
                NN_TempoEvent_t newEvent(tick, bpm, TempoInterpolation::Step);
                tempoTrack->addTempoEvent(newEvent);
            }
        }
        
        update();
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void ArrangementTempoTrackEditor::wheelEvent(QWheelEvent *event)
{
    // Forward to parent for zoom handling
    event->ignore();
}

void ArrangementTempoTrackEditor::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;
    
    QMenu menu(this);
    
    int eventIdx = hitTestTempoEvent(event->pos());
    
    if (eventIdx >= 0 && arrangement->hasTempoTrack()) {
        // Right click on a tempo event
        NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
        const auto& events = tempoTrack->getTempoEvents();
        
        if (eventIdx < static_cast<int>(events.size())) {
            QAction *editAction = menu.addAction(tr("Edit BPM..."));
            connect(editAction, &QAction::triggered, this, [this, eventIdx, tempoTrack]() {
                auto events = tempoTrack->getTempoEvents();
                if (eventIdx < static_cast<int>(events.size())) {
                    bool ok;
                    double bpm = QInputDialog::getDouble(this, tr("Edit Tempo"), tr("BPM:"),
                                                          events[eventIdx].bpm, MIN_BPM, MAX_BPM, 1, &ok);
                    if (ok) {
                        events[eventIdx].bpm = bpm;
                        tempoTrack->setTempoEvents(events);
                        update();
                    }
                }
            });
            
            // Toggle interpolation for this point
            TempoInterpolation currentInterp = events[eventIdx].interpolation;
            QString interpText = currentInterp == TempoInterpolation::Step ?
                tr("Set Linear Interpolation") : tr("Set Step Interpolation");
            QAction *interpAction = menu.addAction(interpText);
            connect(interpAction, &QAction::triggered, this, [this, eventIdx, tempoTrack]() {
                auto events = tempoTrack->getTempoEvents();
                if (eventIdx < static_cast<int>(events.size())) {
                    events[eventIdx].interpolation = 
                        (events[eventIdx].interpolation == TempoInterpolation::Step) ?
                            TempoInterpolation::Linear : TempoInterpolation::Step;
                    tempoTrack->setTempoEvents(events);
                    update();
                }
            });
            
            // Don't allow deleting first event
            if (eventIdx > 0) {
                QAction *deleteAction = menu.addAction(tr("Delete"));
                connect(deleteAction, &QAction::triggered, this, [this, eventIdx, tempoTrack]() {
                    tempoTrack->removeTempoEventAtTick(tempoTrack->getTempoEvents()[eventIdx].tick);
                    update();
                });
            }
        }
    } else if (event->pos().x() > m_headerWidth) {
        // Right click on empty area
        int tick = xToTick(event->pos().x());
        
        QAction *addAction = menu.addAction(tr("Add Tempo Event Here..."));
        connect(addAction, &QAction::triggered, this, [this, tick, arrangement]() {
            bool ok;
            double bpm = QInputDialog::getDouble(this, tr("Add Tempo Event"), tr("BPM:"),
                                                  120.0, MIN_BPM, MAX_BPM, 1, &ok);
            if (ok) {
                if (!arrangement->hasTempoTrack()) {
                    double projectBpm = 120.0;
                    int projectTempo = m_engine->getRuntimeData()->getTempo();
                    if (projectTempo > 0) {
                        projectBpm = 60'000'000.0 / projectTempo;
                    }
                    arrangement->createTempoTrack(projectBpm);
                }
                
                NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
                if (tempoTrack) {
                    int snapResolution = 480;
                    int snappedTick = (tick / snapResolution) * snapResolution;
                    NN_TempoEvent_t newEvent(snappedTick, bpm, TempoInterpolation::Step);
                    tempoTrack->addTempoEvent(newEvent);
                    update();
                }
            }
        });
    }
    
    // General options
    if (!menu.isEmpty()) {
        menu.addSeparator();
    }
    
    if (arrangement->hasTempoTrack()) {
        NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
        QString toggleText = tempoTrack->isTempoTrackActive() ?
            tr("Disable Tempo Track") : tr("Enable Tempo Track");
        QAction *toggleAction = menu.addAction(toggleText);
        connect(toggleAction, &QAction::triggered, this, [arrangement, tempoTrack]() {
            tempoTrack->setTempoTrackActive(!tempoTrack->isTempoTrackActive());
            emit arrangement->tempoTrackChanged();
        });
        
        QAction *removeAction = menu.addAction(tr("Remove Tempo Track"));
        connect(removeAction, &QAction::triggered, this, [arrangement]() {
            arrangement->removeTempoTrack();
        });
        
        // All interpolation options
        menu.addSeparator();
        
        QAction *allStepAction = menu.addAction(tr("All Step"));
        connect(allStepAction, &QAction::triggered, this, [this, tempoTrack]() {
            auto events = tempoTrack->getTempoEvents();
            for (auto &ev : events) {
                ev.interpolation = TempoInterpolation::Step;
            }
            tempoTrack->setTempoEvents(events);
            update();
        });
        
        QAction *allLinearAction = menu.addAction(tr("All Linear"));
        connect(allLinearAction, &QAction::triggered, this, [this, tempoTrack]() {
            auto events = tempoTrack->getTempoEvents();
            for (auto &ev : events) {
                ev.interpolation = TempoInterpolation::Linear;
            }
            tempoTrack->setTempoEvents(events);
            update();
        });
    } else {
        QAction *createAction = menu.addAction(tr("Create Tempo Track"));
        connect(createAction, &QAction::triggered, this, [this, arrangement]() {
            double projectBpm = 120.0;
            int projectTempo = m_engine->getRuntimeData()->getTempo();
            if (projectTempo > 0) {
                projectBpm = 60'000'000.0 / projectTempo;
            }
            arrangement->createTempoTrack(projectBpm);
        });
    }
    
    if (!menu.isEmpty()) {
        menu.exec(event->globalPos());
    }
}
