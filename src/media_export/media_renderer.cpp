#include "media_renderer.h"

#include <cmath>
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>
#include <QDebug>
#include <note_naga_engine/nn_utils.h>
#include <note_naga_engine/core/runtime_data.h>

// Constants for keyboard range
const int FIRST_MIDI_NOTE = 21; // A0
const int LAST_MIDI_NOTE = 108; // C8

MediaRenderer::MediaRenderer(NoteNagaMidiSeq *sequence)
    : m_sequence(sequence), m_arrangement(nullptr), m_runtimeData(nullptr), m_lastLayoutSize(0, 0)
{
    prepareNoteData();
    // Load the default particle pixmap
    m_resourceParticlePixmapCache.load(":/images/sparkle.png");
}

MediaRenderer::MediaRenderer(NoteNagaArrangement* arrangement, NoteNagaRuntimeData* runtimeData)
    : m_sequence(nullptr), m_arrangement(arrangement), m_runtimeData(runtimeData), m_lastLayoutSize(0, 0)
{
    prepareNoteDataFromArrangement();
    // Load the default particle pixmap
    m_resourceParticlePixmapCache.load(":/images/sparkle.png");
}

void MediaRenderer::setRenderSettings(const RenderSettings &settings)
{
    // If the custom particle image changes, we must invalidate the cache
    if (m_settings.customParticleImage.cacheKey() != settings.customParticleImage.cacheKey())
    {
        m_customParticlePixmapCache = QPixmap(); // Invalidates cache
    }
    m_settings = settings;
}

void MediaRenderer::prepareNoteData()
{
    if (!m_sequence) return;
    
    m_notes.clear();
    for (const auto &track : m_sequence->getTracks())
    {
        // Skip muted tracks and tempo tracks (consistent with arrangement mode)
        if (!track || track->isMuted() || track->isTempoTrack()) continue;
        
        QColor trackColor = track->getColor().toQColor();
        for (const auto &note : track->getNotes())
        {
            if (note.start.has_value() && note.length.has_value())
            {
                m_notes.push_back({note.note,
                                   nn_ticks_to_seconds(note.start.value(), m_sequence->getPPQ(), m_sequence->getTempo()),
                                   nn_ticks_to_seconds(note.start.value() + note.length.value(), m_sequence->getPPQ(), m_sequence->getTempo()),
                                   trackColor});
            }
        }
    }
}

