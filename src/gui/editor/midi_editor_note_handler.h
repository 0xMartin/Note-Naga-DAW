#pragma once

#include <QObject>
#include <QPointF>
#include <QList>
#include <QMap>
#include <QSet>
#include <list>
#include "midi_editor_types.h"

class MidiEditorWidget;
class QGraphicsScene;
class QAbstractGraphicsShapeItem;

/**
 * @brief Data for a copied note (includes track info)
 */
struct CopiedNote {
    int trackId;        // Original track ID
    int relativeStart;  // Offset from the first note's start
    int note;           // MIDI note number
    int length;
    int velocity;
    std::optional<int> pan;
};

/**
 * @brief Handles note selection, creation, modification and deletion.
 */
class MidiEditorNoteHandler : public QObject {
    Q_OBJECT
public:
    explicit MidiEditorNoteHandler(MidiEditorWidget *editor, QObject *parent = nullptr);
    ~MidiEditorNoteHandler() = default;
    
    // --- Selection ---
    void selectNote(NoteGraphics *noteGraphics, bool clearPrevious = true);
    void deselectNote(NoteGraphics *noteGraphics);
    void clearSelection();
    void selectNotesInRect(const QRectF &rect);
    void selectAll();
    void invertSelection();
    
    bool hasSelection() const { return !m_selectedNotes.isEmpty(); }
    const QList<NoteGraphics*>& selectedNotes() const { return m_selectedNotes; }
    std::vector<std::pair<NoteNagaTrack*, NN_Note_t>> getSelectedNotesData() const;
    
    // --- Note lookup ---
    NoteGraphics* findNoteUnderCursor(const QPointF &scenePos);
    bool isNoteEdge(NoteGraphics *note, const QPointF &scenePos);
    QRectF getRealNoteRect(const NoteGraphics *ng) const;
    
    // --- Note creation ---
    void addNewNote(const QPointF &scenePos);
    
    // --- Note manipulation ---
    void moveSelectedNotes(const QPointF &delta);
    void resizeSelectedNotes(const QPointF &delta);
    void applyNoteChanges();
    void deleteSelectedNotes();
    void duplicateSelectedNotes();
    void quantizeSelectedNotes();
    void moveSelectedNotesToTrack(NoteNagaTrack *targetTrack);
    
    // --- Copy/Paste ---
    void copySelectedNotes();
    void startPasteMode();
    void cancelPasteMode();
    void commitPaste(const QPointF &scenePos);
    bool isInPasteMode() const { return m_pasteMode; }
    void updatePastePreview(const QPointF &scenePos);
    
    // --- Transpose ---
    void transposeSelectedNotes(int semitones);
    
    // --- Velocity ---
    void setSelectedNotesVelocity(int velocity);
    
    // --- Drag state ---
    void startDrag(const QPointF &pos, NoteDragMode mode);
    void updateDrag(const QPointF &pos);
    void endDrag();
    NoteDragMode dragMode() const { return m_dragMode; }
    QPointF dragStartPos() const { return m_dragStartPos; }
    
    // --- Ghost preview ---
    void updateGhostPreview(const QPointF &currentPos);
    void clearGhostPreview();
    
    // --- Note items management ---
    QMap<int, std::list<NoteGraphics>>& noteItems() { return m_noteItems; }
    void clearNoteItems();
    void clearTrackNoteItems(int trackId);

signals:
    void selectionChanged();
    void notesModified();
    void pasteModeChanged(bool active);
    void noteTrackSelected(NoteNagaTrack *track);  ///< Emitted when a note is clicked to select its track

private:
    MidiEditorWidget *m_editor;
    
    QMap<int, std::list<NoteGraphics>> m_noteItems;
    QList<NoteGraphics*> m_selectedNotes;
    
    // Drag state
    NoteDragMode m_dragMode = NoteDragMode::None;
    QPointF m_dragStartPos;
    QPointF m_lastDragPos;
    QMap<NoteGraphics*, NN_Note_t> m_dragStartNoteStates;
    QList<QGraphicsItem*> m_ghostItems;
    
    // Copy/Paste state
    QList<CopiedNote> m_clipboard;
    bool m_pasteMode = false;
    int m_clipboardBaseNote = 64; // Reference note for vertical positioning
    
    static constexpr int RESIZE_EDGE_MARGIN = 5;
};
