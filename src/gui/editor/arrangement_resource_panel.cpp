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
        
        QString name = tr("Sequence %1").arg(i + 1);
        
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
