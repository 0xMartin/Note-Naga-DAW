#include "project_section.h"
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/project_serializer.h>
#include <note_naga_engine/synth/synth_fluidsynth.h>
#include <note_naga_engine/synth/synth_external_midi.h>

#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QDateTime>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>

#include "../dock_system/advanced_dock_widget.h"

ProjectSection::ProjectSection(NoteNagaEngine *engine,
                               NoteNagaProjectSerializer *serializer,
                               QWidget *parent)
    : QMainWindow(parent)
    , m_engine(engine)
    , m_serializer(serializer)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Remove central widget - we only use docks
    QWidget *dummyCentral = new QWidget(this);
    dummyCentral->setMaximumSize(0, 0);
    setCentralWidget(dummyCentral);
    
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupDockLayout();
}

void ProjectSection::setupDockLayout()
{
    // === LEFT DOCK: Metadata ===
    QWidget *metadataWidget = createMetadataWidget();
    
    auto *metadataDock = new AdvancedDockWidget(
        tr("Project Metadata"),
        QIcon(":/icons/project.svg"),
        nullptr,
        this
    );
    metadataDock->setWidget(metadataWidget);
    metadataDock->setObjectName("metadata");
    metadataDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    metadataDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, metadataDock);
    m_docks["metadata"] = metadataDock;
    
    // === RIGHT TOP DOCK: Statistics ===
    QWidget *statisticsWidget = createStatisticsWidget();
    
    auto *statisticsDock = new AdvancedDockWidget(
        tr("Statistics"),
        QIcon(":/icons/chart.svg"),
        nullptr,
        this
    );
    statisticsDock->setWidget(statisticsWidget);
    statisticsDock->setObjectName("statistics");
    statisticsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    statisticsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, statisticsDock);
    m_docks["statistics"] = statisticsDock;
    
    // === RIGHT MIDDLE DOCK: Synthesizers ===
    QWidget *synthWidget = createSynthesizerWidget();
    
    auto *synthDock = new AdvancedDockWidget(
        tr("Synthesizers"),
        QIcon(":/icons/synth.svg"),
        nullptr,
        this
    );
    synthDock->setWidget(synthWidget);
    synthDock->setObjectName("synthesizers");
    synthDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    synthDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, synthDock);
    m_docks["synthesizers"] = synthDock;
    
    // === RIGHT BOTTOM DOCK: Actions ===
    QWidget *actionsWidget = createActionsWidget();
    
    auto *actionsDock = new AdvancedDockWidget(
        tr("Quick Actions"),
        QIcon(":/icons/actions.svg"),
        nullptr,
        this
    );
    actionsDock->setWidget(actionsWidget);
    actionsDock->setObjectName("actions");
    actionsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    actionsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, actionsDock);
    m_docks["actions"] = actionsDock;
    
    // Stack statistics, synthesizers, and actions vertically
    splitDockWidget(statisticsDock, synthDock, Qt::Vertical);
    splitDockWidget(synthDock, actionsDock, Qt::Vertical);
}

