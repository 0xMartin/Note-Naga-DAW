#include "video_renderer.h"

#include <QPainter>
#include <cmath>
#include <QLinearGradient>

// Konstanty pro rozsah klaviatury
const int FIRST_MIDI_NOTE = 21; // A0
const int LAST_MIDI_NOTE = 108; // C8

VideoRenderer::VideoRenderer(NoteNagaMidiSeq* sequence) 
    : m_sequence(sequence), m_lastLayoutSize(0, 0) 
{
    prepareNoteData();
    // Načteme výchozí pixmapu
    m_resourceParticlePixmapCache.load(":/images/sparkle.png");
}

void VideoRenderer::prepareNoteData() {
    for (const auto& track : m_sequence->getTracks()) {
        QColor trackColor = track->getColor().toQColor();
        for (const auto& note : track->getNotes()) {
            if (note.start.has_value() && note.length.has_value()) {
                m_notes.push_back({
                    note.note,
                    nn_ticks_to_seconds(note.start.value(), m_sequence->getPPQ(), m_sequence->getTempo()),
                    nn_ticks_to_seconds(note.start.value() + note.length.value(), m_sequence->getPPQ(), m_sequence->getTempo()),
                    trackColor
                });
            }
        }
    }
}

void VideoRenderer::prepareKeyboardLayout(const QSize& size) {
    if (size == m_lastLayoutSize) return;

    m_keyboardLayout.clear();
    const float keyboardHeight = size.height() * 0.25f;

    std::vector<bool> is_white_key = {true, false, true, false, true, true, false, true, false, true, false, true};
    int white_key_count = 0;
    for (int i = FIRST_MIDI_NOTE; i <= LAST_MIDI_NOTE; ++i) {
        if (is_white_key[i % 12]) white_key_count++;
    }

    float white_key_width = (float)size.width() / white_key_count;
    float black_key_width = white_key_width * 0.6f;
    float black_key_height = keyboardHeight * 0.6f;

    int current_white_key = 0;
    for (int i = FIRST_MIDI_NOTE; i <= LAST_MIDI_NOTE; ++i) {
        if (is_white_key[i % 12]) {
            m_keyboardLayout[i] = {
                QRectF(current_white_key * white_key_width, size.height() - keyboardHeight, white_key_width, keyboardHeight),
                true
            };
            current_white_key++;
        }
    }
    for (int i = FIRST_MIDI_NOTE; i <= LAST_MIDI_NOTE; ++i) {
        if (!is_white_key[i % 12]) {
            float center_x = m_keyboardLayout[i - 1].rect.right();
            m_keyboardLayout[i] = {
                QRectF(center_x - black_key_width / 2, size.height() - keyboardHeight, black_key_width, black_key_height),
                false
            };
        }
    }
    m_lastLayoutSize = size;
}

// Metoda přijímá 'resolutionScale' 
void VideoRenderer::spawnParticles(const NoteInfo& note, double resolutionScale) {
    // Ověření, zda je nota v layoutu (prevence pádu)
    if (m_keyboardLayout.find(note.note_val) == m_keyboardLayout.end()) return;
    const KeyInfo& key = m_keyboardLayout.at(note.note_val);
    QPointF spawn_pos = QPointF(key.rect.center().x(), key.rect.top());

    // Použijeme nastavení z m_settings 
    int particle_count = m_settings.particleCount;
    
    for (int i = 0; i < particle_count; ++i) {
        // Mírná náhodnost úhlu
        double angle_deg = (rand() % 160) + 10; // Mezi 10 a 170 stupni (nahoru)
        double angle_rad = angle_deg * M_PI / 180.0;

        // Škálování rychlosti
        double speed_base = m_settings.particleSpeed * resolutionScale;
        double speed_rand = speed_base * 0.5;
        double speed = speed_base - (speed_rand / 2) + (rand() % (int)(speed_rand + 1)); // +1 pro zamezení dělení nulou

        // Nastavení délky života
        qreal lifetime = m_settings.particleLifetime + (rand() % 50) / 100.0 - 0.25; // +- 0.25s
        if (lifetime < 0.1) lifetime = 0.1;

        m_particles.push_back({
            spawn_pos,
            QPointF(cos(angle_rad) * speed, -sin(angle_rad) * speed),
            lifetime,
            lifetime,
            note.color
        });
    }
}

