#include "midi_sequence_selector.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <QStyle>

MidiSequenceSelector::MidiSequenceSelector(NoteNagaEngine* engine, QWidget* parent)
    : QFrame(parent), m_engine(engine)
{
    initUI();
    setupConnections();
    refreshSequenceList();
}

void MidiSequenceSelector::initUI()
{
    setObjectName("midiSequenceSelector");
    setFixedHeight(36);
    
    // Styling - slightly darker background, no visible border
    setStyleSheet(R"(
        #midiSequenceSelector {
            background-color: rgba(36, 36, 42, 0.8);
            border: none;
            border-radius: 6px;
            padding: 2px 8px;
        }
        #midiSequenceSelector:hover {
            background-color: rgba(46, 46, 54, 0.9);
        }
        QComboBox {
            background-color: transparent;
            color: #dddddd;
            border: none;
            padding: 2px 4px;
            min-width: 120px;
            font-size: 11px;
            font-weight: 500;
        }
        QComboBox::drop-down {
            border: none;
            width: 16px;
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron-down.svg);
            width: 10px;
            height: 10px;
        }
        QComboBox QAbstractItemView {
            background-color: #2a2a30;
            color: #dddddd;
            selection-background-color: #2563eb;
            border: 1px solid #4a4a52;
            border-radius: 4px;
            padding: 4px;
        }
        QLabel#metadataLabel {
            color: #888888;
            font-size: 10px;
            border: none;
            background: transparent;
        }
        QLabel#iconLabel {
            color: #2563eb;
            border: none;
            background: transparent;
        }
    )");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    // Icon label
    m_iconLabel = new QLabel("♫", this);
    m_iconLabel->setObjectName("iconLabel");
    m_iconLabel->setFixedWidth(16);
    layout->addWidget(m_iconLabel);

    // Sequence combo box
    m_sequenceCombo = new QComboBox(this);
    m_sequenceCombo->setMinimumWidth(140);
    m_sequenceCombo->setMaximumWidth(220);
    m_sequenceCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(m_sequenceCombo);

    // Metadata label (duration, note count)
    m_metadataLabel = new QLabel(this);
    m_metadataLabel->setObjectName("metadataLabel");
    m_metadataLabel->setMinimumWidth(60);
    layout->addWidget(m_metadataLabel);

    layout->addStretch();
}

void MidiSequenceSelector::setupConnections()
{
    connect(m_sequenceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MidiSequenceSelector::onComboIndexChanged);

    // Connect to engine signals
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaRuntimeData* runtime = m_engine->getRuntimeData();
        
        connect(runtime, &NoteNagaRuntimeData::activeSequenceChanged,
                this, &MidiSequenceSelector::onEngineSequenceChanged);
        
        // Refresh list when sequences are added or removed
        connect(runtime, &NoteNagaRuntimeData::sequenceListChanged,
                this, &MidiSequenceSelector::refreshSequenceList);
        
        // Refresh when project is loaded
        connect(runtime, &NoteNagaRuntimeData::projectFileLoaded,
                this, &MidiSequenceSelector::refreshSequenceList);
        
        // Refresh on sequence metadata changes (note count, duration, etc.)
        connect(runtime, &NoteNagaRuntimeData::sequenceMetadataChanged,
                this, [this](NoteNagaMidiSeq*, const std::string&) {
            updateMetadataDisplay();
        });
        
        // Also refresh when track list changes (affects note count display)
        connect(runtime, &NoteNagaRuntimeData::activeSequenceTrackListChanged,
                this, [this](NoteNagaMidiSeq*) {
            updateMetadataDisplay();
        });
    }
}