QWidget* ProjectSection::createMetadataWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setStyleSheet("background: #2a2d35;");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(widget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);
    
    QString labelStyle = "color: #6a7580; font-size: 11px; font-weight: 500;";
    QString inputStyle = R"(
        QLineEdit, QTextEdit {
            background: #1e2228;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 3px;
            padding: 5px 8px;
            font-size: 12px;
        }
        QLineEdit:focus, QTextEdit:focus {
            border-color: #3477c0;
        }
    )";
    QString readonlyStyle = "color: #8899a6; font-size: 11px;";
    
    // Project Name
    QLabel *nameLabel = new QLabel(tr("NAME"));
    nameLabel->setStyleSheet(labelStyle);
    mainLayout->addWidget(nameLabel);
    m_projectNameEdit = new QLineEdit;
    m_projectNameEdit->setStyleSheet(inputStyle);
    m_projectNameEdit->setPlaceholderText(tr("Project name"));
    m_projectNameEdit->setMaximumHeight(28);
    connect(m_projectNameEdit, &QLineEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    mainLayout->addWidget(m_projectNameEdit);
    
    // Author
    QLabel *authorLabel = new QLabel(tr("AUTHOR"));
    authorLabel->setStyleSheet(labelStyle);
    mainLayout->addWidget(authorLabel);
    m_authorEdit = new QLineEdit;
    m_authorEdit->setStyleSheet(inputStyle);
    m_authorEdit->setPlaceholderText(tr("Author name"));
    m_authorEdit->setMaximumHeight(28);
    connect(m_authorEdit, &QLineEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    mainLayout->addWidget(m_authorEdit);
    
    // Description
    QLabel *descLabel = new QLabel(tr("DESCRIPTION"));
    descLabel->setStyleSheet(labelStyle);
    mainLayout->addWidget(descLabel);
    m_descriptionEdit = new QTextEdit;
    m_descriptionEdit->setStyleSheet(inputStyle);
    m_descriptionEdit->setPlaceholderText(tr("Optional description"));
    m_descriptionEdit->setMaximumHeight(60);
    connect(m_descriptionEdit, &QTextEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    mainLayout->addWidget(m_descriptionEdit);
    
    // Separator
    QFrame *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background: #3a4654; max-height: 1px;");
    mainLayout->addWidget(sep);
    
    // Info grid for timestamps and path
    QGridLayout *infoGrid = new QGridLayout;
    infoGrid->setContentsMargins(0, 4, 0, 0);
    infoGrid->setHorizontalSpacing(12);
    infoGrid->setVerticalSpacing(4);
    
    QLabel *createdLabel = new QLabel(tr("Created:"));
    createdLabel->setStyleSheet(labelStyle);
    infoGrid->addWidget(createdLabel, 0, 0);
    m_createdAtLabel = new QLabel("-");
    m_createdAtLabel->setStyleSheet(readonlyStyle);
    infoGrid->addWidget(m_createdAtLabel, 0, 1);
    
    QLabel *modifiedLabel = new QLabel(tr("Modified:"));
    modifiedLabel->setStyleSheet(labelStyle);
    infoGrid->addWidget(modifiedLabel, 1, 0);
    m_modifiedAtLabel = new QLabel("-");
    m_modifiedAtLabel->setStyleSheet(readonlyStyle);
    infoGrid->addWidget(m_modifiedAtLabel, 1, 1);
    
    QLabel *pathLabel = new QLabel(tr("Path:"));
    pathLabel->setStyleSheet(labelStyle);
    infoGrid->addWidget(pathLabel, 2, 0, Qt::AlignTop);
    m_filePathLabel = new QLabel("-");
    m_filePathLabel->setStyleSheet(readonlyStyle);
    m_filePathLabel->setWordWrap(true);
    infoGrid->addWidget(m_filePathLabel, 2, 1);
    
    infoGrid->setColumnStretch(1, 1);
    mainLayout->addLayout(infoGrid);
    
    mainLayout->addStretch();
    
    return widget;
}

QWidget* ProjectSection::createStatisticsWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setStyleSheet("background: #2a2d35;");
    
    QGridLayout *layout = new QGridLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setHorizontalSpacing(16);
    layout->setVerticalSpacing(6);
    
    QString labelStyle = "color: #6a7580; font-size: 11px;";
    QString valueStyle = "color: #d4d8de; font-size: 12px; font-weight: 500;";
    
    // Row 0
    QLabel *tracksLabel = new QLabel(tr("Tracks"));
    tracksLabel->setStyleSheet(labelStyle);
    layout->addWidget(tracksLabel, 0, 0);
    
    m_trackCountLabel = new QLabel("0");
    m_trackCountLabel->setStyleSheet(valueStyle);
    layout->addWidget(m_trackCountLabel, 0, 1);
    
    QLabel *notesLabel = new QLabel(tr("Notes"));
    notesLabel->setStyleSheet(labelStyle);
    layout->addWidget(notesLabel, 0, 2);
    
    m_noteCountLabel = new QLabel("0");
    m_noteCountLabel->setStyleSheet(valueStyle);
    layout->addWidget(m_noteCountLabel, 0, 3);
    
    // Row 1
    QLabel *tempoLabel = new QLabel(tr("Tempo"));
    tempoLabel->setStyleSheet(labelStyle);
    layout->addWidget(tempoLabel, 1, 0);
    
    m_tempoLabel = new QLabel("120 BPM");
    m_tempoLabel->setStyleSheet(valueStyle);
    layout->addWidget(m_tempoLabel, 1, 1);
    
    QLabel *ppqLabel = new QLabel(tr("PPQ"));
    ppqLabel->setStyleSheet(labelStyle);
    layout->addWidget(ppqLabel, 1, 2);
    
    m_ppqLabel = new QLabel("480");
    m_ppqLabel->setStyleSheet(valueStyle);
    layout->addWidget(m_ppqLabel, 1, 3);
    
    // Row 2
    QLabel *durationLabel = new QLabel(tr("Duration"));
    durationLabel->setStyleSheet(labelStyle);
    layout->addWidget(durationLabel, 2, 0);
    
    m_durationLabel = new QLabel("0:00");
    m_durationLabel->setStyleSheet(valueStyle);
    layout->addWidget(m_durationLabel, 2, 1);
    
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);
    layout->setRowStretch(3, 1);
    
    return widget;
}

