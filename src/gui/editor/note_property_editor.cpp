#include "note_property_editor.h"
#include "midi_editor_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QToolTip>
#include <QMenu>
#include <QInputDialog>
#include <cmath>
#include <set>

NotePropertyEditor::NotePropertyEditor(NoteNagaEngine *engine, MidiEditorWidget *midiEditor, QWidget *parent)
    : QWidget(parent),
      m_engine(engine),
      m_midiEditor(midiEditor),
      m_propertyType(PropertyType::Velocity),
      m_expanded(true),
      m_timeScale(1.0),
      m_horizontalScroll(0),
      m_leftMargin(60),  // Match MidiKeyboardRuler width for alignment
      m_isDragging(false),
      m_hasSelection(false),
      m_isSnapping(false),
      m_snapValue(-1),
      m_hoveredBar(nullptr),
      m_editingBar(nullptr),
      m_contextMenuBar(nullptr),
      m_activeTrack(nullptr),
      m_trackColor(QColor(80, 160, 220)),
      // Colors matching MidiEditorWidget
      m_backgroundColor(QColor(0x32, 0x35, 0x3c)),  // #32353c - same as midi editor bg_color
      m_gridColor(QColor(0x46, 0x4a, 0x56)),        // #464a56 - subline_color
      m_barColor(QColor(80, 160, 220)),
      m_barSelectedColor(QColor(255, 180, 80)),
      m_barHoverColor(QColor(120, 200, 255)),
      m_textColor(QColor(0xe0, 0xe6, 0xef))         // #e0e6ef - fg_color
{
    setupUI();
    setMouseTracking(true);
    setMinimumHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    // Connect to MIDI editor signals
    if (m_midiEditor) {
        connect(m_midiEditor, &MidiEditorWidget::horizontalScrollChanged, 
                this, &NotePropertyEditor::setHorizontalScroll);
        connect(m_midiEditor, &MidiEditorWidget::timeScaleChanged, 
                this, &NotePropertyEditor::setTimeScale);
    }
    
    // Connect to project signals for track changes
    if (m_engine) {
        NoteNagaMidiSeq *seq = m_engine->getProject()->getActiveSequence();
        if (seq) {
            connect(seq, &NoteNagaMidiSeq::activeTrackChanged, 
                    this, &NotePropertyEditor::onActiveTrackChanged);
        }
        
        // Also listen for sequence changes to reconnect
        connect(m_engine->getProject(), &NoteNagaProject::activeSequenceChanged,
                this, &NotePropertyEditor::onSequenceChanged);
    }
}

NotePropertyEditor::~NotePropertyEditor()
{
}

