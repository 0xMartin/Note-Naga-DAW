#include "arrangement_resource_panel.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <QMouseEvent>
#include <QApplication>
#include <QDataStream>
#include <QPainter>

// SequenceItemDelegate implementation
SequenceItemDelegate::SequenceItemDelegate(NoteNagaEngine *engine, QObject *parent)
    : QStyledItemDelegate(parent), m_engine(engine)
{
}

void SequenceItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    painter->save();
    
    // Background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, QColor("#2563eb"));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, QColor("#2a2a35"));
    } else {
        painter->fillRect(option.rect, QColor("#252530"));
    }
    
    // Get sequence data
    int sequenceIndex = index.data(Qt::UserRole).toInt();
    int64_t durationTicks = index.data(Qt::UserRole + 1).toLongLong();
    QString displayText = index.data(Qt::DisplayRole).toString();
    QStringList lines = displayText.split('\n');
    QString name = lines.size() > 0 ? lines[0] : "";
    QString info = lines.size() > 1 ? lines[1] : "";
    
    // Get the sequence for preview
    NoteNagaMidiSeq *seq = nullptr;
    if (m_engine && m_engine->getRuntimeData()) {
        auto sequences = m_engine->getRuntimeData()->getSequences();
        if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(sequences.size())) {
            seq = sequences[sequenceIndex];
        }
    }
    
    // Layout: [Color bar][Preview Area][Text]
    int colorBarWidth = 4;
    int previewWidth = 80;
    int textMargin = 8;
    
    QRect colorBarRect(option.rect.left(), option.rect.top() + 2,
                       colorBarWidth, option.rect.height() - 4);
    QRect previewRect(option.rect.left() + colorBarWidth + 4, option.rect.top() + 4,
                      previewWidth, option.rect.height() - 8);
    QRect textRect(previewRect.right() + textMargin, option.rect.top(),
                   option.rect.width() - previewRect.right() - textMargin - 4, option.rect.height());
    
    // Generate a color based on sequence index
    QColor seqColor = QColor::fromHsl((sequenceIndex * 47) % 360, 180, 120);
    
    // Draw color bar
    painter->fillRect(colorBarRect, seqColor);
    
    // Draw preview area background
    painter->fillRect(previewRect, QColor("#1a1a20"));
    painter->setPen(QPen(QColor("#3a3a42"), 1));
    painter->drawRect(previewRect);
    
    // Draw note preview
    if (seq) {
        drawNotePreview(painter, previewRect.adjusted(1, 1, -1, -1), seq, seqColor.lighter(150));
    }
    
    // Draw text
    QColor textColor = (option.state & QStyle::State_Selected) ? Qt::white : QColor("#cccccc");
    QColor infoColor = (option.state & QStyle::State_Selected) ? QColor("#dddddd") : QColor("#888888");
    
    painter->setPen(textColor);
    QFont font = painter->font();
    font.setPointSize(11);
    font.setBold(true);
    painter->setFont(font);
    
    QRect nameRect = textRect.adjusted(0, 4, 0, -textRect.height()/2);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    
    painter->setPen(infoColor);
    font.setBold(false);
    font.setPointSize(10);
    painter->setFont(font);
    
    QRect infoRect = textRect.adjusted(0, textRect.height()/2, 0, -4);
    painter->drawText(infoRect, Qt::AlignLeft | Qt::AlignVCenter, info);
    
    // Draw bottom border
    painter->setPen(QColor("#3a3a42"));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    
    painter->restore();
}

QSize SequenceItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(200, 54);  // Fixed height for consistent appearance
}