QWidget* ProjectSection::createSynthesizerWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setStyleSheet("background: #2a2d35;");
    
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    
    QString listStyle = R"(
        QListWidget {
            background: #1e2228;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 3px;
            padding: 2px;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 5px 8px;
            border-radius: 2px;
        }
        QListWidget::item:selected {
            background: #3477c0;
        }
        QListWidget::item:hover:!selected {
            background: #2d3640;
        }
    )";
    
    m_synthList = new QListWidget;
    m_synthList->setStyleSheet(listStyle);
    m_synthList->setMinimumHeight(80);
    connect(m_synthList, &QListWidget::itemSelectionChanged, this, &ProjectSection::onSynthSelectionChanged);
    layout->addWidget(m_synthList);
    
    QString buttonStyle = R"(
        QPushButton {
            background: #2d3640;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 3px;
            padding: 5px 10px;
            font-size: 11px;
        }
        QPushButton:hover {
            background: #3a4654;
        }
        QPushButton:pressed {
            background: #4a6080;
        }
        QPushButton:disabled {
            background: #1e2228;
            color: #556677;
        }
    )";
    
    QString comboStyle = R"(
        QComboBox {
            background: #1e2228;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 3px;
            padding: 4px 8px;
            font-size: 11px;
            min-width: 90px;
        }
        QComboBox:hover {
            border-color: #4a6080;
        }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox QAbstractItemView {
            background: #1e2228;
            border: 1px solid #3a4654;
            selection-background-color: #3477c0;
        }
    )";
    
    // Add row: type combo + add button
    QHBoxLayout *addLayout = new QHBoxLayout;
    addLayout->setSpacing(6);
    
    m_synthTypeCombo = new QComboBox;
    m_synthTypeCombo->setStyleSheet(comboStyle);
    m_synthTypeCombo->addItem(tr("FluidSynth"), "fluidsynth");
    m_synthTypeCombo->addItem(tr("External MIDI"), "external_midi");
    addLayout->addWidget(m_synthTypeCombo);
    
    m_addSynthBtn = new QPushButton(tr("Add"));
    m_addSynthBtn->setStyleSheet(buttonStyle);
    connect(m_addSynthBtn, &QPushButton::clicked, this, &ProjectSection::onAddSynthClicked);
    addLayout->addWidget(m_addSynthBtn);
    
    addLayout->addStretch();
    layout->addLayout(addLayout);
    
    // Action buttons row
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(6);
    
    m_configureSynthBtn = new QPushButton(tr("Configure"));
    m_configureSynthBtn->setStyleSheet(buttonStyle);
    m_configureSynthBtn->setEnabled(false);
    connect(m_configureSynthBtn, &QPushButton::clicked, this, &ProjectSection::onConfigureSynthClicked);
    btnLayout->addWidget(m_configureSynthBtn);
    
    m_renameSynthBtn = new QPushButton(tr("Rename"));
    m_renameSynthBtn->setStyleSheet(buttonStyle);
    m_renameSynthBtn->setEnabled(false);
    connect(m_renameSynthBtn, &QPushButton::clicked, this, &ProjectSection::onRenameSynthClicked);
    btnLayout->addWidget(m_renameSynthBtn);
    
    m_removeSynthBtn = new QPushButton(tr("Remove"));
    m_removeSynthBtn->setStyleSheet(buttonStyle);
    m_removeSynthBtn->setEnabled(false);
    connect(m_removeSynthBtn, &QPushButton::clicked, this, &ProjectSection::onRemoveSynthClicked);
    btnLayout->addWidget(m_removeSynthBtn);
    
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    return widget;
}

