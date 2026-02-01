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
#include <algorithm>
#include <set>

LilyPondWidget::LilyPondWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_sequence(nullptr)
    , m_process(nullptr)
    , m_tempDir(nullptr)
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
    for (QLabel *label : m_pageLabels) {
        m_pagesLayout->removeWidget(label);
        delete label;
    }
    m_pageLabels.clear();
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
    
    // Write LilyPond source to file
    QString lyPath = m_tempDir->path() + "/notation.ly";
    QFile lyFile(lyPath);
    if (!lyFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Failed to write temporary file"));
        return;
    }
    lyFile.write(lilypondSource.toUtf8());
    lyFile.close();
    
    // Clean up old PNG files
    QDir tempDir(m_tempDir->path());
    for (const QString &file : tempDir.entryList(QStringList() << "*.png")) {
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
    
    // LilyPond arguments for PNG output with configurable resolution
    QStringList args;
    args << "--png"
         << QString("-dresolution=%1").arg(m_settings.resolution)
         << "-o" << "notation"
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
    QString basePath = m_tempDir->path() + "/notation";
    
    // Check for single page output
    if (QFileInfo::exists(basePath + ".png")) {
        QPixmap page(basePath + ".png");
        if (!page.isNull()) {
            m_pagePixmaps << page;
        }
    } else {
        // Check for multi-page output
        for (int pageNum = 1; pageNum <= 100; ++pageNum) {
            QString pagePath = QString("%1-page%2.png").arg(basePath).arg(pageNum);
            if (QFileInfo::exists(pagePath)) {
                QPixmap page(pagePath);
                if (!page.isNull()) {
                    m_pagePixmaps << page;
                }
            } else {
                break;
            }
        }
    }
    
    if (m_pagePixmaps.isEmpty()) {
        showError(tr("LilyPond did not generate output images"));
        emit renderingError("No output generated");
        return;
    }
    
    // Create labels for each page and display
    updateDisplay();
    showPages();
    
    emit renderingComplete();
}

void LilyPondWidget::updateDisplay()
{
    // Clear existing labels
    for (QLabel *label : m_pageLabels) {
        m_pagesLayout->removeWidget(label);
        delete label;
    }
    m_pageLabels.clear();
    
    // Create new labels with scaled pixmaps
    for (const QPixmap &pixmap : m_pagePixmaps) {
        QLabel *pageLabel = new QLabel();
        pageLabel->setAlignment(Qt::AlignCenter);
        
        // Scale pixmap
        QPixmap scaled = pixmap.scaled(
            pixmap.size() * m_zoom,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        
        pageLabel->setPixmap(scaled);
        pageLabel->setFixedSize(scaled.size());
        
        // White paper with shadow effect
        pageLabel->setStyleSheet(R"(
            QLabel {
                background: white;
                border: 1px solid #444;
            }
        )");
        
        m_pagesLayout->addWidget(pageLabel, 0, Qt::AlignHCenter);
        m_pageLabels << pageLabel;
    }
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
