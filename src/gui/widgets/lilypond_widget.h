#ifndef LILYPOND_WIDGET_H
#define LILYPOND_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QProcess>
#include <QTemporaryDir>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <QMap>
#include <QSvgRenderer>

#include <note_naga_engine/note_naga_engine.h>

// Forward declaration for page widget with highlight overlay
class NotationPageWidget;

/**
 * @brief LilyPond-based music notation widget
 * 
 * This widget uses the LilyPond CLI tool to render music notation.
 * LilyPond must be installed on the system:
 * - macOS: brew install lilypond
 * - Linux: apt install lilypond
 * - Windows: Download from https://lilypond.org
 * 
 * The rendering process:
 * 1. Convert MIDI sequence to LilyPond format (.ly file)
 * 2. Call lilypond CLI to generate SVG (with point-and-click metadata)
 * 3. Parse SVG to extract measure positions
 * 4. Display pages with playback position highlighting
 * 
 * Rendering is triggered manually via refresh button.
 */
class LilyPondWidget : public QWidget
{
    Q_OBJECT

public:
    // Notation settings structure
    struct NotationSettings {
        QString keySignature = "c \\major";   // LilyPond key (c, g, d, a, e, b, fis, cis for major; a, e, b, fis, cis, gis, dis for minor)
        QString timeSignature = "4/4";        // Time signature
        QString staffType = "piano";          // "piano" (grand staff), "treble", "bass", "single"
        int fontSize = 18;                    // Staff size (default 18-20)
        bool showBarNumbers = true;           // Show bar numbers
        bool showTempo = true;                // Show tempo marking
        int resolution = 200;                 // PNG resolution in DPI
    };
    
    // Measure position info for playback highlighting
    struct MeasurePosition {
        int pageIndex;          // Which page (0-based)
        double yPosition;       // Y position on the page (normalized 0-1)
        double height;          // Height of the measure area
        int startTick;          // Start tick of this measure
        int endTick;            // End tick of this measure
    };
    
    explicit LilyPondWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~LilyPondWidget();

    void setSequence(NoteNagaMidiSeq *sequence);
    void setTitle(const QString &title);
    void setTrackVisibility(const QList<bool> &visibility);
    void setNotationSettings(const NotationSettings &settings);
    NotationSettings getNotationSettings() const { return m_settings; }
    
    /**
     * @brief Creates a title button widget for use in dock title bar.
     * Contains Refresh and Print buttons.
     * @param parent Parent widget for the button container.
     * @return QWidget containing the title buttons.
     */
    QWidget* createTitleButtonWidget(QWidget *parent = nullptr);
    
    double getZoom() const { return m_zoom; }
    QString getErrorMessage() const { return m_errorMessage; }
    bool isAvailable() const { return m_lilypondAvailable; }
    bool isRendering() const { return m_rendering; }

public slots:
    void render();      // Manual render trigger
    void print();       // Print the notation
    void zoomIn();
    void zoomOut();
    void setZoom(double zoom);
    
    /**
     * @brief Update playback position highlighting
     * @param tick Current playback position in ticks
     */
    void setPlaybackPosition(int tick);
    
    /**
     * @brief Enable/disable auto-scroll during playback
     */
    void setAutoScroll(bool enabled);

signals:
    void renderingStarted();
    void renderingComplete();
    void renderingError(const QString &error);
    void zoomChanged(double zoom);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void setupUi();
    void checkLilypondAvailable();
    void startRendering(const QString &lilypondSource);
    QString getLilypondPath() const;
    QString getMidi2lyPath() const;
    void showError(const QString &message);
    void showPages();
    void updateDisplay();
    void clearPages();
    void buildMeasureMap();
    QList<QPair<double, double>> detectSystemsInPage(const QPixmap &pixmap);
    void updateHighlight();
    void scrollToCurrentPosition();

    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    
    // UI elements
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_toolbarLayout;
    QToolButton *m_zoomInBtn;
    QToolButton *m_zoomOutBtn;
    QLabel *m_zoomLabel;
    QLabel *m_statusLabel;
    
    QScrollArea *m_scrollArea;
    QWidget *m_pagesContainer;
    QVBoxLayout *m_pagesLayout;
    
    QProcess *m_process;
    QTemporaryDir *m_tempDir;
    
    // Data
    QString m_errorMessage;
    QString m_title;
    QList<bool> m_trackVisibility;
    QList<QPixmap> m_pagePixmaps;  // Original page images
    QList<NotationPageWidget*> m_pageWidgets;  // Page widgets with highlight overlay
    NotationSettings m_settings;   // Notation settings
    
    // Playback highlighting
    QList<MeasurePosition> m_measurePositions;  // Measure position map
    int m_currentTick;                          // Current playback tick
    int m_currentMeasureIndex;                  // Current highlighted measure
    bool m_autoScroll;                          // Auto-scroll during playback
    int m_ticksPerMeasure;                      // Ticks per measure based on time signature
    int m_totalMeasures;                        // Total number of measures
    
    double m_zoom;
    bool m_lilypondAvailable;
    bool m_rendering;
    bool m_needsRender;  // Flag indicating data changed
};


/**
 * @brief Custom widget for displaying a notation page with highlight overlay
 */
class NotationPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NotationPageWidget(QWidget *parent = nullptr);
    
    void setPixmap(const QPixmap &pixmap);
    void setHighlightRegion(double yStart, double yEnd);  // Normalized 0-1
    void clearHighlight();
    
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
    bool m_hasHighlight;
    double m_highlightYStart;
    double m_highlightYEnd;
};

#endif // LILYPOND_WIDGET_H
