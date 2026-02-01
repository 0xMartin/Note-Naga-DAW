#include "verovio_widget.h"
#include "../nn_gui_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QPrinter>
#include <QPrintDialog>
#include <QScrollBar>
#include <QSvgRenderer>
#include <algorithm>
#include <cmath>

// Verovio includes
#include "toolkit.h"
#include "vrv.h"

// Re-use NotationPageWidget from lilypond_widget
#include "lilypond_widget.h"

// ============================================================================
// VerovioWidget
// ============================================================================

VerovioWidget::VerovioWidget(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_sequence(nullptr)
    , m_toolkit(nullptr)
    , m_currentTick(0)
    , m_currentMeasureIndex(-1)
    , m_autoScroll(true)
    , m_ticksPerMeasure(480 * 4)
    , m_totalMeasures(0)
    , m_zoom(0.6)
    , m_verovioAvailable(false)
    , m_rendering(false)
    , m_needsRender(false)
{
    setupUi();
    
    m_tempDir = new QTemporaryDir();
    if (!m_tempDir->isValid()) {
        m_errorMessage = tr("Failed to create temporary directory");
        showError(m_errorMessage);
    }
    
    initVerovio();
}

VerovioWidget::~VerovioWidget()
{
    delete m_toolkit;
    delete m_tempDir;
}

void VerovioWidget::initVerovio()
{
    try {
        m_toolkit = new vrv::Toolkit(false);  // Don't init fonts yet
        
        // Find Verovio resources (fonts, etc.)
        // Check common locations
        QStringList resourcePaths = {
            "/opt/homebrew/share/verovio",
            "/usr/local/share/verovio",
            "/usr/share/verovio",
            QApplication::applicationDirPath() + "/../Resources/verovio"
        };
        
        for (const QString &path : resourcePaths) {
            if (QDir(path).exists()) {
                m_verovioResourcePath = path;
                break;
            }
        }
        
        if (!m_verovioResourcePath.isEmpty()) {
            m_toolkit->SetResourcePath(m_verovioResourcePath.toStdString());
        }
        
        m_verovioAvailable = true;
        qDebug() << "Verovio initialized, resource path:" << m_verovioResourcePath;
        
    } catch (const std::exception &e) {
        m_errorMessage = tr("Failed to initialize Verovio: %1").arg(e.what());
        m_verovioAvailable = false;
        qWarning() << m_errorMessage;
    }
}

void VerovioWidget::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Toolbar
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
            border: 1px solid #4a4d55;
            border-radius: 4px;
            color: #ccc;
            font-weight: bold;
        }
        QToolButton:hover { background: #4a4d55; }
        QToolButton:pressed { background: #5a5d65; }
    )");
    connect(m_zoomOutBtn, &QToolButton::clicked, this, &VerovioWidget::zoomOut);
    m_toolbarLayout->addWidget(m_zoomOutBtn);
    
    m_zoomLabel = new QLabel(QString("%1%").arg(static_cast<int>(m_zoom * 100)), this);
    m_zoomLabel->setFixedWidth(50);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setStyleSheet("QLabel { color: #888; font-size: 12px; }");
    m_toolbarLayout->addWidget(m_zoomLabel);
    
    m_zoomInBtn = new QToolButton(this);
    m_zoomInBtn->setText("+");
    m_zoomInBtn->setFixedSize(28, 28);
    m_zoomInBtn->setStyleSheet(m_zoomOutBtn->styleSheet());
    connect(m_zoomInBtn, &QToolButton::clicked, this, &VerovioWidget::zoomIn);
    m_toolbarLayout->addWidget(m_zoomInBtn);
    
    m_mainLayout->addWidget(toolbar);
    
    // Scroll area for pages
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(R"(
        QScrollArea { background: #1a1a1f; border: none; }
        QScrollBar:vertical { background: #2a2d35; width: 12px; }
        QScrollBar::handle:vertical { background: #4a4d55; border-radius: 4px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar:horizontal { background: #2a2d35; height: 12px; }
        QScrollBar::handle:horizontal { background: #4a4d55; border-radius: 4px; min-width: 30px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
    )");
    
    m_pagesContainer = new QWidget();
    m_pagesContainer->setStyleSheet("background: #1a1a1f;");
    m_pagesLayout = new QVBoxLayout(m_pagesContainer);
    m_pagesLayout->setContentsMargins(20, 20, 20, 20);
    m_pagesLayout->setSpacing(20);
    m_pagesLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    
    m_scrollArea->setWidget(m_pagesContainer);
    m_mainLayout->addWidget(m_scrollArea);
    
    // Status label (initially hidden)
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("QLabel { color: #aaa; background: #1e1e24; padding: 40px; font-size: 14px; }");
    m_statusLabel->hide();
    m_mainLayout->addWidget(m_statusLabel);
}

void VerovioWidget::setSequence(NoteNagaMidiSeq *sequence)
{
    m_sequence = sequence;
    m_needsRender = true;
    clearPages();
}

void VerovioWidget::setTitle(const QString &title)
{
    m_title = title;
    m_needsRender = true;
}

void VerovioWidget::setTrackVisibility(const QList<bool> &visibility)
{
    if (m_trackVisibility != visibility) {
        m_trackVisibility = visibility;
        m_needsRender = true;
    }
}

void VerovioWidget::setNotationSettings(const NotationSettings &settings)
{
    m_settings = settings;
    m_needsRender = true;
}

QWidget* VerovioWidget::createTitleButtonWidget(QWidget *parent)
{
    QWidget *container = new QWidget(parent);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    
    QPushButton *refreshBtn = create_small_button(
        ":/icons/reload.svg",
        tr("Render notation"),
        "refreshNotationBtn",
        24,
        container
    );
    connect(refreshBtn, &QPushButton::clicked, this, &VerovioWidget::render);
    layout->addWidget(refreshBtn);
    
    QPushButton *printBtn = create_small_button(
        ":/icons/print.svg",
        tr("Print notation"),
        "printNotationBtn",
        24,
        container
    );
    connect(printBtn, &QPushButton::clicked, this, &VerovioWidget::print);
    layout->addWidget(printBtn);
    
    return container;
}

void VerovioWidget::render()
{
    if (!m_sequence) {
        showError(tr("No sequence loaded"));
        return;
    }
    
    if (!m_verovioAvailable) {
        showError(m_errorMessage);
        return;
    }
    
    if (m_rendering) {
        return;
    }
    
    m_rendering = true;
    m_statusLabel->setText(tr("Generating notation..."));
    m_statusLabel->setStyleSheet("QLabel { color: #aaa; background: #1e1e24; padding: 40px; font-size: 14px; }");
    m_statusLabel->show();
    m_scrollArea->hide();
    
    emit renderingStarted();
    
    // Generate MEI from MIDI
    QString meiContent = generateMEI();
    
    if (meiContent.isEmpty()) {
        showError(tr("Failed to generate MEI from MIDI"));
        m_rendering = false;
        emit renderingError("MEI generation failed");
        return;
    }
    
    // Render using Verovio
    if (!renderNotation()) {
        m_rendering = false;
        emit renderingError(m_errorMessage);
        return;
    }
    
    m_rendering = false;
    m_needsRender = false;
    
    updateDisplay();
    showPages();
    
    emit renderingComplete();
}

QString VerovioWidget::generateMEI()
{
    // Generate MEI (Music Encoding Initiative) XML from MIDI sequence
    // MEI is the native format for Verovio
    
    if (!m_sequence) return QString();
    
    auto tracks = m_sequence->getTracks();
    if (tracks.empty()) return QString();
    
    // Calculate ticks per measure
    int ppq = 480;
    QStringList timeParts = m_settings.timeSignature.split('/');
    int numerator = 4, denominator = 4;
    if (timeParts.size() == 2) {
        numerator = timeParts[0].toInt();
        denominator = timeParts[1].toInt();
    }
    m_ticksPerMeasure = (ppq * 4 * numerator) / denominator;
    
    // Find total duration and count measures
    int totalTicks = 0;
    for (size_t i = 0; i < tracks.size(); ++i) {
        bool isVisible = (i >= static_cast<size_t>(m_trackVisibility.size())) || m_trackVisibility[i];
        if (!isVisible) continue;
        
        auto notes = tracks[i]->getNotes();
        for (const auto &note : notes) {
            int noteEnd = note.start.value_or(0) + note.length.value_or(0);
            totalTicks = qMax(totalTicks, noteEnd);
        }
    }
    
    m_totalMeasures = (totalTicks + m_ticksPerMeasure - 1) / m_ticksPerMeasure;
    if (m_totalMeasures < 1) m_totalMeasures = 1;
    
    // Start MEI document
    QString mei;
    mei += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    mei += "<mei xmlns=\"http://www.music-encoding.org/ns/mei\">\n";
    mei += "  <meiHead>\n";
    mei += "    <fileDesc>\n";
    mei += "      <titleStmt>\n";
    mei += QString("        <title>%1</title>\n").arg(m_title.isEmpty() ? "Untitled" : m_title.toHtmlEscaped());
    mei += "      </titleStmt>\n";
    mei += "      <pubStmt/>\n";
    mei += "    </fileDesc>\n";
    mei += "  </meiHead>\n";
    mei += "  <music>\n";
    mei += "    <body>\n";
    mei += "      <mdiv>\n";
    mei += "        <score>\n";
    mei += "          <scoreDef>\n";
    mei += QString("            <staffGrp symbol=\"brace\" bar.thru=\"true\">\n");
    
    // Add staves for visible tracks
    int staffN = 1;
    for (size_t i = 0; i < tracks.size(); ++i) {
        bool isVisible = (i >= static_cast<size_t>(m_trackVisibility.size())) || m_trackVisibility[i];
        if (!isVisible) continue;
        
        QString clef = (i == 0) ? "G" : "F";
        int clefLine = (i == 0) ? 2 : 4;
        
        mei += QString("              <staffDef n=\"%1\" lines=\"5\" clef.shape=\"%2\" clef.line=\"%3\" "
                      "meter.count=\"%4\" meter.unit=\"%5\" key.sig=\"%6\"/>\n")
               .arg(staffN).arg(clef).arg(clefLine)
               .arg(numerator).arg(denominator).arg(m_settings.keySignature);
        staffN++;
    }
    
    mei += "            </staffGrp>\n";
    mei += "          </scoreDef>\n";
    mei += "          <section>\n";
    
    // Generate measures
    for (int measureNum = 0; measureNum < m_totalMeasures; ++measureNum) {
        int measureStart = measureNum * m_ticksPerMeasure;
        int measureEnd = measureStart + m_ticksPerMeasure;
        
        mei += QString("            <measure n=\"%1\" xml:id=\"m%1\">\n").arg(measureNum + 1);
        
        // Add each visible staff
        staffN = 1;
        for (size_t trackIdx = 0; trackIdx < tracks.size(); ++trackIdx) {
            bool isVisible = (trackIdx >= static_cast<size_t>(m_trackVisibility.size())) || m_trackVisibility[trackIdx];
            if (!isVisible) continue;
            
            mei += QString("              <staff n=\"%1\">\n").arg(staffN);
            mei += "                <layer n=\"1\">\n";
            
            // Get notes in this measure for this track
            auto notes = tracks[trackIdx]->getNotes();
            QList<std::pair<int, int>> measureNotes;  // (pitch, start)
            
            for (const auto &note : notes) {
                int noteStart = note.start.value_or(0);
                int notePitch = note.note;  // MIDI note number
                
                if (noteStart >= measureStart && noteStart < measureEnd) {
                    measureNotes.append({notePitch, noteStart - measureStart});
                }
            }
            
            if (measureNotes.isEmpty()) {
                // Empty measure - add whole rest
                mei += "                  <rest dur=\"1\"/>\n";
            } else {
                // Sort by start time
                std::sort(measureNotes.begin(), measureNotes.end(),
                         [](const auto &a, const auto &b) { return a.second < b.second; });
                
                // Add notes (simplified - treating each as quarter note)
                for (const auto &[pitch, relStart] : measureNotes) {
                    QString meiPitch = midiPitchToMEI(pitch);
                    mei += QString("                  %1\n").arg(meiPitch);
                }
            }
            
            mei += "                </layer>\n";
            mei += "              </staff>\n";
            staffN++;
        }
        
        mei += "            </measure>\n";
    }
    
    mei += "          </section>\n";
    mei += "        </score>\n";
    mei += "      </mdiv>\n";
    mei += "    </body>\n";
    mei += "  </music>\n";
    mei += "</mei>\n";
    
    // Save MEI for debugging
    QString meiPath = m_tempDir->path() + "/notation.mei";
    QFile meiFile(meiPath);
    if (meiFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        meiFile.write(mei.toUtf8());
        meiFile.close();
    }
    
    qDebug() << "Generated MEI with" << m_totalMeasures << "measures";
    
    return mei;
}

QString VerovioWidget::midiPitchToMEI(int midiPitch)
{
    // Convert MIDI pitch to MEI note element
    static const char* noteNames[] = {"c", "c", "d", "d", "e", "f", "f", "g", "g", "a", "a", "b"};
    static const char* accidentals[] = {"", "s", "", "s", "", "", "s", "", "s", "", "s", ""};  // s = sharp
    
    int octave = (midiPitch / 12) - 1;
    int noteIdx = midiPitch % 12;
    
    QString note = QString("<note pname=\"%1\" oct=\"%2\" dur=\"4\"")
                   .arg(noteNames[noteIdx]).arg(octave);
    
    if (QString(accidentals[noteIdx]) == "s") {
        note += " accid=\"s\"";
    }
    
    note += "/>";
    return note;
}

bool VerovioWidget::renderNotation()
{
    if (!m_toolkit) {
        m_errorMessage = tr("Verovio not initialized");
        showError(m_errorMessage);
        return false;
    }
    
    // Read MEI file
    QString meiPath = m_tempDir->path() + "/notation.mei";
    QFile meiFile(meiPath);
    if (!meiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_errorMessage = tr("Failed to read MEI file");
        showError(m_errorMessage);
        return false;
    }
    QString meiContent = QString::fromUtf8(meiFile.readAll());
    meiFile.close();
    
    try {
        // Configure Verovio options
        std::string options = R"({
            "scale": 40,
            "pageWidth": 2100,
            "pageHeight": 2970,
            "adjustPageHeight": true,
            "breaks": "auto",
            "mmOutput": false,
            "footer": "none",
            "header": "none"
        })";
        m_toolkit->SetOptions(options);
        
        // Load MEI
        if (!m_toolkit->LoadData(meiContent.toStdString())) {
            m_errorMessage = tr("Verovio failed to load MEI data");
            showError(m_errorMessage);
            return false;
        }
        
        // Get page count
        int pageCount = m_toolkit->GetPageCount();
        qDebug() << "Verovio rendered" << pageCount << "pages";
        
        // Clear old data
        m_pageSvgs.clear();
        m_pagePixmaps.clear();
        
        // Render each page to SVG
        for (int page = 1; page <= pageCount; ++page) {
            std::string svg = m_toolkit->RenderToSVG(page);
            m_pageSvgs.append(QString::fromStdString(svg));
            
            // Convert SVG to QPixmap
            QSvgRenderer renderer(QByteArray::fromStdString(svg));
            QSize size = renderer.defaultSize();
            if (size.isEmpty()) {
                size = QSize(800, 1200);
            }
            
            QPixmap pixmap(size);
            pixmap.fill(Qt::white);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            painter.end();
            
            m_pagePixmaps.append(pixmap);
        }
        
        // Get timemap for synchronization
        std::string timemapJson = m_toolkit->RenderToTimemap();
        parseTimemap(QString::fromStdString(timemapJson));
        
        // Build measure map
        buildMeasureMap();
        
        return true;
        
    } catch (const std::exception &e) {
        m_errorMessage = tr("Verovio error: %1").arg(e.what());
        showError(m_errorMessage);
        return false;
    }
}

void VerovioWidget::parseTimemap(const QString &timemapJson)
{
    // Parse Verovio timemap JSON
    // Format: [{"qstamp": 0.0, "tstamp": 0.0, "on": ["note-001", ...], "off": [...], "tempo": 120}, ...]
    
    m_timemap.clear();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(timemapJson.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse timemap:" << parseError.errorString();
        return;
    }
    
    if (!doc.isArray()) {
        qWarning() << "Timemap is not an array";
        return;
    }
    
    QJsonArray timemapArray = doc.array();
    
    // PPQ from MIDI (typically 480)
    int ppq = 480;
    
    for (const QJsonValue &entry : timemapArray) {
        QJsonObject obj = entry.toObject();
        
        double qstamp = obj["qstamp"].toDouble();  // Quarter note timestamp
        QJsonArray notesOn = obj["on"].toArray();
        
        // Convert qstamp to MIDI ticks (qstamp 1.0 = 1 quarter note = ppq ticks)
        int tickTime = static_cast<int>(qstamp * ppq);
        
        for (const QJsonValue &noteId : notesOn) {
            NoteTimingInfo info;
            info.elementId = noteId.toString();
            info.onTime = tickTime;
            info.offTime = tickTime + ppq;  // Default duration
            info.measureIndex = tickTime / m_ticksPerMeasure;
            m_timemap.append(info);
        }
    }
    
    qDebug() << "Parsed timemap with" << m_timemap.size() << "note events";
}

void VerovioWidget::buildMeasureMap()
{
    m_measurePositions.clear();
    
    if (m_pagePixmaps.isEmpty()) return;
    
    // With Verovio, we have precise information
    // Each measure has a known ID (m1, m2, etc.)
    // We estimate page/Y position based on measure distribution
    
    int measuresPerPage = qMax(1, m_totalMeasures / m_pagePixmaps.size());
    int systemsPerPage = 6;  // Approximate
    int measuresPerSystem = qMax(1, measuresPerPage / systemsPerPage);
    
    double systemHeight = 0.12;
    double topMargin = 0.08;
    double systemSpacing = 0.14;
    
    for (int measure = 0; measure < m_totalMeasures; ++measure) {
        int pageIdx = measure / measuresPerPage;
        if (pageIdx >= m_pagePixmaps.size()) pageIdx = m_pagePixmaps.size() - 1;
        
        int measureOnPage = measure % measuresPerPage;
        int systemOnPage = measureOnPage / measuresPerSystem;
        
        MeasurePosition pos;
        pos.pageIndex = pageIdx;
        pos.yPosition = topMargin + systemOnPage * systemSpacing;
        pos.height = systemHeight;
        pos.startTick = measure * m_ticksPerMeasure;
        pos.endTick = (measure + 1) * m_ticksPerMeasure;
        pos.measureId = QString("m%1").arg(measure + 1);
        
        m_measurePositions.append(pos);
    }
    
    qDebug() << "Built measure map with" << m_measurePositions.size() << "positions";
}

void VerovioWidget::showError(const QString &message)
{
    m_errorMessage = message;
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet("QLabel { color: #f88; background: #1e1e24; padding: 40px; font-size: 14px; }");
    m_statusLabel->show();
    m_scrollArea->hide();
}

void VerovioWidget::showPages()
{
    m_statusLabel->hide();
    m_scrollArea->show();
}

void VerovioWidget::clearPages()
{
    for (NotationPageWidget *widget : m_pageWidgets) {
        m_pagesLayout->removeWidget(widget);
        delete widget;
    }
    m_pageWidgets.clear();
    m_pagePixmaps.clear();
    m_pageSvgs.clear();
    m_measurePositions.clear();
}

void VerovioWidget::updateDisplay()
{
    clearPages();
    
    for (const QPixmap &pixmap : m_pagePixmaps) {
        NotationPageWidget *pageWidget = new NotationPageWidget();
        
        QPixmap scaled = pixmap.scaled(
            pixmap.size() * m_zoom,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        
        pageWidget->setPixmap(scaled);
        pageWidget->setStyleSheet(R"(
            NotationPageWidget {
                background: white;
                border: 1px solid #444;
            }
        )");
        
        m_pagesLayout->addWidget(pageWidget, 0, Qt::AlignHCenter);
        m_pageWidgets << pageWidget;
    }
    
    updateHighlight();
}

void VerovioWidget::zoomIn()
{
    setZoom(m_zoom + 0.1);
}

void VerovioWidget::zoomOut()
{
    setZoom(m_zoom - 0.1);
}

void VerovioWidget::setZoom(double zoom)
{
    double newZoom = qBound(0.2, zoom, 2.0);
    if (qAbs(newZoom - m_zoom) < 0.01) return;
    
    m_zoom = newZoom;
    m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_zoom * 100)));
    
    updateDisplay();
    emit zoomChanged(m_zoom);
}

