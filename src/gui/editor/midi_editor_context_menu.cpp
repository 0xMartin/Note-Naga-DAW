#include "midi_editor_context_menu.h"
#include "midi_editor_widget.h"

MidiEditorContextMenu::MidiEditorContextMenu(MidiEditorWidget *editor, QObject *parent)
    : QObject(parent), m_editor(editor), m_menu(nullptr), m_colorModeGroup(nullptr)
{
    createMenu();
}

void MidiEditorContextMenu::createMenu() {
    m_menu = new QMenu(m_editor);
    m_menu->setStyleSheet(
        "QMenu { background-color: #2d2d30; color: #e0e0e0; border: 1px solid #555; }"
        "QMenu::item:selected { background-color: #094771; }"
        "QMenu::separator { height: 1px; background: #555; margin: 4px 8px; }");
    
    // Edit actions
    createEditSubmenu(m_menu);
    m_menu->addSeparator();
    
    // Selection actions
    createSelectionSubmenu(m_menu);
    m_menu->addSeparator();
    
    // Transpose actions
    createTransposeSubmenu(m_menu);
    m_menu->addSeparator();
    
    // Velocity actions
    createVelocitySubmenu(m_menu);
    m_menu->addSeparator();
    
    // Move to track submenu
    createMoveToTrackSubmenu(m_menu);
    m_menu->addSeparator();
    
    // Color mode submenu
    createColorModeSubmenu(m_menu);
}

void MidiEditorContextMenu::createEditSubmenu(QMenu *parent) {
    QAction *cutAction = parent->addAction("Cut");
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, &MidiEditorContextMenu::cutRequested);
    
    QAction *copyAction = parent->addAction("Copy");
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &MidiEditorContextMenu::copyRequested);
    
    QAction *pasteAction = parent->addAction("Paste");
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, &MidiEditorContextMenu::pasteRequested);
    
    parent->addSeparator();
    
    QAction *deleteAction = parent->addAction("Delete");
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MidiEditorContextMenu::deleteNotesRequested);
    
    QAction *duplicateAction = parent->addAction("Duplicate");
    duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(duplicateAction, &QAction::triggered, this, &MidiEditorContextMenu::duplicateNotesRequested);
    
    parent->addSeparator();
    
    QAction *quantizeAction = parent->addAction("Quantize to Grid");
    quantizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(quantizeAction, &QAction::triggered, this, &MidiEditorContextMenu::quantizeRequested);
}

void MidiEditorContextMenu::createSelectionSubmenu(QMenu *parent) {
    QMenu *selectionMenu = parent->addMenu("Selection");
    
    QAction *selectAllAction = selectionMenu->addAction("Select All");
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, &MidiEditorContextMenu::selectAllRequested);
    
    QAction *invertAction = selectionMenu->addAction("Invert Selection");
    connect(invertAction, &QAction::triggered, this, &MidiEditorContextMenu::invertSelectionRequested);
}

void MidiEditorContextMenu::createTransposeSubmenu(QMenu *parent) {
    QMenu *transposeMenu = parent->addMenu("Transpose");
    
    QAction *upAction = transposeMenu->addAction("Up (+1 semitone)");
    upAction->setShortcut(QKeySequence(Qt::Key_Up));
    connect(upAction, &QAction::triggered, this, &MidiEditorContextMenu::transposeUpRequested);
    
    QAction *downAction = transposeMenu->addAction("Down (-1 semitone)");
    downAction->setShortcut(QKeySequence(Qt::Key_Down));
    connect(downAction, &QAction::triggered, this, &MidiEditorContextMenu::transposeDownRequested);
    
    transposeMenu->addSeparator();
    
    QAction *octaveUpAction = transposeMenu->addAction("Octave Up (+12)");
    octaveUpAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Up));
    connect(octaveUpAction, &QAction::triggered, this, &MidiEditorContextMenu::transposeOctaveUpRequested);
    
    QAction *octaveDownAction = transposeMenu->addAction("Octave Down (-12)");
    octaveDownAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Down));
    connect(octaveDownAction, &QAction::triggered, this, &MidiEditorContextMenu::transposeOctaveDownRequested);
}

void MidiEditorContextMenu::createVelocitySubmenu(QMenu *parent) {
    QMenu *velocityMenu = parent->addMenu("Set Velocity");
    
    struct VelocityPreset { QString name; int value; };
    std::vector<VelocityPreset> presets = {
        {"Pianissimo (pp)", 16},
        {"Piano (p)", 48},
        {"Mezzo-piano (mp)", 64},
        {"Mezzo-forte (mf)", 80},
        {"Forte (f)", 96},
        {"Fortissimo (ff)", 112},
        {"Maximum", 127}
    };
    
    for (const auto &preset : presets) {
        QAction *action = velocityMenu->addAction(preset.name);
        int vel = preset.value;
        connect(action, &QAction::triggered, this, [this, vel]() {
            emit setVelocityRequested(vel);
        });
    }
}

void MidiEditorContextMenu::createColorModeSubmenu(QMenu *parent) {
    QMenu *colorMenu = parent->addMenu("Note Colors");
    m_colorModeGroup = new QActionGroup(this);
    m_colorModeGroup->setExclusive(true);
    
    auto addColorAction = [&](const QString &name, NoteColorMode mode) {
        QAction *action = colorMenu->addAction(name);
        action->setCheckable(true);
        action->setChecked(mode == m_colorMode);
        action->setActionGroup(m_colorModeGroup);
        connect(action, &QAction::triggered, this, [this, mode]() {
            m_colorMode = mode;
            emit colorModeChanged(mode);
        });
        return action;
    };
    
    addColorAction("Track Color", NoteColorMode::TrackColor);
    addColorAction("Velocity", NoteColorMode::Velocity);
    addColorAction("Pan", NoteColorMode::Pan);
}

void MidiEditorContextMenu::createMoveToTrackSubmenu(QMenu *parent) {
    m_moveToTrackMenu = parent->addMenu("Move to Track");
    // Will be populated dynamically in show()
}

void MidiEditorContextMenu::show(const QPoint &globalPos, bool hasSelection) {
    // Enable/disable actions based on selection
    for (QAction *action : m_menu->actions()) {
        QString text = action->text();
        if (text == "Cut" || text == "Copy" || text == "Delete" || 
            text == "Duplicate" || text == "Quantize to Grid") {
            action->setEnabled(hasSelection);
        }
        if (text == "Transpose") {
            action->setEnabled(hasSelection);
            if (action->menu()) {
                action->menu()->setEnabled(hasSelection);
            }
        }
        if (text == "Set Velocity") {
            action->setEnabled(hasSelection);
            if (action->menu()) {
                action->menu()->setEnabled(hasSelection);
            }
        }
        if (text == "Move to Track") {
            action->setEnabled(hasSelection);
        }
    }
    
    // Populate "Move to Track" submenu with available tracks
    if (m_moveToTrackMenu) {
        m_moveToTrackMenu->clear();
        m_moveToTrackMenu->setEnabled(hasSelection);
        
        auto *seq = m_editor->getSequence();
        if (seq && hasSelection) {
            for (auto *track : seq->getTracks()) {
                QString trackName = QString::fromStdString(track->getName());
                int trackId = track->getId();
                QAction *action = m_moveToTrackMenu->addAction(
                    QString("%1 (Track %2)").arg(trackName).arg(trackId));
                connect(action, &QAction::triggered, this, [this, trackId]() {
                    emit moveToTrackRequested(trackId);
                });
            }
        }
    }
    
    m_menu->exec(globalPos);
}
