#include "pan_analyzer.h"

#include <QColor>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QHBoxLayout>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <cmath>
#include <algorithm>

#include "../nn_gui_utils.h"

PanAnalyzer::PanAnalyzer(NoteNagaPanAnalyzer *panAnalyzer, QWidget *parent)
    : QWidget(parent), m_panAnalyzer(panAnalyzer)
{
    setMinimumSize(200, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_smoothedSegments.resize(PAN_NUM_SEGMENTS, 0.0f);
    m_currentData = {};

    connect(m_panAnalyzer, &NoteNagaPanAnalyzer::panDataUpdated, this,
            &PanAnalyzer::updatePanData);

    setupTitleWidget();
    setupContextMenu();
}

void PanAnalyzer::setupTitleWidget()
{
    m_titleWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(m_titleWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    
    m_btnEnabled = create_small_button(":/icons/active.svg", "Enable/Disable Pan Analyzer", "btnPanEnabled", 20);
    m_btnEnabled->setCheckable(true);
    m_btnEnabled->setChecked(!m_enabled);
    connect(m_btnEnabled, &QPushButton::clicked, this, &PanAnalyzer::toggleEnabled);
    
    layout->addWidget(m_btnEnabled);
}

void PanAnalyzer::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    // Refresh Rate submenu
    QMenu *refreshMenu = m_contextMenu->addMenu("Refresh Rate");
    m_refreshRateGroup = new QActionGroup(this);
    
    QAction *rate60 = refreshMenu->addAction("60 fps (Full)");
    rate60->setCheckable(true);
    rate60->setChecked(m_refreshDivisor == 1);
    rate60->setData(1);
    m_refreshRateGroup->addAction(rate60);
    
    QAction *rate30 = refreshMenu->addAction("30 fps");
    rate30->setCheckable(true);
    rate30->setChecked(m_refreshDivisor == 2);
    rate30->setData(2);
    m_refreshRateGroup->addAction(rate30);
    
    QAction *rate15 = refreshMenu->addAction("15 fps");
    rate15->setCheckable(true);
    rate15->setChecked(m_refreshDivisor == 4);
    rate15->setData(4);
    m_refreshRateGroup->addAction(rate15);
    
    QAction *rate10 = refreshMenu->addAction("10 fps (Low CPU)");
    rate10->setCheckable(true);
    rate10->setChecked(m_refreshDivisor == 6);
    rate10->setData(6);
    m_refreshRateGroup->addAction(rate10);
    
    connect(m_refreshRateGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        setRefreshRate(action->data().toInt());
    });
    
    m_contextMenu->addSeparator();
    
    // Pulse Intensity submenu
    QMenu *pulseMenu = m_contextMenu->addMenu("Pulse Intensity");
    QActionGroup *pulseGroup = new QActionGroup(this);
    
    QAction *pulseSubtle = pulseMenu->addAction("Subtle");
    pulseSubtle->setCheckable(true);
    pulseSubtle->setChecked(m_pulseIntensity == 0.5f);
    pulseGroup->addAction(pulseSubtle);
    connect(pulseSubtle, &QAction::triggered, this, [this]() { m_pulseIntensity = 0.5f; update(); });
    
    QAction *pulseNormal = pulseMenu->addAction("Normal");
    pulseNormal->setCheckable(true);
    pulseNormal->setChecked(m_pulseIntensity == 1.0f);
    pulseGroup->addAction(pulseNormal);
    connect(pulseNormal, &QAction::triggered, this, [this]() { m_pulseIntensity = 1.0f; update(); });
    
    QAction *pulseIntense = pulseMenu->addAction("Intense");
    pulseIntense->setCheckable(true);
    pulseIntense->setChecked(m_pulseIntensity == 1.5f);
    pulseGroup->addAction(pulseIntense);
    connect(pulseIntense, &QAction::triggered, this, [this]() { m_pulseIntensity = 1.5f; update(); });
    
    // Smoothing submenu
    QMenu *smoothMenu = m_contextMenu->addMenu("Smoothing");
    QActionGroup *smoothGroup = new QActionGroup(this);
    
    QAction *smoothFast = smoothMenu->addAction("Fast (Low)");
    smoothFast->setCheckable(true);
    smoothFast->setChecked(m_smoothingFactor >= 0.45f);
    smoothGroup->addAction(smoothFast);
    connect(smoothFast, &QAction::triggered, this, [this]() { m_smoothingFactor = 0.5f; });
    
    QAction *smoothMedium = smoothMenu->addAction("Medium");
    smoothMedium->setCheckable(true);
    smoothMedium->setChecked(m_smoothingFactor >= 0.25f && m_smoothingFactor < 0.45f);
    smoothGroup->addAction(smoothMedium);
    connect(smoothMedium, &QAction::triggered, this, [this]() { m_smoothingFactor = 0.3f; });
    
    QAction *smoothSlow = smoothMenu->addAction("Slow (High)");
    smoothSlow->setCheckable(true);
    smoothSlow->setChecked(m_smoothingFactor < 0.25f);
    smoothGroup->addAction(smoothSlow);
    connect(smoothSlow, &QAction::triggered, this, [this]() { m_smoothingFactor = 0.15f; });
    
    m_contextMenu->addSeparator();
    
    // Show/Hide render time
    QAction *showRenderTime = m_contextMenu->addAction("Show Render Time");
    showRenderTime->setCheckable(true);
    showRenderTime->setChecked(m_showRenderTime);
    connect(showRenderTime, &QAction::triggered, this, [this](bool checked) {
        m_showRenderTime = checked;
        update();
    });
    
    m_contextMenu->addSeparator();
    
    // Reset action
    QAction *resetAction = m_contextMenu->addAction("Reset to Defaults");
    connect(resetAction, &QAction::triggered, this, [this, rate60, pulseNormal, smoothMedium, showRenderTime]() {
        m_refreshDivisor = 1;
        m_pulseIntensity = 1.0f;
        m_smoothingFactor = 0.3f;
        m_showRenderTime = true;
        rate60->setChecked(true);
        pulseNormal->setChecked(true);
        smoothMedium->setChecked(true);
        showRenderTime->setChecked(true);
        update();
    });
}

