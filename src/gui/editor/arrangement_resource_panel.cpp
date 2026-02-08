#include "arrangement_resource_panel.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/audio/audio_manager.h>
#include <note_naga_engine/audio/audio_resource.h>

#include <QMouseEvent>
#include <QApplication>
#include <QDataStream>
#include <QPainter>

/*******************************************************************************************************/
// DraggableSequenceList
/*******************************************************************************************************/

void DraggableSequenceList::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    
    QListWidgetItem *item = currentItem();
    if (!item) return;
    
    int sequenceIndex = item->data(Qt::UserRole).toInt();
    int64_t duration = item->data(Qt::UserRole + 1).toLongLong();
    
    // Create mime data
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream << sequenceIndex << duration;
    mimeData->setData(RESOURCE_MIME_TYPE_MIDI_SEQUENCE, encodedData);
    mimeData->setText(item->text().split('\n').first());
    
    // Create drag
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    
    // Create drag pixmap
    QPixmap pixmap(120, 40);
    pixmap.fill(QColor("#3a3a42"));
    QPainter painter(&pixmap);
    painter.setPen(QColor("#2563eb"));
    painter.drawRect(0, 0, pixmap.width() - 1, pixmap.height() - 1);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, item->text().split('\n').first());
    painter.end();
    
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
    
    drag->exec(Qt::CopyAction);
}

/*******************************************************************************************************/
// DraggableAudioList
/*******************************************************************************************************/

void DraggableAudioList::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    
    QListWidgetItem *item = currentItem();
    if (!item) return;
    
    int resourceId = item->data(Qt::UserRole).toInt();
    int64_t durationSamples = item->data(Qt::UserRole + 1).toLongLong();
    
    // Create mime data
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream << resourceId << durationSamples;
    mimeData->setData(RESOURCE_MIME_TYPE_AUDIO_CLIP, encodedData);
    mimeData->setText(item->text().split('\n').first());
    
    // Create drag
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    
    // Create drag pixmap with waveform color
    QPixmap pixmap(120, 40);
    pixmap.fill(QColor("#3a3a42"));
    QPainter painter(&pixmap);
    painter.setPen(QColor("#10b981"));  // Green for audio
    painter.drawRect(0, 0, pixmap.width() - 1, pixmap.height() - 1);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, item->text().split('\n').first());
    painter.end();
    
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
    
    drag->exec(Qt::CopyAction);
}

/*******************************************************************************************************/
// SequenceItemDelegate
/*******************************************************************************************************/

SequenceItemDelegate::SequenceItemDelegate(NoteNagaEngine *engine, QObject *parent)
    : QStyledItemDelegate(parent), m_engine(engine)
{
}

void SequenceItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    painter->save();
    
    // Background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, QColor("#2563eb"));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, QColor("#2a2a35"));
    } else {
        painter->fillRect(option.rect, QColor("#252530"));
    }
    
    // Get sequence data
    int sequenceIndex = index.data(Qt::UserRole).toInt();
    QString displayText = index.data(Qt::DisplayRole).toString();
    QStringList lines = displayText.split('\n');
    QString name = lines.size() > 0 ? lines[0] : "";
    QString info = lines.size() > 1 ? lines[1] : "";
    
    // Get the sequence for preview
    NoteNagaMidiSeq *seq = nullptr;
    if (m_engine && m_engine->getRuntimeData()) {
        auto sequences = m_engine->getRuntimeData()->getSequences();
        if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(sequences.size())) {
            seq = sequences[sequenceIndex];
        }
    }
    
    // Layout: [Color bar][Preview Area][Text]
    int colorBarWidth = 4;
    int previewWidth = 80;
    int textMargin = 8;
    
    QRect colorBarRect(option.rect.left(), option.rect.top() + 2,
                       colorBarWidth, option.rect.height() - 4);
    QRect previewRect(option.rect.left() + colorBarWidth + 4, option.rect.top() + 4,
                      previewWidth, option.rect.height() - 8);
    QRect textRect(previewRect.right() + textMargin, option.rect.top(),
                   option.rect.width() - previewRect.right() - textMargin - 4, option.rect.height());
    
    // Generate a color based on sequence index
    QColor seqColor = QColor::fromHsl((sequenceIndex * 47) % 360, 180, 120);
    
    // Draw color bar
    painter->fillRect(colorBarRect, seqColor);
    
    // Draw preview area background
    painter->fillRect(previewRect, QColor("#1a1a20"));
    painter->setPen(QPen(QColor("#3a3a42"), 1));
    painter->drawRect(previewRect);
    
    // Draw note preview
    if (seq) {
        drawNotePreview(painter, previewRect.adjusted(1, 1, -1, -1), seq, seqColor.lighter(150));
    }
    
    // Draw text
    QColor textColor = (option.state & QStyle::State_Selected) ? Qt::white : QColor("#cccccc");
    QColor infoColor = (option.state & QStyle::State_Selected) ? QColor("#dddddd") : QColor("#888888");
    
    painter->setPen(textColor);
    QFont font = painter->font();
    font.setPointSize(11);
    font.setBold(true);
    painter->setFont(font);
    
    QRect nameRect = textRect.adjusted(0, 4, 0, -textRect.height()/2);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    
    painter->setPen(infoColor);
    font.setBold(false);
    font.setPointSize(10);
    painter->setFont(font);
    
    QRect infoRect = textRect.adjusted(0, textRect.height()/2, 0, -4);
    painter->drawText(infoRect, Qt::AlignLeft | Qt::AlignVCenter, info);
    
    // Draw bottom border
    painter->setPen(QColor("#3a3a42"));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    
    painter->restore();
}

QSize SequenceItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(200, 54);
}

void SequenceItemDelegate::drawNotePreview(QPainter *painter, const QRect &rect,
                                            NoteNagaMidiSeq *seq, const QColor &color) const
{
    if (!seq) return;
    
    int seqDuration = seq->getMaxTick();
    if (seqDuration <= 0) return;
    
    // Find note range
    int minNote = 127, maxNote = 0;
    for (auto *t : seq->getTracks()) {
        if (!t || t->isTempoTrack()) continue;
        for (const auto &n : t->getNotes()) {
            if (n.start.has_value()) {
                minNote = std::min(minNote, n.note);
                maxNote = std::max(maxNote, n.note);
            }
        }
    }
    
    if (minNote > maxNote) {
        minNote = 48; maxNote = 84;
    }
    int noteRange = std::max(12, maxNote - minNote + 1);
    
    // Draw notes
    for (auto *t : seq->getTracks()) {
        if (!t || t->isTempoTrack() || t->isMuted()) continue;
        
        for (const auto &n : t->getNotes()) {
            if (!n.start.has_value()) continue;
            
            int noteStart = n.start.value();
            int noteLength = n.length.value_or(120);
            int noteKey = n.note;
            
            float xRatio = float(noteStart) / float(seqDuration);
            float wRatio = float(noteLength) / float(seqDuration);
            int noteX = rect.left() + static_cast<int>(xRatio * rect.width());
            int noteW = std::max(1, static_cast<int>(wRatio * rect.width()));
            
            float noteRelY = 1.0f - float(noteKey - minNote) / float(noteRange);
            int noteY = rect.top() + static_cast<int>(noteRelY * (rect.height() - 2));
            int noteH = std::max(1, rect.height() / noteRange);
            
            painter->fillRect(noteX, noteY, noteW, noteH, color);
        }
    }
}

/*******************************************************************************************************/
// AudioItemDelegate
/*******************************************************************************************************/

AudioItemDelegate::AudioItemDelegate(NoteNagaEngine *engine, QObject *parent)
    : QStyledItemDelegate(parent), m_engine(engine)
{
}

void AudioItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    painter->save();
    
    // Background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, QColor("#10b981"));  // Green for audio
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, QColor("#2a2a35"));
    } else {
        painter->fillRect(option.rect, QColor("#252530"));
    }
    
    // Get audio data
    int resourceId = index.data(Qt::UserRole).toInt();
    QString displayText = index.data(Qt::DisplayRole).toString();
    QStringList lines = displayText.split('\n');
    QString name = lines.size() > 0 ? lines[0] : "";
    QString info = lines.size() > 1 ? lines[1] : "";
    
    // Get the audio resource for preview
    NoteNagaAudioResource *resource = nullptr;
    if (m_engine && m_engine->getRuntimeData()) {
        resource = m_engine->getRuntimeData()->getAudioManager().getResource(resourceId);
    }
    
    // Layout: [Color bar][Preview Area][Text]
    int colorBarWidth = 4;
    int previewWidth = 80;
    int textMargin = 8;
    
    QRect colorBarRect(option.rect.left(), option.rect.top() + 2,
                       colorBarWidth, option.rect.height() - 4);
    QRect previewRect(option.rect.left() + colorBarWidth + 4, option.rect.top() + 4,
                      previewWidth, option.rect.height() - 8);
    QRect textRect(previewRect.right() + textMargin, option.rect.top(),
                   option.rect.width() - previewRect.right() - textMargin - 4, option.rect.height());
    
    QColor audioColor("#10b981");  // Green for audio
    
    // Draw color bar
    painter->fillRect(colorBarRect, audioColor);
    
    // Draw preview area background
    painter->fillRect(previewRect, QColor("#1a1a20"));
    painter->setPen(QPen(QColor("#3a3a42"), 1));
    painter->drawRect(previewRect);
    
    // Draw waveform preview
    if (resource) {
        drawWaveformPreview(painter, previewRect.adjusted(1, 1, -1, -1), resource, audioColor.lighter(130));
    }
    
    // Draw text
    QColor textColor = (option.state & QStyle::State_Selected) ? Qt::white : QColor("#cccccc");
    QColor infoColor = (option.state & QStyle::State_Selected) ? QColor("#dddddd") : QColor("#888888");
    
    painter->setPen(textColor);
    QFont font = painter->font();
    font.setPointSize(11);
    font.setBold(true);
    painter->setFont(font);
    
    QRect nameRect = textRect.adjusted(0, 4, 0, -textRect.height()/2);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    
    painter->setPen(infoColor);
    font.setBold(false);
    font.setPointSize(10);
    painter->setFont(font);
    
    QRect infoRect = textRect.adjusted(0, textRect.height()/2, 0, -4);
    painter->drawText(infoRect, Qt::AlignLeft | Qt::AlignVCenter, info);
    
    // Draw bottom border
    painter->setPen(QColor("#3a3a42"));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    
    painter->restore();
}

QSize AudioItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(200, 54);
}

void AudioItemDelegate::drawWaveformPreview(QPainter *painter, const QRect &rect, 
                                             NoteNagaAudioResource *resource, const QColor &color) const
{
    if (!resource || !resource->isLoaded()) return;
    
    const auto& peaks = resource->getWaveformPeaks();
    if (peaks.empty()) return;
    
    int numPeaks = static_cast<int>(peaks.size());
    float peaksPerPixel = static_cast<float>(numPeaks) / rect.width();
    int centerY = rect.center().y();
    int halfHeight = rect.height() / 2 - 1;
    
    painter->setPen(color);
    
    for (int x = 0; x < rect.width(); ++x) {
        int peakStart = static_cast<int>(x * peaksPerPixel);
        int peakEnd = static_cast<int>((x + 1) * peaksPerPixel);
        peakEnd = std::min(peakEnd, numPeaks);
        
        if (peakStart >= numPeaks) break;
        
        // Find min/max across this range
        float minVal = 0, maxVal = 0;
        for (int p = peakStart; p < peakEnd; ++p) {
            minVal = std::min(minVal, std::min(peaks[p].minLeft, peaks[p].minRight));
            maxVal = std::max(maxVal, std::max(peaks[p].maxLeft, peaks[p].maxRight));
        }
        
        int y1 = centerY - static_cast<int>(maxVal * halfHeight);
        int y2 = centerY - static_cast<int>(minVal * halfHeight);
        
        painter->drawLine(rect.left() + x, y1, rect.left() + x, y2);
    }
}

