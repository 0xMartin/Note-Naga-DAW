#include "lilypond_widget.h"
#include "../nn_gui_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QPrinter>
#include <QPrintDialog>
#include <QScrollBar>
#include <algorithm>

// ============================================================================
// NotationPageWidget - Custom page widget with highlight overlay
// ============================================================================

NotationPageWidget::NotationPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_hasHighlight(false)
    , m_highlightYStart(0)
    , m_highlightYEnd(0)
{
    setMinimumSize(100, 100);
}

void NotationPageWidget::setPixmap(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    setFixedSize(pixmap.size());
    update();
}

void NotationPageWidget::setHighlightRegion(double yStart, double yEnd)
{
    m_hasHighlight = true;
    m_highlightYStart = yStart;
    m_highlightYEnd = yEnd;
    update();
}

void NotationPageWidget::clearHighlight()
{
    m_hasHighlight = false;
    update();
}

QSize NotationPageWidget::sizeHint() const
{
    return m_pixmap.size();
}

void NotationPageWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    
    // Draw the page pixmap
    if (!m_pixmap.isNull()) {
        p.drawPixmap(0, 0, m_pixmap);
    }
    
    // Draw highlight overlay if active
    if (m_hasHighlight) {
        int h = height();
        int w = width();
        
        int yStart = static_cast<int>(m_highlightYStart * h);
        int yEnd = static_cast<int>(m_highlightYEnd * h);
        int highlightHeight = qMax(yEnd - yStart, 20);  // Minimum height
        
        // Semi-transparent highlight
        QColor highlightColor(255, 220, 100, 60);  // Yellow-ish
        p.fillRect(0, yStart, w, highlightHeight, highlightColor);
        
        // Highlight border
        p.setPen(QPen(QColor(255, 180, 50, 200), 2));
        p.drawRect(1, yStart, w - 2, highlightHeight);
    }
}

// ============================================================================
// LilyPondWidget
// ============================================================================

LilyPondWidget::LilyPondWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_sequence(nullptr)
    , m_process(nullptr)
    , m_tempDir(nullptr)
    , m_currentTick(0)
    , m_currentMeasureIndex(-1)
    , m_autoScroll(true)
    , m_ticksPerMeasure(480 * 4)  // Default 4/4 at 480 PPQ
    , m_totalMeasures(0)
    , m_zoom(0.6)  // Start at 60% for better fit
    , m_lilypondAvailable(false)
    , m_rendering(false)
    , m_needsRender(false)
{
    setupUi();
    
    // Create temp directory for LilyPond files
    m_tempDir = new QTemporaryDir();
    if (!m_tempDir->isValid()) {
        m_errorMessage = tr("Failed to create temporary directory");
        showError(m_errorMessage);
    }
    
    // Check if LilyPond is available
    checkLilypondAvailable();
}

LilyPondWidget::~LilyPondWidget()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
        delete m_process;
    }
    delete m_tempDir;
}

