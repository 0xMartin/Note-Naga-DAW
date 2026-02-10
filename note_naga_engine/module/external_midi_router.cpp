#include <note_naga_engine/module/external_midi_router.h>
#include <note_naga_engine/synth/synth_external_midi.h>
#include <note_naga_engine/logger.h>

ExternalMidiRouter::ExternalMidiRouter()
{
    refreshDevices();
}

ExternalMidiRouter::~ExternalMidiRouter()
{
    stopAllNotes();
}

std::vector<std::string> ExternalMidiRouter::getAvailableDevices()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_availableDevices;
}

void ExternalMidiRouter::refreshDevices()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_availableDevices = NoteNagaSynthExternalMidi::getAvailableMidiOutputPorts();
}

void ExternalMidiRouter::setTrackRouting(NoteNagaTrack* track, const ExternalMidiRoutingConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (track) {
        m_trackRouting[track] = config;
    }
}

ExternalMidiRoutingConfig ExternalMidiRouter::getTrackRouting(NoteNagaTrack* track) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_trackRouting.find(track);
    if (it != m_trackRouting.end()) {
        return it->second;
    }
    return ExternalMidiRoutingConfig();
}

void ExternalMidiRouter::setArrangementTrackRouting(NoteNagaArrangementTrack* track, const ExternalMidiRoutingConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (track) {
        m_arrangementTrackRouting[track] = config;
    }
}

ExternalMidiRoutingConfig ExternalMidiRouter::getArrangementTrackRouting(NoteNagaArrangementTrack* track) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_arrangementTrackRouting.find(track);
    if (it != m_arrangementTrackRouting.end()) {
        return it->second;
    }
    return ExternalMidiRoutingConfig();
}

void ExternalMidiRouter::clearAllRouting()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_trackRouting.clear();
    m_arrangementTrackRouting.clear();
}

void ExternalMidiRouter::playNote(const NN_Note_t& note, NoteNagaTrack* track)
{
    if (!track) return;
    
    ExternalMidiRoutingConfig config;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_trackRouting.find(track);
        if (it == m_trackRouting.end() || !it->second.enabled || it->second.deviceName.empty()) {
            return;
        }
        config = it->second;
    }
    
    NoteNagaSynthExternalMidi* device = getOrCreateDevice(config.deviceName);
    if (device) {
        // Channel is 0-indexed in MIDI, but we store 1-16
        device->playNote(note, config.channel - 1, 0.0f);
    }
}

void ExternalMidiRouter::stopNote(const NN_Note_t& note, NoteNagaTrack* track)
{
    if (!track) return;
    
    ExternalMidiRoutingConfig config;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_trackRouting.find(track);
        if (it == m_trackRouting.end() || !it->second.enabled || it->second.deviceName.empty()) {
            return;
        }
        config = it->second;
    }
    
    NoteNagaSynthExternalMidi* device = getOrCreateDevice(config.deviceName);
    if (device) {
        device->stopNote(note);
    }
}

void ExternalMidiRouter::playNoteForArrangement(const NN_Note_t& note, NoteNagaArrangementTrack* arrTrack)
{
    if (!arrTrack) return;
    
    ExternalMidiRoutingConfig config;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_arrangementTrackRouting.find(arrTrack);
        if (it == m_arrangementTrackRouting.end() || !it->second.enabled || it->second.deviceName.empty()) {
            return;
        }
        config = it->second;
    }
    
    NoteNagaSynthExternalMidi* device = getOrCreateDevice(config.deviceName);
    if (device) {
        // Channel is 0-indexed in MIDI, but we store 1-16
        device->playNote(note, config.channel - 1, 0.0f);
    }
}

void ExternalMidiRouter::stopNoteForArrangement(const NN_Note_t& note, NoteNagaArrangementTrack* arrTrack)
{
    if (!arrTrack) return;
    
    ExternalMidiRoutingConfig config;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_arrangementTrackRouting.find(arrTrack);
        if (it == m_arrangementTrackRouting.end() || !it->second.enabled || it->second.deviceName.empty()) {
            return;
        }
        config = it->second;
    }
    
    NoteNagaSynthExternalMidi* device = getOrCreateDevice(config.deviceName);
    if (device) {
        device->stopNote(note);
    }
}

void ExternalMidiRouter::stopAllNotes()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [name, device] : m_devices) {
        if (device) {
            device->stopAllNotes();
        }
    }
}

bool ExternalMidiRouter::hasActiveRouting() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [track, config] : m_trackRouting) {
        if (config.enabled && !config.deviceName.empty()) {
            return true;
        }
    }
    
    for (const auto& [track, config] : m_arrangementTrackRouting) {
        if (config.enabled && !config.deviceName.empty()) {
            return true;
        }
    }
    
    return false;
}

NoteNagaSynthExternalMidi* ExternalMidiRouter::getOrCreateDevice(const std::string& deviceName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_devices.find(deviceName);
    if (it != m_devices.end()) {
        return it->second.get();
    }
    
    // Create new device connection
    auto device = std::make_unique<NoteNagaSynthExternalMidi>("ExternalMIDI_" + deviceName, deviceName);
    NoteNagaSynthExternalMidi* rawPtr = device.get();
    m_devices[deviceName] = std::move(device);
    
    NOTE_NAGA_LOG_INFO("Created external MIDI connection to: " + deviceName);
    
    return rawPtr;
}

bool ExternalMidiRouter::connectDevice(const std::string& deviceName)
{
    if (deviceName.empty()) return false;
    
    NoteNagaSynthExternalMidi* device = getOrCreateDevice(deviceName);
    if (device) {
        NOTE_NAGA_LOG_INFO("Connected to external MIDI device: " + deviceName);
        return true;
    }
    return false;
}

void ExternalMidiRouter::disconnectDevice(const std::string& deviceName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_devices.find(deviceName);
    if (it != m_devices.end()) {
        if (it->second) {
            it->second->stopAllNotes();
        }
        m_devices.erase(it);
        NOTE_NAGA_LOG_INFO("Disconnected from external MIDI device: " + deviceName);
    }
}

bool ExternalMidiRouter::isDeviceConnected(const std::string& deviceName) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices.find(deviceName) != m_devices.end();
}

std::vector<std::string> ExternalMidiRouter::getConnectedDevices() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> connected;
    for (const auto& [name, device] : m_devices) {
        if (device) {
            connected.push_back(name);
        }
    }
    return connected;
}