void PanAnalyzer::contextMenuEvent(QContextMenuEvent *event)
{
    m_contextMenu->exec(event->globalPos());
}

void PanAnalyzer::setRefreshRate(int divisor)
{
    m_refreshDivisor = divisor;
    m_updateCounter = 0;
}

void PanAnalyzer::setEnabled(bool enabled)
{
    m_enabled = enabled;
    m_btnEnabled->setChecked(!enabled);
    m_btnEnabled->setIcon(QIcon(enabled ? ":/icons/active.svg" : ":/icons/inactive.svg"));
    m_panAnalyzer->setEnabled(enabled);
    update();
}

void PanAnalyzer::toggleEnabled()
{
    setEnabled(!m_enabled);
}

void PanAnalyzer::updatePanData(const NN_PanData_t &data)
{
    if (!m_enabled) return;
    
    // Apply refresh rate divisor
    if (++m_updateCounter < m_refreshDivisor) {
        // Still update data for smoothing but don't repaint
        m_currentData = data;
        for (int i = 0; i < PAN_NUM_SEGMENTS; ++i) {
            m_smoothedSegments[i] = m_smoothedSegments[i] * (1.0f - m_smoothingFactor) 
                                   + data.segments[i] * m_smoothingFactor;
        }
        m_smoothedPan = m_smoothedPan * (1.0f - m_smoothingFactor) + data.pan * m_smoothingFactor;
        return;
    }
    m_updateCounter = 0;
    
    // Start timing for render time
    QElapsedTimer timer;
    timer.start();
    
    m_currentData = data;
    
    // Smooth segment values
    for (int i = 0; i < PAN_NUM_SEGMENTS; ++i) {
        m_smoothedSegments[i] = m_smoothedSegments[i] * (1.0f - m_smoothingFactor) 
                               + data.segments[i] * m_smoothingFactor;
    }
    
    // Smooth pan position
    m_smoothedPan = m_smoothedPan * (1.0f - m_smoothingFactor) + data.pan * m_smoothingFactor;
    
    update();
    
    // Render time measurement
    qint64 elapsedNs = timer.nsecsElapsed();
    m_lastFrameTimeNs = elapsedNs;
    m_renderTimeAccum += elapsedNs;
    m_renderTimeCount++;
    
    // Update target FPS based on refresh divisor
    m_targetFps = 60 / m_refreshDivisor;
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastStatsUpdate == 0) m_lastStatsUpdate = now;
    
    if (now - m_lastStatsUpdate >= 2000) {  // Update stats every 2 seconds
        if (m_renderTimeCount > 0) {
            // Average frame time in ns
            m_avgFrameTimeNs = float(m_renderTimeAccum / m_renderTimeCount);
            // Total render time per second = avg_frame_ns * target_fps / 1000000
            m_totalRenderTimeMs = (m_avgFrameTimeNs * m_targetFps) / 1000000.0f;
        }
        m_renderTimeAccum = 0.0;
        m_renderTimeCount = 0;
        m_lastStatsUpdate = now;
    }
}