void LilyPondWidget::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Toolbar (zoom controls only - Refresh/Print are in dock title)
    QWidget *toolbar = new QWidget(this);
    toolbar->setStyleSheet("QWidget { background: #2a2d35; border-bottom: 1px solid #3a3d45; }");
    toolbar->setFixedHeight(36);
    
    m_toolbarLayout = new QHBoxLayout(toolbar);
    m_toolbarLayout->setContentsMargins(8, 4, 8, 4);
    m_toolbarLayout->setSpacing(8);
    
    m_toolbarLayout->addStretch();
    
    // Zoom controls
    m_zoomOutBtn = new QToolButton(this);
    m_zoomOutBtn->setText("-");
    m_zoomOutBtn->setFixedSize(28, 28);
    m_zoomOutBtn->setStyleSheet(R"(
        QToolButton {
            background: #3a3d45;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            font-weight: bold;
        }
        QToolButton:hover { background: #4a4d55; }
    )");
    connect(m_zoomOutBtn, &QToolButton::clicked, this, &LilyPondWidget::zoomOut);
    m_toolbarLayout->addWidget(m_zoomOutBtn);
    
    m_zoomLabel = new QLabel("60%", this);
    m_zoomLabel->setStyleSheet("color: #aaa; min-width: 45px;");
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_toolbarLayout->addWidget(m_zoomLabel);
    
    m_zoomInBtn = new QToolButton(this);
    m_zoomInBtn->setText("+");
    m_zoomInBtn->setFixedSize(28, 28);
    m_zoomInBtn->setStyleSheet(R"(
        QToolButton {
            background: #3a3d45;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            font-weight: bold;
        }
        QToolButton:hover { background: #4a4d55; }
    )");
    connect(m_zoomInBtn, &QToolButton::clicked, this, &LilyPondWidget::zoomIn);
    m_toolbarLayout->addWidget(m_zoomInBtn);
    
    m_mainLayout->addWidget(toolbar);
    
    // Status label (for errors/loading)
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("QLabel { color: #888; background: #1e1e24; padding: 40px; font-size: 14px; }");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setText(tr("Click 'Render' to generate notation"));
    m_mainLayout->addWidget(m_statusLabel);
    
    // Scroll area for pages - dark background
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(R"(
        QScrollArea { 
            background: #1e1e24; 
            border: none; 
        }
        QScrollBar:vertical {
            background: #2a2d35;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background: #4a4d55;
            border-radius: 6px;
            min-height: 20px;
        }
    )");
    m_scrollArea->hide();
    
    // Container for pages
    m_pagesContainer = new QWidget();
    m_pagesContainer->setStyleSheet("background: #1e1e24;");
    m_pagesLayout = new QVBoxLayout(m_pagesContainer);
    m_pagesLayout->setContentsMargins(20, 20, 20, 20);
    m_pagesLayout->setSpacing(30);  // Gap between pages
    m_pagesLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    
    m_scrollArea->setWidget(m_pagesContainer);
    m_mainLayout->addWidget(m_scrollArea);
}

void LilyPondWidget::showError(const QString &message)
{
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet("QLabel { color: #ff6b6b; background: #1e1e24; padding: 40px; font-size: 14px; }");
    m_statusLabel->show();
    m_scrollArea->hide();
}

void LilyPondWidget::showPages()
{
    m_statusLabel->hide();
    m_scrollArea->show();
}

void LilyPondWidget::clearPages()
{
    for (NotationPageWidget *widget : m_pageWidgets) {
        m_pagesLayout->removeWidget(widget);
        delete widget;
    }
    m_pageWidgets.clear();
    m_pagePixmaps.clear();
}

QString LilyPondWidget::getLilypondPath() const
{
    // Platform-specific LilyPond paths
    QStringList searchPaths;
    
#ifdef Q_OS_MAC
    searchPaths << "/opt/homebrew/bin/lilypond"
                << "/usr/local/bin/lilypond"
                << "/Applications/LilyPond.app/Contents/Resources/bin/lilypond";
#elif defined(Q_OS_WIN)
    searchPaths << "C:/Program Files/LilyPond/usr/bin/lilypond.exe"
                << "C:/Program Files (x86)/LilyPond/usr/bin/lilypond.exe";
    QString pathEnv = qEnvironmentVariable("PATH");
    for (const QString &dir : pathEnv.split(';')) {
        searchPaths << dir + "/lilypond.exe";
    }
#else // Linux
    searchPaths << "/usr/bin/lilypond"
                << "/usr/local/bin/lilypond";
#endif
    
    QString inPath = QStandardPaths::findExecutable("lilypond");
    if (!inPath.isEmpty()) {
        return inPath;
    }
    
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    
    return "lilypond";
}

QString LilyPondWidget::getMidi2lyPath() const
{
    // Platform-specific midi2ly paths (part of LilyPond installation)
    QStringList searchPaths;
    
#ifdef Q_OS_MAC
    searchPaths << "/opt/homebrew/bin/midi2ly"
                << "/usr/local/bin/midi2ly"
                << "/Applications/LilyPond.app/Contents/Resources/bin/midi2ly";
#elif defined(Q_OS_WIN)
    searchPaths << "C:/Program Files/LilyPond/usr/bin/midi2ly.exe"
                << "C:/Program Files (x86)/LilyPond/usr/bin/midi2ly.exe";
#else // Linux
    searchPaths << "/usr/bin/midi2ly"
                << "/usr/local/bin/midi2ly";
#endif
    
    QString inPath = QStandardPaths::findExecutable("midi2ly");
    if (!inPath.isEmpty()) {
        return inPath;
    }
    
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    
    return "midi2ly";
}

void LilyPondWidget::checkLilypondAvailable()
{
    QString lilypondPath = getLilypondPath();
    
    QProcess testProcess;
    testProcess.start(lilypondPath, QStringList() << "--version");
    testProcess.waitForFinished(5000);
    
    if (testProcess.exitCode() == 0) {
        m_lilypondAvailable = true;
        QString version = testProcess.readAllStandardOutput();
        // LilyPond version: version.split('\n').first()
    } else {
        m_lilypondAvailable = false;
        m_errorMessage = tr("LilyPond not found. Please install LilyPond:\n"
                           "• macOS: brew install lilypond\n"
                           "• Linux: apt install lilypond\n"
                           "• Windows: Download from https://lilypond.org");
        showError(m_errorMessage);
    }
}

void LilyPondWidget::setSequence(NoteNagaMidiSeq *sequence)
{
    m_sequence = sequence;
    m_needsRender = true;
    
    if (!m_sequence) {
        showError(tr("No sequence loaded"));
        return;
    }
    
    // Initialize track visibility if needed
    auto tracks = m_sequence->getTracks();
    if (m_trackVisibility.size() != static_cast<int>(tracks.size())) {
        m_trackVisibility.clear();
        for (size_t i = 0; i < tracks.size(); ++i) {
            m_trackVisibility.append(true);
        }
    }
}

void LilyPondWidget::setTitle(const QString &title)
{
    m_title = title;
    m_needsRender = true;
}

void LilyPondWidget::setTrackVisibility(const QList<bool> &visibility)
{
    m_trackVisibility = visibility;
    m_needsRender = true;
}

void LilyPondWidget::setNotationSettings(const NotationSettings &settings)
{
    m_settings = settings;
    m_needsRender = true;
}

QWidget* LilyPondWidget::createTitleButtonWidget(QWidget *parent)
{
    QWidget *container = new QWidget(parent);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    
    // Refresh button
    QPushButton *refreshBtn = create_small_button(
        ":/icons/reload.svg",
        tr("Render notation"),
        "refreshNotationBtn",
        24,
        container
    );
    connect(refreshBtn, &QPushButton::clicked, this, &LilyPondWidget::render);
    layout->addWidget(refreshBtn);
    
    // Print button
    QPushButton *printBtn = create_small_button(
        ":/icons/print.svg",
        tr("Print notation"),
        "printNotationBtn",
        24,
        container
    );
    connect(printBtn, &QPushButton::clicked, this, &LilyPondWidget::print);
    layout->addWidget(printBtn);
    
    return container;
}

void LilyPondWidget::render()
{
    if (!m_sequence) {
        showError(tr("No sequence loaded"));
        return;
    }
    
    if (!m_lilypondAvailable) {
        showError(m_errorMessage);
        return;
    }
    
    if (m_rendering) {
        return;
    }
    
    // Don't render while MIDI is playing
    if (m_engine && m_engine->isPlaying()) {
        showError(tr("Cannot render while playing. Stop playback first."));
        return;
    }
    
    if (!m_tempDir || !m_tempDir->isValid()) {
        showError(tr("Temporary directory not available"));
        return;
    }
    
    // Clean up old temp files before generating new ones
    QDir tempDir(m_tempDir->path());
    for (const QString &file : tempDir.entryList(QStringList() << "*.mid" << "*.ly" << "*.png", QDir::Files)) {
        tempDir.remove(file);
    }
    
    // Step 1: Export MIDI sequence to temp file (only visible tracks)
    QString midiPath = m_tempDir->path() + "/notation.mid";
    
    // Build set of visible track IDs based on m_trackVisibility
    std::set<int> visibleTrackIds;
    auto tracks = m_sequence->getTracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        // If visibility list is empty or shorter, default to visible
        bool isVisible = (i >= static_cast<size_t>(m_trackVisibility.size())) || m_trackVisibility[i];
        if (isVisible) {
            visibleTrackIds.insert(tracks[i]->getId());
        }
    }
    
    if (!m_sequence->exportToMidi(midiPath.toStdString(), visibleTrackIds)) {
        showError(tr("Failed to export MIDI file"));
        return;
    }
    
    // Step 2: Use midi2ly to convert MIDI to LilyPond
    QString lyPath = m_tempDir->path() + "/notation.ly";
    QString midi2lyPath = getMidi2lyPath();
    
    QProcess midi2lyProcess;
    midi2lyProcess.setWorkingDirectory(m_tempDir->path());
    
    // Use full path for output to ensure it goes to the right place
    QString lyOutputPath = m_tempDir->path() + "/notation";  // midi2ly adds .ly
    
    QStringList midi2lyArgs;
    // Note: --absolute-pitches causes bugs in some midi2ly versions
    // Use -d and -s for duration and start quantization (16 = sixteenth notes)
    midi2lyArgs << "-d" << "16"           // Quantize note durations to 16th notes
                << "-s" << "16"           // Quantize note starts to 16th notes
                << "-o" << lyOutputPath   // Full path output
                << midiPath;
    
    midi2lyProcess.start(midi2lyPath, midi2lyArgs);
    if (!midi2lyProcess.waitForFinished(30000)) {
        showError(tr("midi2ly conversion timeout"));
        return;
    }
    
    QString stdErr = midi2lyProcess.readAllStandardError();
    
    if (midi2lyProcess.exitCode() != 0) {
        qWarning() << "midi2ly error:" << stdErr;
        showError(tr("midi2ly conversion failed:\n%1").arg(stdErr));
        return;
    }
    
    // midi2ly output path - it doesn't add .ly extension automatically
    // The output is just the path we specified
    QString lyPathNoExt = lyOutputPath;  // /path/to/notation
    lyPath = lyOutputPath + ".ly";       // Try with .ly first
    
    // Check if file was created (try both with and without .ly)
    if (!QFileInfo::exists(lyPath)) {
        // Try without .ly extension
        if (QFileInfo::exists(lyPathNoExt)) {
            lyPath = lyPathNoExt;
        } else {
            // Try to find any file that looks like lilypond output
            QDir tempDir(m_tempDir->path());
            QStringList allFiles = tempDir.entryList(QDir::Files);
            
            // Look for notation file
            for (const QString &file : allFiles) {
                if (file.startsWith("notation") && !file.endsWith(".mid")) {
                    lyPath = m_tempDir->path() + "/" + file;
                    break;
                }
            }
            
            if (!QFileInfo::exists(lyPath)) {
                showError(tr("midi2ly did not generate output file"));
                return;
            }
        }
    }
    
    // Step 3: Read and modify the generated .ly file
    QFile lyFile(lyPath);
    if (!lyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showError(tr("Failed to read generated LilyPond file: %1").arg(lyPath));
        return;
    }
    
    QString lilypondSource = QString::fromUtf8(lyFile.readAll());
    lyFile.close();
    
    // Post-process the midi2ly output to create a cleaner piano score
    // midi2ly generates complex multi-voice output that we need to simplify
    
    // Extract the track definitions (everything between the first = and \score)
    int scoreStart = lilypondSource.indexOf("\\score");
    QString trackDefs = lilypondSource.left(scoreStart);
    
    // Find which channels have melody (higher notes) vs bass (lower notes)
    // trackBchannelC and trackBchannelCvoiceB are typically treble clef
    // trackBchannelD and trackBchannelDvoiceB are typically bass clef
    
    // Create a cleaner LilyPond source with proper piano staff
    // Use settings for customization
    QString barNumbersCmd = m_settings.showBarNumbers ? "" : "\\override Score.BarNumber.break-visibility = ##(#f #f #f)\n";
    
    QString cleanSource = QString(R"(\version "2.24.0"

#(set-global-staff-size %1)

\paper {
  #(set-paper-size "a4")
  top-margin = 15\mm
  bottom-margin = 15\mm
  left-margin = 15\mm
  right-margin = 15\mm
}

\header {
  title = "%2"
  tagline = ##f
}

\layout {
  %3
  \context {
    \Voice
    \remove Note_heads_engraver
    \consists Completion_heads_engraver
    \remove Rest_engraver
    \consists Completion_rest_engraver
  }
}

)").arg(m_settings.fontSize).arg(m_title.isEmpty() ? "Untitled" : m_title).arg(barNumbersCmd);

    // Extract individual track definitions from midi2ly output
    // Look for trackBchannelC (melody) and trackBchannelD (bass)
    QRegularExpression trackCRegex("trackBchannelC\\s*=\\s*\\\\relative\\s+c\\s*\\{([^}]+(?:\\{[^}]*\\}[^}]*)*)\\}");
    QRegularExpression trackDRegex("trackBchannelD\\s*=\\s*\\\\relative\\s+c\\s*\\{([^}]+(?:\\{[^}]*\\}[^}]*)*)\\}");
    
    QRegularExpressionMatch matchC = trackCRegex.match(lilypondSource);
    QRegularExpressionMatch matchD = trackDRegex.match(lilypondSource);
    
    QString trebleContent, bassContent;
    
    if (matchC.hasMatch()) {
        trebleContent = matchC.captured(1).trimmed();
    }
    if (matchD.hasMatch()) {
        bassContent = matchD.captured(1).trimmed();
    }
    
    // If we couldn't extract properly, fall back to original
    if (trebleContent.isEmpty() && bassContent.isEmpty()) {
        // Use original midi2ly output but add our paper settings
        cleanSource = lilypondSource;
        // Remove any existing \paper and \header blocks first
        cleanSource.replace(QRegularExpression("\\\\paper\\s*\\{[^}]*\\}"), "");
        cleanSource.replace(QRegularExpression("\\\\header\\s*\\{[^}]*\\}"), "");
        
        // Insert our settings after \version
        int versionEnd = cleanSource.indexOf('\n');
        if (versionEnd > 0) {
            QString settings = QString(R"(

\paper {
  #(set-paper-size "a4")
  top-margin = 15\mm
  bottom-margin = 15\mm
  left-margin = 15\mm
  right-margin = 15\mm
}

\header {
  title = "%1"
  tagline = ##f
}

)").arg(m_title.isEmpty() ? "Untitled" : m_title);
            cleanSource.insert(versionEnd + 1, settings);
        }
    } else {
        // Get track names from the MIDI sequence
        QString trebleName = "Melody";
        QString bassName = "Bass";
        
        // Check track visibility
        bool showTreble = m_trackVisibility.isEmpty() || (m_trackVisibility.size() > 0 && m_trackVisibility[0]);
        bool showBass = m_trackVisibility.isEmpty() || (m_trackVisibility.size() > 1 && m_trackVisibility[1]);
        
        if (m_sequence) {
            auto tracks = m_sequence->getTracks();
            if (tracks.size() >= 1 && !tracks[0]->getName().empty()) {
                trebleName = QString::fromStdString(tracks[0]->getName());
            }
            if (tracks.size() >= 2 && !tracks[1]->getName().empty()) {
                bassName = QString::fromStdString(tracks[1]->getName());
            }
        }
        
        // Build staves based on visibility and settings
        QString keyCmd = QString("\\key %1").arg(m_settings.keySignature);
        QString timeCmd = QString("\\time %1").arg(m_settings.timeSignature);
        
        if (showTreble) {
            cleanSource += QString("treble = \\relative c' {\n  \\clef treble\n  %1\n  %2\n").arg(keyCmd).arg(timeCmd);
            if (!trebleContent.isEmpty()) {
                // Clean up the content - remove voice commands
                trebleContent.replace("\\voiceThree", "");
                trebleContent.replace("\\voiceOne", "");
                cleanSource += "  " + trebleContent + "\n";
            } else {
                cleanSource += "  r1\n";
            }
            cleanSource += "}\n\n";
        }
        
        if (showBass) {
            cleanSource += QString("bass = \\relative c {\n  \\clef bass\n  %1\n  %2\n").arg(keyCmd).arg(timeCmd);
            if (!bassContent.isEmpty()) {
                // Clean up the content
                bassContent.replace("\\voiceTwo", "");
                bassContent.replace("\\voiceFour", "");
                cleanSource += "  " + bassContent + "\n";
            } else {
                cleanSource += "  r1\n";
            }
            cleanSource += "}\n\n";
        }
        
        // Create score based on which staves are visible
        if (showTreble && showBass) {
            // Both staves - use PianoStaff
            cleanSource += QString(R"(\score {
  \new PianoStaff \with {
    instrumentName = "Piano"
  } <<
    \new Staff = "upper" \with {
      instrumentName = "%1"
    } \treble
    \new Staff = "lower" \with {
      instrumentName = "%2"
    } \bass
  >>
  \layout { }
}
)").arg(trebleName).arg(bassName);
        } else if (showTreble) {
            // Only treble
            cleanSource += QString(R"(\score {
  \new Staff \with {
    instrumentName = "%1"
  } \treble
  \layout { }
}
)").arg(trebleName);
        } else if (showBass) {
            // Only bass
            cleanSource += QString(R"(\score {
  \new Staff \with {
    instrumentName = "%1"
  } \bass
  \layout { }
}
)").arg(bassName);
        } else {
            // No tracks visible
            cleanSource += R"(\score {
  { r1^"No tracks visible" }
  \layout { }
}
)";
        }
    }
    
    // Start LilyPond rendering
    startRendering(cleanSource);
}

