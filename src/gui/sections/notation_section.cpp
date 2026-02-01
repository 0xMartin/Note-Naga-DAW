#include "notation_section.h"

#include <QtWidgets>
#include <QScrollArea>
#include <QDockWidget>
#include <QTimer>

#include "../widgets/lilypond_widget.h"
#include "../widgets/midi_control_bar_widget.h"
#include "../dock_system/advanced_dock_widget.h"
#include <note_naga_engine/nn_utils.h>

NotationSection::NotationSection(NoteNagaEngine *engine, QWidget *parent)
    : QMainWindow(parent)
    , m_engine(engine)
    , m_sequence(nullptr)
    , m_sectionActive(false)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Remove central widget - we only use docks
    setCentralWidget(nullptr);
    
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupUi();
    connectSignals();
}

NotationSection::~NotationSection()
{
}

void NotationSection::onSectionActivated()
{
    m_sectionActive = true;
    refreshSequence();
}

void NotationSection::onSectionDeactivated()
{
    m_sectionActive = false;
    // Nothing heavy to stop - notation view doesn't run background processes
}

void NotationSection::setupUi()
{
    // For dock-only layout, we use a dummy empty central widget
    QWidget *dummyCentral = new QWidget(this);
    dummyCentral->setMaximumSize(0, 0);
    setCentralWidget(dummyCentral);
    
    // Create no-sequence placeholder as overlay
    m_noSequenceLabel = new QLabel(tr("No MIDI sequence loaded.\nOpen a MIDI file to view notation."), this);
    m_noSequenceLabel->setAlignment(Qt::AlignCenter);
    m_noSequenceLabel->setStyleSheet("color: #666; font-size: 16px; background-color: #1a1a1f;");
    m_noSequenceLabel->setGeometry(rect());
    m_noSequenceLabel->raise();
    
    setupDockLayout();
    
    // Initially hide docks and show placeholder
    for (auto dock : m_docks) {
        dock->hide();
    }
    m_noSequenceLabel->show();
}

void NotationSection::setupDockLayout()
{
    // === LEFT DOCK: Notation View ===
    QWidget *notationContainer = new QWidget(this);
    notationContainer->setStyleSheet("background: #2a2d35;");
    QVBoxLayout *notationLayout = new QVBoxLayout(notationContainer);
    notationLayout->setContentsMargins(5, 5, 5, 5);
    notationLayout->setSpacing(5);
    
    // Create LilyPond widget
    m_lilypondWidget = new LilyPondWidget(m_engine, this);
    
    if (!m_lilypondWidget->isAvailable()) {
        qWarning() << "LilyPond not available:" << m_lilypondWidget->getErrorMessage();
    }
    
    notationLayout->addWidget(m_lilypondWidget, 1);
    
    // Control bar at bottom
    m_controlBar = new MidiControlBarWidget(m_engine, this);
    notationLayout->addWidget(m_controlBar);
    
    // Create title buttons widget (Refresh, Print)
    QWidget *titleButtons = m_lilypondWidget->createTitleButtonWidget(this);
    
    auto *notationDock = new AdvancedDockWidget(
        tr("Score"),
        QIcon(":/icons/midi.svg"),  // TODO: Add notation icon
        titleButtons,
        this
    );
    notationDock->setWidget(notationContainer);
    notationDock->setObjectName("notation");
    notationDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    notationDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, notationDock);
    m_docks["notation"] = notationDock;
    
    // === RIGHT DOCK: Settings ===
    m_settingsScrollArea = new QScrollArea;
    m_settingsScrollArea->setWidgetResizable(true);
    m_settingsScrollArea->setFrameShape(QFrame::NoFrame);
    m_settingsScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_settingsScrollArea->setMinimumWidth(250);
    
    m_settingsWidget = new QWidget;
    m_settingsWidget->setStyleSheet("background: transparent;");
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsWidget);
    settingsLayout->setContentsMargins(10, 10, 10, 10);
    settingsLayout->setSpacing(10);
    
    QString groupBoxStyle = R"(
        QGroupBox {
            background: #2a2d35;
            border: 1px solid #3a3d45;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 5px;
            color: #79b8ff;
            font-weight: bold;
        }
    )";
    
    // === Track Visibility ===
    m_trackVisibilityGroup = new QGroupBox(tr("Track Visibility"));
    m_trackVisibilityGroup->setStyleSheet(groupBoxStyle);
    m_trackVisibilityLayout = new QVBoxLayout(m_trackVisibilityGroup);
    // Checkboxes will be added dynamically when sequence is loaded
    
    settingsLayout->addWidget(m_trackVisibilityGroup);
    
    // === Info Label ===
    QLabel *infoLabel = new QLabel(tr(
        "<p style='color: #888; font-size: 11px;'>"
        "Use the toolbar buttons above the score to:<br>"
        "• <b>Refresh</b> - Re-render the notation<br>"
        "• <b>+/-</b> - Zoom in/out<br><br>"
        "Notation is rendered using LilyPond."
        "</p>"
    ));
    infoLabel->setWordWrap(true);
    settingsLayout->addWidget(infoLabel);
    
    // Add stretch at bottom
    settingsLayout->addStretch();
    
    m_settingsScrollArea->setWidget(m_settingsWidget);
    
    auto *settingsDock = new AdvancedDockWidget(
        tr("Settings"),
        QIcon(":/icons/settings.svg"),
        nullptr,
        this
    );
    settingsDock->setWidget(m_settingsScrollArea);
    settingsDock->setObjectName("settings");
    settingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    settingsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, settingsDock);
    m_docks["settings"] = settingsDock;
    
    // Configure dock layout
    splitDockWidget(m_docks["notation"], m_docks["settings"], Qt::Horizontal);
    
    // Set initial size ratios (larger notation view)
    QList<QDockWidget*> order = {m_docks["notation"], m_docks["settings"]};
    QList<int> sizes = {800, 250};
    resizeDocks(order, sizes, Qt::Horizontal);
}