// Metoda přijímá 'resolutionScale'
void VideoRenderer::updateAndDrawParticles(double deltaTime, QPainter& painter, double resolutionScale) {
    
    // Pro kruhy a pixmapy
    painter.setRenderHint(QPainter::Antialiasing, true); 
    
    for (auto it = m_particles.begin(); it != m_particles.end(); ) {
        it->pos += it->vel * deltaTime;
        
        // Škálování gravitace
        it->vel.ry() += m_settings.particleGravity * resolutionScale * deltaTime; 
        it->lifetime -= deltaTime;

        if (it->lifetime <= 0) {
            it = m_particles.erase(it);
        } else {
            qreal opacity = it->lifetime / it->initial_lifetime;
            qreal scale = 1.0 - opacity; // 0 (start života) -> 1 (konec života)

            // Výběr typu částice
            if (m_settings.particleType == RenderSettings::Circle)
            {
                QColor particleColor = it->color;
                particleColor.setAlphaF(opacity * 0.8);
                painter.setBrush(particleColor);
                painter.setPen(Qt::NoPen);

                // --- ZMĚNĚNÁ LOGIKA VELIKOSTI ---
                qreal radiusMultiplier = m_settings.particleStartSize + (m_settings.particleEndSize - m_settings.particleStartSize) * scale;
                qreal baseRadius = 10.0 * resolutionScale; 
                qreal radius = baseRadius * radiusMultiplier;
                if (radius < 1.0) radius = 1.0; // Zajistíme minimální velikost
                
                painter.drawEllipse(it->pos, radius, radius);
            }
            else // Resource nebo Custom
            {
                // --- Oprava chyby vláken ---
                QPixmap* pixmap = &m_resourceParticlePixmapCache;

                if (m_settings.particleType == RenderSettings::Custom && !m_settings.customParticleImage.isNull()) {
                    if (m_customParticlePixmapCache.isNull() || m_customParticlePixmapCache.cacheKey() != m_settings.customParticleImage.cacheKey()) {
                         m_customParticlePixmapCache = QPixmap::fromImage(m_settings.customParticleImage);
                    }
                    pixmap = &m_customParticlePixmapCache;
                }

                if (!pixmap || pixmap->isNull()) {
                    ++it;
                    continue; // Přeskočíme kreslení, pokud pixmapa není platná
                }
                
                // Nastavíme celkovou průhlednost pro fade-out
                painter.setOpacity(opacity);

                // --- ZMĚNĚNÁ LOGIKA VELIKOSTI ---
                qreal baseSize = 32.0 * resolutionScale; // Základní velikost
                // Lineární interpolace velikosti
                qreal sizeMultiplier = m_settings.particleStartSize + (m_settings.particleEndSize - m_settings.particleStartSize) * scale;
                qreal size = baseSize * sizeMultiplier;
                if (size < 1.0) size = 1.0; // Minimální velikost
                
                QRectF targetRect(it->pos.x() - size/2, it->pos.y() - size/2, size, size);
                
                // =================================================================
                // ---          FINÁLNÍ OPRAVENÁ LOGIKA TINTU (DESTINATION IN)     ---
                // =================================================================
                
                if (m_settings.tintParticles) {
                    // 1. Vytvoříme dočasný buffer (mimo obrazovku)
                    // Musíme použít QImage pro podporu alfa kanálu při fill
                    QSize bufferSize = targetRect.size().toSize();
                    if (bufferSize.width() < 1 || bufferSize.height() < 1) {
                        ++it;
                        continue; 
                    }
                    
                    // Vytvoříme buffer jako QImage pro správnou práci s alfou
                    QImage bufferImage(bufferSize, QImage::Format_ARGB32);
                    bufferImage.fill(Qt::transparent); // Začneme s průhledným plátnem

                    // 2. Vytvoříme painter pro tento buffer
                    QPainter p(&bufferImage);
                    p.setRenderHint(QPainter::Antialiasing, true);

                    // 3. Vykreslíme CÍL (Destination): plný obdélník v barvě noty
                    p.fillRect(bufferImage.rect(), it->color); 

                    // 4. Nastavíme režim "Cíl Dovnitř Zdroje"
                    // (Zachová Cíl [barvu] pouze tam, kde Zdroj [pixmapa] má alfu)
                    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

                    // 5. Vykreslíme ZDROJ (Source): naši pixmapu jako masku
                    p.drawPixmap(bufferImage.rect(), *pixmap, pixmap->rect());
                    
                    // 6. Ukončíme painter bufferu
                    p.end();

                    // 7. Vykreslíme finální (zabarvený) QImage na hlavní plátno
                    painter.drawImage(targetRect, bufferImage, bufferImage.rect());

                } else {
                    // Pokud je tint vypnutý, kreslíme přímo originální pixmapu
                    painter.drawPixmap(targetRect, *pixmap, pixmap->rect());
                }
                
                painter.setOpacity(1.0); // Vrátíme opacitu pro další kreslení
            }
            
            ++it;
        }
    }
}