void SequenceItemDelegate::drawNotePreview(QPainter *painter, const QRect &rect,
                                            NoteNagaMidiSeq *seq, const QColor &color) const
{
    if (!seq) return;
    
    int seqDuration = seq->getMaxTick();
    if (seqDuration <= 0) return;
    
    // Find note range
    int minNote = 127, maxNote = 0;
    for (auto *t : seq->getTracks()) {
        if (!t || t->isTempoTrack()) continue;
        for (const auto &n : t->getNotes()) {
            if (n.start.has_value()) {
                minNote = std::min(minNote, n.note);
                maxNote = std::max(maxNote, n.note);
            }
        }
    }
    
    if (minNote > maxNote) {
        minNote = 48; maxNote = 84;
    }
    int noteRange = std::max(12, maxNote - minNote + 1);
    
    // Draw notes
    for (auto *t : seq->getTracks()) {
        if (!t || t->isTempoTrack() || t->isMuted()) continue;
        
        for (const auto &n : t->getNotes()) {
            if (!n.start.has_value()) continue;
            
            int noteStart = n.start.value();
            int noteLength = n.length.value_or(120);
            int noteKey = n.note;
            
            // Calculate position
            float xRatio = float(noteStart) / float(seqDuration);
            float wRatio = float(noteLength) / float(seqDuration);
            int noteX = rect.left() + static_cast<int>(xRatio * rect.width());
            int noteW = std::max(1, static_cast<int>(wRatio * rect.width()));
            
            // Vertical position (scaled)
            float noteRelY = 1.0f - float(noteKey - minNote) / float(noteRange);
            int noteY = rect.top() + static_cast<int>(noteRelY * (rect.height() - 2));
            int noteH = std::max(1, rect.height() / noteRange);
            
            painter->fillRect(noteX, noteY, noteW, noteH, color);
        }
    }
}

ArrangementResourcePanel::ArrangementResourcePanel(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    initUI();
    refreshFromProject();
}

void ArrangementResourcePanel::initUI()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #1e1e24;
        }
        QLabel {
            color: #888888;
            font-size: 11px;
            padding: 4px;
        }
        QListWidget {
            background-color: #252530;
            border: none;
            color: #cccccc;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #3a3a42;
        }
        QListWidget::item:selected {
            background-color: #2563eb;
            color: white;
        }
        QListWidget::item:hover:!selected {
            background-color: #2a2a35;
        }
        QPushButton {
            background-color: #3a3a42;
            color: #cccccc;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #454550;
        }
        QPushButton:pressed {
            background-color: #2563eb;
        }
    )");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Sequence list - using custom draggable list with item delegate
    m_sequenceList = new DraggableSequenceList(this);
    m_sequenceList->setItemDelegate(new SequenceItemDelegate(m_engine, m_sequenceList));
    m_sequenceList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sequenceList, &QListWidget::customContextMenuRequested,
            this, &ArrangementResourcePanel::showContextMenu);
    layout->addWidget(m_sequenceList, 1);

    // Info label
    m_infoLabel = new QLabel(tr("Drag sequences to the timeline"), this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_infoLabel);

    // Create button
    m_createBtn = new QPushButton(tr("+ New Sequence"), this);
    layout->addWidget(m_createBtn);

    // Connections
    connect(m_sequenceList, &QListWidget::itemDoubleClicked, 
            this, &ArrangementResourcePanel::onItemDoubleClicked);
    connect(m_createBtn, &QPushButton::clicked, 
            this, &ArrangementResourcePanel::onCreateSequence);
}