void MediaRenderer::prepareNoteDataFromArrangement()
{
    if (!m_arrangement || !m_runtimeData) {
        qDebug() << "prepareNoteDataFromArrangement: INVALID - arrangement:" << m_arrangement << "runtimeData:" << m_runtimeData;
        return;
    }
    
    m_notes.clear();
    
    int ppq = m_runtimeData->getPPQ();
    int tempo = m_runtimeData->getTempo();
    
    qDebug() << "prepareNoteDataFromArrangement: trackCount=" << m_arrangement->getTrackCount() << "ppq=" << ppq << "tempo=" << tempo;
    
    // Iterate through all arrangement tracks
    for (NoteNagaArrangementTrack* arrTrack : m_arrangement->getTracks()) {
        if (!arrTrack || arrTrack->isMuted()) continue;
        
        QColor arrTrackColor = arrTrack->getColor().toQColor();
        
        qDebug() << "  ArrTrack:" << QString::fromStdString(arrTrack->getName()) << "clips:" << arrTrack->getClips().size();
        
        // Process all MIDI clips on this track
        for (const NN_MidiClip_t& clip : arrTrack->getClips()) {
            if (clip.muted) continue;
            
            // Get the referenced sequence
            NoteNagaMidiSeq* seq = m_runtimeData->getSequenceById(clip.sequenceId);
            if (!seq) {
                qDebug() << "    Clip sequenceId=" << clip.sequenceId << "NOT FOUND";
                continue;
            }
            
            int seqLength = seq->getMaxTick();
            if (seqLength <= 0) continue;
            
            qDebug() << "    Clip: seqId=" << clip.sequenceId << "startTick=" << clip.startTick << "duration=" << clip.durationTicks << "seqLength=" << seqLength;
            
            // Calculate clip boundaries in seconds
            double clipStartSec = nn_ticks_to_seconds(clip.startTick, ppq, tempo);
            double clipEndSec = nn_ticks_to_seconds(clip.startTick + clip.durationTicks, ppq, tempo);
            double seqLengthSec = nn_ticks_to_seconds(seqLength, ppq, tempo);
            
            // Get notes from all tracks in the sequence
            for (NoteNagaTrack* midiTrack : seq->getTracks()) {
                if (!midiTrack || midiTrack->isMuted() || midiTrack->isTempoTrack()) continue;
                
                // Use arrangement track color for all notes in this clip
                QColor noteColor = arrTrackColor;
                
                for (const NN_Note_t& note : midiTrack->getNotes()) {
                    if (!note.start.has_value() || !note.length.has_value()) continue;
                    
                    double noteStartInSeq = nn_ticks_to_seconds(note.start.value(), ppq, tempo);
                    double noteEndInSeq = nn_ticks_to_seconds(note.start.value() + note.length.value(), ppq, tempo);
                    
                    // Handle looping - add notes for each loop iteration within the clip
                    if (seqLengthSec > 0) {
                        int loopCount = static_cast<int>(ceil((clipEndSec - clipStartSec) / seqLengthSec));
                        for (int loop = 0; loop < loopCount; ++loop) {
                            double loopOffset = loop * seqLengthSec;
                            double absNoteStart = clipStartSec + loopOffset + noteStartInSeq;
                            double absNoteEnd = clipStartSec + loopOffset + noteEndInSeq;
                            
                            // Clip note to clip boundaries
                            if (absNoteEnd <= clipStartSec) continue;
                            if (absNoteStart >= clipEndSec) continue;
                            
                            absNoteStart = std::max(absNoteStart, clipStartSec);
                            absNoteEnd = std::min(absNoteEnd, clipEndSec);
                            
                            if (absNoteEnd > absNoteStart) {
                                m_notes.push_back({
                                    note.note,
                                    absNoteStart,
                                    absNoteEnd,
                                    noteColor
                                });
                            }
                        }
                    }
                }
            }
        }
    }
}

void MediaRenderer::prepareKeyboardLayout(const QSize &size)
{
    // ... (unchanged) ...
    if (size == m_lastLayoutSize)
        return;

    m_keyboardLayout.clear();
    const float keyboardHeight = size.height() * 0.25f;

    std::vector<bool> is_white_key = {true, false, true, false, true, true, false, true, false, true, false, true};
    int white_key_count = 0;
    for (int i = FIRST_MIDI_NOTE; i <= LAST_MIDI_NOTE; ++i)
    {
        if (is_white_key[i % 12])
            white_key_count++;
    }

    float white_key_width = (float)size.width() / white_key_count;
    float black_key_width = white_key_width * 0.6f;
    float black_key_height = keyboardHeight * 0.6f;

    int current_white_key = 0;
    for (int i = FIRST_MIDI_NOTE; i <= LAST_MIDI_NOTE; ++i)
    {
        if (is_white_key[i % 12])
        {
            m_keyboardLayout[i] = {
                QRectF(current_white_key * white_key_width, size.height() - keyboardHeight, white_key_width, keyboardHeight),
                true};
            current_white_key++;
        }
    }
    for (int i = FIRST_MIDI_NOTE; i <= LAST_MIDI_NOTE; ++i)
    {
        if (!is_white_key[i % 12])
        {
            float center_x = m_keyboardLayout[i - 1].rect.right();
            m_keyboardLayout[i] = {
                QRectF(center_x - black_key_width / 2, size.height() - keyboardHeight, black_key_width, black_key_height),
                false};
        }
    }
    m_lastLayoutSize = size;
}