void LilyPondWidget::zoomIn()
{
    setZoom(m_zoom + 0.1);
}

void LilyPondWidget::zoomOut()
{
    setZoom(m_zoom - 0.1);
}

void LilyPondWidget::setZoom(double zoom)
{
    double newZoom = qBound(0.2, zoom, 2.0);
    if (qAbs(newZoom - m_zoom) < 0.01) return;
    
    m_zoom = newZoom;
    m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_zoom * 100)));
    
    updateDisplay();
    emit zoomChanged(m_zoom);
}

void LilyPondWidget::print()
{
    if (m_pagePixmaps.isEmpty()) {
        return;
    }
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Notation"));
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    QPainter painter(&printer);
    
    for (int i = 0; i < m_pagePixmaps.size(); ++i) {
        if (i > 0) {
            printer.newPage();
        }
        
        const QPixmap &page = m_pagePixmaps[i];
        
        // Scale to fit printer page while maintaining aspect ratio
        QRect pageRect = printer.pageRect(QPrinter::DevicePixel).toRect();
        QSize scaledSize = page.size().scaled(pageRect.size(), Qt::KeepAspectRatio);
        
        // Center on page
        int x = (pageRect.width() - scaledSize.width()) / 2;
        int y = (pageRect.height() - scaledSize.height()) / 2;
        
        painter.drawPixmap(x, y, scaledSize.width(), scaledSize.height(), page);
    }
    
    painter.end();
}

