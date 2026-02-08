#pragma once

#include <QObject>
#include <QSize>
#include <QTimer>
#include <QMutex>
#include "media_renderer.h"

class NoteNagaMidiSeq;
class NoteNagaArrangement;
class NoteNagaRuntimeData;

/**
 * @brief This worker runs in a separate thread and handles preview rendering.
 * It receives requests from the GUI thread and sends back finished QImages.
 */
class PreviewWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct worker for single MIDI sequence mode.
     */
    explicit PreviewWorker(NoteNagaMidiSeq* sequence);
    
    /**
     * @brief Construct worker for Arrangement mode.
     */
    explicit PreviewWorker(NoteNagaArrangement* arrangement, NoteNagaRuntimeData* runtimeData);
    
    ~PreviewWorker();

signals:
    /**
     * @brief Emitted when a new preview frame is ready.
     * @param frame The finished QImage frame.
     */
    void frameReady(const QImage& frame);

public slots:
    /**
     * @brief Initializes the worker (must be called AFTER moving to the thread).
     */
    void init();
    
    /**
     * @brief Updates the target time for rendering.
     */
    void updateTime(double time);
    
    /**
     * @brief Updates the rendering settings.
     */
    void updateSettings(const MediaRenderer::RenderSettings& settings);
    
    /**
     * @brief Updates the visible time range (scale).
     */
    void updateScale(double secondsVisible);
    
    /**
     * @brief Updates the preview size.
     */
    void updateSize(const QSize& size);

private slots:
    /**
     * @brief Performs the actual rendering. Called by a timer to
     * merge multiple requests (e.g., during fast slider scrubbing).
     */
    void doRender();

private:
    MediaRenderer* m_renderer;
    NoteNagaMidiSeq* m_sequence;
    NoteNagaArrangement* m_arrangement;
    NoteNagaRuntimeData* m_runtimeData;
    QTimer* m_renderTimer;

    // Mutex to protect shared data between slots and doRender()
    QMutex m_mutex; 
    
    // Last requested values
    double m_time;
    QSize m_size;
    MediaRenderer::RenderSettings m_settings;
    double m_scale;
    bool m_pendingRender;
};