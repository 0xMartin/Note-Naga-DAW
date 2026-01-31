#include "audio_bars_visualizer.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <cmath>

AudioBarsVisualizer::AudioBarsVisualizer(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent),
      m_engine(engine),
      m_barCount(32),
      m_isPlaying(false),
      m_smoothingFactor(0.25f),
      m_decayRate(0.015f),
      m_randomFactor(0.4f),
      m_leftLevel(0.0f),
      m_rightLevel(0.0f),
      m_time(0.0f),
      m_pulsePhase(0.0f),
      m_barColor(QColor(60, 140, 200)),
      m_barColorBright(QColor(100, 200, 255)),
      m_backgroundColor(QColor(12, 12, 18))
{
    m_currentLevels.resize(m_barCount, 0.0f);
    m_targetLevels.resize(m_barCount, 0.0f);
    m_velocities.resize(m_barCount, 0.0f);
    m_peakLevels.resize(m_barCount, 0.0f);
    m_peakDecay.resize(m_barCount, 0.0f);
    m_hueOffsets.resize(m_barCount);
    
    // Initialize random hue offsets for each bar
    for (int i = 0; i < m_barCount; ++i) {
        m_hueOffsets[i] = QRandomGenerator::global()->bounded(30) - 15;
    }
    
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
    auto dbToVisual = [](float db) -> float {
        const float minDb = -50.0f;
        const float maxDb = 0.0f;
        
        if (db <= minDb) return 0.0f;
        if (db >= maxDb) return 1.0f;
        
        // Slightly curved response for more dynamic feel
        float linear = (db - minDb) / (maxDb - minDb);
        return std::pow(linear, 0.85f);
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
    m_time = 0.0f;
    m_animationTimer->start(16);  // ~60 fps for smooth animation
}

void AudioBarsVisualizer::stop()
{
    m_isPlaying = false;
    m_animationTimer->stop();
    
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
    m_peakLevels.resize(m_barCount, 0.0f);
    m_peakDecay.resize(m_barCount, 0.0f);
    m_hueOffsets.resize(m_barCount);
    for (int i = 0; i < m_barCount; ++i) {
        m_hueOffsets[i] = QRandomGenerator::global()->bounded(30) - 15;
    }
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
    // Increment time for wave effects
    m_time += 0.016f;
    m_pulsePhase += 0.08f;
    
    // Fetch current volume from engine if available
    if (m_isPlaying && m_engine && m_engine->getDSPEngine()) {
        auto dbs = m_engine->getDSPEngine()->getCurrentVolumeDb();
        setVolumesDb(dbs.first, dbs.second);
    }
    
    float avgLevel = (m_leftLevel + m_rightLevel) / 2.0f;
    
    for (int i = 0; i < m_barCount; ++i) {
        float normalizedPos = (float)i / (m_barCount - 1);  // 0 to 1
        float centerDist = std::abs(normalizedPos - 0.5f) * 2.0f;  // 0 at center, 1 at edges
        
        if (m_isPlaying) {
            // Stereo: left bars use left level, right bars use right level
            float stereoBlend = normalizedPos;  // 0=left, 1=right
            float stereoLevel = m_leftLevel * (1.0f - stereoBlend) + m_rightLevel * stereoBlend;
            
            // Wave effect - bars oscillate slightly based on position and time
            float waveOffset = std::sin(m_time * 3.0f + i * 0.3f) * 0.08f;
            
            // Center bars slightly taller
            float centerBoost = 1.0f + (1.0f - centerDist) * 0.15f;
            
            // Pulse effect synced to audio
            float pulse = 1.0f + std::sin(m_pulsePhase) * 0.05f * avgLevel;
            
            // Calculate base level with all modulations
            float baseLevel = stereoLevel * centerBoost * pulse;
            
            // Add subtle random variation (different per bar, changes over time)
            float randomPhase = m_time * 2.0f + i * 1.7f;
            float randomVariation = (std::sin(randomPhase) * 0.5f + 0.5f) * 0.15f;
            if (avgLevel > 0.05f) {
                baseLevel += randomVariation * baseLevel;
            }
            
            // Add wave offset
            baseLevel += waveOffset * avgLevel;
            
            m_targetLevels[i] = std::clamp(baseLevel, 0.0f, 1.0f);
        } else {
            m_targetLevels[i] *= (1.0f - m_decayRate * 2.0f);
        }
        
        // Smooth interpolation with spring-like physics
        float diff = m_targetLevels[i] - m_currentLevels[i];
        m_velocities[i] += diff * 0.3f;
        m_velocities[i] *= 0.7f;  // Damping
        m_currentLevels[i] += m_velocities[i];
        m_currentLevels[i] = std::clamp(m_currentLevels[i], 0.0f, 1.0f);
        
        // Peak hold with gravity
        if (m_currentLevels[i] > m_peakLevels[i]) {
            m_peakLevels[i] = m_currentLevels[i];
            m_peakDecay[i] = 0.0f;
        } else {
            m_peakDecay[i] += 0.001f;  // Gravity acceleration
            m_peakLevels[i] -= m_peakDecay[i];
            if (m_peakLevels[i] < m_currentLevels[i]) {
                m_peakLevels[i] = m_currentLevels[i];
            }
        }
    }
    
    update();
}

void AudioBarsVisualizer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    
    const int w = width();
    const int h = height();
    
    // Gradient background
    QLinearGradient bgGrad(0, 0, 0, h);
    bgGrad.setColorAt(0.0, QColor(18, 18, 28));
    bgGrad.setColorAt(0.5, QColor(12, 12, 20));
    bgGrad.setColorAt(1.0, QColor(8, 8, 14));
    p.fillRect(rect(), bgGrad);
    
    // Draw subtle grid lines
    p.setPen(QPen(QColor(40, 40, 50), 1));
    for (int y = h / 4; y < h; y += h / 4) {
        p.drawLine(0, y, w, y);
    }
    
    const int margin = 15;
    const int barSpacing = 3;
    const int availableWidth = w - 2 * margin;
    const int barWidth = std::max(4, (availableWidth - (m_barCount - 1) * barSpacing) / m_barCount);
    const int maxBarHeight = h - 2 * margin - 30;  // Leave space for title
    
    float avgLevel = (m_leftLevel + m_rightLevel) / 2.0f;
    
    for (int i = 0; i < m_barCount; ++i) {
        float normalizedPos = (float)i / (m_barCount - 1);
        int x = margin + i * (barWidth + barSpacing);
        float level = m_currentLevels[i];
        int barHeight = std::max(3, int(level * maxBarHeight));
        int y = h - margin - barHeight;
        
        // Calculate color based on position and level
        // Hue shifts from cyan (180) through blue (220) to purple (280) based on position
        // Higher levels shift towards warmer colors
        float baseHue = 180.0f + normalizedPos * 60.0f + m_hueOffsets[i];
        float levelHueShift = level * 40.0f;  // Shift towards pink/red when loud
        int hue = int(baseHue + levelHueShift) % 360;
        
        // Saturation and value based on level
        int sat = 180 + int(level * 75);
        int val = 150 + int(level * 105);
        
        QColor barColorBottom = QColor::fromHsv(hue, sat, val);
        QColor barColorTop = QColor::fromHsv((hue + 20) % 360, sat - 30, std::min(255, val + 50));
        
        // Draw glow behind bar when level is high
        if (level > 0.3f) {
            float glowIntensity = (level - 0.3f) / 0.7f;
            QColor glowColor = barColorTop;
            glowColor.setAlpha(int(glowIntensity * 80));
            
            // Soft glow
            for (int g = 3; g >= 1; --g) {
                QColor gc = glowColor;
                gc.setAlpha(int(glowIntensity * 25 * g));
                p.setPen(Qt::NoPen);
                p.setBrush(gc);
                p.drawRoundedRect(x - g * 2, y - g * 2, barWidth + g * 4, barHeight + g * 4, 4, 4);
            }
        }
        
        // Main bar gradient
        QLinearGradient barGrad(x, y, x, h - margin);
        barGrad.setColorAt(0.0, barColorTop);
        barGrad.setColorAt(0.3, barColorBottom);
        barGrad.setColorAt(1.0, barColorBottom.darker(150));
        
        // Draw bar
        QPainterPath barPath;
        int cornerRadius = std::min(barWidth / 2, 5);
        barPath.addRoundedRect(x, y, barWidth, barHeight, cornerRadius, cornerRadius);
        p.fillPath(barPath, barGrad);
        
        // Draw glossy highlight on top portion
        if (barHeight > 10) {
            QLinearGradient glossGrad(x, y, x, y + barHeight * 0.3f);
            glossGrad.setColorAt(0.0, QColor(255, 255, 255, int(60 + level * 40)));
            glossGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
            
            QPainterPath glossPath;
            glossPath.addRoundedRect(x + 1, y + 1, barWidth - 2, barHeight * 0.25f, cornerRadius - 1, cornerRadius - 1);
            p.fillPath(glossPath, glossGrad);
        }
        
        // Draw peak indicator (floating dot)
        if (m_peakLevels[i] > 0.02f) {
            int peakY = h - margin - int(m_peakLevels[i] * maxBarHeight);
            QColor peakColor = QColor::fromHsv((hue + 30) % 360, 200, 255);
            
            // Glow behind peak
            QRadialGradient peakGlow(x + barWidth / 2, peakY, barWidth);
            peakGlow.setColorAt(0.0, QColor(peakColor.red(), peakColor.green(), peakColor.blue(), 150));
            peakGlow.setColorAt(1.0, QColor(peakColor.red(), peakColor.green(), peakColor.blue(), 0));
            p.setBrush(peakGlow);
            p.setPen(Qt::NoPen);
            p.drawEllipse(x - 2, peakY - 4, barWidth + 4, 8);
            
            // Peak line
            p.setPen(QPen(peakColor, 2));
            p.drawLine(x, peakY, x + barWidth, peakY);
        }
    }
    
    // Draw center reflection/glow based on audio level
    if (avgLevel > 0.1f) {
        QRadialGradient centerGlow(w / 2, h, w * 0.6f);
        int glowAlpha = int(avgLevel * 40);
        centerGlow.setColorAt(0.0, QColor(100, 180, 255, glowAlpha));
        centerGlow.setColorAt(0.5, QColor(80, 120, 200, glowAlpha / 2));
        centerGlow.setColorAt(1.0, QColor(60, 80, 150, 0));
        p.fillRect(rect(), centerGlow);
    }
    
    // Draw "Audio Export" text with glow
    QFont font = p.font();
    font.setPointSize(16);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    p.setFont(font);
    
    QString title = "AUDIO EXPORT";
    QRect textRect(0, 8, w, 30);
    
    // Text glow
    if (avgLevel > 0.1f) {
        p.setPen(QColor(100, 200, 255, int(avgLevel * 100)));
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                p.drawText(textRect.translated(dx, dy), Qt::AlignCenter, title);
            }
        }
    }
    
    // Main text
    QLinearGradient textGrad(0, 8, 0, 38);
    textGrad.setColorAt(0.0, QColor(200, 220, 255));
    textGrad.setColorAt(1.0, QColor(120, 150, 200));
    
    QPen textPen;
    textPen.setBrush(textGrad);
    p.setPen(textPen);
    p.drawText(textRect, Qt::AlignCenter, title);
}

void AudioBarsVisualizer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}
