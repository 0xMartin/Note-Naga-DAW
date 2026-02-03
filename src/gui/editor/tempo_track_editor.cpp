#include "tempo_track_editor.h"
#include "midi_editor_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QToolTip>
#include <QScrollBar>
#include <QCoreApplication>
#include <cmath>

TempoTrackEditor::TempoTrackEditor(NoteNagaEngine *engine, MidiEditorWidget *midiEditor, QWidget *parent)
    : QWidget(parent),
      m_engine(engine),
      m_midiEditor(midiEditor),
      m_tempoTrack(nullptr),
      m_expanded(true),
      m_timeScale(1.0),
      m_horizontalScroll(0),
      m_leftMargin(60),
      m_currentTick(0),
      m_currentDisplayBPM(120.0),
      m_isDragging(false),
      m_draggedEventIndex(-1),
      m_hoveredPoint(nullptr),
      m_contextMenuPoint(nullptr),
      // Colors matching MidiEditorWidget
      m_backgroundColor(QColor(0x32, 0x35, 0x3c)),
      m_gridColor(QColor(0x46, 0x4a, 0x56)),
      m_curveColor(QColor(255, 140, 60)),       // Orange tempo curve
      m_pointColor(QColor(255, 180, 80)),
      m_pointHoverColor(QColor(255, 220, 120)),
      m_pointSelectedColor(QColor(255, 100, 100)),
      m_textColor(QColor(0xe0, 0xe6, 0xef))
{
    setupUI();
    setMouseTracking(true);
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    // Connect to MIDI editor signals
    if (m_midiEditor) {
        connect(m_midiEditor, &MidiEditorWidget::horizontalScrollChanged, 
                this, &TempoTrackEditor::setHorizontalScroll);
        connect(m_midiEditor, &MidiEditorWidget::timeScaleChanged, 
                this, &TempoTrackEditor::setTimeScale);
        
        // Sync initial values from the MIDI editor
        if (m_midiEditor->getConfig()) {
            m_timeScale = m_midiEditor->getConfig()->time_scale;
        }
        m_horizontalScroll = m_midiEditor->horizontalScrollBar()->value();
    }
    
    // Connect to runtime data for realtime tempo display
    if (m_engine && m_engine->getRuntimeData()) {
        connect(m_engine->getRuntimeData(), &NoteNagaRuntimeData::currentTempoChanged,
                this, &TempoTrackEditor::onCurrentTempoChanged);
        connect(m_engine->getRuntimeData(), &NoteNagaRuntimeData::currentTickChanged,
                this, &TempoTrackEditor::setCurrentTick);
    }
}

TempoTrackEditor::~TempoTrackEditor()
{
}

