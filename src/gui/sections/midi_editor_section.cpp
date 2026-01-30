#include "midi_editor_section.h"

#include "../dock_system/advanced_dock_widget.h"
#include "../editor/midi_editor_widget.h"
#include "../widgets/midi_control_bar_widget.h"
#include "../widgets/midi_keyboard_ruler.h"
#include "../widgets/midi_tact_ruler.h"
#include "../widgets/track_list_widget.h"
#include "../widgets/track_mixer_widget.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QDockWidget>
#include <QScrollBar>

MidiEditorSection::MidiEditorSection(NoteNagaEngine *engine, QWidget *parent)
    : QMainWindow(parent), m_engine(engine)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Set background
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupDockLayout();
    connectSignals();
}

MidiEditorSection::~MidiEditorSection()
{
}

void MidiEditorSection::setupDockLayout()
{
    // === Editor dock (center) ===
    QWidget *editorMain = new QWidget();
    QGridLayout *grid = new QGridLayout(editorMain);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    m_midiEditor = new MidiEditorWidget(m_engine, this);
    m_midiEditor->setMouseTracking(true);
    m_midiEditor->setMinimumWidth(250);
    m_midiEditor->setMinimumHeight(250);

    m_midiKeyboardRuler = new MidiKeyboardRuler(m_engine, 16, this);
    m_midiKeyboardRuler->setFixedWidth(60);
    m_midiTactRuler = new MidiTactRuler(m_engine, this);
    m_midiTactRuler->setTimeScale(m_midiEditor->getConfig()->time_scale);

    grid->addWidget(new QWidget(), 0, 0);
    grid->addWidget(m_midiTactRuler, 0, 1);
    grid->addWidget(m_midiKeyboardRuler, 1, 0);
    grid->addWidget(m_midiEditor, 1, 1);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(1, 1);

    QVBoxLayout *editorLayout = new QVBoxLayout();
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(editorMain, 1);
    
    m_controlBar = new MidiControlBarWidget(m_engine, this);
    editorLayout->addWidget(m_controlBar);
    
    QFrame *editorContainer = new QFrame();
    editorContainer->setObjectName("EditorContainer");
    editorContainer->setStyleSheet("QFrame#EditorContainer { border: 1px solid #19191f; }");
    editorContainer->setLayout(editorLayout);

    auto *editorDock = new AdvancedDockWidget(
        tr("MIDI Editor"), 
        QIcon(":/icons/midi.svg"),
        m_midiEditor->getTitleWidget(), 
        this
    );
    editorDock->setWidget(editorContainer);
    editorDock->setObjectName("editor");
    editorDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    editorDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                            QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, editorDock);
    m_docks["editor"] = editorDock;

    // === Track list dock (left) ===
    m_trackListWidget = new TrackListWidget(m_engine, this);
    
    auto *tracklistDock = new AdvancedDockWidget(
        tr("Tracks"), 
        QIcon(":/icons/track.svg"),
        m_trackListWidget->getTitleWidget(), 
        this
    );
    tracklistDock->setWidget(m_trackListWidget);
    tracklistDock->setObjectName("tracklist");
    tracklistDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    tracklistDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                               QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, tracklistDock);
    m_docks["tracklist"] = tracklistDock;

    // === Mixer dock (right) ===
    m_mixerWidget = new TrackMixerWidget(m_engine, this);
    
    auto *mixerDock = new AdvancedDockWidget(
        tr("Track Mixer"), 
        QIcon(":/icons/mixer.svg"),
        m_mixerWidget->getTitleWidget(), 
        this
    );
    mixerDock->setWidget(m_mixerWidget);
    mixerDock->setObjectName("mixer");
    mixerDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    mixerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                           QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, mixerDock);
    m_docks["mixer"] = mixerDock;

    // === Configure layout ===
    m_docks["editor"]->setParent(this);
    m_docks["tracklist"]->setParent(this);
    m_docks["mixer"]->setParent(this);

    tabifyDockWidget(m_docks["editor"], m_docks["mixer"]);
    splitDockWidget(m_docks["tracklist"], m_docks["editor"], Qt::Horizontal);
    m_docks["editor"]->raise();
    m_docks["tracklist"]->setFloating(false);
    m_docks["mixer"]->setFloating(false);

    for (auto dock : m_docks) {
        dock->setVisible(true);
    }

    // Adjust initial size ratios
    QList<QDockWidget*> order = {m_docks["tracklist"], m_docks["editor"], m_docks["mixer"]};
    QList<int> sizes = {200, 1000, 200};
    resizeDocks(order, sizes, Qt::Horizontal);
}

void MidiEditorSection::connectSignals()
{
    // Editor scroll signals
    connect(m_midiEditor, &MidiEditorWidget::horizontalScrollChanged, 
            m_midiTactRuler, &MidiTactRuler::setHorizontalScroll);
    connect(m_midiEditor, &MidiEditorWidget::timeScaleChanged, 
            m_midiTactRuler, &MidiTactRuler::setTimeScale);
    connect(m_midiEditor, &MidiEditorWidget::verticalScrollChanged, 
            m_midiKeyboardRuler, &MidiKeyboardRuler::setVerticalScroll);
    connect(m_midiEditor, &MidiEditorWidget::keyHeightChanged, 
            m_midiKeyboardRuler, &MidiKeyboardRuler::setRowHeight);
}

void MidiEditorSection::showHideDock(const QString &name, bool checked)
{
    QDockWidget *dock = m_docks.value(name, nullptr);
    if (!dock) return;

    if (checked) {
        if (!dock->parentWidget()) {
            if (name == "tracklist") {
                addDockWidget(Qt::LeftDockWidgetArea, dock);
            } else {
                addDockWidget(Qt::RightDockWidgetArea, dock);
                if (m_docks.contains("editor") && m_docks.contains("mixer")) {
                    tabifyDockWidget(m_docks["editor"], m_docks["mixer"]);
                }
            }
        }
        dock->show();
        dock->raise();
    } else {
        dock->hide();
    }
}

void MidiEditorSection::resetLayout()
{
    // Add back all docks
    if (!m_docks["tracklist"]->parentWidget()) {
        addDockWidget(Qt::LeftDockWidgetArea, m_docks["tracklist"]);
    }
    if (!m_docks["editor"]->parentWidget()) {
        addDockWidget(Qt::RightDockWidgetArea, m_docks["editor"]);
    }
    if (!m_docks["mixer"]->parentWidget()) {
        addDockWidget(Qt::RightDockWidgetArea, m_docks["mixer"]);
    }

    // Show all
    m_docks["tracklist"]->setVisible(true);
    m_docks["editor"]->setVisible(true);
    m_docks["mixer"]->setVisible(true);

    // Tabify editor and mixer
    tabifyDockWidget(m_docks["editor"], m_docks["mixer"]);
    m_docks["editor"]->raise();

    // Split between tracklist and editor
    splitDockWidget(m_docks["tracklist"], m_docks["editor"], Qt::Horizontal);

    // Set sizes
    QList<QDockWidget*> order = {m_docks["tracklist"], m_docks["editor"], m_docks["mixer"]};
    QList<int> sizes = {200, 1000, 200};
    resizeDocks(order, sizes, Qt::Horizontal);
}
