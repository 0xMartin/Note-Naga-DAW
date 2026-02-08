#include "preview_worker.h"
#include <QDebug>
#include <note_naga_engine/core/types.h>

PreviewWorker::PreviewWorker(NoteNagaMidiSeq* sequence)
    : m_sequence(sequence),
      m_arrangement(nullptr),
      m_runtimeData(nullptr),
      m_renderer(nullptr),
      m_renderTimer(nullptr),
      m_time(0.0),
      m_size(320, 240),
      m_scale(5.0),
      m_pendingRender(false)
{
}

PreviewWorker::PreviewWorker(NoteNagaArrangement* arrangement, NoteNagaRuntimeData* runtimeData)
    : m_sequence(nullptr),
      m_arrangement(arrangement),
      m_runtimeData(runtimeData),
      m_renderer(nullptr),
      m_renderTimer(nullptr),
      m_time(0.0),
      m_size(320, 240),
      m_scale(5.0),
      m_pendingRender(false)
{
}

PreviewWorker::~PreviewWorker()
{
    // Renderer is deleted in `init` or if the thread never started
    delete m_renderer; 
}

void PreviewWorker::init()
{
    // This method is called via signal after the object is moved to its thread.
    
    qDebug() << "PreviewWorker::init - m_arrangement:" << m_arrangement << "m_runtimeData:" << m_runtimeData;
    
    // m_renderer is owned and used only by this thread
    if (m_arrangement && m_runtimeData) {
        qDebug() << "  Creating MediaRenderer for ARRANGEMENT";
        m_renderer = new MediaRenderer(m_arrangement, m_runtimeData);
    } else {
        qDebug() << "  Creating MediaRenderer for SEQUENCE";
        m_renderer = new MediaRenderer(m_sequence);
    } 
    
    // A timer that runs in the same thread as this worker
    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    // 10ms "debounce" timer to avoid triggering renders too frequently
    m_renderTimer->setInterval(10); 
    
    connect(m_renderTimer, &QTimer::timeout, this, &PreviewWorker::doRender);
    
    // Force an initial render
    m_pendingRender = true;
    doRender();
}

void PreviewWorker::updateTime(double time)
{
    QMutexLocker locker(&m_mutex);
    m_time = time;
    m_pendingRender = true;
    m_renderTimer->start(); // Restarts/starts the timer
}

void PreviewWorker::updateSettings(const MediaRenderer::RenderSettings& settings)
{
    QMutexLocker locker(&m_mutex);
    m_settings = settings;
    m_pendingRender = true;
    m_renderTimer->start();
}

void PreviewWorker::updateScale(double secondsVisible)
{
    QMutexLocker locker(&m_mutex);
    m_scale = secondsVisible;
    m_pendingRender = true;
    m_renderTimer->start();
}

void PreviewWorker::updateSize(const QSize& size)
{
    QMutexLocker locker(&m_mutex);
    if (size.isEmpty()) return;
    m_size = size;
    m_pendingRender = true;
    m_renderTimer->start();
}

void PreviewWorker::doRender()
{
    if (!m_pendingRender) return;
    m_pendingRender = false;
    
    // Local copy of data for rendering (under lock)
    double time;
    QSize size;
    MediaRenderer::RenderSettings settings;
    double scale;
    
    {
        QMutexLocker locker(&m_mutex);
        time = m_time;
        size = m_size;
        settings = m_settings;
        scale = m_scale;
    }
    
    // The renderer is called without a lock to avoid blocking the GUI thread
    // when it calls updateTime/updateSettings
    
    // If settings changed, apply them
    m_renderer->setRenderSettings(settings);
    m_renderer->setSecondsVisible(scale);

    // Actual rendering.
    // renderFrame for preview remains stateful (calculates its own delta)
    QImage frame = m_renderer->renderFrame(time, size);
    
    // Send the finished frame back to the GUI thread
    emit frameReady(frame);
}