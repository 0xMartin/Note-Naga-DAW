#ifndef NOTE_NAGA_AUDIO_MANAGER_H
#define NOTE_NAGA_AUDIO_MANAGER_H

#include "../note_naga_api.h"
#include "audio_resource.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#ifndef QT_DEACTIVATED
#include <QObject>
#endif

/**
 * @brief Manages all audio resources in a project.
 * Handles loading, caching, and provides access to audio data.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaAudioManager : public QObject {
    Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaAudioManager {
#endif

public:
    /**
     * @brief Constructor.
     * @param sampleRate Target sample rate for all audio.
     */
    explicit NoteNagaAudioManager(int sampleRate = 44100);
    
    /**
     * @brief Destructor.
     */
    virtual ~NoteNagaAudioManager();
    
    /**
     * @brief Set the target sample rate.
     * @param sampleRate Sample rate in Hz.
     */
    void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }
    
    /**
     * @brief Get the target sample rate.
     */
    int getSampleRate() const { return sampleRate_; }
    
    /**
     * @brief Import an audio file and add it to the resource pool.
     * @param filePath Path to the audio file.
     * @return Pointer to the created resource, or nullptr on failure.
     */
    NoteNagaAudioResource* importAudio(const std::string& filePath);
    
    /**
     * @brief Remove an audio resource by ID.
     * @param resourceId Resource ID to remove.
     * @return True if removed.
     */
    bool removeAudioResource(int resourceId);
    
    /**
     * @brief Get an audio resource by ID.
     * @param resourceId Resource ID.
     * @return Pointer to resource, or nullptr if not found.
     */
    NoteNagaAudioResource* getResource(int resourceId);
    const NoteNagaAudioResource* getResource(int resourceId) const;
    
    /**
     * @brief Get an audio resource by file path.
     * @param filePath File path.
     * @return Pointer to resource, or nullptr if not found.
     */
    NoteNagaAudioResource* getResourceByPath(const std::string& filePath);
    
    /**
     * @brief Get all audio resources.
     */
    const std::vector<std::unique_ptr<NoteNagaAudioResource>>& getResources() const {
        return resources_;
    }
    
    /**
     * @brief Get all audio resources as raw pointers (for serialization).
     */
    std::vector<NoteNagaAudioResource*> getAllResources() const {
        std::vector<NoteNagaAudioResource*> result;
        result.reserve(resources_.size());
        for (const auto& r : resources_) {
            result.push_back(r.get());
        }
        return result;
    }
    
    /**
     * @brief Get number of loaded resources.
     */
    size_t getResourceCount() const { return resources_.size(); }
    
    /**
     * @brief Clear all resources.
     */
    void clear();
    
    /**
     * @brief Prepare streaming buffers for a given tick position.
     * Call this during playback to ensure audio data is ready.
     * @param tick Current arrangement tick.
     * @param ppq Pulses per quarter note.
     * @param tempo Tempo in microseconds per quarter note.
     */
    void prepareForPlayback(int64_t tick, int ppq, int tempo);
    
    /**
     * @brief Get the next resource ID.
     */
    int getNextResourceId() const { return nextResourceId_; }
    
    /**
     * @brief Set the next resource ID (for deserialization).
     */
    void setNextResourceId(int id) { nextResourceId_ = id; }
    
    /**
     * @brief Update a resource's ID and re-index in the lookup map.
     * Used during deserialization to restore original IDs.
     * @param resource Pointer to the resource.
     * @param newId New ID to assign.
     */
    void updateResourceId(NoteNagaAudioResource* resource, int newId);

private:
    int sampleRate_;
    int nextResourceId_ = 1;
    std::vector<std::unique_ptr<NoteNagaAudioResource>> resources_;
    std::unordered_map<int, NoteNagaAudioResource*> resourceById_;
    std::unordered_map<std::string, NoteNagaAudioResource*> resourceByPath_;

#ifndef QT_DEACTIVATED
Q_SIGNALS:
    void resourceAdded(NoteNagaAudioResource* resource);
    void resourceRemoved(int resourceId);
    void resourcesChanged();
#endif
};

#endif // NOTE_NAGA_AUDIO_MANAGER_H