void VerovioWidget::print()
{
    if (m_pagePixmaps.isEmpty()) return;
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Notation"));
    
    if (dialog.exec() != QDialog::Accepted) return;
    
    QPainter painter(&printer);
    
    for (int i = 0; i < m_pagePixmaps.size(); ++i) {
        if (i > 0) printer.newPage();
        
        const QPixmap &page = m_pagePixmaps[i];
        QRect pageRect = printer.pageRect(QPrinter::DevicePixel).toRect();
        QSize scaledSize = page.size().scaled(pageRect.size(), Qt::KeepAspectRatio);
        
        int x = (pageRect.width() - scaledSize.width()) / 2;
        int y = (pageRect.height() - scaledSize.height()) / 2;
        
        painter.drawPixmap(x, y, scaledSize.width(), scaledSize.height(), page);
    }
    
    painter.end();
}

void VerovioWidget::setPlaybackPosition(int tick)
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

void VerovioWidget::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
}

void VerovioWidget::updateHighlight()
{
    for (NotationPageWidget *widget : m_pageWidgets) {
        widget->clearHighlight();
    }
    
    if (m_currentMeasureIndex >= 0 && m_currentMeasureIndex < m_measurePositions.size()) {
        const MeasurePosition &pos = m_measurePositions[m_currentMeasureIndex];
        
        if (pos.pageIndex >= 0 && pos.pageIndex < m_pageWidgets.size()) {
            NotationPageWidget *widget = m_pageWidgets[pos.pageIndex];
            widget->setHighlightRegion(pos.yPosition, pos.yPosition + pos.height);
        }
    }
}

void VerovioWidget::scrollToCurrentPosition()
{
    if (m_currentMeasureIndex < 0 || m_currentMeasureIndex >= m_measurePositions.size()) return;
    
    const MeasurePosition &pos = m_measurePositions[m_currentMeasureIndex];
    if (pos.pageIndex < 0 || pos.pageIndex >= m_pageWidgets.size()) return;
    
    NotationPageWidget *widget = m_pageWidgets[pos.pageIndex];
    
    int widgetY = widget->mapTo(m_pagesContainer, QPoint(0, 0)).y();
    int highlightY = widgetY + static_cast<int>(pos.yPosition * widget->height());
    
    QScrollBar *vBar = m_scrollArea->verticalScrollBar();
    int viewportHeight = m_scrollArea->viewport()->height();
    int targetScroll = highlightY - (viewportHeight / 3);
    
    vBar->setValue(qBound(0, targetScroll, vBar->maximum()));
}