void PanAnalyzer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void PanAnalyzer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    drawBackground(p);
    drawSemicircle(p);
    
    if (m_enabled) {
        drawPulsingShape(p);
    }
    
    drawLabels(p);
    drawRenderTime(p);
}

void PanAnalyzer::drawBackground(QPainter &p)
{
    // Same background gradient as spectrum analyzer
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(15, 15, 22));
    bg.setColorAt(1.0, QColor(8, 8, 14));
    p.fillRect(rect(), bg);
}

void PanAnalyzer::drawSemicircle(QPainter &p)
{
    const int w = width();
    const int h = height();
    
    // Calculate semicircle dimensions
    const int centerX = w / 2;
    const int centerY = h - MARGIN;
    const int radius = std::min(w / 2 - MARGIN, h - MARGIN * 2);
    
    // Draw outer arc (semicircle outline)
    QPen arcPen(QColor(60, 60, 70));
    arcPen.setWidth(2);
    p.setPen(arcPen);
    p.setBrush(Qt::NoBrush);
    
    QRect arcRect(centerX - radius, centerY - radius, radius * 2, radius * 2);
    p.drawArc(arcRect, 0 * 16, 180 * 16);  // 0 to 180 degrees (top semicircle)
    
    // Draw radial lines for segments
    QPen linePen(QColor(40, 40, 50));
    linePen.setWidth(1);
    p.setPen(linePen);
    
    for (int i = 0; i <= PAN_NUM_SEGMENTS; ++i) {
        float angle = M_PI * i / PAN_NUM_SEGMENTS;
        int x = centerX + int(cos(M_PI - angle) * radius);
        int y = centerY - int(sin(angle) * radius);
        p.drawLine(centerX, centerY, x, y);
    }
    
    // Draw inner arc guides
    for (int r = radius / 3; r < radius; r += radius / 3) {
        QRect innerArcRect(centerX - r, centerY - r, r * 2, r * 2);
        p.drawArc(innerArcRect, 0 * 16, 180 * 16);
    }
}

void PanAnalyzer::drawSegments(QPainter &p)
{
    // Not used anymore - replaced by drawPulsingShape
    Q_UNUSED(p);
}

