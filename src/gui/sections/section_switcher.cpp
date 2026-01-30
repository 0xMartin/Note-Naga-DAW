#include "section_switcher.h"

#include <QStyle>

// === SectionButton implementation ===

SectionButton::SectionButton(const QIcon &icon, const QString &text, QWidget *parent)
    : QPushButton(parent)
{
    setIcon(icon);
    setText(text);
    setCheckable(true);
    setIconSize(QSize(24, 24));
    setMinimumHeight(48);
    setMinimumWidth(140);
    
    // DaVinci Resolve-like styling - matching app theme
    setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a42;
            color: #aaaaaa;
            border: none;
            border-radius: 4px;
            padding: 10px 20px;
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

SectionSwitcher::SectionSwitcher(QWidget *parent)
    : QWidget(parent), m_currentSection(AppSection::MidiEditor)
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

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    // Create section buttons
    struct SectionInfo {
        AppSection section;
        QString iconPath;
        QString title;
    };

    QList<SectionInfo> sections = {
        {AppSection::MidiEditor, ":/icons/app_section_midi.svg", tr("MIDI Editor")},
        {AppSection::DspEditor, ":/icons/app_section_dsp.svg", tr("DSP Editor")},
        {AppSection::MediaExport, ":/icons/app_section_media.svg", tr("Media Export")}
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
