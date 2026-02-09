#include "external_midi_section.h"

#include <QtWidgets>
#include <QScrollArea>
#include <QCheckBox>

#include "../dock_system/advanced_dock_widget.h"
#include <note_naga_engine/synth/synth_external_midi.h>
#include <note_naga_engine/module/external_midi_router.h>
#include <note_naga_engine/note_naga_engine.h>

ExternalMidiSection::ExternalMidiSection(NoteNagaEngine* engine, QWidget* parent)
    : QMainWindow(parent), m_engine(engine), m_sectionActive(false)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Remove central widget - we only use docks
    setCentralWidget(nullptr);
    
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupUi();
    connectEngineSignals();
}

ExternalMidiSection::~ExternalMidiSection()
{
}

void ExternalMidiSection::onSectionActivated()
{
    m_sectionActive = true;
    refreshDevices();
    refreshTracks();
}

void ExternalMidiSection::onSectionDeactivated()
{
    m_sectionActive = false;
}

ExternalMidiRouting ExternalMidiSection::getRoutingForTrack(NoteNagaTrack* track) const
{
    if (m_midiTrackRouting.contains(track)) {
        return m_midiTrackRouting[track];
    }
    return ExternalMidiRouting();
}

ExternalMidiRouting ExternalMidiSection::getRoutingForArrangementTrack(NoteNagaArrangementTrack* track) const
{
    if (m_arrangementTrackRouting.contains(track)) {
        return m_arrangementTrackRouting[track];
    }
    return ExternalMidiRouting();
}

QStringList ExternalMidiSection::getAvailableDevices() const
{
    return m_availableDevices;
}

void ExternalMidiSection::setPlaybackMode(PlaybackMode mode)
{
    Q_UNUSED(mode);
    if (m_sectionActive) {
        refreshTracks();
    }
}

void ExternalMidiSection::refreshDevices()
{
    m_availableDevices.clear();
    
    // Get available MIDI output ports
    std::vector<std::string> ports = NoteNagaSynthExternalMidi::getAvailableMidiOutputPorts();
    for (const auto& port : ports) {
        m_availableDevices.append(QString::fromStdString(port));
    }
    
    updateDeviceList();
}

void ExternalMidiSection::refreshTracks()
{
    rebuildTrackList();
}

void ExternalMidiSection::onDeviceSelected(int index)
{
    Q_UNUSED(index);
    // Device selection from combo boxes in track rows is handled in onTrackRoutingChanged
}

void ExternalMidiSection::onDeviceItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    m_selectedDevice = item->text();
    
    // Update status label
    if (!m_selectedDevice.isEmpty()) {
        m_deviceStatusLabel->setText(tr("Selected: %1").arg(m_selectedDevice));
        m_deviceStatusLabel->setStyleSheet("color: #66bb6a;");
    }
}

void ExternalMidiSection::onTrackRoutingChanged()
{
    ExternalMidiRouter* router = m_engine->getExternalMidiRouter();
    if (!router) return;
    
    // Find which row triggered this and update its routing
    for (auto& row : m_trackRows) {
        if (!row.container) continue;
        
        ExternalMidiRouting routing;
        routing.enabled = row.enableCheck->isChecked();
        routing.deviceName = row.deviceCombo->currentText();
        routing.channel = row.channelSpin->value();
        
        // Convert UI routing to engine routing config
        ExternalMidiRoutingConfig config;
        config.enabled = routing.enabled;
        config.deviceName = routing.deviceName.toStdString();
        config.channel = routing.channel;
        
        if (row.midiTrack) {
            m_midiTrackRouting[row.midiTrack] = routing;
            router->setTrackRouting(row.midiTrack, config);
        } else if (row.arrangementTrack) {
            m_arrangementTrackRouting[row.arrangementTrack] = routing;
            router->setArrangementTrackRouting(row.arrangementTrack, config);
        }
    }
    
    emit routingChanged();
}

void ExternalMidiSection::onActiveSequenceChanged(NoteNagaMidiSeq* sequence)
{
    Q_UNUSED(sequence);
    if (m_sectionActive && !isArrangementMode()) {
        refreshTracks();
    }
}

