#include "project_section.h"
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/project_serializer.h>

#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QDateTime>

ProjectSection::ProjectSection(NoteNagaEngine *engine,
                               NoteNagaProjectSerializer *serializer,
                               QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_serializer(serializer)
{
    setupUI();
}

void ProjectSection::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // Create scroll area for content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; }");

    QWidget *scrollContent = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(0, 0, 20, 0);
    contentLayout->setSpacing(24);

    // === Header ===
    QLabel *headerLabel = new QLabel("📁 Project Information");
    headerLabel->setFont(QFont("Segoe UI", 20, QFont::Bold));
    headerLabel->setStyleSheet("color: #7eb8f9; letter-spacing: 1px; margin-bottom: 5px;");
    contentLayout->addWidget(headerLabel);

    // === Metadata Group ===
    QGroupBox *metadataGroup = new QGroupBox("Project Metadata");
    metadataGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 14px;
            font-weight: 600;
            color: #b0b8c0;
            border: 1px solid #3a4654;
            border-radius: 8px;
            margin-top: 16px;
            padding-top: 16px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 8px;
            background: #1e242c;
        }
    )");

    QGridLayout *metadataLayout = new QGridLayout(metadataGroup);
    metadataLayout->setHorizontalSpacing(16);
    metadataLayout->setVerticalSpacing(12);
    metadataLayout->setContentsMargins(16, 24, 16, 16);

    QString labelStyle = "color: #8899a6; font-size: 12px;";
    QString inputStyle = R"(
        QLineEdit, QTextEdit {
            background: #232b38;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
        }
        QLineEdit:focus, QTextEdit:focus {
            border-color: #5590c7;
        }
    )";
    QString readonlyStyle = "color: #7eb8f9; font-size: 13px; font-weight: 500;";

    int row = 0;

    // Project Name
    QLabel *nameLabel = new QLabel("Project Name:");
    nameLabel->setStyleSheet(labelStyle);
    metadataLayout->addWidget(nameLabel, row, 0);

    m_projectNameEdit = new QLineEdit();
    m_projectNameEdit->setStyleSheet(inputStyle);
    m_projectNameEdit->setPlaceholderText("Enter project name");
    connect(m_projectNameEdit, &QLineEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    metadataLayout->addWidget(m_projectNameEdit, row, 1);
    row++;

    // Author
    QLabel *authorLabel = new QLabel("Author:");
    authorLabel->setStyleSheet(labelStyle);
    metadataLayout->addWidget(authorLabel, row, 0);

    m_authorEdit = new QLineEdit();
    m_authorEdit->setStyleSheet(inputStyle);
    m_authorEdit->setPlaceholderText("Enter author name");
    connect(m_authorEdit, &QLineEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    metadataLayout->addWidget(m_authorEdit, row, 1);
    row++;

    // Description
    QLabel *descLabel = new QLabel("Description:");
    descLabel->setStyleSheet(labelStyle);
    descLabel->setAlignment(Qt::AlignTop);
    metadataLayout->addWidget(descLabel, row, 0);

    m_descriptionEdit = new QTextEdit();
    m_descriptionEdit->setStyleSheet(inputStyle);
    m_descriptionEdit->setPlaceholderText("Enter project description (optional)");
    m_descriptionEdit->setMaximumHeight(100);
    connect(m_descriptionEdit, &QTextEdit::textChanged, this, &ProjectSection::onMetadataEdited);
    metadataLayout->addWidget(m_descriptionEdit, row, 1);
    row++;

    // File Path (read-only)
    QLabel *pathLabel = new QLabel("File Path:");
    pathLabel->setStyleSheet(labelStyle);
    metadataLayout->addWidget(pathLabel, row, 0);

    m_filePathLabel = new QLabel("-");
    m_filePathLabel->setStyleSheet("color: #667788; font-size: 12px;");
    m_filePathLabel->setWordWrap(true);
    metadataLayout->addWidget(m_filePathLabel, row, 1);
    row++;

    // Created At
    QLabel *createdLabel = new QLabel("Created:");
    createdLabel->setStyleSheet(labelStyle);
    metadataLayout->addWidget(createdLabel, row, 0);

    m_createdAtLabel = new QLabel("-");
    m_createdAtLabel->setStyleSheet(readonlyStyle);
    metadataLayout->addWidget(m_createdAtLabel, row, 1);
    row++;

    // Modified At
    QLabel *modifiedLabel = new QLabel("Last Modified:");
    modifiedLabel->setStyleSheet(labelStyle);
    metadataLayout->addWidget(modifiedLabel, row, 0);

    m_modifiedAtLabel = new QLabel("-");
    m_modifiedAtLabel->setStyleSheet(readonlyStyle);
    metadataLayout->addWidget(m_modifiedAtLabel, row, 1);

    metadataLayout->setColumnStretch(1, 1);
    contentLayout->addWidget(metadataGroup);

    // === Statistics Group ===
    QGroupBox *statsGroup = new QGroupBox("Project Statistics");
    statsGroup->setStyleSheet(metadataGroup->styleSheet());

    QGridLayout *statsLayout = new QGridLayout(statsGroup);
    statsLayout->setHorizontalSpacing(40);
    statsLayout->setVerticalSpacing(12);
    statsLayout->setContentsMargins(16, 24, 16, 16);

    // Row 1
    QLabel *tracksLabel = new QLabel("Tracks:");
    tracksLabel->setStyleSheet(labelStyle);
    statsLayout->addWidget(tracksLabel, 0, 0);

    m_trackCountLabel = new QLabel("0");
    m_trackCountLabel->setStyleSheet(readonlyStyle);
    statsLayout->addWidget(m_trackCountLabel, 0, 1);

    QLabel *notesLabel = new QLabel("Total Notes:");
    notesLabel->setStyleSheet(labelStyle);
    statsLayout->addWidget(notesLabel, 0, 2);

    m_noteCountLabel = new QLabel("0");
    m_noteCountLabel->setStyleSheet(readonlyStyle);
    statsLayout->addWidget(m_noteCountLabel, 0, 3);

    // Row 2
    QLabel *tempoLabel = new QLabel("Tempo:");
    tempoLabel->setStyleSheet(labelStyle);
    statsLayout->addWidget(tempoLabel, 1, 0);

    m_tempoLabel = new QLabel("120 BPM");
    m_tempoLabel->setStyleSheet(readonlyStyle);
    statsLayout->addWidget(m_tempoLabel, 1, 1);

    QLabel *ppqLabel = new QLabel("PPQ:");
    ppqLabel->setStyleSheet(labelStyle);
    statsLayout->addWidget(ppqLabel, 1, 2);

    m_ppqLabel = new QLabel("480");
    m_ppqLabel->setStyleSheet(readonlyStyle);
    statsLayout->addWidget(m_ppqLabel, 1, 3);

    // Row 3
    QLabel *durationLabel = new QLabel("Duration:");
    durationLabel->setStyleSheet(labelStyle);
    statsLayout->addWidget(durationLabel, 2, 0);

    m_durationLabel = new QLabel("0:00");
    m_durationLabel->setStyleSheet(readonlyStyle);
    statsLayout->addWidget(m_durationLabel, 2, 1);

    statsLayout->setColumnStretch(1, 1);
    statsLayout->setColumnStretch(3, 1);
    contentLayout->addWidget(statsGroup);

    // === Actions Group ===
    QGroupBox *actionsGroup = new QGroupBox("Quick Actions");
    actionsGroup->setStyleSheet(metadataGroup->styleSheet());

    QHBoxLayout *actionsLayout = new QHBoxLayout(actionsGroup);
    actionsLayout->setSpacing(16);
    actionsLayout->setContentsMargins(16, 24, 16, 16);

    QString buttonStyle = R"(
        QPushButton {
            background: #2d3640;
            color: #d4d8de;
            border: 1px solid #3a4654;
            border-radius: 6px;
            padding: 12px 24px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: #3a4654;
            border-color: #4a6080;
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
            border-radius: 6px;
            padding: 12px 24px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #4a8ad0;
        }
        QPushButton:pressed {
            background: #2a6090;
        }
    )";

    m_saveBtn = new QPushButton("💾  Save");
    m_saveBtn->setStyleSheet(primaryButtonStyle);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProjectSection::onSaveClicked);
    actionsLayout->addWidget(m_saveBtn);

    m_saveAsBtn = new QPushButton("📝  Save As...");
    m_saveAsBtn->setStyleSheet(buttonStyle);
    m_saveAsBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &ProjectSection::onSaveAsClicked);
    actionsLayout->addWidget(m_saveAsBtn);

    m_exportMidiBtn = new QPushButton("🎹  Export MIDI...");
    m_exportMidiBtn->setStyleSheet(buttonStyle);
    m_exportMidiBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportMidiBtn, &QPushButton::clicked, this, &ProjectSection::onExportMidiClicked);
    actionsLayout->addWidget(m_exportMidiBtn);

    actionsLayout->addStretch();
    contentLayout->addWidget(actionsGroup);

    contentLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);
}

void ProjectSection::onSectionActivated()
{
    refreshUI();
    updateStatistics();
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
    m_filePathLabel->setText(filePath.isEmpty() ? "Not saved yet" : filePath);
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

    m_filePathLabel->setText(m_projectFilePath.isEmpty() ? "Not saved yet" : m_projectFilePath);
}

void ProjectSection::updateStatistics()
{
    if (!m_engine) return;

    NoteNagaProject *project = m_engine->getProject();
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
