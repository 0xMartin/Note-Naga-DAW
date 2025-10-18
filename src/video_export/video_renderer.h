#pragma once

#include <QImage>
#include <QColor>
#include <QPixmap>
#include <map>
#include <vector>
#include <note_naga_engine/core/types.h>

class VideoRenderer {
public:
    // Struktura pro všechna nastavení renderování
    struct RenderSettings {
       bool renderParticles = true;
        bool renderKeyboard = true;
        bool renderNotes = true;
        
        enum ParticleType { Resource, Circle, Custom };
        ParticleType particleType = Resource;
        
        QImage customParticleImage; 
        int particleCount = 15;
        double particleLifetime = 0.75; 
        double particleSpeed = 75.0; 
        double particleGravity = 200.0;
        bool tintParticles = true;
        double particleStartSize = 0.5;
        double particleEndSize = 1.0;

        QColor backgroundColor = QColor(25, 25, 35);
        QImage backgroundImage;
        bool renderPianoGlow = true;
        double noteStartOpacity = 1.0; 
        double noteEndOpacity = 1.0; 
    };

    VideoRenderer(NoteNagaMidiSeq* sequence);
    QImage renderFrame(double currentTime, const QSize& size);
    
    // Metoda pro nastavení vertikálního měřítka z UI
    void setSecondsVisible(double seconds) { m_secondsVisible = seconds; }
    
    // Metoda pro nastavení všech ostatních voleb
    void setRenderSettings(const RenderSettings& settings) { m_settings = settings; }

private:
    // Struktura pro uchování informací o notě pro renderování
    struct NoteInfo {
        int note_val;
        double start_time;
        double end_time;
        QColor color;
    };
    // Struktura pro uchování informací o klávese
    struct KeyInfo {
        QRectF rect;
        bool is_white;
    };
    // Struktura pro částice
    struct Particle {
        QPointF pos;
        QPointF vel;
        qreal lifetime;
        qreal initial_lifetime;
        QColor color;
    };

    void prepareNoteData();
    void prepareKeyboardLayout(const QSize& size);
    
    // Metody pro efekty
    void updateAndDrawParticles(double deltaTime, QPainter& painter, double resolutionScale);
    void spawnParticles(const NoteInfo& note, double resolutionScale);

    RenderSettings m_settings;
    NoteNagaMidiSeq* m_sequence;

    std::vector<NoteInfo> m_notes;
    std::map<int, KeyInfo> m_keyboardLayout;

    QSize m_lastLayoutSize;
    double m_secondsVisible = 5.0;

    QPixmap m_resourceParticlePixmapCache; // Cache pro výchozí obrázek
    QPixmap m_customParticlePixmapCache;   // Cache pro vlastní obrázek
    std::vector<Particle> m_particles;
    std::map<int, bool> m_activeNotesState;
    double m_lastFrameTime = -1.0;
};