void PanAnalyzer::drawPulsingShape(QPainter &p)
{
    const int w = width();
    const int h = height();
    
    const int centerX = w / 2;
    const int centerY = h - MARGIN;
    const int maxRadius = std::min(w / 2 - MARGIN, h - MARGIN * 2);
    const int minRadius = maxRadius / 10;  // Minimum visible radius
    
    // Build smooth curve through all segment points using Catmull-Rom spline
    // We need extra points at the ends for proper spline calculation
    std::vector<QPointF> controlPoints;
    controlPoints.reserve(PAN_NUM_SEGMENTS + 4);
    
    // Calculate radius for each segment based on intensity
    // Use logarithmic scale for better visibility at lower volumes
    auto getRadiusForSegment = [&](int segIdx) -> float {
        // Clamp index to valid range
        int idx = std::max(0, std::min(segIdx, PAN_NUM_SEGMENTS - 1));
        float value = m_smoothedSegments[idx];
        
        // Convert linear RMS to logarithmic scale (dB-like)
        // This makes quieter sounds more visible
        float logValue = 0.0f;
        if (value > 0.0001f) {
            // Map to 0-1 range using log scale
            // -80dB to 0dB range -> 0 to 1
            float db = 20.0f * log10f(value);
            logValue = (db + 60.0f) / 60.0f;  // -60dB = 0, 0dB = 1
            logValue = std::max(0.0f, std::min(1.0f, logValue));
            
            // Apply additional boost for better visibility
            logValue = powf(logValue, 0.6f);  // Compress dynamic range
            
            // Apply pulse intensity setting
            logValue *= m_pulseIntensity;
            logValue = std::min(1.0f, logValue);  // Clamp to max
        }
        
        return minRadius + (maxRadius - minRadius) * logValue;
    };
    
    // Get point position for a segment
    auto getPointForSegment = [&](int segIdx, float radiusOverride = -1.0f) -> QPointF {
        float angle = M_PI * (segIdx + 0.5f) / PAN_NUM_SEGMENTS;  // Center of segment
        float radius = radiusOverride >= 0 ? radiusOverride : getRadiusForSegment(segIdx);
        float x = centerX + cos(M_PI - angle) * radius;
        float y = centerY - sin(angle) * radius;
        return QPointF(x, y);
    };
    
    // Add virtual point before first (for spline)
    controlPoints.push_back(getPointForSegment(-1, getRadiusForSegment(0)));
    
    // Add all segment points
    for (int i = 0; i < PAN_NUM_SEGMENTS; ++i) {
        controlPoints.push_back(getPointForSegment(i));
    }
    
    // Add virtual point after last (for spline)
    controlPoints.push_back(getPointForSegment(PAN_NUM_SEGMENTS, getRadiusForSegment(PAN_NUM_SEGMENTS - 1)));
    
    // Catmull-Rom spline interpolation
    auto catmullRom = [](const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, float t) -> QPointF {
        float t2 = t * t;
        float t3 = t2 * t;
        
        float x = 0.5f * ((2.0f * p1.x()) +
                          (-p0.x() + p2.x()) * t +
                          (2.0f * p0.x() - 5.0f * p1.x() + 4.0f * p2.x() - p3.x()) * t2 +
                          (-p0.x() + 3.0f * p1.x() - 3.0f * p2.x() + p3.x()) * t3);
        
        float y = 0.5f * ((2.0f * p1.y()) +
                          (-p0.y() + p2.y()) * t +
                          (2.0f * p0.y() - 5.0f * p1.y() + 4.0f * p2.y() - p3.y()) * t2 +
                          (-p0.y() + 3.0f * p1.y() - 3.0f * p2.y() + p3.y()) * t3);
        
        return QPointF(x, y);
    };
    
    // Build the smooth path
    QPainterPath shapePath;
    shapePath.moveTo(centerX, centerY);  // Start at center
    
    // First line to the first curve point (left edge)
    float firstAngle = M_PI * 0.5f / PAN_NUM_SEGMENTS;
    float firstRadius = getRadiusForSegment(0);
    shapePath.lineTo(centerX + cos(M_PI - firstAngle * 0.1f) * firstRadius, 
                     centerY - sin(firstAngle * 0.1f) * firstRadius);
    
    // Generate smooth curve through control points (increased detail for smoother curves)
    const int stepsPerSegment = 16;  // Increased from 8 for smoother curves
    for (size_t i = 1; i < controlPoints.size() - 2; ++i) {
        for (int step = 0; step < stepsPerSegment; ++step) {
            float t = float(step) / stepsPerSegment;
            QPointF pt = catmullRom(controlPoints[i - 1], controlPoints[i], 
                                    controlPoints[i + 1], controlPoints[i + 2], t);
            shapePath.lineTo(pt);
        }
    }
    
    // Last point
    shapePath.lineTo(controlPoints[controlPoints.size() - 2]);
    
    // Close to center
    shapePath.lineTo(centerX, centerY);
    shapePath.closeSubpath();
    
    // Create gradient fill - cyan on left, magenta on right
    QLinearGradient gradient(centerX - maxRadius, centerY, centerX + maxRadius, centerY);
    gradient.setColorAt(0.0, QColor(0, 200, 255, 180));   // Cyan left
    gradient.setColorAt(0.5, QColor(100, 100, 255, 180)); // Blue center
    gradient.setColorAt(1.0, QColor(255, 0, 200, 180));   // Magenta right
    
    // Draw filled shape
    p.setPen(Qt::NoPen);
    p.setBrush(gradient);
    p.drawPath(shapePath);
    
    // Draw outline with glow effect
    for (int glow = 3; glow >= 1; --glow) {
        QPen glowPen(QColor(255, 255, 255, 30 / glow));
        glowPen.setWidth(glow * 2);
        p.setPen(glowPen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(shapePath);
    }
    
    // Draw bright outline
    QPen outlinePen(QColor(255, 255, 255, 200));
    outlinePen.setWidth(2);
    p.setPen(outlinePen);
    p.setBrush(Qt::NoBrush);
    
    // Draw only the curved part (not the lines to center)
    QPainterPath curvePath;
    bool first = true;
    for (size_t i = 1; i < controlPoints.size() - 2; ++i) {
        for (int step = 0; step < stepsPerSegment; ++step) {
            float t = float(step) / stepsPerSegment;
            QPointF pt = catmullRom(controlPoints[i - 1], controlPoints[i], 
                                    controlPoints[i + 1], controlPoints[i + 2], t);
            if (first) {
                curvePath.moveTo(pt);
                first = false;
            } else {
                curvePath.lineTo(pt);
            }
        }
    }
    curvePath.lineTo(controlPoints[controlPoints.size() - 2]);
    p.drawPath(curvePath);
    
    // Draw small dots at segment peaks
    p.setPen(Qt::NoPen);
    for (int i = 0; i < PAN_NUM_SEGMENTS; ++i) {
        float value = m_smoothedSegments[i];
        if (value > 0.05f) {
            QPointF pt = getPointForSegment(i);
            
            // Color based on position
            float hue = 0.5f + float(i) / PAN_NUM_SEGMENTS * 0.4f;
            QColor dotColor = QColor::fromHsvF(hue, 0.8f, 1.0f);
            
            p.setBrush(dotColor);
            int dotSize = 3 + int(value * 4);
            p.drawEllipse(pt, dotSize, dotSize);
        }
    }
}

void PanAnalyzer::drawPanIndicator(QPainter &p)
{
    // Not used anymore - replaced by drawPulsingShape
    Q_UNUSED(p);
}

void PanAnalyzer::drawLabels(QPainter &p)
{
    const int w = width();
    const int h = height();
    
    const int centerX = w / 2;
    const int centerY = h - MARGIN;
    const int radius = std::min(w / 2 - MARGIN, h - MARGIN * 2);
    
    QFont font = p.font();
    font.setPointSize(9);
    font.setBold(true);
    p.setFont(font);
    
    if (m_enabled) {
        p.setPen(QColor(180, 180, 180));
    } else {
        p.setPen(QColor(80, 80, 80));
    }
    
    // Left label
    p.drawText(MARGIN, centerY - 5, "L");
    
    // Right label
    p.drawText(w - MARGIN - 10, centerY - 5, "R");
    
    // Center label
    p.drawText(centerX - 5, centerY - radius - 5, "C");
    
    // If disabled, show "DISABLED" text
    if (!m_enabled) {
        font.setPointSize(12);
        p.setFont(font);
        p.setPen(QColor(100, 100, 100));
        
        QRect textRect = rect();
        textRect.setTop(h / 3);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, "DISABLED");
    }
}

