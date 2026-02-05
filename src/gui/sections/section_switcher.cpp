#include "section_switcher.h"
#include "../components/track_stereo_meter.h"
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/module/dsp_engine.h>

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
      m_globalMeter(nullptr), m_meterTimer(nullptr)
{
    setupUi();
}

void SectionSwitcher::setupUi()
{
    setFixedHeight(56);
    setStyleSheet("background-color: #2a2a30; border-top: 1px solid #3a3a42;");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create global stereo meter on the left
    m_globalMeter = new TrackStereoMeter(this);
    m_globalMeter->setMinimumWidth(100);
    m_globalMeter->setMaximumWidth(140);
    m_globalMeter->setFixedHeight(48);
    layout->addWidget(m_globalMeter);

    // Setup meter update timer
    m_meterTimer = new QTimer(this);
    connect(m_meterTimer, &QTimer::timeout, this, [this]() {
        if (m_engine && m_engine->getDSPEngine()) {
            auto dbs = m_engine->getDSPEngine()->getCurrentVolumeDb();
            m_globalMeter->setVolumesDb(dbs.first, dbs.second);
        }
    });
    m_meterTimer->start(50);

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
        {AppSection::MediaExport, ":/icons/app_section_media.svg", tr("Media Export")},
        {AppSection::Notation, ":/icons/app_section_notation.svg", tr("Notation")}
    };

    // Add stretch before buttons to center them
    layout->addStretch();

    for (const auto &info : sections) {
        SectionButton *btn = new SectionButton(QIcon(info.iconPath), info.title, this);
        m_buttonGroup->addButton(btn, static_cast<int>(info.section));
        m_buttons.append(btn);
        layout->addWidget(btn);
    }

    // Add stretch after buttons to center them
    layout->addStretch();

    // Add invisible spacer on the right to balance the meter on the left
    QWidget *rightSpacer = new QWidget(this);
    rightSpacer->setStyleSheet("background: transparent;");
    rightSpacer->setMinimumWidth(100);
    rightSpacer->setMaximumWidth(140);
    rightSpacer->setFixedHeight(48);
    layout->addWidget(rightSpacer);

    // Set default checked button
    if (!m_buttons.isEmpty()) {
        m_buttons.first()->setChecked(true);
    }

    // Connect button group signal
    connect(m_buttonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_currentSection = static_cast<AppSection>(id);
        emit sectionChanged(m_currentSection);
    });
}

void SectionSwitcher::setCurrentSection(AppSection section)
{
    m_currentSection = section;
    int idx = static_cast<int>(section);
    if (idx >= 0 && idx < m_buttons.size()) {
        m_buttons[idx]->setChecked(true);
    }
}
