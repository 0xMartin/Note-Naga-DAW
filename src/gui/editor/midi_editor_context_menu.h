#pragma once

#include <QObject>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QPoint>
#include "midi_editor_types.h"

class MidiEditorWidget;

/**
 * @brief Handles context menu creation and actions for the MIDI editor.
 */
class MidiEditorContextMenu : public QObject {
    Q_OBJECT
public:
    explicit MidiEditorContextMenu(MidiEditorWidget *editor, QObject *parent = nullptr);
    ~MidiEditorContextMenu() = default;
    
    /**
     * @brief Shows the context menu at the specified position.
     * @param globalPos The global screen position for the menu.
     * @param hasSelection Whether there are selected notes.
     */
    void show(const QPoint &globalPos, bool hasSelection);
    
    /**
     * @brief Gets the current note color mode.
     */
    NoteColorMode colorMode() const { return m_colorMode; }

signals:
    void colorModeChanged(NoteColorMode mode);
    void deleteNotesRequested();
    void duplicateNotesRequested();
    void selectAllRequested();
    void invertSelectionRequested();
    void quantizeRequested();
    void transposeUpRequested();
    void transposeDownRequested();
    void transposeOctaveUpRequested();
    void transposeOctaveDownRequested();
    void setVelocityRequested(int velocity);
    void copyRequested();
    void cutRequested();
    void pasteRequested();

private:
    void createMenu();
    void createColorModeSubmenu(QMenu *parent);
    void createEditSubmenu(QMenu *parent);
    void createSelectionSubmenu(QMenu *parent);
    void createTransposeSubmenu(QMenu *parent);
    void createVelocitySubmenu(QMenu *parent);
    
    MidiEditorWidget *m_editor;
    QMenu *m_menu;
    NoteColorMode m_colorMode = NoteColorMode::TrackColor;
    QActionGroup *m_colorModeGroup;
};