void TempoTrackEditor::setupUI()
{
    // Create toggle button
    m_toggleButton = new QPushButton(this);
    m_toggleButton->setFixedSize(20, 20);
    m_toggleButton->setToolTip(tr("Toggle Tempo Track Editor"));
    m_toggleButton->setStyleSheet(R"(
        QPushButton {
            background: #2a2d35;
            border: 1px solid #3d424d;
            border-radius: 3px;
            color: #9a9aa5;
            font-size: 10px;
            font-weight: bold;
            padding: 0;
            min-width: 20px;
            max-width: 20px;
            min-height: 20px;
            max-height: 20px;
        }
        QPushButton:hover { 
            background: #353945; 
            color: #e0e6ef;
            border-color: #4a5160;
        }
        QPushButton:pressed { background: #404550; }
    )");
    m_toggleButton->setText("▼");
    connect(m_toggleButton, &QPushButton::clicked, this, [this]() {
        setExpanded(!m_expanded);
    });
    
    // Title label - will be positioned in resizeEvent
    m_titleLabel = new QLabel("Tempo", this);
    m_titleLabel->setStyleSheet("color: #ff8c3c; font-size: 10px; font-weight: bold; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    // Value label
    m_valueLabel = new QLabel(this);
    m_valueLabel->setStyleSheet("color: #ffb450; font-size: 9px; background: transparent;");
    m_valueLabel->setAlignment(Qt::AlignCenter);
    
    m_toggleButton->move(5, 4);
}

void TempoTrackEditor::setTempoTrack(NoteNagaTrack *track)
{
    if (m_tempoTrack) {
        disconnect(m_tempoTrack, &NoteNagaTrack::tempoEventsChanged,
                   this, &TempoTrackEditor::onTempoEventsChanged);
    }
    
    m_tempoTrack = track;
    
    if (m_tempoTrack) {
        connect(m_tempoTrack, &NoteNagaTrack::tempoEventsChanged,
                this, &TempoTrackEditor::onTempoEventsChanged);
    }
    
    rebuildTempoPoints();
    update();
}

void TempoTrackEditor::setExpanded(bool expanded)
{
    if (m_expanded != expanded) {
        m_expanded = expanded;
        m_toggleButton->setText(expanded ? "▼" : "▲");
        m_valueLabel->setVisible(expanded);
        
        if (expanded) {
            setMinimumHeight(100);
            setMaximumHeight(16777215);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        } else {
            setMinimumHeight(28);
            setMaximumHeight(28);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        
        // Trigger parent layout update for proper collapse
        if (parentWidget()) {
            parentWidget()->updateGeometry();
        }
        
        emit expandedChanged(expanded);
        update();
    }
}

void TempoTrackEditor::setHorizontalScroll(int value)
{
    if (m_horizontalScroll != value) {
        m_horizontalScroll = value;
        rebuildTempoPoints();
        update();
    }
}

void TempoTrackEditor::setTimeScale(double scale)
{
    if (m_timeScale != scale) {
        m_timeScale = scale;
        rebuildTempoPoints();
        update();
    }
}

void TempoTrackEditor::setCurrentTick(int tick)
{
    if (m_currentTick != tick) {
        m_currentTick = tick;
        update();
    }
}

void TempoTrackEditor::refresh()
{
    rebuildTempoPoints();
    update();
}

void TempoTrackEditor::onTempoEventsChanged()
{
    rebuildTempoPoints();
    update();
}

void TempoTrackEditor::onCurrentTempoChanged(double bpm)
{
    m_currentDisplayBPM = bpm;
    // BPM is displayed in the MidiControlBarWidget below
}

void TempoTrackEditor::rebuildTempoPoints()
{
    m_tempoPoints.clear();
    m_hoveredPoint = nullptr;
    
    if (!m_tempoTrack) return;
    
    const auto &events = m_tempoTrack->getTempoEvents();
    for (size_t i = 0; i < events.size(); ++i) {
        const auto &ev = events[i];
        TempoPoint point;
        point.tick = ev.tick;
        point.bpm = ev.bpm;
        point.interpolation = ev.interpolation;
        point.x = xFromTick(ev.tick);
        point.y = yFromBPM(ev.bpm);
        point.hovered = false;
        point.selected = false;
        point.eventIndex = static_cast<int>(i);
        m_tempoPoints.push_back(point);
    }
}

int TempoTrackEditor::tickFromX(int x) const
{
    // Convert screen X to tick, accounting for left margin and scroll
    double pixelsPerTick = m_timeScale * 0.1;  // Same as MidiEditorWidget
    return static_cast<int>((x - m_leftMargin + m_horizontalScroll) / pixelsPerTick);
}

int TempoTrackEditor::xFromTick(int tick) const
{
    double pixelsPerTick = m_timeScale * 0.1;
    return m_leftMargin + static_cast<int>(tick * pixelsPerTick) - m_horizontalScroll;
}

double TempoTrackEditor::bpmFromY(int y) const
{
    int h = height();
    int drawHeight = h - 40;  // Top and bottom padding
    int topPadding = 28;
    
    // Clamp Y to valid range
    y = std::max(topPadding, std::min(y, topPadding + drawHeight));
    
    // Y is inverted: top = MAX_BPM, bottom = MIN_BPM
    double ratio = 1.0 - (double)(y - topPadding) / drawHeight;
    return MIN_BPM + ratio * (MAX_BPM - MIN_BPM);
}

int TempoTrackEditor::yFromBPM(double bpm) const
{
    int h = height();
    int drawHeight = h - 40;
    int topPadding = 28;
    
    // Clamp BPM
    bpm = std::max(MIN_BPM, std::min(bpm, MAX_BPM));
    
    double ratio = (bpm - MIN_BPM) / (MAX_BPM - MIN_BPM);
    return topPadding + static_cast<int>((1.0 - ratio) * drawHeight);
}

TempoTrackEditor::TempoPoint* TempoTrackEditor::hitTest(const QPoint &pos)
{
    const int hitRadius = 8;
    
    for (auto &point : m_tempoPoints) {
        int dx = pos.x() - point.x;
        int dy = pos.y() - point.y;
        if (dx * dx + dy * dy <= hitRadius * hitRadius) {
            return &point;
        }
    }
    
    return nullptr;
}

void TempoTrackEditor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    
    int w = width();
    int h = height();
    
    // Background
    p.fillRect(rect(), m_backgroundColor);
    
    // If collapsed, minimal display
    if (!m_expanded) {
        p.fillRect(0, 0, m_leftMargin, h, QColor(0x29, 0x2a, 0x2e));
        p.setPen(QPen(m_gridColor, 1));
        p.drawLine(0, h - 1, w, h - 1);
        return;
    }
    
    // Draw grid
    drawGrid(p);
    
    // Draw tempo curve
    drawTempoCurve(p);
    
    // Draw tempo points
    drawTempoPoints(p);
    
    // Left margin
    p.fillRect(0, 0, m_leftMargin, h, QColor(0x29, 0x2a, 0x2e));
    
    // Vertical separator
    p.setPen(QPen(QColor(0x23, 0x27, 0x31), 1));
    p.drawLine(m_leftMargin - 1, 0, m_leftMargin - 1, h);
    
    // Scale markers
    p.setPen(QColor(0x9a, 0x9a, 0xa0));
    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    
    // Draw BPM markers at key values
    int topPadding = 28;
    int drawHeight = h - 40;
    
    for (int bpm : {300, 200, 120, 60, 20}) {
        if (bpm < MIN_BPM || bpm > MAX_BPM) continue;
        int y = yFromBPM(bpm);
        p.drawText(QRect(2, y - 7, m_leftMargin - 6, 15), Qt::AlignRight, QString::number(bpm));
        
        // Draw faint horizontal line
        p.setPen(QPen(m_gridColor, 1, Qt::DotLine));
        p.drawLine(m_leftMargin, y, w, y);
        p.setPen(QColor(0x9a, 0x9a, 0xa0));
    }
    
    // Bottom separator
    p.setPen(QPen(QColor(0x23, 0x27, 0x31), 1));
    p.drawLine(0, h - 1, w, h - 1);
    
    // Draw playback position indicator (red line)
    int playbackX = m_leftMargin + static_cast<int>(m_currentTick * m_timeScale) - m_horizontalScroll;
    if (playbackX >= m_leftMargin && playbackX <= w) {
        p.setPen(QPen(QColor(192, 74, 74), 2));  // #c04a4a - same as MIDI editor
        p.drawLine(playbackX, 0, playbackX, h);
    }
    
    // Show current hovered value
    if (m_hoveredPoint) {
        QString info = QString("%1 BPM @ tick %2")
            .arg(m_hoveredPoint->bpm, 0, 'f', 1)
            .arg(m_hoveredPoint->tick);
        m_valueLabel->setText(info);
        m_valueLabel->move(m_leftMargin + 10, h - 20);
        m_valueLabel->setVisible(true);
    }
}

void TempoTrackEditor::drawGrid(QPainter &p)
{
    // Vertical grid lines synced with MIDI editor
    // For now, simple lines every bar (we can enhance this)
    p.setPen(QPen(m_gridColor, 1, Qt::DotLine));
    
    int ppq = 480;  // Ticks per quarter
    int ticksPerBar = ppq * 4;  // Assuming 4/4
    
    int startTick = tickFromX(m_leftMargin);
    int endTick = tickFromX(width());
    
    startTick = (startTick / ticksPerBar) * ticksPerBar;
    
    for (int tick = startTick; tick <= endTick; tick += ticksPerBar) {
        int x = xFromTick(tick);
        if (x >= m_leftMargin && x <= width()) {
            p.drawLine(x, 28, x, height() - 12);
        }
    }
}

void TempoTrackEditor::drawTempoCurve(QPainter &p)
{
    if (m_tempoPoints.empty()) return;
    
    QPainterPath path;
    bool firstVisible = true;
    
    // Draw curve connecting tempo points
    for (size_t i = 0; i < m_tempoPoints.size(); ++i) {
        const auto &pt = m_tempoPoints[i];
        
        if (firstVisible) {
            // Draw horizontal line from left edge to first point
            int y = yFromBPM(pt.bpm);
            path.moveTo(m_leftMargin, y);
            path.lineTo(pt.x, y);
            firstVisible = false;
        }
        
        if (i + 1 < m_tempoPoints.size()) {
            const auto &nextPt = m_tempoPoints[i + 1];
            
            if (pt.interpolation == TempoInterpolation::Step) {
                // Step: horizontal line then vertical
                path.lineTo(nextPt.x, pt.y);
                path.lineTo(nextPt.x, nextPt.y);
            } else {
                // Linear: direct line to next point
                path.lineTo(nextPt.x, nextPt.y);
            }
        } else {
            // Last point: extend to right edge
            path.lineTo(width(), pt.y);
        }
    }
    
    // Draw the curve
    p.setPen(QPen(m_curveColor, 2));
    p.drawPath(path);
    
    // Fill under curve (optional - gives visual feedback)
    QPainterPath fillPath = path;
    fillPath.lineTo(width(), height() - 12);
    fillPath.lineTo(m_leftMargin, height() - 12);
    fillPath.closeSubpath();
    
    QColor fillColor = m_curveColor;
    fillColor.setAlpha(30);
    p.fillPath(fillPath, fillColor);
}

void TempoTrackEditor::drawTempoPoints(QPainter &p)
{
    const int pointRadius = 6;
    
    for (auto &pt : m_tempoPoints) {
        QColor color = m_pointColor;
        if (pt.hovered || &pt == m_hoveredPoint) {
            color = m_pointHoverColor;
        }
        if (pt.selected) {
            color = m_pointSelectedColor;
        }
        
        // Draw point circle
        p.setPen(QPen(color.darker(120), 2));
        p.setBrush(color);
        p.drawEllipse(QPoint(pt.x, pt.y), pointRadius, pointRadius);
        
        // Draw interpolation indicator (small icon below)
        if (pt.interpolation == TempoInterpolation::Linear) {
            // Small diagonal line indicating linear
            p.setPen(QPen(color, 1));
            p.drawLine(pt.x - 3, pt.y + pointRadius + 4, pt.x + 3, pt.y + pointRadius + 8);
        } else {
            // Small step indicator
            p.setPen(QPen(color, 1));
            p.drawLine(pt.x - 3, pt.y + pointRadius + 6, pt.x, pt.y + pointRadius + 6);
            p.drawLine(pt.x, pt.y + pointRadius + 6, pt.x, pt.y + pointRadius + 4);
            p.drawLine(pt.x, pt.y + pointRadius + 4, pt.x + 3, pt.y + pointRadius + 4);
        }
    }
}

void TempoTrackEditor::mousePressEvent(QMouseEvent *event)
{
    if (!m_expanded || !m_tempoTrack) {
        QWidget::mousePressEvent(event);
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        TempoPoint *hit = hitTest(event->pos());
        
        if (hit) {
            // Start dragging the point
            m_isDragging = true;
            m_draggedEventIndex = hit->eventIndex;
            m_dragStartPos = event->pos();
            m_dragStartBPM = hit->bpm;
            m_dragStartTick = hit->tick;
            hit->selected = true;
            update();
        }
    }
}

void TempoTrackEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_expanded || !m_tempoTrack) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    if (m_isDragging && m_draggedEventIndex >= 0) {
        // Update tempo event position
        double newBPM = bpmFromY(event->pos().y());
        newBPM = std::round(newBPM * 10) / 10;  // Round to 0.1 BPM
        newBPM = std::max(MIN_BPM, std::min(newBPM, MAX_BPM));
        
        // Horizontal movement - tick position
        int newTick = tickFromX(event->pos().x());
        newTick = std::max(0, newTick);
        
        // Smart snapping: only snap if close to grid line
        // Use smaller grid (16th notes = PPQ/4) for finer control
        int ppq = 480;
        if (m_midiEditor) {
            auto *seq = m_midiEditor->getSequence();
            if (seq) ppq = seq->getPPQ();
        }
        
        // Snap to 16th notes (ppq/4) if within snap threshold (8 pixels)
        int gridStep = ppq / 4;  // 16th note grid
        int nearestGrid = ((newTick + gridStep/2) / gridStep) * gridStep;
        int snapThresholdTicks = static_cast<int>(8.0 / m_timeScale);  // ~8 pixels worth of ticks
        
        if (std::abs(newTick - nearestGrid) < snapThresholdTicks) {
            newTick = nearestGrid;
        }
        
        // Update the event
        auto events = m_tempoTrack->getTempoEvents();
        if (m_draggedEventIndex < static_cast<int>(events.size())) {
            events[m_draggedEventIndex].bpm = newBPM;
            events[m_draggedEventIndex].tick = newTick;
            m_tempoTrack->setTempoEvents(events);
            
            emit tempoEventChanged(newTick, newBPM);
        }
        
        // Show tooltip with current value
        QToolTip::showText(event->globalPosition().toPoint(), 
                           QString("%1 BPM").arg(newBPM, 0, 'f', 1));
        
    } else {
        // Hover detection
        TempoPoint *oldHovered = m_hoveredPoint;
        m_hoveredPoint = hitTest(event->pos());
        
        if (m_hoveredPoint != oldHovered) {
            if (oldHovered) oldHovered->hovered = false;
            if (m_hoveredPoint) m_hoveredPoint->hovered = true;
            update();
        }
        
        // Show cursor
        setCursor(m_hoveredPoint ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void TempoTrackEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        m_isDragging = false;
        m_draggedEventIndex = -1;
        
        // Deselect all
        for (auto &pt : m_tempoPoints) {
            pt.selected = false;
        }
        update();
    }
    
    QWidget::mouseReleaseEvent(event);
}

void TempoTrackEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_expanded || !m_tempoTrack) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        TempoPoint *hit = hitTest(event->pos());
        
        if (hit) {
            // Edit existing tempo point
            showEditTempoDialog(hit);
        } else if (event->pos().x() > m_leftMargin) {
            // Add new tempo point
            int tick = tickFromX(event->pos().x());
            showAddTempoDialog(tick);
        }
    }
}

