#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/types.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

class NoteNagaSynthExternalMidi;

/**
 * @brief Routing configuration for a track to external MIDI
 */
struct NOTE_NAGA_ENGINE_API ExternalMidiRoutingConfig {
    std::string deviceName;  ///< MIDI output device name (empty = disabled)
    int channel = 1;         ///< MIDI channel (1-16)
    bool enabled = false;    ///< Whether external MIDI output is enabled
};

/**
 * @brief Manager for routing MIDI notes to external devices.
 * 
 * This class manages external MIDI device connections and routing
 * configurations for tracks. It allows sending notes to multiple
 * external MIDI devices based on per-track configuration.
 */
class NOTE_NAGA_ENGINE_API ExternalMidiRouter {
public:
    ExternalMidiRouter();
    ~ExternalMidiRouter();

    /**
     * @brief Get list of available MIDI output devices
     * @return Vector of device names
     */
    std::vector<std::string> getAvailableDevices();

    /**
     * @brief Refresh the list of available devices
     */
    void refreshDevices();

    /**
     * @brief Set routing configuration for a MIDI track
     * @param track Pointer to the track
     * @param config Routing configuration
     */
    void setTrackRouting(NoteNagaTrack* track, const ExternalMidiRoutingConfig& config);

    /**
     * @brief Get routing configuration for a MIDI track
     * @param track Pointer to the track
     * @return Routing configuration (default if not set)
     */
    ExternalMidiRoutingConfig getTrackRouting(NoteNagaTrack* track) const;

    /**
     * @brief Set routing configuration for an arrangement track
     * @param track Pointer to the arrangement track
     * @param config Routing configuration
     */
    void setArrangementTrackRouting(NoteNagaArrangementTrack* track, const ExternalMidiRoutingConfig& config);

    /**
     * @brief Get routing configuration for an arrangement track
     * @param track Pointer to the arrangement track
     * @return Routing configuration (default if not set)
     */
    ExternalMidiRoutingConfig getArrangementTrackRouting(NoteNagaArrangementTrack* track) const;

    /**
     * @brief Clear all routing configurations
     */
    void clearAllRouting();

    /**
     * @brief Send a note to external MIDI based on track routing
     * @param note The note to send
     * @param track The track the note belongs to (for routing lookup)
     */
    void playNote(const NN_Note_t& note, NoteNagaTrack* track);

    /**
     * @brief Stop a note on external MIDI based on track routing
     * @param note The note to stop
     * @param track The track the note belongs to (for routing lookup)
     */
    void stopNote(const NN_Note_t& note, NoteNagaTrack* track);

    /**
     * @brief Play a note using arrangement track routing
     * @param note The note to send
     * @param arrTrack The arrangement track for routing lookup
     */
    void playNoteForArrangement(const NN_Note_t& note, NoteNagaArrangementTrack* arrTrack);

    /**
     * @brief Stop a note using arrangement track routing
     * @param note The note to stop
     * @param arrTrack The arrangement track for routing lookup
     */
    void stopNoteForArrangement(const NN_Note_t& note, NoteNagaArrangementTrack* arrTrack);

    /**
     * @brief Stop all notes on all connected devices
     */
    void stopAllNotes();

    /**
     * @brief Check if any routing is enabled
     * @return True if at least one track has external routing enabled
     */
    bool hasActiveRouting() const;

    /**
     * @brief Connect to a specific MIDI device
     * @param deviceName Name of the MIDI device
     * @return True if connection was successful
     */
    bool connectDevice(const std::string& deviceName);

    /**
     * @brief Disconnect from a specific MIDI device
     * @param deviceName Name of the MIDI device
     */
    void disconnectDevice(const std::string& deviceName);

    /**
     * @brief Check if a device is currently connected
     * @param deviceName Name of the MIDI device
     * @return True if device is connected
     */
    bool isDeviceConnected(const std::string& deviceName) const;

    /**
     * @brief Get list of currently connected device names
     * @return Vector of connected device names
     */
    std::vector<std::string> getConnectedDevices() const;

private:
    /**
     * @brief Get or create external MIDI synthesizer for a device
     * @param deviceName Name of the MIDI device
     * @return Pointer to the synthesizer, or nullptr if connection failed
     */
    NoteNagaSynthExternalMidi* getOrCreateDevice(const std::string& deviceName);

    mutable std::mutex m_mutex;
    
    // Routing configurations
    std::map<NoteNagaTrack*, ExternalMidiRoutingConfig> m_trackRouting;
    std::map<NoteNagaArrangementTrack*, ExternalMidiRoutingConfig> m_arrangementTrackRouting;
    
    // Device connections (cached)
    std::map<std::string, std::unique_ptr<NoteNagaSynthExternalMidi>> m_devices;
    
    // Available devices cache
    std::vector<std::string> m_availableDevices;
};

