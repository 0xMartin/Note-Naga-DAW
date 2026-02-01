#include "notation_section.h"

#include <QtWidgets>
#include <QScrollArea>
#include <QDockWidget>
#include <QTimer>

#include "../widgets/verovio_widget.h"
#include "../widgets/midi_control_bar_widget.h"
#include "../dock_system/advanced_dock_widget.h"
#include <note_naga_engine/nn_utils.h>

NotationSection::NotationSection(NoteNagaEngine *engine, QWidget *parent)
    : QMainWindow(parent)
    , m_engine(engine)
    , m_sequence(nullptr)
    , m_sectionActive(false)
    , m_autoRenderDone(false)
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
    
    // Create Verovio widget
    m_notationWidget = new VerovioWidget(m_engine, this);
    
    if (!m_notationWidget->isAvailable()) {
        qWarning() << "Verovio not available:" << m_notationWidget->getErrorMessage();
    }
    
    notationLayout->addWidget(m_notationWidget, 1);
    
    // Control bar at bottom
    m_controlBar = new MidiControlBarWidget(m_engine, this);
    notationLayout->addWidget(m_controlBar);
    
    // Create title buttons widget (Refresh, Print)
    QWidget *titleButtons = m_notationWidget->createTitleButtonWidget(this);
    
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
    
    // === Notation Settings ===
    m_notationSettingsGroup = new QGroupBox(tr("Notation Settings"));
    m_notationSettingsGroup->setStyleSheet(groupBoxStyle);
    QFormLayout *notationFormLayout = new QFormLayout(m_notationSettingsGroup);
    notationFormLayout->setContentsMargins(10, 15, 10, 10);
    notationFormLayout->setSpacing(8);
    
    QString comboStyle = R"(
        QComboBox {
            background: #3a3d45;
            border: 1px solid #4a4d55;
            border-radius: 4px;
            padding: 4px 8px;
            color: white;
            min-width: 100px;
        }
        QComboBox:hover { border-color: #5a5d65; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background: #2a2d35;
            border: 1px solid #4a4d55;
            selection-background-color: #4a9eff;
        }
    )";
    
    QString spinBoxStyle = R"(
        QSpinBox {
            background: #3a3d45;
            border: 1px solid #4a4d55;
            border-radius: 4px;
            padding: 4px 8px;
            color: white;
        }
        QSpinBox:hover { border-color: #5a5d65; }
    )";
    
    // Key Signature (Verovio uses -7 to +7 format)
    m_keySignatureCombo = new QComboBox;
    m_keySignatureCombo->setStyleSheet(comboStyle);
    m_keySignatureCombo->addItem(tr("C Major / A minor"), "0");
    m_keySignatureCombo->addItem(tr("G Major / E minor"), "1");
    m_keySignatureCombo->addItem(tr("D Major / B minor"), "2");
    m_keySignatureCombo->addItem(tr("A Major / F# minor"), "3");
    m_keySignatureCombo->addItem(tr("E Major / C# minor"), "4");
    m_keySignatureCombo->addItem(tr("B Major / G# minor"), "5");
    m_keySignatureCombo->addItem(tr("F Major / D minor"), "-1");
    m_keySignatureCombo->addItem(tr("Bb Major / G minor"), "-2");
    m_keySignatureCombo->addItem(tr("Eb Major / C minor"), "-3");
    m_keySignatureCombo->addItem(tr("Ab Major / F minor"), "-4");
    notationFormLayout->addRow(tr("Key:"), m_keySignatureCombo);
    
    // Time Signature
    m_timeSignatureCombo = new QComboBox;
    m_timeSignatureCombo->setStyleSheet(comboStyle);
    m_timeSignatureCombo->addItem("4/4", "4/4");
    m_timeSignatureCombo->addItem("3/4", "3/4");
    m_timeSignatureCombo->addItem("2/4", "2/4");
    m_timeSignatureCombo->addItem("6/8", "6/8");
    m_timeSignatureCombo->addItem("2/2", "2/2");
    m_timeSignatureCombo->addItem("3/8", "3/8");
    m_timeSignatureCombo->addItem("12/8", "12/8");
    notationFormLayout->addRow(tr("Time:"), m_timeSignatureCombo);
    
    // Scale (replaces font size)
    m_scaleSpinBox = new QSpinBox;
    m_scaleSpinBox->setStyleSheet(spinBoxStyle);
    m_scaleSpinBox->setRange(20, 80);
    m_scaleSpinBox->setValue(40);
    m_scaleSpinBox->setSuffix("%");
    notationFormLayout->addRow(tr("Scale:"), m_scaleSpinBox);
    
    // Show Bar Numbers
    m_showBarNumbersCheckbox = new QCheckBox(tr("Show bar numbers"));
    m_showBarNumbersCheckbox->setChecked(true);
    m_showBarNumbersCheckbox->setStyleSheet("QCheckBox { color: #ccc; }");
    notationFormLayout->addRow("", m_showBarNumbersCheckbox);
    
    // Show Title
    m_showTitleCheckbox = new QCheckBox(tr("Show title"));
    m_showTitleCheckbox->setChecked(true);
    m_showTitleCheckbox->setStyleSheet("QCheckBox { color: #ccc; }");
    notationFormLayout->addRow("", m_showTitleCheckbox);
    
    // Show Tempo
    m_showTempoCheckbox = new QCheckBox(tr("Show tempo"));
    m_showTempoCheckbox->setChecked(true);
    m_showTempoCheckbox->setStyleSheet("QCheckBox { color: #ccc; }");
    notationFormLayout->addRow("", m_showTempoCheckbox);
    
    // Show Instrument Names
    m_showInstrumentNamesCheckbox = new QCheckBox(tr("Show instrument names"));
    m_showInstrumentNamesCheckbox->setChecked(true);
    m_showInstrumentNamesCheckbox->setStyleSheet("QCheckBox { color: #ccc; }");
    notationFormLayout->addRow("", m_showInstrumentNamesCheckbox);
    
    // Separator
    QFrame *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("QFrame { color: #4a4d55; }");
    notationFormLayout->addRow(separator);
    
    // Composer
    m_composerEdit = new QLineEdit;
    m_composerEdit->setStyleSheet(R"(
        QLineEdit {
            background: #3a3d45;
            border: 1px solid #4a4d55;
            border-radius: 4px;
            padding: 4px 8px;
            color: white;
        }
        QLineEdit:focus { border-color: #4a9eff; }
    )");
    m_composerEdit->setPlaceholderText(tr("Enter composer name..."));
    notationFormLayout->addRow(tr("Composer:"), m_composerEdit);
    
    // Page Size
    m_pageSizeCombo = new QComboBox;
    m_pageSizeCombo->setStyleSheet(comboStyle);
    m_pageSizeCombo->addItem(tr("A4 (210×297mm)"), QSize(2100, 2970));
    m_pageSizeCombo->addItem(tr("Letter (216×279mm)"), QSize(2160, 2790));
    m_pageSizeCombo->addItem(tr("A3 (297×420mm)"), QSize(2970, 4200));
    m_pageSizeCombo->addItem(tr("Legal (216×356mm)"), QSize(2160, 3560));
    notationFormLayout->addRow(tr("Page:"), m_pageSizeCombo);
    
    // Landscape
    m_landscapeCheckbox = new QCheckBox(tr("Landscape orientation"));
    m_landscapeCheckbox->setStyleSheet("QCheckBox { color: #ccc; }");
    notationFormLayout->addRow("", m_landscapeCheckbox);
    
    settingsLayout->addWidget(m_notationSettingsGroup);
    
    // === Info Label ===
    QLabel *infoLabel = new QLabel(tr(
        "<p style='color: #888; font-size: 11px;'>"
        "Click the <img src=':/icons/reload.svg' width='14' height='14' style='vertical-align: middle;'> Render button in the dock title "
        "to apply settings and re-render the notation.<br><br>"
        "Notation is rendered using Verovio."
        "</p>"
    ));
    infoLabel->setWordWrap(true);
    infoLabel->setTextFormat(Qt::RichText);
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
    
    // Connect notation settings changes to apply function
    connect(m_keySignatureCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NotationSection::applyNotationSettings);
    connect(m_timeSignatureCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NotationSection::applyNotationSettings);
    connect(m_scaleSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NotationSection::applyNotationSettings);
    connect(m_showBarNumbersCheckbox, &QCheckBox::toggled,
            this, &NotationSection::applyNotationSettings);
    connect(m_showTitleCheckbox, &QCheckBox::toggled,
            this, &NotationSection::applyNotationSettings);
    connect(m_showTempoCheckbox, &QCheckBox::toggled,
            this, &NotationSection::applyNotationSettings);
    connect(m_showInstrumentNamesCheckbox, &QCheckBox::toggled,
            this, &NotationSection::applyNotationSettings);
    connect(m_composerEdit, &QLineEdit::textChanged,
            this, &NotationSection::applyNotationSettings);
    connect(m_pageSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NotationSection::applyNotationSettings);
    connect(m_landscapeCheckbox, &QCheckBox::toggled,
            this, &NotationSection::applyNotationSettings);
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
    m_notationWidget->setTitle(title);
    
    // Update Verovio widget
    m_notationWidget->setSequence(m_sequence);
    
    // Auto-render on first open if sequence has notes
    if (!m_autoRenderDone && m_sectionActive) {
        // Check if sequence has at least one track with notes
        auto tracks = m_sequence->getTracks();
        bool hasNotes = false;
        for (const auto &track : tracks) {
            if (track && !track->getNotes().empty()) {
                hasNotes = true;
                break;
            }
        }
        
        if (hasNotes && m_notationWidget->isAvailable()) {
            m_autoRenderDone = true;
            // Use QTimer to delay render slightly so UI is fully set up
            QTimer::singleShot(100, m_notationWidget, &VerovioWidget::render);
        }
    }
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
        // Only first track visible by default (for cleaner notation view)
        cb->setChecked(i == 0);
        
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
            m_notationWidget->setTrackVisibility(visibility);
        });
        
        m_trackVisibilityLayout->addWidget(cb);
        m_trackVisibilityCheckboxes.append(cb);
    }
    
    // Apply initial visibility (only first track)
    QList<bool> initialVisibility;
    for (int i = 0; i < m_trackVisibilityCheckboxes.size(); ++i) {
        initialVisibility.append(i == 0);
    }
    m_notationWidget->setTrackVisibility(initialVisibility);
}