QWidget* ProjectSection::createActionsWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setStyleSheet("background: #2a2d35;");
    
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    QString buttonStyle = R"(
        QPushButton {
            background: #2d3640;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 3px;
            padding: 6px 12px;
            font-size: 11px;
        }
        QPushButton:hover {
            background: #3a4654;
        }
        QPushButton:pressed {
            background: #4a6080;
        }
    )";

    QString primaryButtonStyle = R"(
        QPushButton {
            background: #3477c0;
            color: #ffffff;
            border: none;
            border-radius: 3px;
            padding: 6px 12px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #4a8ad0;
        }
        QPushButton:pressed {
            background: #2a6090;
        }
    )";
    
    m_saveBtn = new QPushButton(tr("Save"));
    m_saveBtn->setStyleSheet(primaryButtonStyle);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProjectSection::onSaveClicked);
    layout->addWidget(m_saveBtn);
    
    m_saveAsBtn = new QPushButton(tr("Save As..."));
    m_saveAsBtn->setStyleSheet(buttonStyle);
    m_saveAsBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &ProjectSection::onSaveAsClicked);
    layout->addWidget(m_saveAsBtn);
    
    m_exportMidiBtn = new QPushButton(tr("Export MIDI..."));
    m_exportMidiBtn->setStyleSheet(buttonStyle);
    m_exportMidiBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportMidiBtn, &QPushButton::clicked, this, &ProjectSection::onExportMidiClicked);
    layout->addWidget(m_exportMidiBtn);
    
    layout->addStretch();
    
    return widget;
}

void ProjectSection::refreshSynthesizerList()
{
    m_synthList->clear();
    
    if (!m_engine) return;
    
    const auto &synths = m_engine->getSynthesizers();
    for (NoteNagaSynthesizer *synth : synths) {
        if (synth) {
            QString name = QString::fromStdString(synth->getName());
            if (name.isEmpty()) {
                name = tr("Unnamed Synth");
            }
            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(synth)));
            m_synthList->addItem(item);
        }
    }
    
    bool hasSelection = m_synthList->currentRow() >= 0;
    m_renameSynthBtn->setEnabled(hasSelection);
    m_removeSynthBtn->setEnabled(hasSelection);
    m_configureSynthBtn->setEnabled(hasSelection);
}

void ProjectSection::onSynthSelectionChanged()
{
    bool hasSelection = m_synthList->currentRow() >= 0;
    m_renameSynthBtn->setEnabled(hasSelection);
    m_removeSynthBtn->setEnabled(hasSelection);
    m_configureSynthBtn->setEnabled(hasSelection);
}

void ProjectSection::onRenameSynthClicked()
{
    int row = m_synthList->currentRow();
    if (row < 0) return;
    
    const auto &synths = m_engine->getSynthesizers();
    if (row >= static_cast<int>(synths.size())) return;
    
    NoteNagaSynthesizer *synth = synths[row];
    if (!synth) return;
    
    bool ok;
    QString newName = QInputDialog::getText(
        this,
        tr("Rename Synthesizer"),
        tr("Enter new name:"),
        QLineEdit::Normal,
        QString::fromStdString(synth->getName()),
        &ok
    );
    
    if (ok && !newName.isEmpty()) {
        synth->setName(newName.toStdString());
        m_synthList->item(row)->setText(newName);
        onMetadataEdited(); // Mark as unsaved
    }
}