void NotePropertyEditor::setupUI()
{
    // Create toggle button (expand/collapse) - positioned at top-left corner
    m_toggleButton = new QPushButton(this);
    m_toggleButton->setFixedSize(16, 16);
    m_toggleButton->setToolTip(tr("Toggle Note Property Editor"));
    m_toggleButton->setStyleSheet(R"(
        QPushButton {
            background: #32353c;
            border: 1px solid #464a56;
            border-radius: 0px;
            color: #888;
            font-size: 9px;
            font-weight: bold;
            padding: 0;
        }
        QPushButton:hover { background: #3a3d45; color: #fff; }
        QPushButton:pressed { background: #4a4d55; }
    )");
    m_toggleButton->setText("▼");
    connect(m_toggleButton, &QPushButton::clicked, this, [this]() {
        setExpanded(!m_expanded);
    });
    
    // Track name label - top right
    m_trackNameLabel = new QLabel(this);
    m_trackNameLabel->setStyleSheet("color: #e0e6ef; font-size: 11px; font-weight: bold; background: transparent;");
    m_trackNameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_trackNameLabel->setText(tr("No Track"));
    
    // Create property selector buttons - positioned at top right
    m_velocityButton = new QPushButton(tr("Vel"), this);
    m_velocityButton->setCheckable(true);
    m_velocityButton->setChecked(true);
    m_velocityButton->setFixedSize(36, 20);
    m_velocityButton->setToolTip(tr("Edit Velocity"));
    
    m_panButton = new QPushButton(tr("Pan"), this);
    m_panButton->setCheckable(true);
    m_panButton->setChecked(false);
    m_panButton->setFixedSize(36, 20);
    m_panButton->setToolTip(tr("Edit Pan"));
    
    // Initial button style (will be updated by track color)
    updateTrackColorStyles();
    
    connect(m_velocityButton, &QPushButton::clicked, this, [this]() {
        setPropertyType(PropertyType::Velocity);
    });
    connect(m_panButton, &QPushButton::clicked, this, [this]() {
        setPropertyType(PropertyType::Pan);
    });
    
    // Value label - bottom left
    m_valueLabel = new QLabel(this);
    m_valueLabel->setStyleSheet("color: #8af; font-size: 11px; background: transparent;");
    m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_valueLabel->setFixedWidth(40);
    
    // Position controls - toggle at left, rest at right (will be repositioned in resizeEvent)
    m_toggleButton->move(5, 5);
    m_valueLabel->move(5, height() - 25);
}

void NotePropertyEditor::setPropertyType(PropertyType type)
{
    if (m_propertyType != type) {
        m_propertyType = type;
        updatePropertyButtons();
        rebuildNoteBars();
        update();
    }
}

void NotePropertyEditor::updatePropertyButtons()
{
    m_velocityButton->setChecked(m_propertyType == PropertyType::Velocity);
    m_panButton->setChecked(m_propertyType == PropertyType::Pan);
}

void NotePropertyEditor::setExpanded(bool expanded)
{
    if (m_expanded != expanded) {
        m_expanded = expanded;
        m_toggleButton->setText(expanded ? "▼" : "▲");
        
        if (expanded) {
            setMinimumHeight(80);
            setMaximumHeight(16777215);
        } else {
            setFixedHeight(28);
        }
        
        emit expandedChanged(expanded);
        update();
    }
}

void NotePropertyEditor::setHorizontalScroll(int value)
{
    if (m_horizontalScroll != value) {
        m_horizontalScroll = value;
        update();
    }
}

void NotePropertyEditor::setTimeScale(double scale)
{
    if (m_timeScale != scale) {
        m_timeScale = scale;
        rebuildNoteBars();
        update();
    }
}

void NotePropertyEditor::onSelectionChanged()
{
    updateActiveTrack();
    rebuildNoteBars();
    update();
}

void NotePropertyEditor::onNotesChanged()
{
    rebuildNoteBars();
    update();
}

void NotePropertyEditor::onActiveTrackChanged(NoteNagaTrack *track)
{
    Q_UNUSED(track);
    // Force update of active track info and rebuild bars
    m_activeTrack = nullptr;  // Reset to force update
    updateActiveTrack();
    rebuildNoteBars();
    update();
}

void NotePropertyEditor::onSequenceChanged(NoteNagaMidiSeq *seq)
{
    // Disconnect from old sequence and connect to new
    if (seq) {
        connect(seq, &NoteNagaMidiSeq::activeTrackChanged, 
                this, &NotePropertyEditor::onActiveTrackChanged, Qt::UniqueConnection);
    }
    
    m_activeTrack = nullptr;
    updateActiveTrack();
    rebuildNoteBars();
    update();
}

void NotePropertyEditor::rebuildNoteBars()
{
    m_noteBars.clear();
    m_hasSelection = false;
    
    if (!m_engine || !m_midiEditor) return;
    
    NoteNagaMidiSeq *seq = m_engine->getProject()->getActiveSequence();
    if (!seq) return;
    
    // Update active track info
    updateActiveTrack();
    
    // Only show notes from active track
    if (!m_activeTrack) return;
    
    auto config = m_midiEditor->getConfig();
    auto selectedNotes = m_midiEditor->getSelectedNotes();
    
    // Build set of selected notes for quick lookup (by note ID)
    std::set<unsigned long> selectedIds;
    for (const auto &sel : selectedNotes) {
        selectedIds.insert(sel.second.id);
    }
    m_hasSelection = !selectedIds.empty();
    
    // Only collect notes from active track
    std::vector<NN_Note_t> notes = m_activeTrack->getNotes();
    for (size_t i = 0; i < notes.size(); ++i) {
        const NN_Note_t &note = notes[i];
        
        // Skip notes without start position
        if (!note.start.has_value()) continue;
        
        NoteBar bar;
        bar.x = int(note.start.value() * config->time_scale);
        bar.width = std::max(4, int(note.length.value_or(100) * config->time_scale));
        bar.track = m_activeTrack;
        bar.noteIndex = (int)i;
        bar.note = note;
        bar.selected = selectedIds.count(note.id) > 0;
        
        // Get value based on property type
        if (m_propertyType == PropertyType::Velocity) {
            bar.value = note.velocity.value_or(100);
        } else if (m_propertyType == PropertyType::Pan) {
            // Read pan from note, default to center (64) if not set
            bar.value = note.pan.value_or(64);
        }
        
        m_noteBars.push_back(bar);
    }
}

void NotePropertyEditor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    
    int w = width();
    int h = height();
    
    // Background - matches MIDI editor bg_color
    p.fillRect(rect(), m_backgroundColor);
    
    // If collapsed, just draw minimal header (no label - buttons already indicate mode)
    if (!m_expanded) {
        // Draw left margin background only
        p.fillRect(0, 0, m_leftMargin, h, QColor(0x29, 0x2a, 0x2e));  // #292a2e
        
        // Draw separator line at bottom
        p.setPen(QPen(m_gridColor, 1));
        p.drawLine(0, h - 1, w, h - 1);
        return;
    }
    
    // Draw property-specific content
    if (m_propertyType == PropertyType::Velocity) {
        drawVelocityLane(p);
    } else {
        drawPanLane(p);
    }
    
    // Draw left margin background - matches grid_row_color2 from MIDI editor
    p.fillRect(0, 0, m_leftMargin, h, QColor(0x29, 0x2a, 0x2e));  // #292a2e
    
    // Draw vertical separator line at margin edge
    p.setPen(QPen(QColor(0x23, 0x27, 0x31), 1));  // line_color
    p.drawLine(m_leftMargin - 1, 0, m_leftMargin - 1, h);
    
    // Draw scale markers on left margin
    p.setPen(QColor(0x9a, 0x9a, 0xa0));
    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    
    if (m_propertyType == PropertyType::Velocity) {
        p.drawText(QRect(2, 28, m_leftMargin - 6, 15), Qt::AlignRight, "127");
        p.drawText(QRect(2, h / 2 - 7, m_leftMargin - 6, 15), Qt::AlignRight, "64");
        p.drawText(QRect(2, h - 20, m_leftMargin - 6, 15), Qt::AlignRight, "0");
    } else {
        p.drawText(QRect(2, 28, m_leftMargin - 6, 15), Qt::AlignRight, "R");
        p.drawText(QRect(2, h / 2 - 7, m_leftMargin - 6, 15), Qt::AlignRight, "C");
        p.drawText(QRect(2, h - 20, m_leftMargin - 6, 15), Qt::AlignRight, "L");
    }
    
    // Draw snap line indicator when snapping
    if (m_isDragging && m_isSnapping && m_snapValue >= 0) {
        int snapY = yFromValue(m_snapValue);
        p.setPen(QPen(QColor(255, 200, 50, 180), 2, Qt::SolidLine));
        p.drawLine(m_leftMargin, snapY, w, snapY);
        
        // Draw small value indicator
        p.setPen(QColor(255, 200, 50));
        p.drawText(QRect(2, snapY - 7, m_leftMargin - 6, 15), Qt::AlignRight, QString::number(m_snapValue));
    }
    
    // Draw separator line at bottom
    p.setPen(QPen(QColor(0x23, 0x27, 0x31), 1));  // line_color
    p.drawLine(0, h - 1, w, h - 1);
}

void NotePropertyEditor::drawGridLines(QPainter &p)
{
    int w = width();
    int h = height();
    
    // Horizontal grid lines (value levels)
    p.setPen(QPen(m_gridColor, 1, Qt::DotLine));
    
    // Draw at 25%, 50%, 75%
    for (int i = 1; i < 4; ++i) {
        int y = h * i / 4;
        p.drawLine(m_leftMargin, y, w, y);
    }
    
    // Center line (more prominent for pan)
    if (m_propertyType == PropertyType::Pan) {
        p.setPen(QPen(QColor(0x61, 0x77, 0xd1), 1));  // grid_bar_color
        int centerY = h / 2;
        p.drawLine(m_leftMargin, centerY, w, centerY);
    }
}

void NotePropertyEditor::drawVelocityLane(QPainter &p)
{
    int h = height();
    int drawHeight = h - 10;  // Padding
    
    // Draw grid
    drawGridLines(p);
    
    // Draw note velocity bars
    for (auto &bar : m_noteBars) {
        int x = m_leftMargin + bar.x - m_horizontalScroll;
        int barWidth = std::max(3, bar.width);
        
        // Skip if outside visible area
        if (x + barWidth < m_leftMargin || x > width()) continue;
        
        // Calculate bar height based on velocity
        int barHeight = (bar.value * drawHeight) / 127;
        int y = h - 5 - barHeight;
        
        // Determine opacity based on selection state
        // If any note is selected, non-selected notes become semi-transparent
        double opacity = 1.0;
        if (m_hasSelection && !bar.selected) {
            opacity = 0.25;  // Make unselected bars very transparent
        }
        
        // Choose color
        QColor color = m_barColor;
        if (&bar == m_hoveredBar && (!m_hasSelection || bar.selected)) {
            color = m_barHoverColor;
        } else if (bar.selected) {
            color = m_barSelectedColor;
        } else {
            // Gradient based on velocity
            int r = 60 + (bar.value * 150 / 127);
            int g = 140 + (bar.value * 60 / 127);
            int b = 200 - (bar.value * 80 / 127);
            color = QColor(r, g, b);
        }
        
        // Apply opacity
        color.setAlphaF(opacity);
        
        // Draw bar with gradient
        QLinearGradient grad(x, y, x, h - 5);
        QColor lightColor = color.lighter(130);
        QColor darkColor = color.darker(130);
        lightColor.setAlphaF(opacity);
        darkColor.setAlphaF(opacity);
        grad.setColorAt(0.0, lightColor);
        grad.setColorAt(0.4, color);
        grad.setColorAt(1.0, darkColor);
        
        QPainterPath path;
        path.addRoundedRect(x, y, barWidth, barHeight, 2, 2);
        p.fillPath(path, grad);
        
        // Draw top highlight (only for visible bars)
        if (barHeight > 5 && opacity > 0.5) {
            QColor highlightColor = color.lighter(160);
            highlightColor.setAlphaF(opacity);
            p.setPen(QPen(highlightColor, 1));
            p.drawLine(x + 1, y + 1, x + barWidth - 1, y + 1);
        }
        
        // Draw outline for selected/hovered
        if (bar.selected || (&bar == m_hoveredBar && !m_hasSelection)) {
            QColor outlineColor = color.lighter(150);
            outlineColor.setAlphaF(opacity);
            p.setPen(QPen(outlineColor, 1));
            p.drawPath(path);
        }
    }
}

void NotePropertyEditor::drawPanLane(QPainter &p)
{
    int h = height();
    int centerY = h / 2;
    
    // Draw grid
    drawGridLines(p);
    
    // Draw pan bars (bidirectional from center)
    for (auto &bar : m_noteBars) {
        int x = m_leftMargin + bar.x - m_horizontalScroll;
        int barWidth = std::max(3, bar.width);
        
        // Skip if outside visible area
        if (x + barWidth < m_leftMargin || x > width()) continue;
        
        // Pan value: 0 = full left, 64 = center, 127 = full right
        int panOffset = bar.value - 64;  // -64 to +63
        int barHeight = std::abs(panOffset) * (h / 2 - 10) / 64;
        int y = (panOffset >= 0) ? centerY - barHeight : centerY;
        
        // Determine opacity based on selection state
        double opacity = 1.0;
        if (m_hasSelection && !bar.selected) {
            opacity = 0.25;
        }
        
        // Choose color
        QColor color;
        if (&bar == m_hoveredBar && (!m_hasSelection || bar.selected)) {
            color = m_barHoverColor;
        } else if (bar.selected) {
            color = m_barSelectedColor;
        } else {
            // Blue for left, orange for right
            if (panOffset < 0) {
                color = QColor(80, 140, 220);
            } else if (panOffset > 0) {
                color = QColor(220, 140, 80);
            } else {
                color = QColor(120, 120, 130);
            }
        }
        
        // Apply opacity
        color.setAlphaF(opacity);
        
        // Draw bar
        QLinearGradient grad(x, y, x, y + barHeight);
        QColor lightColor = color.lighter(120);
        QColor darkColor = color.darker(110);
        lightColor.setAlphaF(opacity);
        darkColor.setAlphaF(opacity);
        grad.setColorAt(0.0, lightColor);
        grad.setColorAt(1.0, darkColor);
        
        QPainterPath path;
        path.addRoundedRect(x, y, barWidth, std::max(2, barHeight), 2, 2);
        p.fillPath(path, grad);
        
        if (bar.selected || (&bar == m_hoveredBar && !m_hasSelection)) {
            QColor outlineColor = color.lighter(150);
            outlineColor.setAlphaF(opacity);
            p.setPen(QPen(outlineColor, 1));
            p.drawPath(path);
        }
    }
}

NotePropertyEditor::NoteBar* NotePropertyEditor::hitTest(const QPoint &pos)
{
    if (pos.x() < m_leftMargin) return nullptr;
    
    int h = height();
    
    for (auto &bar : m_noteBars) {
        // If there's a selection, only allow hitting selected bars
        if (m_hasSelection && !bar.selected) continue;
        
        int x = m_leftMargin + bar.x - m_horizontalScroll;
        int barWidth = std::max(6, bar.width);  // Minimum hit area
        
        if (pos.x() >= x && pos.x() <= x + barWidth) {
            // Check vertical hit based on property type
            if (m_propertyType == PropertyType::Velocity) {
                int barHeight = (bar.value * (h - 10)) / 127;
                int y = h - 5 - barHeight;
                if (pos.y() >= y - 5 && pos.y() <= h) {
                    return &bar;
                }
            } else {
                // Pan - bidirectional
                return &bar;
            }
        }
    }
    return nullptr;
}

int NotePropertyEditor::valueFromY(int y) const
{
    int h = height();
    
    if (m_propertyType == PropertyType::Velocity) {
        // Y at top = 127, Y at bottom = 0
        int value = 127 - ((y - 5) * 127 / (h - 10));
        return std::clamp(value, 0, 127);
    } else {
        // Pan: center = 64, top = 127, bottom = 0
        int centerY = h / 2;
        int value = 64 + ((centerY - y) * 64 / (h / 2 - 10));
        return std::clamp(value, 0, 127);
    }
}

int NotePropertyEditor::yFromValue(int value) const
{
    int h = height();
    
    if (m_propertyType == PropertyType::Velocity) {
        return h - 5 - (value * (h - 10) / 127);
    } else {
        int centerY = h / 2;
        return centerY - ((value - 64) * (h / 2 - 10) / 64);
    }
}

void NotePropertyEditor::mousePressEvent(QMouseEvent *event)
{
    if (!m_expanded || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    
    NoteBar *bar = hitTest(event->pos());
    if (bar) {
        m_isDragging = true;
        m_dragStartPos = event->pos();
        m_editingBar = bar;
        m_dragStartValue = bar->value;
        setCursor(Qt::SizeVerCursor);
    }
}

void NotePropertyEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_expanded) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    if (m_isDragging && m_editingBar) {
        // Calculate new value from Y position
        int rawValue = valueFromY(event->pos().y());
        
        // Apply snap-to-neighbors logic
        int newValue = rawValue;
        int snapThreshold = 4;  // Snap within 4 units of neighbor value
        
        int prevValue = findNeighborValue(m_editingBar, -1);
        int nextValue = findNeighborValue(m_editingBar, 1);
        
        m_isSnapping = false;
        m_snapValue = -1;
        
        if (prevValue >= 0 && std::abs(rawValue - prevValue) <= snapThreshold) {
            newValue = prevValue;
            m_isSnapping = true;
            m_snapValue = prevValue;
        } else if (nextValue >= 0 && std::abs(rawValue - nextValue) <= snapThreshold) {
            newValue = nextValue;
            m_isSnapping = true;
            m_snapValue = nextValue;
        }
        
        // Also snap to common values (0, 64, 127)
        if (!m_isSnapping) {
            if (std::abs(rawValue - 64) <= 3) {
                newValue = 64;
                m_isSnapping = true;
                m_snapValue = 64;
            } else if (rawValue <= 3) {
                newValue = 0;
                m_isSnapping = true;
                m_snapValue = 0;
            } else if (rawValue >= 124) {
                newValue = 127;
                m_isSnapping = true;
                m_snapValue = 127;
            }
        }
        
        if (newValue != m_editingBar->value) {
            m_editingBar->value = newValue;
            
            // Update the actual note in the engine
            std::vector<NN_Note_t> notes = m_editingBar->track->getNotes();
            if (m_editingBar->noteIndex >= 0 && m_editingBar->noteIndex < (int)notes.size()) {
                if (m_propertyType == PropertyType::Velocity) {
                    notes[m_editingBar->noteIndex].velocity = newValue;
                } else if (m_propertyType == PropertyType::Pan) {
                    notes[m_editingBar->noteIndex].pan = newValue;
                }
                m_editingBar->track->setNotes(notes);
                emit notePropertyChanged(m_editingBar->track, m_editingBar->noteIndex, newValue);
            }
            
            // Show value tooltip with snap indicator
            QString valueText;
            QString snapIndicator = m_isSnapping ? " ⚡" : "";
            if (m_propertyType == PropertyType::Velocity) {
                valueText = QString("Velocity: %1%2").arg(newValue).arg(snapIndicator);
            } else {
                if (newValue < 64) {
                    valueText = QString("Pan: L%1%2").arg(64 - newValue).arg(snapIndicator);
                } else if (newValue > 64) {
                    valueText = QString("Pan: R%1%2").arg(newValue - 64).arg(snapIndicator);
                } else {
                    valueText = QString("Pan: Center%1").arg(snapIndicator);
                }
            }
            QToolTip::showText(event->globalPosition().toPoint(), valueText, this);
            m_valueLabel->setText(QString::number(newValue));
            
            update();
        }
    } else {
        // Update hover state
        NoteBar *newHover = hitTest(event->pos());
        if (newHover != m_hoveredBar) {
            m_hoveredBar = newHover;
            setCursor(newHover ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }
}

void NotePropertyEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        m_isDragging = false;
        m_isSnapping = false;
        m_snapValue = -1;
        m_editingBar = nullptr;
        setCursor(Qt::ArrowCursor);
        m_valueLabel->clear();
        
        // Notify MIDI editor of change
        if (m_midiEditor) {
            m_midiEditor->update();
        }
        update();  // Redraw to remove snap line
    }
    QWidget::mouseReleaseEvent(event);
}