void ArrangementResourcePanel::refreshFromProject()
{
    m_sequenceList->clear();
    
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    
    for (size_t i = 0; i < sequences.size(); ++i) {
        NoteNagaMidiSeq *seq = sequences[i];
        if (!seq) continue;
        
        QString name;
        // Use file path as display name if available
        if (!seq->getFilePath().empty()) {
            QString path = QString::fromStdString(seq->getFilePath());
            int lastSlash = path.lastIndexOf('/');
            if (lastSlash >= 0) {
                name = path.mid(lastSlash + 1);
            } else {
                name = path;
            }
            // Remove extension
            int lastDot = name.lastIndexOf('.');
            if (lastDot > 0) {
                name = name.left(lastDot);
            }
        } else {
            name = tr("Sequence %1").arg(i + 1);
        }
        
        // Calculate duration info
        int64_t durationTicks = seq->getMaxTick();
        int bars = static_cast<int>(durationTicks / (480 * 4)) + 1; // Assuming 4/4 time, 480 PPQ
        
        // Count notes across all tracks
        int noteCount = 0;
        for (auto *track : seq->getTracks()) {
            if (track) {
                noteCount += static_cast<int>(track->getNotes().size());
            }
        }
        
        QString displayText = QString("%1\n%2 bars, %3 notes")
            .arg(name)
            .arg(bars)
            .arg(noteCount);
        
        QListWidgetItem *item = new QListWidgetItem(m_sequenceList);
        item->setText(displayText);
        item->setData(Qt::UserRole, static_cast<int>(i));  // Sequence index
        item->setData(Qt::UserRole + 1, durationTicks);    // Duration for drag preview
        
        // Set tooltip with more info
        item->setToolTip(tr("Duration: %1 ticks\nNotes: %2\nDouble-click to edit")
                            .arg(durationTicks)
                            .arg(noteCount));
        
        m_sequenceList->addItem(item);
    }
    
    // Update info label
    if (sequences.empty()) {
        m_infoLabel->setText(tr("No sequences. Create one to get started."));
    } else {
        m_infoLabel->setText(tr("Drag sequences to the timeline"));
    }
}

void ArrangementResourcePanel::onItemDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        int sequenceIndex = item->data(Qt::UserRole).toInt();
        emit editSequenceRequested(sequenceIndex);
    }
}

void ArrangementResourcePanel::onCreateSequence()
{
    emit createSequenceRequested();
}

void ArrangementResourcePanel::showContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_sequenceList->itemAt(pos);
    if (!item) return;
    
    int sequenceIndex = item->data(Qt::UserRole).toInt();
    
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #2a2a30;
            color: #cccccc;
            border: 1px solid #4a4a52;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 20px;
        }
        QMenu::item:selected {
            background-color: #2563eb;
        }
    )");
    
    QAction *editAction = menu.addAction(tr("Edit Sequence"));
    QAction *renameAction = menu.addAction(tr("Rename Sequence"));
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(tr("Delete Sequence"));
    
    QAction *selected = menu.exec(m_sequenceList->mapToGlobal(pos));
    
    if (selected == editAction) {
        emit editSequenceRequested(sequenceIndex);
    } else if (selected == renameAction) {
        renameSequence(sequenceIndex);
    } else if (selected == deleteAction) {
        deleteSequence(sequenceIndex);
    }
}

void ArrangementResourcePanel::renameSequence(int sequenceIndex)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    if (sequenceIndex < 0 || sequenceIndex >= static_cast<int>(sequences.size())) return;
    
    NoteNagaMidiSeq *seq = sequences[sequenceIndex];
    if (!seq) return;
    
    // Get current name
    QString currentName;
    if (!seq->getFilePath().empty()) {
        QString path = QString::fromStdString(seq->getFilePath());
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash >= 0) {
            currentName = path.mid(lastSlash + 1);
        } else {
            currentName = path;
        }
        int lastDot = currentName.lastIndexOf('.');
        if (lastDot > 0) {
            currentName = currentName.left(lastDot);
        }
    } else {
        currentName = tr("Sequence %1").arg(sequenceIndex + 1);
    }
    
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Sequence"),
                                            tr("New name:"), QLineEdit::Normal,
                                            currentName, &ok);
    if (ok && !newName.isEmpty()) {
        // Set the new name as file path (without extension for display)
        seq->setFilePath(newName.toStdString());
        refreshFromProject();
    }
}

void ArrangementResourcePanel::deleteSequence(int sequenceIndex)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    if (sequenceIndex < 0 || sequenceIndex >= static_cast<int>(sequences.size())) return;
    
    NoteNagaMidiSeq *seq = sequences[sequenceIndex];
    if (!seq) return;
    
    // Confirm deletion
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Sequence"),
        tr("Are you sure you want to delete this sequence?\nThis action cannot be undone."),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_engine->getRuntimeData()->removeSequence(seq);
        refreshFromProject();
    }
}
