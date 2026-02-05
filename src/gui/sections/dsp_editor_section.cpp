#include "dsp_editor_section.h"

#include "../dock_system/advanced_dock_widget.h"
#include "../widgets/dsp_engine_widget.h"
#include "../components/spectrum_analyzer.h"
#include "../components/pan_analyzer.h"
#include "track_preview_widget.h"

#include <QDockWidget>
#include <QVBoxLayout>

DSPEditorSection::DSPEditorSection(NoteNagaEngine *engine, QWidget *parent)
    : QMainWindow(parent), m_engine(engine)
{
    // Remove window frame for embedded use
    setWindowFlags(Qt::Widget);
    setDockNestingEnabled(true);
    
    // Set background
    setStyleSheet("QMainWindow { background-color: #1a1a1f; }");
    
    setupDockLayout();
}

DSPEditorSection::~DSPEditorSection()
{
}

void DSPEditorSection::setupDockLayout()
{
    // Enable spectrum analyzer right away for testing
    if (m_engine->getSpectrumAnalyzer()) {
        m_engine->getSpectrumAnalyzer()->setEnableSpectrumAnalysis(true);
    }
    
    // Enable pan analyzer
    if (m_engine->getPanAnalyzer()) {
        m_engine->getPanAnalyzer()->setEnabled(true);
    }
    
    // === Spectrum Analyzer dock (left top) ===
    m_spectrumAnalyzer = new SpectrumAnalyzer(m_engine->getSpectrumAnalyzer(), this);
    m_spectrumAnalyzer->setMinimumSize(300, 120);
    m_spectrumAnalyzer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    auto *spectrumDock = new AdvancedDockWidget(
        tr("Spectrum Analyzer"), 
        QIcon(":/icons/audio-signal.svg"),
        m_spectrumAnalyzer->getTitleWidget(), 
        this
    );
    spectrumDock->setWidget(m_spectrumAnalyzer);
    spectrumDock->setObjectName("spectrum");
    spectrumDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    spectrumDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::TopDockWidgetArea, spectrumDock);
    m_docks["spectrum"] = spectrumDock;

    // === Pan Analyzer dock (center top - between spectrum and track preview) ===
    m_panAnalyzer = new PanAnalyzer(m_engine->getPanAnalyzer(), this);
    m_panAnalyzer->setMinimumSize(150, 120);
    m_panAnalyzer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    auto *panDock = new AdvancedDockWidget(
        tr("Pan Analyzer"), 
        QIcon(":/icons/audio-signal.svg"),
        m_panAnalyzer->getTitleWidget(), 
        this
    );
    panDock->setWidget(m_panAnalyzer);
    panDock->setObjectName("pan");
    panDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    panDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::TopDockWidgetArea, panDock);
    m_docks["pan"] = panDock;

    // === DSP dock (bottom - full width) ===
    m_dspWidget = new DSPEngineWidget(m_engine, this);
    m_dspWidget->setMinimumHeight(60);
    m_dspWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    auto *dspDock = new AdvancedDockWidget(
        tr("DSP"), 
        QIcon(":/icons/audio-signal.svg"),
        m_dspWidget->getTitleWidget(), 
        this,
        AdvancedDockWidget::TitleBarPosition::TitleLeft
    );
    dspDock->setWidget(m_dspWidget);
    dspDock->setObjectName("dsp");
    dspDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dspDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, dspDock);
    m_docks["dsp"] = dspDock;

    // === Track Preview dock (right side - full height) ===
    // Control bar is now global in SectionSwitcher
    m_trackPreview = new TrackPreviewWidget(m_engine, this);
    m_trackPreview->setMinimumSize(300, 150);
    m_trackPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    auto *previewDock = new AdvancedDockWidget(
        tr("Track Preview"), 
        QIcon(":/icons/track.svg"),
        m_trackPreview->getTitleWidget(), 
        this
    );
    previewDock->setWidget(m_trackPreview);
    previewDock->setObjectName("trackpreview");
    previewDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    previewDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::TopDockWidgetArea, previewDock);
    m_docks["trackpreview"] = previewDock;

    // === Configure layout ===
    // Layout: [Spectrum | Pan | Track Preview] (top row - 3 columns)
    //         [        DSP Engine            ] (bottom row - full width)
    
    // Arrange top row: spectrum -> pan -> preview (horizontally)
    splitDockWidget(spectrumDock, panDock, Qt::Horizontal);
    splitDockWidget(panDock, previewDock, Qt::Horizontal);
    
    // Show all docks
    for (auto dock : m_docks) {
        dock->setVisible(true);
    }

    // Set horizontal size ratios - spectrum:pan:preview = 40:20:40
    QList<QDockWidget*> horizDocks = {spectrumDock, panDock, previewDock};
    QList<int> horizSizes = {400, 200, 400};
    resizeDocks(horizDocks, horizSizes, Qt::Horizontal);
    
    // Set vertical ratio - top row : dsp = 80:20
    QList<QDockWidget*> vertDocks = {spectrumDock, dspDock};
    QList<int> vertSizes = {500, 100};
    resizeDocks(vertDocks, vertSizes, Qt::Vertical);
}

void DSPEditorSection::onSectionActivated()
{
    // Enable spectrum analysis when section is visible
    if (m_engine->getSpectrumAnalyzer()) {
        m_engine->getSpectrumAnalyzer()->setEnableSpectrumAnalysis(true);
    }
    // Enable pan analysis when section is visible
    if (m_engine->getPanAnalyzer()) {
        m_engine->getPanAnalyzer()->setEnabled(true);
    }
}

void DSPEditorSection::onSectionDeactivated()
{
    // Disable spectrum analysis when section is hidden to save resources
    if (m_engine->getSpectrumAnalyzer()) {
        m_engine->getSpectrumAnalyzer()->setEnableSpectrumAnalysis(false);
    }
    // Disable pan analysis when section is hidden to save resources
    if (m_engine->getPanAnalyzer()) {
        m_engine->getPanAnalyzer()->setEnabled(false);
    }
}

void DSPEditorSection::refreshDSPWidgets()
{
    if (m_dspWidget) {
        m_dspWidget->refresh();
    }
}