void LilyPondWidget::startRendering(const QString &lilypondSource)
{
    if (m_rendering) {
        return;
    }
    
    if (!m_tempDir || !m_tempDir->isValid()) {
        showError(tr("Temporary directory not available"));
        return;
    }
    
    // Write LilyPond source to file with fixed name
    QString lyPath = m_tempDir->path() + "/notation_input.ly";
    QFile lyFile(lyPath);
    if (!lyFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Failed to write temporary file"));
        return;
    }
    lyFile.write(lilypondSource.toUtf8());
    lyFile.close();
    
    // Clean up old SVG and PNG files
    QDir tempDir(m_tempDir->path());
    for (const QString &file : tempDir.entryList(QStringList() << "*.png" << "*.svg")) {
        tempDir.remove(file);
    }
    
    // Start LilyPond process
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
        delete m_process;
    }
    
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LilyPondWidget::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &LilyPondWidget::onProcessError);
    
    m_process->setWorkingDirectory(m_tempDir->path());
    
    // LilyPond arguments for PNG output
    QStringList args;
    args << "--png"                    // PNG output
         << "-dresolution=" + QString::number(m_settings.resolution)
         << lyPath;
    
    QString lilypondPath = getLilypondPath();
    
    m_rendering = true;
    m_statusLabel->setText(tr("Rendering notation..."));
    m_statusLabel->setStyleSheet("QLabel { color: #aaa; background: #1e1e24; padding: 40px; font-size: 14px; }");
    m_statusLabel->show();
    m_scrollArea->hide();
    
    emit renderingStarted();
    m_process->start(lilypondPath, args);
}

void LilyPondWidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_rendering = false;
    m_needsRender = false;
    
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString errorOutput = m_process->readAllStandardError();
        m_errorMessage = tr("LilyPond error:\n%1").arg(errorOutput);
        showError(m_errorMessage);
        emit renderingError(m_errorMessage);
        return;
    }
    
    // Clear old pages
    clearPages();
    
    // Find all generated PNG pages
    // LilyPond outputs: notation_input.png (single page) or 
    // notation_input-page1.png, notation_input-page2.png, etc.
    QString baseName = m_tempDir->path() + "/notation_input";
    
    // Check for single page output
    if (QFileInfo::exists(baseName + ".png")) {
        QPixmap pixmap(baseName + ".png");
        if (!pixmap.isNull()) {
            m_pagePixmaps << pixmap;
        }
    } else {
        // Check for multi-page output
        for (int pageNum = 1; pageNum <= 100; ++pageNum) {
            QString pagePath = QString("%1-page%2.png").arg(baseName).arg(pageNum);
            if (QFileInfo::exists(pagePath)) {
                QPixmap pixmap(pagePath);
                if (!pixmap.isNull()) {
                    m_pagePixmaps << pixmap;
                }
            } else {
                break;
            }
        }
    }
    
    if (m_pagePixmaps.isEmpty()) {
        showError(tr("LilyPond did not generate output"));
        emit renderingError("No output generated");
        return;
    }
    
    // Build measure map for playback highlighting
    buildMeasureMap();
    
    // Create labels for each page and display
    updateDisplay();
    showPages();
    
    emit renderingComplete();
}