void NotePropertyEditor::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_expanded) {
        QWidget::contextMenuEvent(event);
        return;
    }
    
    NoteBar *bar = hitTest(event->pos());
    if (!bar) {
        QWidget::contextMenuEvent(event);
        return;
    }
    
    m_contextMenuBar = bar;
    
    QMenu menu(this);
    
    QString propName = (m_propertyType == PropertyType::Velocity) ? "Velocity" : "Pan";
    
    // Current value
    QAction *currentAction = menu.addAction(QString("%1: %2").arg(propName).arg(bar->value));
    currentAction->setEnabled(false);
    menu.addSeparator();
    
    // Set value directly
    QAction *setValueAction = menu.addAction(QString("Set %1...").arg(propName));
    connect(setValueAction, &QAction::triggered, this, &NotePropertyEditor::onSetValueTriggered);
    
    menu.addSeparator();
    
    // Snap options
    QAction *snapToPrevAction = menu.addAction("Snap to Previous Note");
    connect(snapToPrevAction, &QAction::triggered, this, &NotePropertyEditor::onSnapToPreviousTriggered);
    
    QAction *snapToNextAction = menu.addAction("Snap to Next Note");
    connect(snapToNextAction, &QAction::triggered, this, &NotePropertyEditor::onSnapToNextTriggered);
    
    QAction *snapToAvgAction = menu.addAction("Snap to Average of Neighbors");
    connect(snapToAvgAction, &QAction::triggered, this, &NotePropertyEditor::onSnapToAverageTriggered);
    
    menu.addSeparator();
    
    // Preset values
    if (m_propertyType == PropertyType::Velocity) {
        QAction *maxAction = menu.addAction("Set to Maximum (127)");
        connect(maxAction, &QAction::triggered, this, [this]() { applyValueToContextBar(127); });
        
        QAction *midAction = menu.addAction("Set to Medium (64)");
        connect(midAction, &QAction::triggered, this, [this]() { applyValueToContextBar(64); });
        
        QAction *lowAction = menu.addAction("Set to Low (32)");
        connect(lowAction, &QAction::triggered, this, [this]() { applyValueToContextBar(32); });
    } else {
        QAction *leftAction = menu.addAction("Set to Left (0)");
        connect(leftAction, &QAction::triggered, this, [this]() { applyValueToContextBar(0); });
        
        QAction *centerAction = menu.addAction("Set to Center (64)");
        connect(centerAction, &QAction::triggered, this, [this]() { applyValueToContextBar(64); });
        
        QAction *rightAction = menu.addAction("Set to Right (127)");
        connect(rightAction, &QAction::triggered, this, [this]() { applyValueToContextBar(127); });
    }
    
    menu.exec(event->globalPos());
    m_contextMenuBar = nullptr;
}

