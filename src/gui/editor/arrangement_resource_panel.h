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
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QFileDialog>

class NoteNagaEngine;
class NoteNagaMidiSeq;
class NoteNagaAudioResource;

// Custom MIME types for drag&drop
static const char* RESOURCE_MIME_TYPE_MIDI_SEQUENCE = "application/x-notenaga-midi-sequence";
static const char* RESOURCE_MIME_TYPE_AUDIO_CLIP = "application/x-notenaga-audio-clip";

/**
 * @brief Custom delegate for drawing MIDI sequence items with note preview
 */
class SequenceItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    
public:
    explicit SequenceItemDelegate(NoteNagaEngine *engine, QObject *parent = nullptr);
    
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    NoteNagaEngine *m_engine;
    
    void drawNotePreview(QPainter *painter, const QRect &rect, NoteNagaMidiSeq *seq, const QColor &color) const;
};

/**
 * @brief Custom delegate for drawing Audio resource items with waveform preview
 */
class AudioItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    
public:
    explicit AudioItemDelegate(NoteNagaEngine *engine, QObject *parent = nullptr);
    
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    NoteNagaEngine *m_engine;
    
    void drawWaveformPreview(QPainter *painter, const QRect &rect, 
                             NoteNagaAudioResource *resource, const QColor &color) const;
};

/**
 * @brief Custom list widget that handles drag with proper MIME data for MIDI sequences
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
    void startDrag(Qt::DropActions supportedActions) override;
};

/**
 * @brief Custom list widget for dragging audio resources
 */
class DraggableAudioList : public QListWidget
{
    Q_OBJECT
    
public:
    explicit DraggableAudioList(QWidget *parent = nullptr) : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }
    
protected:
    void startDrag(Qt::DropActions supportedActions) override;
};

/**
 * @brief Panel displaying available resources (MIDI sequences and Audio) for the Arrangement
 * 
 * Contains tabs for MIDI sequences and Audio files that can be dragged onto
 * the arrangement timeline to create clips.
 */
class ArrangementResourcePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ArrangementResourcePanel(NoteNagaEngine *engine, QWidget *parent = nullptr);

    /// Refresh the lists from project data
    void refreshFromProject();

signals:
    /// Emitted when user wants to edit a sequence
    void editSequenceRequested(int sequenceIndex);
    
    /// Emitted when user creates a new sequence
    void createSequenceRequested();
    
    /// Emitted when a MIDI sequence is deleted (for undo history cleanup)
    void sequenceDeleted(int sequenceId);
    
    /// Emitted when an audio resource is deleted (for undo history cleanup)
    void audioResourceDeleted(int resourceId);

public:
    /// Set the project file path (needed for audio recording)
    void setProjectFilePath(const QString &path) { m_projectFilePath = path; }

private slots:
    void onSequenceDoubleClicked(QListWidgetItem *item);
    void onAudioDoubleClicked(QListWidgetItem *item);
    void onCreateSequence();
    void onImportAudio();
    void onRecordAudio();
    void showSequenceContextMenu(const QPoint &pos);
    void showAudioContextMenu(const QPoint &pos);

private:
    void initUI();
    void initMidiTab(QWidget *tab);
    void initAudioTab(QWidget *tab);
    
    void refreshMidiList();
    void refreshAudioList();
    
    void renameSequence(int sequenceIndex);
    void deleteSequence(int sequenceIndex);
    void removeAudioResource(int resourceId);

    NoteNagaEngine *m_engine;
    QString m_projectFilePath;
    
    QTabWidget *m_tabWidget;
    
    // MIDI tab
    DraggableSequenceList *m_sequenceList;
    QPushButton *m_createSeqBtn;
    QLabel *m_midiInfoLabel;
    
    // Audio tab
    DraggableAudioList *m_audioList;
    QPushButton *m_importAudioBtn;
    QPushButton *m_recordAudioBtn;
    QLabel *m_audioInfoLabel;
};

#endif // ARRANGEMENT_RESOURCE_PANEL_H
