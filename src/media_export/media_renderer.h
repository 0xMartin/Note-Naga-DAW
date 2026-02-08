#pragma once

#include <QImage>
#include <QColor>
#include <QPixmap>
#include <map>
#include <vector>
#include <note_naga_engine/core/types.h>

class NoteNagaArrangement;
class NoteNagaRuntimeData;

class MediaRenderer {
public:
    // Structure for particles (public so FrameState can use it)
    struct Particle {
        QPointF pos;
        QPointF vel;
        qreal lifetime;
        qreal initial_lifetime;
        QColor color;
    };
    
    // Structure to hold the state for a single frame
    struct FrameState {
        std::vector<Particle> particles;
        std::map<int, bool> activeNotes;
    };

    // Structure for all render settings
    struct RenderSettings {
        // General settings
        bool renderParticles = true;
        bool renderKeyboard = true;
        bool renderNotes = true;
        bool renderLightning = false;
        
        // Particle settings
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

        // Note appearance settings
        bool renderPianoGlow = true;
        double noteStartOpacity = 1.0; 
        double noteEndOpacity = 1.0; 

        // Background settings
        QColor backgroundColor = QColor(25, 25, 35);
        QImage backgroundImage;
        bool renderBgShake = false;
        double bgShakeIntensity = 5.0;

        // Lightning settings
        QColor lightningColor = QColor(100, 200, 255);
        double lightningThickness = 2.0;
        int lightningLines = 3;
        double lightningJitterY = 3.0; 
        double lightningJitterX = 2.0;
    };

    /**
     * @brief Construct renderer for a single MIDI sequence.
     */
    MediaRenderer(NoteNagaMidiSeq* sequence);
    
    /**
     * @brief Construct renderer for arrangement (multiple sequences via clips).
     * @param arrangement The arrangement containing tracks with clips.
     * @param runtimeData Runtime data for sequence lookup.
     */
    MediaRenderer(NoteNagaArrangement* arrangement, NoteNagaRuntimeData* runtimeData);
    
    /**
     * @brief Renders a frame (Stateful version for preview).
     * This version calculates its own simulation and stores its state.
     */
    QImage renderFrame(double currentTime, const QSize& size);

    /**
     * @brief Renders a frame (Stateless version for export).
     * This version receives a pre-calculated state.
     */
    QImage renderFrame(double currentTime, const QSize& size, const FrameState& state);

    /**
     * @brief Calculates the state for the next frame based on the previous one. (Stateless)
     * @param previousState The state of the previous frame.
     * @param currentTime The current absolute time.
     * @param deltaTime The time elapsed since the previous frame.
     * @return The pre-calculated state for the current frame.
     */
    FrameState calculateNextState(const FrameState& previousState, double currentTime, double deltaTime);
    
    /**
     * @brief Resets the internal simulation state (e.g., when scrubbing the timeline).
     */
    void resetSimulation();

    void setSecondsVisible(double seconds) { m_secondsVisible = seconds; }
    void setRenderSettings(const RenderSettings& settings);
    void prepareKeyboardLayout(const QSize& size);
    
private:
    // Structure for holding note info for rendering
    struct NoteInfo {
        int note_val;
        double start_time;
        double end_time;
        QColor color;
    };
    // Structure for holding key info
    struct KeyInfo {
        QRectF rect;
        bool is_white;
    };

    void prepareNoteData();
    void prepareNoteDataFromArrangement();
    
    // Simulation methods
    void updateParticles(double deltaTime, std::vector<Particle>& particles);
    void spawnParticles(const NoteInfo& note, double resolutionScale, std::vector<Particle>& particles);

    // Drawing methods
    void drawParticles(QPainter& painter, const std::vector<Particle>& particles, double resolutionScale);
    void drawNotesAndKeyboard(QPainter& painter, double currentTime, const QSize& size, const std::map<int, bool>& activeNotes);

    RenderSettings m_settings;
    NoteNagaMidiSeq* m_sequence;
    NoteNagaArrangement* m_arrangement;
    NoteNagaRuntimeData* m_runtimeData;

    std::vector<NoteInfo> m_notes;
    std::map<int, KeyInfo> m_keyboardLayout;
    QSize m_lastLayoutSize;
    double m_secondsVisible = 5.0;

    QPixmap m_resourceParticlePixmapCache; // Cache for default particle image
    QPixmap m_customParticlePixmapCache;   // Cache for custom particle image
    
    // --- Internal state for stateful rendering (preview) ---
    FrameState m_currentState;
    double m_lastFrameTime = -1.0;
};