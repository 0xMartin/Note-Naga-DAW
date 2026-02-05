#include "arrangement_resource_panel.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <QMouseEvent>
#include <QApplication>
#include <QDataStream>
#include <QPainter>

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

    // Header
    QLabel *header = new QLabel(tr("MIDI Sequences"), this);
    header->setStyleSheet("font-weight: bold; color: #cccccc; font-size: 12px;");
    layout->addWidget(header);

    // Sequence list - using custom draggable list
    m_sequenceList = new DraggableSequenceList(this);
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
    QAction *renameAction = menu.addAction(tr("Rename..."));
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(tr("Delete"));
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    
    QAction *selected = menu.exec(m_sequenceList->mapToGlobal(pos));
    
    if (selected == editAction) {
        emit editSequenceRequested(sequenceIndex);
    } else if (selected == renameAction) {
        renameSequence(sequenceIndex);
    } else if (selected == deleteAction) {
        deleteSequence(sequenceIndex);
    }
}

void ArrangementResourcePanel::renameSequence(int index)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    if (index < 0 || index >= static_cast<int>(sequences.size())) return;
    
    NoteNagaMidiSeq *seq = sequences[index];
    if (!seq) return;
    
    // Get current name
    QString currentName;
    if (!seq->getFilePath().empty()) {
        QString path = QString::fromStdString(seq->getFilePath());
        int lastSlash = path.lastIndexOf('/');
        currentName = (lastSlash >= 0) ? path.mid(lastSlash + 1) : path;
        int lastDot = currentName.lastIndexOf('.');
        if (lastDot > 0) currentName = currentName.left(lastDot);
    } else {
        currentName = tr("Sequence %1").arg(index + 1);
    }
    
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Sequence"),
                                            tr("Enter new name:"),
                                            QLineEdit::Normal, currentName, &ok);
    
    if (ok && !newName.isEmpty() && newName != currentName) {
        // For now, we store name in a simple way - could extend engine API
        // The name will be reflected in clip display
        emit sequenceRenamed(index, newName);
        refreshFromProject();
    }
}

void ArrangementResourcePanel::deleteSequence(int index)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    if (index < 0 || index >= static_cast<int>(sequences.size())) return;
    
    // Confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Delete Sequence"),
        tr("Are you sure you want to delete this sequence?\n"
           "This action cannot be undone."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        NoteNagaMidiSeq *seq = sequences[index];
        if (seq) {
            m_engine->getRuntimeData()->removeSequence(seq);
            emit sequenceDeleted(index);
            refreshFromProject();
        }
    }
}