void ProjectSection::onAddSynthClicked()
{
    QString type = m_synthTypeCombo->currentData().toString();
    QString baseName;
    
    if (type == "fluidsynth") {
        baseName = "FluidSynth";
    } else if (type == "external_midi") {
        baseName = "External MIDI";
    } else {
        return;
    }
    
    // Generate unique name
    int suffix = 1;
    QString finalName;
    bool nameExists;
    do {
        nameExists = false;
        finalName = suffix == 1 ? baseName : QString("%1 %2").arg(baseName).arg(suffix);
        
        for (int i = 0; i < m_synthList->count(); ++i) {
            if (m_synthList->item(i)->text() == finalName) {
                nameExists = true;
                break;
            }
        }
        if (nameExists) suffix++;
    } while (nameExists);
    
    try {
        NoteNagaSynthesizer *newSynth = nullptr;
        
        if (type == "fluidsynth") {
            newSynth = new NoteNagaSynthFluidSynth(finalName.toStdString(), "");
        } else if (type == "external_midi") {
            newSynth = new NoteNagaSynthExternalMidi(finalName.toStdString());
        }
        
        if (newSynth) {
            m_engine->addSynthesizer(newSynth);
            refreshSynthesizerList();
            
            // Select new synth
            for (int i = 0; i < m_synthList->count(); ++i) {
                if (m_synthList->item(i)->data(Qt::UserRole).value<void*>() == newSynth) {
                    m_synthList->setCurrentRow(i);
                    break;
                }
            }
            onMetadataEdited();
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to create synthesizer: %1").arg(e.what()));
    }
}

void ProjectSection::onRemoveSynthClicked()
{
    int row = m_synthList->currentRow();
    if (row < 0) return;
    
    QListWidgetItem *item = m_synthList->item(row);
    if (!item) return;
    
    NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer*>(
        item->data(Qt::UserRole).value<void*>());
    
    if (!synth) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Remove Synthesizer"),
        tr("Are you sure you want to remove '%1'?").arg(item->text()),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_engine->removeSynthesizer(synth);
        refreshSynthesizerList();
        onMetadataEdited();
    }
}

void ProjectSection::onConfigureSynthClicked()
{
    int row = m_synthList->currentRow();
    if (row < 0) return;
    
    QListWidgetItem *item = m_synthList->item(row);
    if (!item) return;
    
    NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer*>(
        item->data(Qt::UserRole).value<void*>());
    
    if (!synth) return;
    
    // Check synth type and show appropriate configuration
    NoteNagaSynthFluidSynth *fluidSynth = dynamic_cast<NoteNagaSynthFluidSynth*>(synth);
    NoteNagaSynthExternalMidi *externalMidi = dynamic_cast<NoteNagaSynthExternalMidi*>(synth);
    
    if (fluidSynth) {
        // Configure FluidSynth - select SoundFont
        QString currentSF = QString::fromStdString(fluidSynth->getSoundFontPath());
        QString sfPath = QFileDialog::getOpenFileName(
            this,
            tr("Select SoundFont"),
            currentSF.isEmpty() ? QDir::homePath() : QFileInfo(currentSF).absolutePath(),
            tr("SoundFont Files (*.sf2 *.sf3 *.dls);;All Files (*)")
        );
        
        if (!sfPath.isEmpty()) {
            fluidSynth->setSoundFont(sfPath.toStdString());
            onMetadataEdited();
        }
    } else if (externalMidi) {
        // Configure External MIDI - select port
        auto ports = NoteNagaSynthExternalMidi::getAvailableMidiOutputPorts();
        QStringList portNames;
        for (const auto &port : ports) {
            portNames << QString::fromStdString(port);
        }
        
        if (portNames.isEmpty()) {
            QMessageBox::information(this, tr("No MIDI Ports"),
                tr("No external MIDI ports are available."));
            return;
        }
        
        bool ok;
        QString selected = QInputDialog::getItem(
            this,
            tr("Select MIDI Port"),
            tr("Choose MIDI output port:"),
            portNames,
            0,
            false,
            &ok
        );
        
        if (ok && !selected.isEmpty()) {
            externalMidi->setMidiOutputPort(selected.toStdString());
            onMetadataEdited();
        }
    }
}

void ProjectSection::onSectionActivated()
{
    refreshUI();
    updateStatistics();
    refreshSynthesizerList();
}

void ProjectSection::onSectionDeactivated()
{
    // Nothing heavy to stop
}

void ProjectSection::setProjectMetadata(const NoteNagaProjectMetadata &metadata)
{
    m_metadata = metadata;
    refreshUI();
}

NoteNagaProjectMetadata ProjectSection::getProjectMetadata() const
{
    NoteNagaProjectMetadata meta = m_metadata;
    meta.name = m_projectNameEdit->text();
    meta.author = m_authorEdit->text();
    meta.description = m_descriptionEdit->toPlainText();
    return meta;
}

