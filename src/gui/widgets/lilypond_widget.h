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

#include <note_naga_engine/note_naga_engine.h>

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
 * 2. Call lilypond CLI to generate PNG (multiple pages)
 * 3. Display pages as white paper on dark background
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
    QList<QLabel*> m_pageLabels;   // Page display labels
    NotationSettings m_settings;   // Notation settings
    
    double m_zoom;
    bool m_lilypondAvailable;
    bool m_rendering;
    bool m_needsRender;  // Flag indicating data changed
};

#endif // LILYPOND_WIDGET_H
