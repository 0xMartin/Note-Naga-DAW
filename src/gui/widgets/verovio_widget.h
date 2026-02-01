#ifndef VEROVIO_WIDGET_H
#define VEROVIO_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QTemporaryDir>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <QMap>
#include <QSvgRenderer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <note_naga_engine/note_naga_engine.h>

// Forward declarations
namespace vrv {
    class Toolkit;
}
class NotationPageWidget;

/**
 * @brief Verovio-based music notation widget with precise playback synchronization
 * 
 * This widget uses the Verovio C++ library to render music notation.
 * Key features:
 * - Generates MEI (Music Encoding Initiative) XML from MIDI data
 * - Renders to SVG using Verovio toolkit
 * - Uses Verovio timemap for precise note-level synchronization
 * 
 * The synchronization is exact because Verovio provides:
 * - Element IDs for each note/rest in the rendered SVG
 * - Timemap mapping MIDI ticks to element IDs
 */
class VerovioWidget : public QWidget
{
    Q_OBJECT

public:
    // Notation settings structure
    struct NotationSettings {
        QString keySignature = "0";           // Key signature (-7 to +7, 0 = C major)
        QString timeSignature = "4/4";        // Time signature
        int scale = 40;                       // Scale percentage (30-100)
        bool showTitle = true;                // Show title at top
        bool showTempo = true;                // Show tempo marking
        bool showInstrumentNames = true;      // Show instrument names on staves
        QString composer = "";                // Composer name (optional)
        int pageWidth = 2100;                 // Page width in tenths of mm
        int pageHeight = 2970;                // Page height in tenths of mm
        bool landscape = false;               // Landscape orientation
    };
    
    // Measure position info for playback highlighting (in pixels, absolute)
    struct MeasurePosition {
        int pageIndex;          // Which page (0-based)
        int xStart;             // X start position in pixels
        int xEnd;               // X end position in pixels  
        int yStart;             // Y start position in pixels
        int yEnd;               // Y end position in pixels
        int startTick;          // Start tick of this measure
        int endTick;            // End tick of this measure
        QString measureId;      // MEI element ID for this measure
    };
    
    // Note timing from Verovio timemap
    struct NoteTimingInfo {
        QString elementId;      // SVG element ID
        int onTime;             // MIDI tick when note starts
        int offTime;            // MIDI tick when note ends
        int measureIndex;       // Which measure (0-based)
    };
    
    explicit VerovioWidget(NoteNagaEngine *engine, QWidget *parent = nullptr);
    ~VerovioWidget();

    void setSequence(NoteNagaMidiSeq *sequence);
    void setTitle(const QString &title);
    void setTrackVisibility(const QList<bool> &visibility);
    void setNotationSettings(const NotationSettings &settings);
    NotationSettings getNotationSettings() const { return m_settings; }
    
    /**
     * @brief Creates a title button widget for use in dock title bar.
     */
    QWidget* createTitleButtonWidget(QWidget *parent = nullptr);
    
    double getZoom() const { return m_zoom; }
    QString getErrorMessage() const { return m_errorMessage; }
    bool isAvailable() const { return m_verovioAvailable; }
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

private:
    void setupUi();
    void initVerovio();
    QString generateMEI();
    bool renderNotation();
    void parseTimemap(const QString &timemapJson);
    void buildMeasureMap();
    void showError(const QString &message);
    void showPages();
    void updateDisplay();
    void clearPages();
    void updateHighlight();
    void scrollToCurrentPosition();
    QString fixNestedSvgElements(const QString &svg);
    void parseSvgMeasurePositions();  // Extract measure bounding boxes from SVG
    
    // MEI generation helpers
    QString midiPitchToMEI(int midiPitch, int durationTicks, int ppq);
    QString ticksToDuration(int ticks, int ppq);

    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    
    // Verovio toolkit
    vrv::Toolkit *m_toolkit;
    QString m_verovioResourcePath;
    
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
    
    QTemporaryDir *m_tempDir;
    
    // Data
    QString m_errorMessage;
    QString m_title;
    QList<bool> m_trackVisibility;
    QList<QPixmap> m_pagePixmaps;
    QList<NotationPageWidget*> m_pageWidgets;
    NotationSettings m_settings;
    
    // Verovio output
    QStringList m_pageSvgs;           // SVG content for each page
    QList<NoteTimingInfo> m_timemap;  // Note timing from Verovio
    
    // Playback highlighting
    QList<MeasurePosition> m_measurePositions;
    int m_currentTick;
    int m_currentMeasureIndex;
    bool m_autoScroll;
    int m_ticksPerMeasure;
    int m_totalMeasures;
    
    double m_zoom;
    bool m_verovioAvailable;
    bool m_rendering;
    bool m_needsRender;
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
    void setHighlightRect(const QRect &rect, const QSize &originalSize);  // Precise pixel rectangle
    void clearHighlight();
    
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
    bool m_hasHighlight;
    QRect m_highlightRect;
    QSize m_originalSize;
};

#endif // VEROVIO_WIDGET_H