void ProjectSection::setProjectFilePath(const QString &filePath)
{
    m_projectFilePath = filePath;
    m_filePathLabel->setText(filePath.isEmpty() ? tr("Not saved yet") : filePath);
}

void ProjectSection::markAsSaved()
{
    m_hasUnsavedChanges = false;
    m_metadata.modifiedAt = QDateTime::currentDateTime();
    m_modifiedAtLabel->setText(m_metadata.modifiedAt.toString("yyyy-MM-dd hh:mm:ss"));
    emit unsavedChangesChanged(false);
}

void ProjectSection::onMetadataEdited()
{
    if (!m_hasUnsavedChanges) {
        m_hasUnsavedChanges = true;
        emit unsavedChangesChanged(true);
    }
    emit metadataChanged();
}

void ProjectSection::onSaveClicked()
{
    emit saveRequested();
}

void ProjectSection::onSaveAsClicked()
{
    emit saveAsRequested();
}

void ProjectSection::onExportMidiClicked()
{
    emit exportMidiRequested();
}

void ProjectSection::refreshUI()
{
    // Block signals while updating
    m_projectNameEdit->blockSignals(true);
    m_authorEdit->blockSignals(true);
    m_descriptionEdit->blockSignals(true);

    m_projectNameEdit->setText(m_metadata.name);
    m_authorEdit->setText(m_metadata.author);
    m_descriptionEdit->setPlainText(m_metadata.description);

    m_projectNameEdit->blockSignals(false);
    m_authorEdit->blockSignals(false);
    m_descriptionEdit->blockSignals(false);

    // Update timestamps
    if (m_metadata.createdAt.isValid()) {
        m_createdAtLabel->setText(m_metadata.createdAt.toString("yyyy-MM-dd hh:mm:ss"));
    } else {
        m_createdAtLabel->setText("-");
    }

    if (m_metadata.modifiedAt.isValid()) {
        m_modifiedAtLabel->setText(m_metadata.modifiedAt.toString("yyyy-MM-dd hh:mm:ss"));
    } else {
        m_modifiedAtLabel->setText("-");
    }

    m_filePathLabel->setText(m_projectFilePath.isEmpty() ? tr("Not saved yet") : m_projectFilePath);
}

void ProjectSection::updateStatistics()
{
    if (!m_engine) return;

    NoteNagaRuntimeData *project = m_engine->getRuntimeData();
    if (!project) return;

    NoteNagaMidiSeq *seq = project->getActiveSequence();
    if (!seq) {
        m_trackCountLabel->setText("0");
        m_noteCountLabel->setText("0");
        m_tempoLabel->setText("- BPM");
        m_ppqLabel->setText("-");
        m_durationLabel->setText("0:00");
        return;
    }

    // Track count
    int trackCount = static_cast<int>(seq->getTracks().size());
    m_trackCountLabel->setText(QString::number(trackCount));

    // Total notes
    int totalNotes = 0;
    for (NoteNagaTrack *track : seq->getTracks()) {
        if (track) {
            totalNotes += static_cast<int>(track->getNotes().size());
        }
    }
    m_noteCountLabel->setText(QString::number(totalNotes));

    // Tempo - convert from microseconds per quarter note to BPM
    int tempoMicros = seq->getTempo();
    double bpm = tempoMicros > 0 ? (60000000.0 / tempoMicros) : 120.0;
    m_tempoLabel->setText(QString("%1 BPM").arg(bpm, 0, 'f', 1));

    // PPQ
    m_ppqLabel->setText(QString::number(seq->getPPQ()));

    // Duration
    int maxTick = seq->getMaxTick();
    int ppq = seq->getPPQ();
    int tempo = seq->getTempo();
    
    if (ppq > 0 && tempo > 0) {
        // tempo is in microseconds per quarter note
        double secondsPerBeat = tempo / 1000000.0;
        double beats = static_cast<double>(maxTick) / ppq;
        double seconds = beats * secondsPerBeat;
        int minutes = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        m_durationLabel->setText(QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0')));
    } else {
        m_durationLabel->setText("0:00");
    }
}