void NotePropertyEditor::onSetValueTriggered()
{
    if (!m_contextMenuBar) return;
    
    QString propName = (m_propertyType == PropertyType::Velocity) ? "Velocity" : "Pan";
    bool ok;
    int value = QInputDialog::getInt(this, QString("Set %1").arg(propName),
                                      QString("Enter %1 value (0-127):").arg(propName),
                                      m_contextMenuBar->value, 0, 127, 1, &ok);
    if (ok) {
        applyValueToContextBar(value);
    }
}

void NotePropertyEditor::onSnapToPreviousTriggered()
{
    if (!m_contextMenuBar) return;
    
    int prevValue = findNeighborValue(m_contextMenuBar, -1);
    if (prevValue >= 0) {
        applyValueToContextBar(prevValue);
    }
}

void NotePropertyEditor::onSnapToNextTriggered()
{
    if (!m_contextMenuBar) return;
    
    int nextValue = findNeighborValue(m_contextMenuBar, 1);
    if (nextValue >= 0) {
        applyValueToContextBar(nextValue);
    }
}

void NotePropertyEditor::onSnapToAverageTriggered()
{
    if (!m_contextMenuBar) return;
    
    int prevValue = findNeighborValue(m_contextMenuBar, -1);
    int nextValue = findNeighborValue(m_contextMenuBar, 1);
    
    int avg = -1;
    if (prevValue >= 0 && nextValue >= 0) {
        avg = (prevValue + nextValue) / 2;
    } else if (prevValue >= 0) {
        avg = prevValue;
    } else if (nextValue >= 0) {
        avg = nextValue;
    }
    
    if (avg >= 0) {
        applyValueToContextBar(avg);
    }
}