void ExternalMidiSection::setupUi()
{
    // For dock-only layout, we use a dummy empty central widget
    QWidget *dummyCentral = new QWidget(this);
    dummyCentral->setMaximumSize(0, 0);
    setCentralWidget(dummyCentral);

    // Create no-content placeholder as overlay
    m_noContentLabel = new QLabel(tr("No MIDI devices found.\nConnect an external MIDI device and click Refresh."), this);
    m_noContentLabel->setAlignment(Qt::AlignCenter);
    m_noContentLabel->setStyleSheet("color: #666; font-size: 16px; background-color: #1a1a1f;");
    m_noContentLabel->setGeometry(rect());
    m_noContentLabel->hide();
    
    setupDockLayout();
}

void ExternalMidiSection::setupDockLayout()
{
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
    
    QString buttonStyle = R"(
        QPushButton {
            background-color: #3a3d45;
            color: #fff;
            border: 1px solid #4a4d55;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #4a4d55;
            border-color: #5a5d65;
        }
        QPushButton:pressed {
            background-color: #2a2d35;
        }
    )";

    // === LEFT DOCK: MIDI Devices ===
    QWidget *devicesContainer = new QWidget(this);
    devicesContainer->setStyleSheet("background: transparent;");
    QVBoxLayout *devicesLayout = new QVBoxLayout(devicesContainer);
    devicesLayout->setContentsMargins(10, 10, 10, 10);
    devicesLayout->setSpacing(10);

    // Device list header with refresh button
    QHBoxLayout *deviceHeaderLayout = new QHBoxLayout;
    QLabel *deviceHeaderLabel = new QLabel(tr("Available MIDI Output Devices"));
    deviceHeaderLabel->setStyleSheet("color: #79b8ff; font-size: 14px; font-weight: bold;");
    deviceHeaderLayout->addWidget(deviceHeaderLabel);
    deviceHeaderLayout->addStretch();
    
    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setStyleSheet(buttonStyle);
    m_refreshBtn->setIcon(QIcon(":/icons/refresh.svg"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &ExternalMidiSection::refreshDevices);
    deviceHeaderLayout->addWidget(m_refreshBtn);
    devicesLayout->addLayout(deviceHeaderLayout);

    // Device list
    m_deviceList = new QListWidget;
    m_deviceList->setStyleSheet(R"(
        QListWidget {
            background-color: #2a2d35;
            border: 1px solid #3a3d45;
            border-radius: 6px;
            color: #eee;
            font-size: 13px;
        }
        QListWidget::item {
            padding: 10px;
            border-bottom: 1px solid #3a3d45;
        }
        QListWidget::item:selected {
            background-color: #3d5a80;
        }
        QListWidget::item:hover {
            background-color: #3a3d45;
        }
    )");
    m_deviceList->setMinimumHeight(200);
    connect(m_deviceList, &QListWidget::itemClicked, this, &ExternalMidiSection::onDeviceItemClicked);
    devicesLayout->addWidget(m_deviceList);

    // Status label
    m_deviceStatusLabel = new QLabel(tr("No device selected"));
    m_deviceStatusLabel->setStyleSheet("color: #888; font-size: 12px;");
    devicesLayout->addWidget(m_deviceStatusLabel);

    // Info box
    QLabel *infoLabel = new QLabel(tr(
        "💡 Tip: External MIDI output sends notes to hardware synthesizers, "
        "drum machines, or other MIDI software in addition to internal synthesis."
    ));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(R"(
        QLabel {
            background-color: #2a3d35;
            border: 1px solid #3a5d45;
            border-radius: 6px;
            color: #aad4aa;
            padding: 10px;
            font-size: 12px;
        }
    )");
    devicesLayout->addWidget(infoLabel);
    
    devicesLayout->addStretch();

    auto *devicesDock = new AdvancedDockWidget(
        tr("MIDI Devices"), 
        QIcon(":/icons/midi.svg"),
        nullptr, 
        this
    );
    devicesDock->setWidget(devicesContainer);
    devicesDock->setObjectName("devices");
    devicesDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    devicesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    devicesDock->setMinimumWidth(300);
    addDockWidget(Qt::LeftDockWidgetArea, devicesDock);
    m_docks["devices"] = devicesDock;

    // === RIGHT DOCK: Track Routing ===
    QWidget *trackContainer = new QWidget(this);
    trackContainer->setStyleSheet("background: transparent;");
    QVBoxLayout *trackLayout = new QVBoxLayout(trackContainer);
    trackLayout->setContentsMargins(10, 10, 10, 10);
    trackLayout->setSpacing(10);

    // Header
    QLabel *trackHeaderLabel = new QLabel(tr("Track MIDI Output Routing"));
    trackHeaderLabel->setStyleSheet("color: #79b8ff; font-size: 14px; font-weight: bold;");
    trackLayout->addWidget(trackHeaderLabel);

    // Mode indicator
    QLabel *modeLabel = new QLabel;
    modeLabel->setObjectName("modeLabel");
    modeLabel->setStyleSheet("color: #888; font-size: 12px; padding: 5px 0;");
    trackLayout->addWidget(modeLabel);

    // Scroll area for track list
    m_trackScrollArea = new QScrollArea;
    m_trackScrollArea->setWidgetResizable(true);
    m_trackScrollArea->setFrameShape(QFrame::NoFrame);
    m_trackScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    
    m_trackListWidget = new QWidget;
    m_trackListWidget->setStyleSheet("background: transparent;");
    m_trackListLayout = new QVBoxLayout(m_trackListWidget);
    m_trackListLayout->setContentsMargins(0, 0, 0, 0);
    m_trackListLayout->setSpacing(5);
    m_trackListLayout->addStretch();
    
    m_trackScrollArea->setWidget(m_trackListWidget);
    trackLayout->addWidget(m_trackScrollArea, 1);

    auto *trackDock = new AdvancedDockWidget(
        tr("Track Routing"), 
        QIcon(":/icons/routing.svg"),
        nullptr, 
        this
    );
    trackDock->setWidget(trackContainer);
    trackDock->setObjectName("routing");
    trackDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    trackDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    trackDock->setMinimumWidth(400);
    addDockWidget(Qt::RightDockWidgetArea, trackDock);
    m_docks["routing"] = trackDock;
}