void PanAnalyzer::drawRenderTime(QPainter &p)
{
    if (!m_enabled || !m_showRenderTime) return;
    if (m_avgFrameTimeNs < 1.0f && m_lastFrameTimeNs < 1) return;
    
    // Color based on total render time per second: green < 5ms, yellow < 20ms, red >= 20ms
    QColor textColor;
    if (m_totalRenderTimeMs < 5.0f) {
        textColor = QColor(100, 200, 100);  // Green
    } else if (m_totalRenderTimeMs < 20.0f) {
        textColor = QColor(200, 200, 100); // Yellow
    } else {
        textColor = QColor(200, 100, 100);  // Red
    }
    
    QFont font = p.font();
    font.setPointSize(7);
    p.setFont(font);
    p.setPen(textColor);
    
    // Format frame time - use μs if >= 1000ns, otherwise use ns
    QString frameText;
    if (m_avgFrameTimeNs >= 1000.0f) {
        frameText = QString("%1 μs").arg(m_avgFrameTimeNs / 1000.0f, 0, 'f', 1);
    } else {
        frameText = QString("%1 ns").arg(int(m_avgFrameTimeNs));
    }
    
    // Format total time per second
    QString totalText = QString("%1 ms/s @%2fps").arg(m_totalRenderTimeMs, 0, 'f', 2).arg(m_targetFps);
    
    // Draw in top-right corner, two lines
    QRect frameRect(width() - 70, 3, 65, 10);
    QRect totalRect(width() - 95, 13, 90, 10);
    
    p.drawText(frameRect, Qt::AlignRight, frameText);
    p.setPen(textColor.darker(110));
    p.drawText(totalRect, Qt::AlignRight, totalText);
}