/*******************************************************************************************************/
// ArrangementResourcePanel
/*******************************************************************************************************/

ArrangementResourcePanel::ArrangementResourcePanel(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    initUI();
    refreshFromProject();
}

void ArrangementResourcePanel::initUI()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #1e1e24;
        }
        QTabWidget::pane {
            border: none;
            background-color: #1e1e24;
        }
        QTabBar::tab {
            background-color: #252530;
            color: #888888;
            padding: 8px 16px;
            border: none;
            border-bottom: 2px solid transparent;
        }
        QTabBar::tab:selected {
            color: #cccccc;
            border-bottom: 2px solid #2563eb;
        }
        QTabBar::tab:hover:!selected {
            color: #aaaaaa;
            background-color: #2a2a35;
        }
        QLabel {
            color: #888888;
            font-size: 11px;
            padding: 4px;
        }
        QListWidget {
            background-color: #252530;
            border: none;
            color: #cccccc;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #3a3a42;
        }
        QListWidget::item:selected {
            background-color: #2563eb;
            color: white;
        }
        QListWidget::item:hover:!selected {
            background-color: #2a2a35;
        }
        QPushButton {
            background-color: #3a3a42;
            color: #cccccc;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #454550;
        }
        QPushButton:pressed {
            background-color: #2563eb;
        }
    )");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    mainLayout->addWidget(m_tabWidget);
    
    // Create MIDI tab
    QWidget *midiTab = new QWidget();
    initMidiTab(midiTab);
    m_tabWidget->addTab(midiTab, tr("MIDI"));
    
    // Create Audio tab
    QWidget *audioTab = new QWidget();
    initAudioTab(audioTab);
    m_tabWidget->addTab(audioTab, tr("Audio"));
}

void ArrangementResourcePanel::initMidiTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    // Sequence list
    m_sequenceList = new DraggableSequenceList(tab);
    m_sequenceList->setItemDelegate(new SequenceItemDelegate(m_engine, m_sequenceList));
    m_sequenceList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sequenceList, &QListWidget::customContextMenuRequested,
            this, &ArrangementResourcePanel::showSequenceContextMenu);
    connect(m_sequenceList, &QListWidget::itemDoubleClicked,
            this, &ArrangementResourcePanel::onSequenceDoubleClicked);
    layout->addWidget(m_sequenceList, 1);
    
    // Info label
    m_midiInfoLabel = new QLabel(tr("Drag sequences to timeline"), tab);
    m_midiInfoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_midiInfoLabel);
    
    // Create button
    m_createSeqBtn = new QPushButton(tr("+ New Sequence"), tab);
    connect(m_createSeqBtn, &QPushButton::clicked, this, &ArrangementResourcePanel::onCreateSequence);
    layout->addWidget(m_createSeqBtn);
}

void ArrangementResourcePanel::initAudioTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    // Audio list
    m_audioList = new DraggableAudioList(tab);
    m_audioList->setItemDelegate(new AudioItemDelegate(m_engine, m_audioList));
    m_audioList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_audioList, &QListWidget::customContextMenuRequested,
            this, &ArrangementResourcePanel::showAudioContextMenu);
    connect(m_audioList, &QListWidget::itemDoubleClicked,
            this, &ArrangementResourcePanel::onAudioDoubleClicked);
    layout->addWidget(m_audioList, 1);
    
    // Info label
    m_audioInfoLabel = new QLabel(tr("Drag audio to timeline"), tab);
    m_audioInfoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_audioInfoLabel);
    
    // Import button
    m_importAudioBtn = new QPushButton(tr("+ Import Audio"), tab);
    connect(m_importAudioBtn, &QPushButton::clicked, this, &ArrangementResourcePanel::onImportAudio);
    layout->addWidget(m_importAudioBtn);
}

void ArrangementResourcePanel::refreshFromProject()
{
    refreshMidiList();
    refreshAudioList();
}