void ExternalMidiSection::connectEngineSignals()
{
    // Listen for sequence changes
    connect(m_engine->getRuntimeData(), &NoteNagaRuntimeData::activeSequenceChanged,
            this, &ExternalMidiSection::onActiveSequenceChanged);
}

void ExternalMidiSection::updateDeviceList()
{
    m_deviceList->clear();
    
    if (m_availableDevices.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("No MIDI output devices found"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(QColor("#888"));
        m_deviceList->addItem(item);
        m_deviceStatusLabel->setText(tr("No devices available"));
        m_deviceStatusLabel->setStyleSheet("color: #ff8866;");
    } else {
        for (const QString& device : m_availableDevices) {
            QListWidgetItem *item = new QListWidgetItem(QIcon(":/icons/midi.svg"), device);
            m_deviceList->addItem(item);
        }
        m_deviceStatusLabel->setText(tr("%1 device(s) found").arg(m_availableDevices.count()));
        m_deviceStatusLabel->setStyleSheet("color: #66bb6a;");
    }
}

void ExternalMidiSection::rebuildTrackList()
{
    clearTrackList();
    
    bool isArrangement = isArrangementMode();
    
    // Update mode label
    QLabel *modeLabel = m_trackListWidget->parentWidget()->findChild<QLabel*>("modeLabel");
    if (modeLabel) {
        if (isArrangement) {
            modeLabel->setText(tr("Mode: Arrangement - Routing arrangement tracks"));
        } else {
            modeLabel->setText(tr("Mode: MIDI Sequence - Routing MIDI tracks"));
        }
    }
    
    QString rowStyle = R"(
        QWidget#trackRow {
            background-color: #2a2d35;
            border: 1px solid #3a3d45;
            border-radius: 6px;
        }
        QWidget#trackRow:hover {
            border-color: #4a4d55;
        }
    )";
    
    QString comboStyle = R"(
        QComboBox {
            background-color: #3a3d45;
            color: #eee;
            border: 1px solid #4a4d55;
            border-radius: 4px;
            padding: 4px 8px;
            min-width: 120px;
        }
        QComboBox:hover {
            border-color: #5a5d65;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #2a2d35;
            color: #eee;
            selection-background-color: #3d5a80;
        }
    )";
    
    QString spinStyle = R"(
        QSpinBox {
            background-color: #3a3d45;
            color: #eee;
            border: 1px solid #4a4d55;
            border-radius: 4px;
            padding: 4px;
            min-width: 50px;
        }
        QSpinBox:hover {
            border-color: #5a5d65;
        }
    )";

    if (isArrangement) {
        // Build list from arrangement tracks
        NoteNagaArrangement* arrangement = m_engine->getRuntimeData()->getArrangement();
        if (!arrangement) return;
        
        for (NoteNagaArrangementTrack* arrTrack : arrangement->getTracks()) {
            if (!arrTrack) continue;
            
            TrackRoutingRow row;
            row.midiTrack = nullptr;
            row.arrangementTrack = arrTrack;
            
            row.container = new QWidget;
            row.container->setObjectName("trackRow");
            row.container->setStyleSheet(rowStyle);
            
            QHBoxLayout *rowLayout = new QHBoxLayout(row.container);
            rowLayout->setContentsMargins(10, 8, 10, 8);
            rowLayout->setSpacing(10);
            
            // Enable checkbox
            row.enableCheck = new QCheckBox;
            row.enableCheck->setToolTip(tr("Enable external MIDI output"));
            ExternalMidiRouting existingRouting = getRoutingForArrangementTrack(arrTrack);
            row.enableCheck->setChecked(existingRouting.enabled);
            connect(row.enableCheck, &QCheckBox::toggled, this, &ExternalMidiSection::onTrackRoutingChanged);
            rowLayout->addWidget(row.enableCheck);
            
            // Track name with color indicator
            QWidget *nameWidget = new QWidget;
            QHBoxLayout *nameLayout = new QHBoxLayout(nameWidget);
            nameLayout->setContentsMargins(0, 0, 0, 0);
            nameLayout->setSpacing(5);
            
            QLabel *colorLabel = new QLabel;
            colorLabel->setFixedSize(12, 12);
            colorLabel->setStyleSheet(QString("background-color: %1; border-radius: 6px;")
                .arg(arrTrack->getColor().toQColor().name()));
            nameLayout->addWidget(colorLabel);
            
            QString trackName = QString::fromStdString(arrTrack->getName());
            if (trackName.isEmpty()) trackName = tr("Untitled Track");
            row.nameLabel = new QLabel(trackName);
            row.nameLabel->setStyleSheet("color: #eee; font-weight: bold;");
            row.nameLabel->setMinimumWidth(120);
            nameLayout->addWidget(row.nameLabel);
            nameLayout->addStretch();
            rowLayout->addWidget(nameWidget, 1);
            
            // Device combo
            QLabel *deviceLabel = new QLabel(tr("Device:"));
            deviceLabel->setStyleSheet("color: #888;");
            rowLayout->addWidget(deviceLabel);
            
            row.deviceCombo = new QComboBox;
            row.deviceCombo->setStyleSheet(comboStyle);
            row.deviceCombo->addItem(tr("(None)"));
            for (const QString& device : m_availableDevices) {
                row.deviceCombo->addItem(device);
            }
            if (!existingRouting.deviceName.isEmpty()) {
                int idx = row.deviceCombo->findText(existingRouting.deviceName);
                if (idx >= 0) row.deviceCombo->setCurrentIndex(idx);
            }
            connect(row.deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                    this, &ExternalMidiSection::onTrackRoutingChanged);
            rowLayout->addWidget(row.deviceCombo);
            
            // Channel spinbox
            QLabel *channelLabel = new QLabel(tr("Ch:"));
            channelLabel->setStyleSheet("color: #888;");
            rowLayout->addWidget(channelLabel);
            
            row.channelSpin = new QSpinBox;
            row.channelSpin->setStyleSheet(spinStyle);
            row.channelSpin->setRange(1, 16);
            row.channelSpin->setValue(existingRouting.channel);
            connect(row.channelSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
                    this, &ExternalMidiSection::onTrackRoutingChanged);
            rowLayout->addWidget(row.channelSpin);
            
            m_trackRows.append(row);
            m_trackListLayout->insertWidget(m_trackListLayout->count() - 1, row.container);
        }
    } else {
        // Build list from MIDI sequence tracks
        NoteNagaMidiSeq* seq = m_engine->getRuntimeData()->getActiveSequence();
        if (!seq) return;
        
        for (NoteNagaTrack* track : seq->getTracks()) {
            if (!track || track->isTempoTrack()) continue;
            
            TrackRoutingRow row;
            row.midiTrack = track;
            row.arrangementTrack = nullptr;
            
            row.container = new QWidget;
            row.container->setObjectName("trackRow");
            row.container->setStyleSheet(rowStyle);
            
            QHBoxLayout *rowLayout = new QHBoxLayout(row.container);
            rowLayout->setContentsMargins(10, 8, 10, 8);
            rowLayout->setSpacing(10);
            
            // Enable checkbox
            row.enableCheck = new QCheckBox;
            row.enableCheck->setToolTip(tr("Enable external MIDI output"));
            ExternalMidiRouting existingRouting = getRoutingForTrack(track);
            row.enableCheck->setChecked(existingRouting.enabled);
            connect(row.enableCheck, &QCheckBox::toggled, this, &ExternalMidiSection::onTrackRoutingChanged);
            rowLayout->addWidget(row.enableCheck);
            
            // Track name with color indicator
            QWidget *nameWidget = new QWidget;
            QHBoxLayout *nameLayout = new QHBoxLayout(nameWidget);
            nameLayout->setContentsMargins(0, 0, 0, 0);
            nameLayout->setSpacing(5);
            
            QLabel *colorLabel = new QLabel;
            colorLabel->setFixedSize(12, 12);
            colorLabel->setStyleSheet(QString("background-color: %1; border-radius: 6px;")
                .arg(track->getColor().toQColor().name()));
            nameLayout->addWidget(colorLabel);
            
            QString trackName = QString::fromStdString(track->getName());
            if (trackName.isEmpty()) {
                // Find track index by iterating
                int trackIdx = 0;
                for (NoteNagaTrack* t : seq->getTracks()) {
                    if (t == track) break;
                    if (t && !t->isTempoTrack()) trackIdx++;
                }
                trackName = tr("Track %1").arg(trackIdx + 1);
            }
            row.nameLabel = new QLabel(trackName);
            row.nameLabel->setStyleSheet("color: #eee; font-weight: bold;");
            row.nameLabel->setMinimumWidth(120);
            nameLayout->addWidget(row.nameLabel);
            nameLayout->addStretch();
            rowLayout->addWidget(nameWidget, 1);
            
            // Device combo
            QLabel *deviceLabel = new QLabel(tr("Device:"));
            deviceLabel->setStyleSheet("color: #888;");
            rowLayout->addWidget(deviceLabel);
            
            row.deviceCombo = new QComboBox;
            row.deviceCombo->setStyleSheet(comboStyle);
            row.deviceCombo->addItem(tr("(None)"));
            for (const QString& device : m_availableDevices) {
                row.deviceCombo->addItem(device);
            }
            if (!existingRouting.deviceName.isEmpty()) {
                int idx = row.deviceCombo->findText(existingRouting.deviceName);
                if (idx >= 0) row.deviceCombo->setCurrentIndex(idx);
            }
            connect(row.deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                    this, &ExternalMidiSection::onTrackRoutingChanged);
            rowLayout->addWidget(row.deviceCombo);
            
            // Channel spinbox
            QLabel *channelLabel = new QLabel(tr("Ch:"));
            channelLabel->setStyleSheet("color: #888;");
            rowLayout->addWidget(channelLabel);
            
            row.channelSpin = new QSpinBox;
            row.channelSpin->setStyleSheet(spinStyle);
            row.channelSpin->setRange(1, 16);
            row.channelSpin->setValue(existingRouting.channel);
            connect(row.channelSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
                    this, &ExternalMidiSection::onTrackRoutingChanged);
            rowLayout->addWidget(row.channelSpin);
            
            m_trackRows.append(row);
            m_trackListLayout->insertWidget(m_trackListLayout->count() - 1, row.container);
        }
    }
}

void ExternalMidiSection::clearTrackList()
{
    for (auto& row : m_trackRows) {
        if (row.container) {
            row.container->deleteLater();
        }
    }
    m_trackRows.clear();
}

bool ExternalMidiSection::isArrangementMode() const
{
    return m_engine->getPlaybackWorker()->getPlaybackMode() == PlaybackMode::Arrangement;
}

void ExternalMidiSection::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_noContentLabel && m_noContentLabel->isVisible()) {
        m_noContentLabel->setGeometry(rect());
    }
}

void ExternalMidiSection::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_sectionActive) {
        refreshDevices();
        refreshTracks();
    }
}