int NotePropertyEditor::findNeighborValue(NoteBar *bar, int direction)
{
    if (!bar || !bar->track) return -1;
    
    // Find the bar's position in the sorted note list
    std::vector<NN_Note_t> notes = bar->track->getNotes();
    if (notes.empty()) return -1;
    
    // Sort bars by x position (start time)
    std::vector<NoteBar*> sortedBars;
    for (auto &b : m_noteBars) {
        if (b.track == bar->track) {
            sortedBars.push_back(&b);
        }
    }
    std::sort(sortedBars.begin(), sortedBars.end(), 
              [](NoteBar *a, NoteBar *b) { return a->x < b->x; });
    
    // Find current bar index
    int idx = -1;
    for (size_t i = 0; i < sortedBars.size(); i++) {
        if (sortedBars[i] == bar) {
            idx = static_cast<int>(i);
            break;
        }
    }
    
    if (idx < 0) return -1;
    
    int neighborIdx = idx + direction;
    if (neighborIdx < 0 || neighborIdx >= static_cast<int>(sortedBars.size())) {
        return -1;
    }
    
    return sortedBars[neighborIdx]->value;
}

void NotePropertyEditor::applyValueToContextBar(int value)
{
    if (!m_contextMenuBar) return;
    
    value = qBound(0, value, 127);
    m_contextMenuBar->value = value;
    
    // Update the actual note in the engine
    std::vector<NN_Note_t> notes = m_contextMenuBar->track->getNotes();
    if (m_contextMenuBar->noteIndex >= 0 && m_contextMenuBar->noteIndex < (int)notes.size()) {
        if (m_propertyType == PropertyType::Velocity) {
            notes[m_contextMenuBar->noteIndex].velocity = value;
        } else if (m_propertyType == PropertyType::Pan) {
            notes[m_contextMenuBar->noteIndex].pan = value;
        }
        m_contextMenuBar->track->setNotes(notes);
        emit notePropertyChanged(m_contextMenuBar->track, m_contextMenuBar->noteIndex, value);
    }
    
    // Notify MIDI editor of change
    if (m_midiEditor) {
        m_midiEditor->update();
    }
    
    update();
}