void MediaRenderer::resetSimulation()
{
    m_currentState.particles.clear();
    m_currentState.activeNotes.clear();
    m_lastFrameTime = -1.0;
}

// =========================================================================
// Stateful version for PREVIEW
// =========================================================================

QImage MediaRenderer::renderFrame(double currentTime, const QSize &size)
{
    // Detect backward time travel (scrubbing)
    if (m_lastFrameTime >= 0 && currentTime < m_lastFrameTime)
    {
        resetSimulation();
    }

    double deltaTime = (m_lastFrameTime < 0) ? 0.0 : currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;

    // 1. Calculate the new state
    FrameState nextState = calculateNextState(m_currentState, currentTime, deltaTime);
    m_currentState = nextState; // Store the new state

    // 2. Render the state
    return renderFrame(currentTime, size, m_currentState);
}

// =========================================================================
// Simulation Logic (Stateless)
// =========================================================================

MediaRenderer::FrameState MediaRenderer::calculateNextState(const FrameState &previousState, double currentTime, double deltaTime)
{
    // Create a copy of the *previous* state to modify
    FrameState newState = previousState;

    std::map<int, bool> currentActiveNotes;

    // 1. Find active notes (for keyboard and particles)
    for (const auto &note : m_notes)
    {
        if (currentTime >= note.start_time && currentTime < note.end_time)
        {
            currentActiveNotes[note.note_val] = true;
        }
    }

    // 2. Spawn new particles (if enabled)
    if (m_settings.renderParticles)
    {
        double resolutionScale = (double)m_lastLayoutSize.height() / 720.0;

        for (const auto &pair : currentActiveNotes)
        {
            // Was this note active in the previous state?
            bool wasActive = previousState.activeNotes.find(pair.first) != previousState.activeNotes.end();
            if (!wasActive)
            {
                // Note just started playing, find its data
                for (const auto &noteInfo : m_notes)
                {
                    if (noteInfo.note_val == pair.first)
                    {
                        // Check if it's the "real" start of the note
                        if (std::abs(noteInfo.start_time - currentTime) < (deltaTime + 0.01))
                        {
                            if (m_keyboardLayout.find(noteInfo.note_val) != m_keyboardLayout.end())
                            {
                                // Add particles to the *new* state
                                spawnParticles(noteInfo, resolutionScale, newState.particles);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // 3. Update existing particles
    if (m_settings.renderParticles)
    {
        updateParticles(deltaTime, newState.particles);
    }

    // 4. Store active notes for the next frame
    newState.activeNotes = currentActiveNotes;
    return newState;
}

// =========================================================================
// Stateless version for EXPORT
// =========================================================================

QImage MediaRenderer::renderFrame(double currentTime, const QSize &size, const FrameState &state)
{

    prepareKeyboardLayout(size);
    QImage frame(size, QImage::Format_ARGB32);

    // 1. Draw background
    if (!m_settings.backgroundImage.isNull())
    {
        QPainter bgPainter(&frame);

        QRectF targetRect = frame.rect();

        // Aplikujeme "třesení" pozadí, pokud je povoleno
        if (m_settings.renderBgShake)
        {
            // Výpočet posunu pomocí sin/cos pro plynulý, pomalý a náhodně vypadající pohyb
            double shakeX = sin(currentTime * 0.15) * m_settings.bgShakeIntensity + cos(currentTime * 0.4) * (m_settings.bgShakeIntensity * 0.5);
            double shakeY = cos(currentTime * 0.2) * m_settings.bgShakeIntensity + sin(currentTime * 0.3) * (m_settings.bgShakeIntensity * 0.5);

            // Obrázek mírně zvětšíme, abychom při posunu neviděli černé okraje
            double expansion = m_settings.bgShakeIntensity * 1.5;
            targetRect = frame.rect().adjusted(-expansion, -expansion, expansion, expansion);
            targetRect.translate(shakeX, shakeY);
        }

        // Vykreslíme pozadí s ořezem na původní velikost framu
        bgPainter.save();
        bgPainter.setClipRect(frame.rect());
        bgPainter.drawImage(targetRect, m_settings.backgroundImage);
        bgPainter.restore();

        bgPainter.end();
    }
    else
    {
        frame.fill(m_settings.backgroundColor); // Jednolitá barva se "třást" nemůže
    }

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing);

    // 2. Draw notes and keyboard
    // Use the active notes state from 'state'
    drawNotesAndKeyboard(painter, currentTime, size, state.activeNotes);

    // 3. Draw particles
    if (m_settings.renderParticles)
    {
        double resolutionScale = (double)size.height() / 720.0;
        // Use the particle state from 'state'
        drawParticles(painter, state.particles, resolutionScale);
    }

    painter.end();
    return frame;
}

// =========================================================================
// Helper Methods (Simulation)
// =========================================================================

void MediaRenderer::spawnParticles(const NoteInfo &note, double resolutionScale, std::vector<Particle> &particles)
{
    if (m_keyboardLayout.find(note.note_val) == m_keyboardLayout.end())
        return;
    const KeyInfo &key = m_keyboardLayout.at(note.note_val);
    QPointF spawn_pos = QPointF(key.rect.center().x(), key.rect.top());

    int particle_count = m_settings.particleCount;

    for (int i = 0; i < particle_count; ++i)
    {
        double angle_deg = (rand() % 160) + 10; // Between 10 and 170 degrees (upwards)
        double angle_rad = angle_deg * M_PI / 180.0;
        double speed_base = m_settings.particleSpeed * resolutionScale;
        double speed_rand = speed_base * 0.5;
        double speed = speed_base - (speed_rand / 2) + (rand() % (int)(speed_rand + 1));
        qreal lifetime = m_settings.particleLifetime + (rand() % 50) / 100.0 - 0.25; // +/- 0.25s
        if (lifetime < 0.1)
            lifetime = 0.1;

        particles.push_back({spawn_pos,
                             QPointF(cos(angle_rad) * speed, -sin(angle_rad) * speed),
                             lifetime,
                             lifetime,
                             note.color});
    }
}

void MediaRenderer::updateParticles(double deltaTime, std::vector<Particle> &particles)
{
    double resolutionScale = (double)m_lastLayoutSize.height() / 720.0;

    for (auto it = particles.begin(); it != particles.end();)
    {
        it->pos += it->vel * deltaTime;
        // Scale gravity
        it->vel.ry() += m_settings.particleGravity * resolutionScale * deltaTime;
        it->lifetime -= deltaTime;

        if (it->lifetime <= 0)
        {
            it = particles.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// =========================================================================
// Helper Methods (Drawing)
// =========================================================================

void MediaRenderer::drawNotesAndKeyboard(QPainter &painter, double currentTime,
                                         const QSize &size, const std::map<int, bool> &activeNotes)
{
    double resolutionScale = (double)size.height() / 720.0;
    const float keyboardHeight = size.height() * 0.25f;
    const float render_area_height = size.height() - keyboardHeight;
    const float pixels_per_second = render_area_height / m_secondsVisible;

    // --- Draw falling notes ---
    if (m_settings.renderNotes)
    {
        for (const auto &note : m_notes)
        {
            if (note.end_time < currentTime - 1.0 || note.start_time > currentTime + m_secondsVisible)
                continue;

            //bool isActive = (activeNotes.find(note.note_val) != activeNotes.end());
            bool isCurrentlyPlaying = (currentTime >= note.start_time && currentTime < note.end_time);

            if (m_keyboardLayout.find(note.note_val) == m_keyboardLayout.end())
                continue;
            const KeyInfo &key = m_keyboardLayout.at(note.note_val);

            float y_start = render_area_height - (float)(note.start_time - currentTime) * pixels_per_second;
            float y_end = render_area_height - (float)(note.end_time - currentTime) * pixels_per_second;
            QRectF note_rect(key.rect.x(), y_end, key.rect.width(), y_start - y_end);

            // Clip drawing to the area above the keyboard
            note_rect = note_rect.intersected(QRectF(0, 0, size.width(), render_area_height));
            if (note_rect.isEmpty())
                continue;

            QColor note_color = note.color;
            // Calculate opacity
            float progress = std::clamp(y_start / render_area_height, 0.0f, 1.0f);
            qreal opacity = m_settings.noteStartOpacity + (m_settings.noteEndOpacity - m_settings.noteStartOpacity) * progress;
            note_color.setAlphaF(std::clamp(opacity, 0.0, 1.0));

            // Define a "glow zone" just above the keyboard
            float glow_zone_height = 30.0f * resolutionScale;
            float glow_zone_top = render_area_height - glow_zone_height;
            QRectF glow_zone_rect(0, glow_zone_top, size.width(), glow_zone_height);

            // The glow is the intersection of the note and the glow zone
            QRectF glow_rect = note_rect.intersected(glow_zone_rect);

            if (isCurrentlyPlaying)
            {
                painter.setPen(Qt::NoPen);
                // Vykreslíme několik zvětšených obdélníků s nízkou opacitou
                // pro simulaci "záře" za notou.
                for (int i = 0; i < 10; ++i)
                {
                    QColor glow_color = note_color;
                    // Opacita záře je závislá na opacitě noty
                    glow_color.setAlphaF(note_color.alphaF() * 0.05);
                    painter.setBrush(glow_color);

                    // Aplikujeme záři na celý obdélník noty, mírně zvětšený
                    painter.drawRect(note_rect.adjusted(-i, -i, i, i));
                }
            }

            // Draw the note rectangle
            painter.setBrush(note_color);
            QColor penColor = note_color.darker(120);
            penColor.setAlphaF(note_color.alphaF()); // Pen has same opacity
            painter.setPen(penColor);
            painter.drawRect(note_rect);
        }
    }

    // --- Draw Keyboard ---
    if (m_settings.renderKeyboard)
    {
        // Draw white keys
        for (const auto &pair : m_keyboardLayout)
        {
            if (pair.second.is_white)
            {
                bool isActive = (activeNotes.find(pair.first) != activeNotes.end());
                if (isActive)
                {
                    QColor activeColor = QColor(150, 150, 255);
                    for (const auto &note : m_notes)
                    {
                        if (note.note_val == pair.first && currentTime >= note.start_time && currentTime < note.end_time)
                        {
                            activeColor = note.color;
                            break;
                        }
                    }
                    painter.setBrush(activeColor.lighter(120));
                    painter.setPen(activeColor);
                }
                else
                {
                    painter.setBrush(QColor(245, 245, 245));
                    painter.setPen(QColor(200, 200, 200));
                }
                painter.drawRect(pair.second.rect);
            }
        }
        // Draw black keys
        for (const auto &pair : m_keyboardLayout)
        {
            if (!pair.second.is_white)
            {
                bool isActive = (activeNotes.find(pair.first) != activeNotes.end());
                if (isActive)
                {
                    QColor activeColor = QColor(150, 150, 255);
                    for (const auto &note : m_notes)
                    {
                        if (note.note_val == pair.first && currentTime >= note.start_time && currentTime < note.end_time)
                        {
                            activeColor = note.color;
                            break;
                        }
                    }
                    painter.setBrush(activeColor.lighter(120));
                    painter.setPen(activeColor);
                }
                else
                {
                    painter.setBrush(QColor(40, 40, 40));
                    painter.setPen(Qt::black);
                }
                painter.drawRect(pair.second.rect);
            }
        }

        // --- Draw "Piano Glow" effect ---
        if (m_settings.renderPianoGlow)
        {
            float glowHeight = 25.0f * resolutionScale; // Glow height
            QRectF glowRect(0, render_area_height - glowHeight, size.width(), glowHeight);

            QLinearGradient glow(0, render_area_height - glowHeight, 0, render_area_height);
            glow.setColorAt(0.0, QColor(255, 255, 255, 0));
            glow.setColorAt(0.8, QColor(255, 255, 255, 70));
            glow.setColorAt(1.0, QColor(255, 255, 255, 100));

            painter.setCompositionMode(QPainter::CompositionMode_Plus); // Additive blending
            painter.fillRect(glowRect, glow);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver); // Reset
        }

        // --- Draw "Lightning" effect ---
        if (m_settings.renderLightning)
        {
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setCompositionMode(QPainter::CompositionMode_Plus); // Additive glow

            float y_base = render_area_height; // The seam
            int numPoints = size.width() / 10; // A point every 10 pixels
            if (numPoints < 2) numPoints = 2;

            // Základní frekvence
            const double baseFreqTimeY = 15.0; // Rychlost v čase pro Y
            const double baseFreqTimeX = 8.0;  // Rychlost v čase pro X
            const double baseFreqPosX = 0.05; // Jak moc se tvar mění podél X osy (pro X jitter)
            const double baseFreqPosY = 0.02; // Jak moc se tvar mění podél X osy (pro Y jitter)

            for (int i = 0; i < m_settings.lightningLines; ++i)
            {
                // --- De-synchronizace ---
                // Vytvoříme unikátní, ale deterministické modifikátory pro každou čáru (i)
                // aby se každá hýbala trochu jinak (jiná rychlost, jiná "nervozita")
                float freqModTimeY = 1.0f + (i % 5) * 0.15f;    // 1.0, 1.15, 1.30, 1.45, 1.60...
                float freqModTimeX = 1.0f + ((i + 2) % 5) * 0.2f; // 1.4, 1.6, 1.0, 1.2, 1.4...
                float freqModPosX = 1.0f + (i % 3) * 0.2f;       // 1.0, 1.2, 1.4...
                float freqModPosY = 1.0f + ((i+1) % 3) * 0.15f;  // 1.15, 1.3, 1.0...
                
                // Unikátní fázový posun pro tuto čáru
                float timeOffset = i * 1.345f;
                
                // Aplikujeme modifikátory na základní frekvence
                double freqTimeY = baseFreqTimeY * freqModTimeY;
                double freqTimeX = baseFreqTimeX * freqModTimeX;
                double freqPosX = baseFreqPosX * freqModPosX;
                double freqPosY = baseFreqPosY * freqModPosY;

                // Variace pro každou čáru
                float thickness = std::max(0.5, m_settings.lightningThickness + (i - m_settings.lightningLines / 2.0) * 0.5 * resolutionScale);
                
                // Mírná variace barvy a alfa
                QColor color = m_settings.lightningColor;
                color.setHsv(color.hue() + (i - m_settings.lightningLines / 2.0) * 5, 
                            std::clamp(color.saturation() + i * 10, 0, 255), 
                            color.value());
                color.setAlphaF(std::clamp(0.4 + (rand() % 100) / 500.0, 0.2, 0.7));

                painter.setPen(QPen(color, thickness, Qt::SolidLine, Qt::RoundCap));
                
                QPainterPath path;
                
                // Počáteční bod
                // Použijeme JitterY pro mírnou vertikální variaci na začátku
                float y_start = y_base + sin(currentTime * (freqTimeY * 0.8) + timeOffset) * (m_settings.lightningJitterY * 0.5) * resolutionScale;
                path.moveTo(0, y_start);

                // Kreslení segmentů
                for (int seg = 1; seg <= numPoints; ++seg)
                {
                    float x = (float)seg / numPoints * size.width();
                    
                    // Deformace v Y (hlavní "blesk") - řízená novým parametrem lightningJitterY
                    float y_warp = sin(currentTime * freqTimeY + timeOffset + x * freqPosY) * m_settings.lightningJitterY * resolutionScale;
                    
                    // Menší deformace v X (křivení) - řízená novým parametrem lightningJitterX
                    float x_warp = cos(currentTime * freqTimeX + timeOffset + x * freqPosX) * m_settings.lightningJitterX * resolutionScale;
                    
                    path.lineTo(x + x_warp, y_base + y_warp);
                }
                painter.drawPath(path);
            }
            
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver); // Reset
        }
    }
}

void MediaRenderer::drawParticles(QPainter &painter, const std::vector<Particle> &particles, double resolutionScale)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const auto &particle : particles)
    {

        qreal opacity = particle.lifetime / particle.initial_lifetime;
        qreal scale = 1.0 - opacity; // 0 (start of life) -> 1 (end of life)

        // Select particle type
        if (m_settings.particleType == RenderSettings::Circle)
        {
            QColor particleColor = particle.color;
            particleColor.setAlphaF(opacity * 0.8);
            painter.setBrush(particleColor);
            painter.setPen(Qt::NoPen);

            // --- Size logic ---
            qreal radiusMultiplier = m_settings.particleStartSize + (m_settings.particleEndSize - m_settings.particleStartSize) * scale;
            qreal baseRadius = 10.0 * resolutionScale;
            qreal radius = baseRadius * radiusMultiplier;
            if (radius < 1.0)
                radius = 1.0; // Ensure minimum size

            painter.drawEllipse(particle.pos, radius, radius);
        }
        else // Resource or Custom
        {
            // --- Thread-safe pixmap caching ---
            QPixmap *pixmap = &m_resourceParticlePixmapCache;

            if (m_settings.particleType == RenderSettings::Custom && !m_settings.customParticleImage.isNull())
            {
                if (m_customParticlePixmapCache.isNull())
                {
                    m_customParticlePixmapCache = QPixmap::fromImage(m_settings.customParticleImage);
                }
                pixmap = &m_customParticlePixmapCache;
            }

            if (!pixmap || pixmap->isNull())
            {
                continue; // Skip drawing if pixmap isn't valid
            }

            // Set global opacity for fade-out
            painter.setOpacity(opacity);

            // --- Size logic ---
            qreal baseSize = 32.0 * resolutionScale; // Base size
            // Linear size interpolation
            qreal sizeMultiplier = m_settings.particleStartSize + (m_settings.particleEndSize - m_settings.particleStartSize) * scale;
            qreal size = baseSize * sizeMultiplier;
            if (size < 1.0)
                size = 1.0; // Minimum size

            QRectF targetRect(particle.pos.x() - size / 2, particle.pos.y() - size / 2, size, size);

            // =================================================================
            // ---          Tinting Logic (Destination In)                   ---
            // =================================================================

            if (m_settings.tintParticles)
            {
                // 1. Create a temporary buffer (off-screen)
                QSize bufferSize = targetRect.size().toSize();
                if (bufferSize.width() < 1 || bufferSize.height() < 1)
                {
                    continue;
                }

                // Use QImage for buffer to support alpha fill
                QImage bufferImage(bufferSize, QImage::Format_ARGB32);
                bufferImage.fill(Qt::transparent); // Start with a transparent canvas

                // 2. Create a painter for this buffer
                QPainter p(&bufferImage);
                p.setRenderHint(QPainter::Antialiasing, true);

                // 3. Draw the DESTINATION: a solid rectangle of the note color
                p.fillRect(bufferImage.rect(), particle.color);

                // 4. Set "Destination In" mode
                // (Keeps the Destination [color] only where the Source [pixmap] has alpha)
                p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

                // 5. Draw the SOURCE: our pixmap as a mask
                p.drawPixmap(bufferImage.rect(), *pixmap, pixmap->rect());

                // 6. End the buffer painter
                p.end();

                // 7. Draw the final (tinted) QImage onto the main canvas
                painter.drawImage(targetRect, bufferImage, bufferImage.rect());
            }
            else
            {
                // If tint is off, just draw the original pixmap
                painter.drawPixmap(targetRect, *pixmap, pixmap->rect());
            }

            painter.setOpacity(1.0); // Reset opacity for other drawing
        }
    }
}