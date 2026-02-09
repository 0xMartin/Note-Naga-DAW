#include "section_switcher.h"
#include "../widgets/global_transport_bar.h"
#include "../components/track_stereo_meter.h"
#include "../components/midi_sequence_selector.h"
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/module/dsp_engine.h>
#include <note_naga_engine/module/playback_worker.h>

#include <QStyle>

// === SectionButton implementation ===

SectionButton::SectionButton(const QIcon &icon, const QString &text, QWidget *parent)
    : QPushButton(parent)
{
    setIcon(icon);
    setText(text);
    setCheckable(true);
    setIconSize(QSize(32, 32));
    setMinimumHeight(40);
    setMinimumWidth(130);
    
    // DaVinci Resolve-like styling - matching app theme
    setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a42;
            color: #aaaaaa;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 12px;
            font-weight: 600;
            text-align: center;
            margin: 4px 2px;
        }
        QPushButton:hover {
            background-color: #454550;
            color: #dddddd;
        }
        QPushButton:checked {
            background-color: #2563eb;
            color: #ffffff;
        }
        QPushButton:checked:hover {
            background-color: #3b82f6;
        }
        QPushButton:pressed {
            background-color: #1d4ed8;
        }
    )");
}

// === SectionSwitcher implementation ===

SectionSwitcher::SectionSwitcher(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine), m_currentSection(AppSection::Project),
      m_transportBar(nullptr), m_sequenceSelector(nullptr)
{
    setupUi();
}

void SectionSwitcher::setupUi()
{
    // Main vertical layout: transport bar on top, section buttons below
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === Global Transport Bar (full width at top) ===
    m_transportBar = new GlobalTransportBar(m_engine, this);
    mainLayout->addWidget(m_transportBar);
    
    // Connect playback mode changes to engine's playback worker and DSP engine
    if (m_engine && m_engine->getPlaybackWorker()) {
        connect(m_transportBar, &GlobalTransportBar::playbackModeChanged, this, 
                [this](PlaybackMode mode) {
            // Stop playback before changing mode to prevent audio glitches
            bool wasPlaying = m_engine->getPlaybackWorker()->isPlaying();
            if (wasPlaying) {
                m_engine->stopPlayback();
            }
            
            m_engine->getPlaybackWorker()->setPlaybackMode(mode);
            // Also update DSP engine so it renders the correct sequences
            if (m_engine->getDSPEngine()) {
                m_engine->getDSPEngine()->setPlaybackMode(mode);
            }
            
            // Hide/show Notation section based on playback mode
            // Notation is only available in Sequence mode
            int notationIndex = static_cast<int>(AppSection::Notation);
            if (notationIndex < m_buttons.size()) {
                bool isArrangementMode = (mode == PlaybackMode::Arrangement);
                m_buttons[notationIndex]->setVisible(!isArrangementMode);
                
                // If currently on Notation section and switching to Arrangement mode,
                // switch to a different section (Arrangement section)
                if (isArrangementMode && m_currentSection == AppSection::Notation) {
                    setCurrentSection(AppSection::Arrangement);
                }
            }
        });
    }

    // Setup meter update timer for the transport bar's stereo meter
    QTimer *meterTimer = new QTimer(this);
    connect(meterTimer, &QTimer::timeout, this, [this]() {
        if (m_engine && m_engine->getDSPEngine() && m_transportBar) {
            auto dbs = m_engine->getDSPEngine()->getCurrentVolumeDb();
            m_transportBar->getStereoMeter()->setVolumesDb(dbs.first, dbs.second);
        }
    });
    meterTimer->start(50);

    // === Section Buttons Row ===
    QWidget *buttonRow = new QWidget(this);
    buttonRow->setStyleSheet("background-color: #2a2a30;");
    buttonRow->setFixedHeight(48);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(8, 0, 8, 0);
    buttonLayout->setSpacing(0);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    // Create section buttons
    struct SectionInfo {
        AppSection section;
        QString iconPath;
        QString title;
    };

    QList<SectionInfo> sections = {
        {AppSection::Project, ":/icons/app_section_project.svg", tr("Project")},
        {AppSection::MidiEditor, ":/icons/app_section_midi.svg", tr("MIDI Editor")},
        {AppSection::DspEditor, ":/icons/app_section_dsp.svg", tr("DSP Editor")},
        {AppSection::Arrangement, ":/icons/app_section_arrangement.svg", tr("Arrangement")},
        {AppSection::MediaExport, ":/icons/app_section_media.svg", tr("Media Export")},
        {AppSection::Notation, ":/icons/app_section_notation.svg", tr("Notation")},
        {AppSection::ExternalMidi, ":/icons/app_section_external.svg", tr("External")}
    };

    // === Section Buttons (left side) ===
    for (const auto &info : sections) {
        SectionButton *btn = new SectionButton(QIcon(info.iconPath), info.title, this);
        m_buttonGroup->addButton(btn, static_cast<int>(info.section));
        m_buttons.append(btn);
        buttonLayout->addWidget(btn);
    }

    // Add stretch to push selector to the right
    buttonLayout->addStretch();

    // === MIDI Sequence Selector (right side) ===
    m_sequenceSelector = new MidiSequenceSelector(m_engine, this);
    m_sequenceSelector->setFixedWidth(240);
    buttonLayout->addWidget(m_sequenceSelector);

    mainLayout->addWidget(buttonRow);

    // Set default checked button
    if (!m_buttons.isEmpty()) {
        m_buttons.first()->setChecked(true);
    }

    // Connect button group signal
    connect(m_buttonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_currentSection = static_cast<AppSection>(id);
        emit sectionChanged(m_currentSection);
    });

    // Set overall styling
    setStyleSheet("background-color: #2a2a30; border-top: 1px solid #3a3a42;");
}

void SectionSwitcher::setCurrentSection(AppSection section)
{
    m_currentSection = section;
    int idx = static_cast<int>(section);
    if (idx >= 0 && idx < m_buttons.size()) {
        m_buttons[idx]->setChecked(true);
    }
}