QImage VideoRenderer::renderFrame(double currentTime, const QSize& size) {
    prepareKeyboardLayout(size);

    QImage frame(size, QImage::Format_ARGB32);

    // --- Kreslení pozadí (Bod 1) ---
    if (!m_settings.backgroundImage.isNull()) {
        // Vykreslíme obrázek na pozadí (roztažený na celou plochu)
        QPainter bgPainter(&frame);
        bgPainter.drawImage(frame.rect(), m_settings.backgroundImage);
        bgPainter.end();
    } else {
        // Vyplníme barvou pozadí
        frame.fill(m_settings.backgroundColor);
    }
    // --- Konec pozadí ---

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing);

    double deltaTime = (m_lastFrameTime < 0) ? 0.0 : currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;

    double resolutionScale = (double)size.height() / 720.0;

    const float keyboardHeight = size.height() * 0.25f;
    const float render_area_height = size.height() - keyboardHeight;
    const float pixels_per_second = render_area_height / m_secondsVisible;

    std::map<int, bool> currentActiveNotes;

    // --- Kreslení padajících not (Bod 5) ---
    if (m_settings.renderNotes) {
        for (const auto& note : m_notes) {
            if (note.end_time < currentTime - 1.0 || note.start_time > currentTime + m_secondsVisible) continue;
            
            bool isActive = (currentTime >= note.start_time && currentTime < note.end_time);
            if (isActive) {
                currentActiveNotes[note.note_val] = true;
            }

            if (m_keyboardLayout.find(note.note_val) == m_keyboardLayout.end()) continue;
            const KeyInfo& key = m_keyboardLayout.at(note.note_val);
            
            float y_start = render_area_height - (float)(note.start_time - currentTime) * pixels_per_second;
            float y_end = render_area_height - (float)(note.end_time - currentTime) * pixels_per_second;
            QRectF note_rect(key.rect.x(), y_end, key.rect.width(), y_start - y_end);
            
            // Omezení kreslení na oblast nad klávesnicí
            note_rect = note_rect.intersected(QRectF(0, 0, size.width(), render_area_height));
            if (note_rect.isEmpty()) continue;

            QColor note_color = note.color;

            // --- Výpočet opacity noty (Bod 2) ---
            // 'progress' = 0.0 nahoře (y=0), 1.0 dole (y=render_area_height)
            float progress = std::clamp(y_start / render_area_height, 0.0f, 1.0f); 
            qreal opacity = m_settings.noteStartOpacity + (m_settings.noteEndOpacity - m_settings.noteStartOpacity) * progress;
            note_color.setAlphaF(std::clamp(opacity, 0.0, 1.0));
            // --- Konec výpočtu opacity ---


            if (isActive) {
                painter.setPen(Qt::NoPen);
                for (int i = 0; i < 10; ++i) {
                    QColor glow_color = note_color;
                    // Opacita záře je závislá na opacitě noty
                    glow_color.setAlphaF(note_color.alphaF() * 0.05); 
                    painter.setBrush(glow_color);
                    painter.drawRect(note_rect.adjusted(-i, -i, i, i));
                }
            }
            
            painter.setBrush(note_color);
            QColor penColor = note_color.darker(120);
            penColor.setAlphaF(note_color.alphaF()); // Pero má stejnou opacitu
            painter.setPen(penColor);
            painter.drawRect(note_rect);
        }
    } else {
         for (const auto& note : m_notes) {
            if (currentTime >= note.start_time && currentTime < note.end_time) {
                 currentActiveNotes[note.note_val] = true;
            }
         }
    }
    
    // ... (část pro spouštění částic beze změny) ...
    for(const auto& pair : currentActiveNotes) {
        if (m_activeNotesState.find(pair.first) == m_activeNotesState.end() || !m_activeNotesState[pair.first]) 
        {
            for(const auto& noteInfo : m_notes) {
                if(noteInfo.note_val == pair.first) {
                     if (std::abs(noteInfo.start_time - currentTime) < 0.05) {
                        if (m_settings.renderParticles) {
                             if (m_keyboardLayout.find(noteInfo.note_val) != m_keyboardLayout.end()) {
                                spawnParticles(noteInfo, resolutionScale);
                             }
                        }
                        break;
                     }
                }
            }
        }
    }
    m_activeNotesState = currentActiveNotes;

    // --- Kreslení klávesnice ---
    if (m_settings.renderKeyboard) {
        // ... (kreslení bílých a černých kláves beze změny) ...
        for (const auto& pair : m_keyboardLayout) {
            if (pair.second.is_white) {
                bool isActive = (currentActiveNotes.find(pair.first) != currentActiveNotes.end());
                if (isActive) {
                    QColor activeColor = QColor(150, 150, 255);
                    for (const auto& note : m_notes) {
                        if (note.note_val == pair.first && currentTime >= note.start_time && currentTime < note.end_time) {
                            activeColor = note.color;
                            break;
                        }
                    }
                    painter.setBrush(activeColor.lighter(120));
                    painter.setPen(activeColor);
                } else {
                    painter.setBrush(QColor(245, 245, 245));
                    painter.setPen(QColor(200, 200, 200));
                }
                painter.drawRect(pair.second.rect);
            }
        }
        for (const auto& pair : m_keyboardLayout) {
            if (!pair.second.is_white) {
                 bool isActive = (currentActiveNotes.find(pair.first) != currentActiveNotes.end());
                 if (isActive) {
                    QColor activeColor = QColor(150, 150, 255);
                    for (const auto& note : m_notes) {
                        if (note.note_val == pair.first && currentTime >= note.start_time && currentTime < note.end_time) {
                            activeColor = note.color;
                            break;
                        }
                    }
                    painter.setBrush(activeColor.lighter(120));
                    painter.setPen(activeColor);
                 } else {
                    painter.setBrush(QColor(40, 40, 40)); 
                    painter.setPen(Qt::black);
                 }
                 painter.drawRect(pair.second.rect);
            }
        }

        // --- Kreslení "Piano Glow" efektu (Bod 3) ---
        if (m_settings.renderPianoGlow) {
            float glowHeight = 25.0f * resolutionScale; // Výška záře
            QRectF glowRect(0, render_area_height - glowHeight, size.width(), glowHeight);
            
            QLinearGradient glow(0, render_area_height - glowHeight, 0, render_area_height);
            glow.setColorAt(0.0, QColor(255, 255, 255, 0));
            glow.setColorAt(0.8, QColor(255, 255, 255, 70));
            glow.setColorAt(1.0, QColor(255, 255, 255, 100));
            
            painter.setCompositionMode(QPainter::CompositionMode_Plus); // Aditivní míchání
            painter.fillRect(glowRect, glow);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver); // Vrátit zpět
        }
        // --- Konec "Piano Glow" ---
    }

    // --- Kreslení částic (Bod 5) ---
    if (m_settings.renderParticles) {
        updateAndDrawParticles(deltaTime, painter, resolutionScale);
    }

    painter.end();
    return frame;
}