void NotationSection::connectSignals()
{
    // Connect to engine signals
    connect(m_engine->getProject(), &NoteNagaProject::activeSequenceChanged,
            this, &NotationSection::onSequenceChanged);
    
    // Playback tick updates - connect to project's currentTickChanged
    connect(m_engine->getProject(), &NoteNagaProject::currentTickChanged,
            this, &NotationSection::onPlaybackTickChanged);
    
    // Control bar playback signals
    connect(m_controlBar, &MidiControlBarWidget::playToggled, this, [this]() {
        if (m_engine->isPlaying()) {
            m_engine->stopPlayback();
        } else {
            m_engine->startPlayback();
        }
    });
    connect(m_controlBar, &MidiControlBarWidget::goToStart, this, [this]() {
        m_engine->getProject()->setCurrentTick(0);
    });
    connect(m_controlBar, &MidiControlBarWidget::goToEnd, this, [this]() {
        if (m_sequence) {
            m_engine->getProject()->setCurrentTick(m_sequence->getMaxTick());
        }
    });
}

void NotationSection::onSequenceChanged()
{
    refreshSequence();
}

void NotationSection::refreshSequence()
{
    m_sequence = m_engine->getProject()->getActiveSequence();
    
    if (!m_sequence) {
        // Show placeholder, hide docks
        for (auto dock : m_docks) {
            dock->hide();
        }
        m_noSequenceLabel->setGeometry(rect());
        m_noSequenceLabel->show();
        m_noSequenceLabel->raise();
        return;
    }
    
    // Hide placeholder, show docks
    m_noSequenceLabel->hide();
    for (auto dock : m_docks) {
        dock->show();
    }
    
    // Update track visibility checkboxes
    updateTrackVisibilityCheckboxes();
    
    // Set sequence title from file path
    QString filePath = QString::fromStdString(m_sequence->getFilePath());
    QString title;
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        title = fileInfo.completeBaseName();  // Get filename without extension
    }
    if (title.isEmpty()) {
        title = tr("Untitled");
    }
    m_lilypondWidget->setTitle(title);
    
    // Update LilyPond widget
    m_lilypondWidget->setSequence(m_sequence);
}

void NotationSection::updateTrackVisibilityCheckboxes()
{
    // Remove old checkboxes
    for (auto *cb : m_trackVisibilityCheckboxes) {
        m_trackVisibilityLayout->removeWidget(cb);
        delete cb;
    }
    m_trackVisibilityCheckboxes.clear();
    
    if (!m_sequence) return;
    
    auto tracks = m_sequence->getTracks();
    for (int i = 0; i < tracks.size(); ++i) {
        QString name = QString::fromStdString(tracks[i]->getName());
        if (name.isEmpty()) {
            name = tr("Track %1").arg(i + 1);
        }
        
        QCheckBox *cb = new QCheckBox(name);
        cb->setChecked(true);  // All tracks visible by default
        
        // Get track color for icon
        NN_Color_t trackColor = tracks[i]->getColor();
        QColor color(trackColor.red, trackColor.green, trackColor.blue);
        
        // Create colored icon
        QPixmap colorPixmap(12, 12);
        colorPixmap.fill(color);
        cb->setIcon(QIcon(colorPixmap));
        
        connect(cb, &QCheckBox::toggled, this, [this](bool) {
            // Collect visibility state from all checkboxes
            QList<bool> visibility;
            for (auto *checkbox : m_trackVisibilityCheckboxes) {
                visibility.append(checkbox->isChecked());
            }
            m_lilypondWidget->setTrackVisibility(visibility);
        });
        
        m_trackVisibilityLayout->addWidget(cb);
        m_trackVisibilityCheckboxes.append(cb);
    }
}

void NotationSection::onPlaybackTickChanged(int tick)
{
    if (!m_sectionActive) return;
    
    // TODO: Implement playback highlighting
    Q_UNUSED(tick);
}