void LilyPondWidget::updateDisplay()
{
    // Clear existing page widgets
    for (NotationPageWidget *widget : m_pageWidgets) {
        m_pagesLayout->removeWidget(widget);
        delete widget;
    }
    m_pageWidgets.clear();
    
    // Create new page widgets with scaled pixmaps
    for (const QPixmap &pixmap : m_pagePixmaps) {
        NotationPageWidget *pageWidget = new NotationPageWidget();
        
        // Scale pixmap
        QPixmap scaled = pixmap.scaled(
            pixmap.size() * m_zoom,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        
        pageWidget->setPixmap(scaled);
        
        // White paper with shadow effect
        pageWidget->setStyleSheet(R"(
            NotationPageWidget {
                background: white;
                border: 1px solid #444;
            }
        )");
        
        m_pagesLayout->addWidget(pageWidget, 0, Qt::AlignHCenter);
        m_pageWidgets << pageWidget;
    }
    
    // Build measure map for playback highlighting
    buildMeasureMap();
    
    // Update current highlight
    updateHighlight();
}

void LilyPondWidget::buildMeasureMap()
{
    m_measurePositions.clear();
    
    if (!m_sequence || m_pageWidgets.isEmpty() || m_pagePixmaps.isEmpty()) {
        return;
    }
    
    // Calculate ticks per measure based on time signature
    int ppq = 480;
    QStringList timeParts = m_settings.timeSignature.split('/');
    int numerator = 4, denominator = 4;
    if (timeParts.size() == 2) {
        numerator = timeParts[0].toInt();
        denominator = timeParts[1].toInt();
    }
    m_ticksPerMeasure = (ppq * 4 * numerator) / denominator;
    
    // Find total duration from all tracks
    int totalTicks = 0;
    auto tracks = m_sequence->getTracks();
    for (auto *track : tracks) {
        auto notes = track->getNotes();
        for (const auto &note : notes) {
            int noteStart = note.start.value_or(0);
            int noteLength = note.length.value_or(0);
            int endTick = noteStart + noteLength;
            totalTicks = qMax(totalTicks, endTick);
        }
    }
    
    m_totalMeasures = (totalTicks + m_ticksPerMeasure - 1) / m_ticksPerMeasure;
    if (m_totalMeasures < 1) m_totalMeasures = 1;
    
    // Detect systems in each page by scanning pixels
    QList<QPair<int, QPair<double, double>>> allSystems;  // (pageIndex, (yStart, yEnd))
    
    for (int pageIdx = 0; pageIdx < m_pagePixmaps.size(); ++pageIdx) {
        QList<QPair<double, double>> pageSystems = detectSystemsInPage(m_pagePixmaps[pageIdx]);
        qDebug() << "Page" << pageIdx << "detected" << pageSystems.size() << "systems";
        for (int i = 0; i < pageSystems.size(); ++i) {
            qDebug() << "  System" << i << ": y" << pageSystems[i].first << "-" << pageSystems[i].second;
        }
        for (const auto &sys : pageSystems) {
            allSystems.append(qMakePair(pageIdx, sys));
        }
    }
    
    qDebug() << "Total systems detected:" << allSystems.size() << "for" << m_totalMeasures << "measures";
    
    if (allSystems.isEmpty()) {
        qDebug() << "Detection failed, using fallback";
        // Fallback to simple approximation if detection fails
        int numPages = m_pageWidgets.size();
        double systemHeight = 0.12;
        int systemsPerPage = 7;
        
        for (int measure = 0; measure < m_totalMeasures; ++measure) {
            MeasurePosition pos;
            int sysIndex = measure / 3;  // ~3 measures per system
            pos.pageIndex = sysIndex / systemsPerPage;
            if (pos.pageIndex >= numPages) pos.pageIndex = numPages - 1;
            int sysOnPage = sysIndex % systemsPerPage;
            pos.yPosition = 0.08 + sysOnPage * 0.13;
            pos.height = systemHeight;
            pos.startTick = measure * m_ticksPerMeasure;
            pos.endTick = (measure + 1) * m_ticksPerMeasure;
            m_measurePositions.append(pos);
        }
        return;
    }
    
    // Distribute measures evenly across detected systems
    int measuresPerSystem = qMax(1, (m_totalMeasures + allSystems.size() - 1) / allSystems.size());
    qDebug() << "Measures per system:" << measuresPerSystem;
    
    for (int measure = 0; measure < m_totalMeasures; ++measure) {
        int sysIndex = measure / measuresPerSystem;
        if (sysIndex >= allSystems.size()) {
            sysIndex = allSystems.size() - 1;
        }
        
        MeasurePosition pos;
        pos.pageIndex = allSystems[sysIndex].first;
        pos.yPosition = allSystems[sysIndex].second.first;
        pos.height = allSystems[sysIndex].second.second - allSystems[sysIndex].second.first;
        pos.startTick = measure * m_ticksPerMeasure;
        pos.endTick = (measure + 1) * m_ticksPerMeasure;
        
        m_measurePositions.append(pos);
    }
    
    // Debug first few measures
    for (int i = 0; i < qMin(5, m_measurePositions.size()); ++i) {
        qDebug() << "Measure" << i << ": page" << m_measurePositions[i].pageIndex 
                 << "y" << m_measurePositions[i].yPosition;
    }
}

QList<QPair<double, double>> LilyPondWidget::detectSystemsInPage(const QPixmap &pixmap)
{
    // Detect notation systems by scanning a vertical line on the LEFT side of the page
    // where staff lines are most reliably present (near the clef/time signature area)
    
    QList<QPair<double, double>> systems;  // (yStart, yEnd) normalized to 0-1
    
    if (pixmap.isNull()) return systems;
    
    QImage image = pixmap.toImage();
    int width = image.width();
    int height = image.height();
    
    if (width == 0 || height == 0) return systems;
    
    // Scan position: ~8% from left (just after the left margin, where staff lines begin)
    // This is where the clef and time signature are, so there are always dark pixels
    int scanX = static_cast<int>(width * 0.08);
    
    // Threshold for "dark" pixel (lower = darker)
    const int darkThreshold = 240;  // More lenient threshold to catch gray staff lines
    
    // Scan from top to bottom and detect dark regions
    QVector<bool> isDark(height, false);
    
    // Average over a wider range of columns for more robust detection
    for (int y = 0; y < height; ++y) {
        int darkCount = 0;
        // Scan across a wider band (from 5% to 15% of page width)
        for (int xPercent = 5; xPercent <= 15; xPercent += 2) {
            int x = static_cast<int>(width * xPercent / 100.0);
            x = qBound(0, x, width - 1);
            QRgb pixel = image.pixel(x, y);
            int gray = qGray(pixel);
            if (gray < darkThreshold) {
                darkCount++;
            }
        }
        isDark[y] = (darkCount >= 1);  // At least 1 of 6 columns is dark
    }
    
    // Find contiguous dark regions (systems)
    // A system needs at least minSystemHeight pixels
    const int minSystemHeight = height / 40;  // Minimum 2.5% of page height (smaller threshold)
    const int minGap = height / 60;           // Minimum gap between systems (~1.7%)
    
    bool inSystem = false;
    int systemStart = 0;
    int gapCount = 0;
    
    for (int y = 0; y < height; ++y) {
        if (isDark[y]) {
            if (!inSystem) {
                // Start of new system
                inSystem = true;
                systemStart = y;
            }
            gapCount = 0;
        } else {
            if (inSystem) {
                gapCount++;
                // If gap is large enough, end the system
                if (gapCount >= minGap) {
                    int systemEnd = y - gapCount;
                    int sysHeight = systemEnd - systemStart;
                    if (sysHeight >= minSystemHeight) {
                        // Valid system found
                        double yStart = static_cast<double>(systemStart) / height;
                        double yEnd = static_cast<double>(systemEnd) / height;
                        systems.append(qMakePair(yStart, yEnd));
                    }
                    inSystem = false;
                }
            }
        }
    }
    
    // Don't forget last system if we ended while in one
    if (inSystem) {
        int systemEnd = height - gapCount;
        int sysHeight = systemEnd - systemStart;
        if (sysHeight >= minSystemHeight) {
            double yStart = static_cast<double>(systemStart) / height;
            double yEnd = static_cast<double>(systemEnd) / height;
            systems.append(qMakePair(yStart, yEnd));
        }
    }
    
    // MERGE CLOSE SYSTEMS: Piano staff has treble + bass clef close together
    // They should be treated as ONE system, not two separate ones
    // Merge systems that are within 5% of page height of each other
    const double mergeThreshold = 0.05;  // 5% of page height
    QList<QPair<double, double>> mergedSystems;
    
    for (int i = 0; i < systems.size(); ++i) {
        if (mergedSystems.isEmpty()) {
            mergedSystems.append(systems[i]);
        } else {
            // Check if this system is close to the previous one
            double prevEnd = mergedSystems.last().second;
            double currStart = systems[i].first;
            
            if (currStart - prevEnd < mergeThreshold) {
                // Merge: extend the previous system to include this one
                mergedSystems.last().second = systems[i].second;
            } else {
                // Gap is large enough, this is a new system
                mergedSystems.append(systems[i]);
            }
        }
    }
    
    return mergedSystems;
}

void LilyPondWidget::setPlaybackPosition(int tick)
{
    if (tick == m_currentTick) return;
    
    m_currentTick = tick;
    
    // Find which measure we're in
    int newMeasureIndex = -1;
    for (int i = 0; i < m_measurePositions.size(); ++i) {
        if (tick >= m_measurePositions[i].startTick && tick < m_measurePositions[i].endTick) {
            newMeasureIndex = i;
            break;
        }
    }
    
    // If past the last measure, use the last one
    if (newMeasureIndex < 0 && !m_measurePositions.isEmpty()) {
        if (tick >= m_measurePositions.last().startTick) {
            newMeasureIndex = m_measurePositions.size() - 1;
        }
    }
    
    if (newMeasureIndex != m_currentMeasureIndex) {
        m_currentMeasureIndex = newMeasureIndex;
        updateHighlight();
        
        if (m_autoScroll) {
            scrollToCurrentPosition();
        }
    }
}

void LilyPondWidget::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
}

