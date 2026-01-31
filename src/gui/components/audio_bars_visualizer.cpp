#include "audio_bars_visualizer.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRandomGenerator>
#include <cmath>

AudioBarsVisualizer::AudioBarsVisualizer(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent),
      m_engine(engine),
      m_barCount(16),
      m_isPlaying(false),
      m_smoothingFactor(0.15f),
      m_decayRate(0.03f),
      m_randomFactor(0.4f),
      m_leftLevel(0.0f),
      m_rightLevel(0.0f),
      m_barColor(QColor(60, 140, 200)),
      m_barColorBright(QColor(100, 200, 255)),
      m_backgroundColor(QColor(15, 15, 22))
{
    m_currentLevels.resize(m_barCount, 0.0f);
    m_targetLevels.resize(m_barCount, 0.0f);
    m_velocities.resize(m_barCount, 0.0f);
    
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &AudioBarsVisualizer::updateAnimation);
    
    setMinimumSize(100, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void AudioBarsVisualizer::setEngine(NoteNagaEngine *engine)
{
    m_engine = engine;
}

void AudioBarsVisualizer::setVolumesDb(float leftDb, float rightDb)
{
    // Convert dB to visual level (0.0-1.0 range)
    // Map -60 dB to 0.0 and 0 dB to 1.0 (standard metering range)
    auto dbToVisual = [](float db) -> float {
        const float minDb = -60.0f;  // Minimum visible level
        const float maxDb = 0.0f;    // Full scale
        
        if (db <= minDb) return 0.0f;
        if (db >= maxDb) return 1.0f;
        
        // Linear mapping from dB range to 0-1
        return (db - minDb) / (maxDb - minDb);
    };
    
    m_leftLevel = dbToVisual(leftDb);
    m_rightLevel = dbToVisual(rightDb);
}

AudioBarsVisualizer::~AudioBarsVisualizer()
{
    stop();
}

void AudioBarsVisualizer::start()
{
    m_isPlaying = true;
    m_animationTimer->start(33);  // ~30 fps
}

void AudioBarsVisualizer::stop()
{
    m_isPlaying = false;
    m_animationTimer->stop();
    
    // Decay all bars
    for (int i = 0; i < m_barCount; ++i) {
        m_targetLevels[i] = 0.0f;
    }
    update();
}

void AudioBarsVisualizer::setBarCount(int count)
{
    m_barCount = count;
    m_currentLevels.resize(m_barCount, 0.0f);
    m_targetLevels.resize(m_barCount, 0.0f);
    m_velocities.resize(m_barCount, 0.0f);
    update();
}

void AudioBarsVisualizer::setLevels(const std::vector<float> &levels)
{
    for (int i = 0; i < std::min(m_barCount, (int)levels.size()); ++i) {
        m_targetLevels[i] = std::max(m_targetLevels[i], levels[i]);
    }
}

void AudioBarsVisualizer::updateAnimation()
{
    bool needsUpdate = false;
    
    // Fetch current volume from engine if available
    if (m_isPlaying && m_engine && m_engine->getDSPEngine()) {
        auto dbs = m_engine->getDSPEngine()->getCurrentVolumeDb();
        setVolumesDb(dbs.first, dbs.second);
    }
    
    // Use left and right levels directly - they should be 0-1 range from setVolumesDb
    float avgLevel = (m_leftLevel + m_rightLevel) / 2.0f;
    
    for (int i = 0; i < m_barCount; ++i) {
        // When playing, modulate bars based on actual audio level
        if (m_isPlaying) {
            // Create a wave-like pattern across bars based on position
            float centerWeight = 1.0f - std::abs((i - m_barCount/2.0f) / (m_barCount/2.0f)) * 0.3f;
            
            // Use left level for left bars, right for right bars
            float stereoLevel = (i < m_barCount / 2) ? m_leftLevel : m_rightLevel;
            
            // Direct use of audio level with some center enhancement
            float baseLevel = stereoLevel * centerWeight;
            
            // Add random variation for visual interest (more pronounced pulsing)
            float randomVariation = 0.0f;
            if (avgLevel > 0.01f) {
                // Random variation between -30% and +30% of base level
                float randFactor = (QRandomGenerator::global()->bounded(100) - 50) / 100.0f;
                randomVariation = randFactor * 0.35f * baseLevel;
            }
            
            // Set target directly to current audio level (no decay when audio is present)
            m_targetLevels[i] = std::clamp(baseLevel + randomVariation, 0.0f, 1.0f);
        } else {
            // Only decay when not playing
            m_targetLevels[i] *= (1.0f - m_decayRate);
        }
        
        // Smooth towards target
        float diff = m_targetLevels[i] - m_currentLevels[i];
        m_currentLevels[i] += diff * m_smoothingFactor;
        
        if (std::abs(diff) > 0.001f) {
            needsUpdate = true;
        }
    }
    
    if (needsUpdate) {
        update();
    }
}

void AudioBarsVisualizer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    
    // Background
    p.fillRect(rect(), m_backgroundColor);
    
    const int w = width();
    const int h = height();
    const int margin = 10;
    const int barSpacing = 4;
    const int availableWidth = w - 2 * margin;
    const int barWidth = (availableWidth - (m_barCount - 1) * barSpacing) / m_barCount;
    const int maxBarHeight = h - 2 * margin;
    
    for (int i = 0; i < m_barCount; ++i) {
        int x = margin + i * (barWidth + barSpacing);
        float level = m_currentLevels[i];
        int barHeight = int(level * maxBarHeight);
        
        if (barHeight < 2) barHeight = 2;  // Minimum visible height
        
        int y = h - margin - barHeight;
        
        // Create gradient for bar
        QLinearGradient gradient(x, y, x, h - margin);
        
        // Brighter at top, darker at bottom
        QColor topColor = m_barColorBright;
        QColor bottomColor = m_barColor.darker(120);
        
        // Adjust brightness based on level
        topColor.setAlpha(int(150 + level * 105));
        bottomColor.setAlpha(int(100 + level * 100));
        
        gradient.setColorAt(0.0, topColor);
        gradient.setColorAt(1.0, bottomColor);
        
        // Draw bar with rounded top
        QPainterPath barPath;
        int cornerRadius = std::min(barWidth / 3, 4);
        barPath.addRoundedRect(x, y, barWidth, barHeight, cornerRadius, cornerRadius);
        
        p.fillPath(barPath, gradient);
        
        // Add subtle glow on top
        if (level > 0.5f) {
            QColor glowColor = m_barColorBright;
            glowColor.setAlpha(int((level - 0.5f) * 100));
            p.setPen(QPen(glowColor, 2));
            p.drawLine(x + 2, y + 2, x + barWidth - 2, y + 2);
        }
    }
    
    // Draw "Audio Only" text at top
    p.setPen(QColor(150, 150, 150));
    QFont font = p.font();
    font.setPointSize(14);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRect(0, 10, w, 30), Qt::AlignCenter, "Audio Only");
}

void AudioBarsVisualizer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}