void TempoTrackEditor::wheelEvent(QWheelEvent *event)
{
    if (!m_expanded) {
        QWidget::wheelEvent(event);
        return;
    }
    
    // Scroll through time (horizontal scroll)
    if (m_midiEditor) {
        QWheelEvent *fwdEvent = new QWheelEvent(
            event->position(), event->globalPosition(),
            event->pixelDelta(), event->angleDelta(),
            event->buttons(), event->modifiers(),
            event->phase(), event->inverted()
        );
        QCoreApplication::sendEvent(m_midiEditor, fwdEvent);
    }
}

void TempoTrackEditor::resizeEvent(QResizeEvent *event)
{
    rebuildTempoPoints();
    // Position title label on right side
    m_titleLabel->setFixedWidth(60);
    m_titleLabel->move(width() - 70, 6);
    QWidget::resizeEvent(event);
}

void TempoTrackEditor::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_expanded || !m_tempoTrack) {
        QWidget::contextMenuEvent(event);
        return;
    }
    
    m_contextMenuPoint = hitTest(event->pos());
    
    QMenu menu(this);
    
    if (m_contextMenuPoint) {
        // Context menu for existing tempo point
        QAction *editAction = menu.addAction(QIcon(":/icons/edit.svg"), "Edit Tempo...");
        connect(editAction, &QAction::triggered, this, [this]() {
            showEditTempoDialog(m_contextMenuPoint);
        });
        
        QAction *toggleInterpAction = menu.addAction(
            m_contextMenuPoint->interpolation == TempoInterpolation::Step ? 
                "Set Linear Interpolation" : "Set Step Interpolation");
        connect(toggleInterpAction, &QAction::triggered, this, [this]() {
            toggleInterpolation(m_contextMenuPoint);
        });
        
        menu.addSeparator();
        
        QAction *deleteAction = menu.addAction(QIcon(":/icons/delete.svg"), "Delete Tempo Point");
        connect(deleteAction, &QAction::triggered, this, [this]() {
            deleteTempoPoint(m_contextMenuPoint);
        });
    } else if (event->pos().x() > m_leftMargin) {
        // Context menu for empty area - add new point
        int tick = tickFromX(event->pos().x());
        
        QAction *addAction = menu.addAction(QIcon(":/icons/add.svg"), "Add Tempo Point...");
        connect(addAction, &QAction::triggered, this, [this, tick]() {
            showAddTempoDialog(tick);
        });
    }
    
    if (!menu.isEmpty()) {
        menu.exec(event->globalPos());
    }
}

