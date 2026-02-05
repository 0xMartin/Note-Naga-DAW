#ifndef ARRANGEMENT_RESOURCE_PANEL_H
#define ARRANGEMENT_RESOURCE_PANEL_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMimeData>
#include <QDrag>
#include <QPainter>

class NoteNagaEngine;

// Custom MIME type for MIDI sequences
static const char* RESOURCE_MIME_TYPE_MIDI_SEQUENCE = "application/x-notenaga-midi-sequence";

/**
 * @brief Custom list widget that handles drag with proper MIME data
 */
class DraggableSequenceList : public QListWidget
{
    Q_OBJECT
    
public:
    explicit DraggableSequenceList(QWidget *parent = nullptr) : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }
    
protected:
    void startDrag(Qt::DropActions supportedActions) override {
        Q_UNUSED(supportedActions);
        
        QListWidgetItem *item = currentItem();
        if (!item) return;
        
        int sequenceIndex = item->data(Qt::UserRole).toInt();
        int64_t duration = item->data(Qt::UserRole + 1).toLongLong();
        
        // Create mime data
        QMimeData *mimeData = new QMimeData();
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        stream << sequenceIndex << duration;
        mimeData->setData(RESOURCE_MIME_TYPE_MIDI_SEQUENCE, encodedData);
        mimeData->setText(item->text().split('\n').first());
        
        // Create drag
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        
        // Create drag pixmap
        QPixmap pixmap(120, 40);
        pixmap.fill(QColor("#3a3a42"));
        QPainter painter(&pixmap);
        painter.setPen(QColor("#2563eb"));
        painter.drawRect(0, 0, pixmap.width() - 1, pixmap.height() - 1);
        painter.setPen(Qt::white);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, item->text().split('\n').first());
        painter.end();
        
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
        
        drag->exec(Qt::CopyAction);
    }
};

/**
 * @brief Panel displaying available MIDI sequences for the Arrangement
 * 
 * Lists all MIDI sequences in the project that can be dragged onto
 * the arrangement timeline to create clips.
 */
class ArrangementResourcePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ArrangementResourcePanel(NoteNagaEngine *engine, QWidget *parent = nullptr);

    /// Refresh the list from project data
    void refreshFromProject();

signals:
    /// Emitted when user wants to edit a sequence
    void editSequenceRequested(int sequenceIndex);
    
    /// Emitted when user creates a new sequence
    void createSequenceRequested();

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);
    void onCreateSequence();

private:
    void initUI();

    NoteNagaEngine *m_engine;
    
    DraggableSequenceList *m_sequenceList;
    QPushButton *m_createBtn;
    QLabel *m_infoLabel;
};

#endif // ARRANGEMENT_RESOURCE_PANEL_H