void MidiSequenceSelector::refreshSequenceList()
{
    if (!m_engine || !m_engine->getRuntimeData()) return;

    // Block signals during update
    m_sequenceCombo->blockSignals(true);
    m_sequenceCombo->clear();

    NoteNagaRuntimeData* runtime = m_engine->getRuntimeData();
    const auto& sequences = runtime->getSequences();

    for (auto* seq : sequences) {
        if (!seq) continue;
        
        QString displayName = QString("Sequence %1").arg(seq->getId());
        // Use file path as display name if available
        if (!seq->getFilePath().empty()) {
            QString path = QString::fromStdString(seq->getFilePath());
            int lastSlash = path.lastIndexOf('/');
            if (lastSlash >= 0) {
                displayName = path.mid(lastSlash + 1);
            } else {
                displayName = path;
            }
            // Remove extension
            int lastDot = displayName.lastIndexOf('.');
            if (lastDot > 0) {
                displayName = displayName.left(lastDot);
            }
        }
        
        m_sequenceCombo->addItem(displayName, seq->getId());
    }

    // Select active sequence, or auto-select if only one sequence exists
    NoteNagaMidiSeq* activeSeq = runtime->getActiveSequence();
    if (activeSeq) {
        int index = m_sequenceCombo->findData(activeSeq->getId());
        if (index >= 0) {
            m_sequenceCombo->setCurrentIndex(index);
        }
    } else if (m_sequenceCombo->count() == 1) {
        // Auto-select the only available sequence
        m_sequenceCombo->setCurrentIndex(0);
        int seqId = m_sequenceCombo->itemData(0).toInt();
        NoteNagaMidiSeq* seq = runtime->getSequenceById(seqId);
        if (seq) {
            runtime->setActiveSequence(seq);
        }
    } else if (m_sequenceCombo->count() > 0 && m_sequenceCombo->currentIndex() < 0) {
        // Select first sequence if nothing is selected
        m_sequenceCombo->setCurrentIndex(0);
    }

    m_sequenceCombo->blockSignals(false);
    updateMetadataDisplay();
}

void MidiSequenceSelector::setCurrentSequenceById(int sequenceId)
{
    int index = m_sequenceCombo->findData(sequenceId);
    if (index >= 0) {
        m_sequenceCombo->setCurrentIndex(index);
    }
}

NoteNagaMidiSeq* MidiSequenceSelector::getCurrentSequence() const
{
    if (!m_engine || !m_engine->getRuntimeData()) return nullptr;
    
    int seqId = m_sequenceCombo->currentData().toInt();
    return m_engine->getRuntimeData()->getSequenceById(seqId);
}

void MidiSequenceSelector::onComboIndexChanged(int index)
{
    if (index < 0) return;
    
    int seqId = m_sequenceCombo->itemData(index).toInt();
    
    if (m_engine && m_engine->getRuntimeData()) {
        NoteNagaMidiSeq* seq = m_engine->getRuntimeData()->getSequenceById(seqId);
        if (seq) {
            m_engine->getRuntimeData()->setActiveSequence(seq);
            emit sequenceSelected(seq);
        }
    }
    
    updateMetadataDisplay();
}

void MidiSequenceSelector::onEngineSequenceChanged(NoteNagaMidiSeq* sequence)
{
    if (!sequence) return;
    
    // Update combo box selection without triggering signals
    m_sequenceCombo->blockSignals(true);
    int index = m_sequenceCombo->findData(sequence->getId());
    if (index >= 0) {
        m_sequenceCombo->setCurrentIndex(index);
    }
    m_sequenceCombo->blockSignals(false);
    
    updateMetadataDisplay();
}

void MidiSequenceSelector::updateMetadataDisplay()
{
    NoteNagaMidiSeq* seq = getCurrentSequence();
    if (!seq) {
        m_metadataLabel->setText("");
        return;
    }

    // Calculate total notes
    int totalNotes = 0;
    for (auto* track : seq->getTracks()) {
        if (track && !track->isTempoTrack()) {
            totalNotes += track->getNotes().size();
        }
    }

    // Format duration
    QString duration = formatDuration(seq->getMaxTick(), seq->getPPQ(), seq->getTempo());
    
    m_metadataLabel->setText(QString("%1 | %2 notes").arg(duration).arg(totalNotes));
}

QString MidiSequenceSelector::formatDuration(int ticks, int ppq, int tempo) const
{
    if (ppq <= 0 || tempo <= 0) return "0:00";
    
    // Convert ticks to seconds
    double usPerTick = static_cast<double>(tempo) / ppq;
    double totalSeconds = (ticks * usPerTick) / 1'000'000.0;
    
    int minutes = static_cast<int>(totalSeconds) / 60;
    int seconds = static_cast<int>(totalSeconds) % 60;
    
    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}