void ArrangementResourcePanel::refreshMidiList()
{
    m_sequenceList->clear();
    
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    
    for (size_t i = 0; i < sequences.size(); ++i) {
        NoteNagaMidiSeq *seq = sequences[i];
        if (!seq) continue;
        
        QString name;
        if (!seq->getFilePath().empty()) {
            QString path = QString::fromStdString(seq->getFilePath());
            int lastSlash = path.lastIndexOf('/');
            if (lastSlash >= 0) {
                name = path.mid(lastSlash + 1);
            } else {
                name = path;
            }
            int lastDot = name.lastIndexOf('.');
            if (lastDot > 0) {
                name = name.left(lastDot);
            }
        } else {
            name = tr("Sequence %1").arg(i + 1);
        }
        
        int64_t durationTicks = seq->getMaxTick();
        int bars = static_cast<int>(durationTicks / (480 * 4)) + 1;
        
        int noteCount = 0;
        for (auto *track : seq->getTracks()) {
            if (track) {
                noteCount += static_cast<int>(track->getNotes().size());
            }
        }
        
        QString displayText = QString("%1\n%2 bars, %3 notes")
            .arg(name)
            .arg(bars)
            .arg(noteCount);
        
        QListWidgetItem *item = new QListWidgetItem(m_sequenceList);
        item->setText(displayText);
        item->setData(Qt::UserRole, static_cast<int>(i));
        item->setData(Qt::UserRole + 1, durationTicks);
        item->setToolTip(tr("Duration: %1 ticks\nNotes: %2\nDouble-click to edit")
                            .arg(durationTicks).arg(noteCount));
        m_sequenceList->addItem(item);
    }
    
    if (sequences.empty()) {
        m_midiInfoLabel->setText(tr("No sequences. Create one to get started."));
    } else {
        m_midiInfoLabel->setText(tr("Drag sequences to timeline"));
    }
}

void ArrangementResourcePanel::refreshAudioList()
{
    m_audioList->clear();
    
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaAudioManager& audioManager = m_engine->getRuntimeData()->getAudioManager();
    auto resources = audioManager.getAllResources();
    
    for (NoteNagaAudioResource *resource : resources) {
        if (!resource) continue;
        
        QString name = QString::fromStdString(resource->getFileName());
        
        // Format duration
        double durationSec = resource->getDurationSeconds();
        int minutes = static_cast<int>(durationSec) / 60;
        int seconds = static_cast<int>(durationSec) % 60;
        QString durationStr = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
        
        // Sample rate info
        QString info = QString("%1 • %2 Hz")
            .arg(durationStr)
            .arg(resource->getSampleRate());
        
        QString displayText = QString("%1\n%2").arg(name).arg(info);
        
        QListWidgetItem *item = new QListWidgetItem(m_audioList);
        item->setText(displayText);
        item->setData(Qt::UserRole, resource->getId());
        item->setData(Qt::UserRole + 1, resource->getTotalSamples());
        item->setToolTip(tr("File: %1\nDuration: %2\nSample Rate: %3 Hz\nChannels: %4")
                            .arg(QString::fromStdString(resource->getFilePath()))
                            .arg(durationStr)
                            .arg(resource->getSampleRate())
                            .arg(resource->getChannels()));
        m_audioList->addItem(item);
    }
    
    if (resources.empty()) {
        m_audioInfoLabel->setText(tr("No audio files. Import some to get started."));
    } else {
        m_audioInfoLabel->setText(tr("Drag audio to timeline"));
    }
}

void ArrangementResourcePanel::onSequenceDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        int sequenceIndex = item->data(Qt::UserRole).toInt();
        emit editSequenceRequested(sequenceIndex);
    }
}

void ArrangementResourcePanel::onAudioDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    // Could open audio properties dialog in the future
}

void ArrangementResourcePanel::onCreateSequence()
{
    emit createSequenceRequested();
}