void NotePropertyEditor::wheelEvent(QWheelEvent *event)
{
    // Forward wheel event to parent for synchronized scrolling
    // We can't call wheelEvent directly as it's protected, 
    // so we let the event bubble up through the event system
    event->ignore();
}

void NotePropertyEditor::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    int w = width();
    int rightMargin = 8;
    
    // Position controls at top right: [Track Name] [Vel] [Pan]
    int panX = w - rightMargin - m_panButton->width();
    int velX = panX - 4 - m_velocityButton->width();
    
    m_velocityButton->move(velX, 5);
    m_panButton->move(panX, 5);
    
    // Track name label left of buttons
    int labelWidth = velX - 10 - m_leftMargin;
    m_trackNameLabel->setFixedWidth(std::max(50, labelWidth));
    m_trackNameLabel->move(velX - 10 - m_trackNameLabel->width(), 5);
    
    // Value label at bottom left
    m_valueLabel->move(5, height() - 25);
}

void NotePropertyEditor::updateActiveTrack()
{
    // Determine active track - prefer the sequence's active track
    NoteNagaTrack *newActiveTrack = nullptr;
    
    if (m_engine) {
        NoteNagaMidiSeq *seq = m_engine->getProject()->getActiveSequence();
        if (seq) {
            // Use the sequence's active track (set by track list selection)
            newActiveTrack = seq->getActiveTrack();
        }
    }
    
    // Fallback: use track from selected notes
    if (!newActiveTrack && m_midiEditor) {
        auto selectedNotes = m_midiEditor->getSelectedNotes();
        if (!selectedNotes.empty()) {
            newActiveTrack = selectedNotes.front().first;
        }
    }
    
    // Last fallback: first visible track
    if (!newActiveTrack && m_engine) {
        NoteNagaMidiSeq *seq = m_engine->getProject()->getActiveSequence();
        if (seq) {
            const auto &tracks = seq->getTracks();
            for (NoteNagaTrack *track : tracks) {
                if (track && track->isVisible()) {
                    newActiveTrack = track;
                    break;
                }
            }
        }
    }
    
    if (newActiveTrack != m_activeTrack) {
        m_activeTrack = newActiveTrack;
        
        if (m_activeTrack) {
            // Update track name label
            QString name = QString::fromStdString(m_activeTrack->getName());
            if (name.isEmpty()) {
                name = tr("Track %1").arg(m_activeTrack->getId());
            }
            m_trackNameLabel->setText(name);
            
            // Update track color
            m_trackColor = m_activeTrack->getColor().toQColor();
        } else {
            m_trackNameLabel->setText(tr("No Track"));
            m_trackColor = QColor(80, 160, 220);  // Default blue
        }
        
        updateTrackColorStyles();
    }
}

void NotePropertyEditor::updateTrackColorStyles()
{
    // Create button styles using track color
    QColor baseColor = m_trackColor;
    QColor darkerColor = baseColor.darker(140);
    QColor lighterColor = baseColor.lighter(120);
    
    QString buttonStyle = QString(R"(
        QPushButton {
            background: #32353c;
            border: 1px solid #464a56;
            border-radius: 3px;
            color: #aaa;
            font-size: 10px;
        }
        QPushButton:hover { 
            background: %1; 
            color: #fff;
            border-color: %2;
        }
        QPushButton:checked { 
            background: %3; 
            color: #fff; 
            border-color: %4; 
        }
    )").arg(darkerColor.name(), baseColor.name(), baseColor.name(), lighterColor.name());
    
    m_velocityButton->setStyleSheet(buttonStyle);
    m_panButton->setStyleSheet(buttonStyle);
    
    // Update track name label color
    m_trackNameLabel->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;")
                                     .arg(baseColor.lighter(130).name()));
}