void NotationSection::onPlaybackTickChanged(int tick)
{
    // Only update highlighting when section is active (visible)
    if (!m_sectionActive) return;
    
    // Only update if we have a valid widget and it's not rendering
    if (!m_notationWidget || m_notationWidget->isRendering()) return;
    
    // Update playback position highlighting in the notation widget
    m_notationWidget->setPlaybackPosition(tick);
}

void NotationSection::applyNotationSettings()
{
    if (!m_notationWidget) return;
    
    VerovioWidget::NotationSettings settings;
    settings.keySignature = m_keySignatureCombo->currentData().toString();
    settings.timeSignature = m_timeSignatureCombo->currentData().toString();
    settings.scale = m_scaleSpinBox->value();
    settings.showBarNumbers = m_showBarNumbersCheckbox->isChecked();
    settings.showTitle = m_showTitleCheckbox->isChecked();
    settings.showTempo = m_showTempoCheckbox->isChecked();
    settings.showInstrumentNames = m_showInstrumentNamesCheckbox->isChecked();
    settings.composer = m_composerEdit->text();
    
    QSize pageSize = m_pageSizeCombo->currentData().toSize();
    settings.pageWidth = pageSize.width();
    settings.pageHeight = pageSize.height();
    settings.landscape = m_landscapeCheckbox->isChecked();
    
    m_notationWidget->setNotationSettings(settings);
}