void ArrangementResourcePanel::onImportAudio()
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        tr("Import Audio Files"),
        QString(),
        tr("Audio Files (*.wav *.WAV);;All Files (*)")
    );
    
    if (fileNames.isEmpty()) return;
    
    NoteNagaAudioManager& audioManager = m_engine->getRuntimeData()->getAudioManager();
    int importedCount = 0;
    
    for (const QString &fileName : fileNames) {
        NoteNagaAudioResource *resource = audioManager.importAudio(fileName.toStdString());
        if (resource) {
            importedCount++;
        } else {
            QMessageBox::warning(this, tr("Import Failed"),
                                 tr("Failed to import: %1").arg(fileName));
        }
    }
    
    if (importedCount > 0) {
        refreshAudioList();
    }
}

void ArrangementResourcePanel::showSequenceContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_sequenceList->itemAt(pos);
    if (!item) return;
    
    int sequenceIndex = item->data(Qt::UserRole).toInt();
    
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #2a2a30;
            color: #cccccc;
            border: 1px solid #4a4a52;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 20px;
        }
        QMenu::item:selected {
            background-color: #2563eb;
        }
    )");
    
    QAction *editAction = menu.addAction(tr("Edit Sequence"));
    QAction *renameAction = menu.addAction(tr("Rename Sequence"));
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(tr("Delete Sequence"));
    
    QAction *selected = menu.exec(m_sequenceList->mapToGlobal(pos));
    
    if (selected == editAction) {
        emit editSequenceRequested(sequenceIndex);
    } else if (selected == renameAction) {
        renameSequence(sequenceIndex);
    } else if (selected == deleteAction) {
        deleteSequence(sequenceIndex);
    }
}

void ArrangementResourcePanel::showAudioContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_audioList->itemAt(pos);
    if (!item) return;
    
    int resourceId = item->data(Qt::UserRole).toInt();
    
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #2a2a30;
            color: #cccccc;
            border: 1px solid #4a4a52;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 20px;
        }
        QMenu::item:selected {
            background-color: #10b981;
        }
    )");
    
    QAction *removeAction = menu.addAction(tr("Remove from Project"));
    
    QAction *selected = menu.exec(m_audioList->mapToGlobal(pos));
    
    if (selected == removeAction) {
        removeAudioResource(resourceId);
    }
}

void ArrangementResourcePanel::renameSequence(int sequenceIndex)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    if (sequenceIndex < 0 || sequenceIndex >= static_cast<int>(sequences.size())) return;
    
    NoteNagaMidiSeq *seq = sequences[sequenceIndex];
    if (!seq) return;
    
    QString currentName;
    if (!seq->getFilePath().empty()) {
        QString path = QString::fromStdString(seq->getFilePath());
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash >= 0) {
            currentName = path.mid(lastSlash + 1);
        } else {
            currentName = path;
        }
        int lastDot = currentName.lastIndexOf('.');
        if (lastDot > 0) {
            currentName = currentName.left(lastDot);
        }
    } else {
        currentName = tr("Sequence %1").arg(sequenceIndex + 1);
    }
    
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Sequence"),
                                            tr("New name:"), QLineEdit::Normal,
                                            currentName, &ok);
    if (ok && !newName.isEmpty()) {
        seq->setFilePath(newName.toStdString());
        refreshMidiList();
    }
}

void ArrangementResourcePanel::deleteSequence(int sequenceIndex)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    auto sequences = m_engine->getRuntimeData()->getSequences();
    if (sequenceIndex < 0 || sequenceIndex >= static_cast<int>(sequences.size())) return;
    
    NoteNagaMidiSeq *seq = sequences[sequenceIndex];
    if (!seq) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Sequence"),
        tr("Are you sure you want to delete this sequence?\nThis action cannot be undone."),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        int sequenceId = seq->getId();
        m_engine->getRuntimeData()->removeSequence(seq);
        emit sequenceDeleted(sequenceId);
        refreshMidiList();
    }
}

void ArrangementResourcePanel::removeAudioResource(int resourceId)
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Remove Audio"),
        tr("Are you sure you want to remove this audio file from the project?\n"
           "This will also remove all clips using this audio."),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_engine->getRuntimeData()->getAudioManager().removeAudioResource(resourceId);
        emit audioResourceDeleted(resourceId);
        refreshAudioList();
    }
}