void TempoTrackEditor::showAddTempoDialog(int tick)
{
    bool ok;
    double bpm = QInputDialog::getDouble(this, "Add Tempo Point",
                                          QString("BPM at tick %1:").arg(tick),
                                          120.0, MIN_BPM, MAX_BPM, 1, &ok);
    
    if (ok) {
        NN_TempoEvent_t event;
        event.tick = tick;
        event.bpm = bpm;
        event.interpolation = TempoInterpolation::Step;
        
        m_tempoTrack->addTempoEvent(event);
    }
}

void TempoTrackEditor::showEditTempoDialog(TempoPoint *point)
{
    if (!point) return;
    
    bool ok;
    double bpm = QInputDialog::getDouble(this, "Edit Tempo Point",
                                          QString("BPM at tick %1:").arg(point->tick),
                                          point->bpm, MIN_BPM, MAX_BPM, 1, &ok);
    
    if (ok) {
        auto events = m_tempoTrack->getTempoEvents();
        if (point->eventIndex < static_cast<int>(events.size())) {
            events[point->eventIndex].bpm = bpm;
            m_tempoTrack->setTempoEvents(events);
        }
    }
}

void TempoTrackEditor::deleteTempoPoint(TempoPoint *point)
{
    if (!point) return;
    
    // Don't delete the last tempo point
    if (m_tempoPoints.size() <= 1) {
        QMessageBox::warning(this, "Cannot Delete",
                             "Cannot delete the last tempo point. There must be at least one tempo event.");
        return;
    }
    
    m_tempoTrack->removeTempoEventAtTick(point->tick);
}

void TempoTrackEditor::toggleInterpolation(TempoPoint *point)
{
    if (!point) return;
    
    auto events = m_tempoTrack->getTempoEvents();
    if (point->eventIndex < static_cast<int>(events.size())) {
        events[point->eventIndex].interpolation = 
            (events[point->eventIndex].interpolation == TempoInterpolation::Step) ?
                TempoInterpolation::Linear : TempoInterpolation::Step;
        m_tempoTrack->setTempoEvents(events);
    }
}