void LilyPondWidget::updateHighlight()
{
    // Clear all highlights first
    for (NotationPageWidget *widget : m_pageWidgets) {
        widget->clearHighlight();
    }
    
    // Set highlight on the current measure's page
    if (m_currentMeasureIndex >= 0 && m_currentMeasureIndex < m_measurePositions.size()) {
        const MeasurePosition &pos = m_measurePositions[m_currentMeasureIndex];
        
        if (pos.pageIndex >= 0 && pos.pageIndex < m_pageWidgets.size()) {
            NotationPageWidget *widget = m_pageWidgets[pos.pageIndex];
            widget->setHighlightRegion(pos.yPosition, pos.yPosition + pos.height);
        }
    }
}

void LilyPondWidget::scrollToCurrentPosition()
{
    if (m_currentMeasureIndex < 0 || m_currentMeasureIndex >= m_measurePositions.size()) {
        return;
    }
    
    const MeasurePosition &pos = m_measurePositions[m_currentMeasureIndex];
    
    if (pos.pageIndex < 0 || pos.pageIndex >= m_pageWidgets.size()) {
        return;
    }
    
    NotationPageWidget *widget = m_pageWidgets[pos.pageIndex];
    
    // Calculate the global Y position within the scroll area
    int widgetY = widget->mapTo(m_pagesContainer, QPoint(0, 0)).y();
    int highlightY = widgetY + static_cast<int>(pos.yPosition * widget->height());
    
    // Scroll to make the highlight visible (centered if possible)
    QScrollBar *vBar = m_scrollArea->verticalScrollBar();
    int viewportHeight = m_scrollArea->viewport()->height();
    int targetScroll = highlightY - (viewportHeight / 3);  // Show highlight in upper third
    
    vBar->setValue(qBound(0, targetScroll, vBar->maximum()));
}

void LilyPondWidget::onProcessError(QProcess::ProcessError error)
{
    m_rendering = false;
    
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = tr("LilyPond failed to start. Is it installed?");
            break;
        case QProcess::Crashed:
            errorMsg = tr("LilyPond crashed");
            break;
        case QProcess::Timedout:
            errorMsg = tr("LilyPond timed out");
            break;
        default:
            errorMsg = tr("LilyPond process error");
            break;
    }
    
    m_errorMessage = errorMsg;
    showError(errorMsg);
    emit renderingError(errorMsg);
}
