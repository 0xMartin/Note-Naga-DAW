#include "note_naga_engine/audio/audio_manager.h"
#include "note_naga_engine/logger.h"
#include "note_naga_engine/nn_utils.h"

NoteNagaAudioManager::NoteNagaAudioManager(int sampleRate)
    : sampleRate_(sampleRate)
{
}

NoteNagaAudioManager::~NoteNagaAudioManager()
{
    clear();
}

NoteNagaAudioResource* NoteNagaAudioManager::importAudio(const std::string& filePath)
{
    // Check if already loaded
    auto existingIt = resourceByPath_.find(filePath);
    if (existingIt != resourceByPath_.end()) {
        NOTE_NAGA_LOG_INFO("Audio already loaded: " + filePath);
        return existingIt->second;
    }
    
    // Create new resource
    auto resource = std::make_unique<NoteNagaAudioResource>(filePath);
    resource->setId(nextResourceId_++);
    
    // Load the audio
    if (!resource->load(sampleRate_)) {
        NOTE_NAGA_LOG_ERROR("Failed to load audio: " + filePath);
        return nullptr;
    }
    
    NoteNagaAudioResource* rawPtr = resource.get();
    
    // Add to containers
    resourceById_[rawPtr->getId()] = rawPtr;
    resourceByPath_[filePath] = rawPtr;
    resources_.push_back(std::move(resource));
    
    NOTE_NAGA_LOG_INFO("Imported audio resource ID " + std::to_string(rawPtr->getId()) + 
                        ": " + rawPtr->getFileName());
    
    NN_QT_EMIT(resourceAdded(rawPtr));
    NN_QT_EMIT(resourcesChanged());
    
    return rawPtr;
}

bool NoteNagaAudioManager::removeAudioResource(int resourceId)
{
    auto it = resourceById_.find(resourceId);
    if (it == resourceById_.end()) {
        return false;
    }
    
    NoteNagaAudioResource* resource = it->second;
    std::string filePath = resource->getFilePath();
    
    // Remove from maps
    resourceById_.erase(it);
    resourceByPath_.erase(filePath);
    
    // Remove from vector
    for (auto vecIt = resources_.begin(); vecIt != resources_.end(); ++vecIt) {
        if (vecIt->get() == resource) {
            resources_.erase(vecIt);
            break;
        }
    }
    
    NOTE_NAGA_LOG_INFO("Removed audio resource ID " + std::to_string(resourceId));
    
    NN_QT_EMIT(resourceRemoved(resourceId));
    NN_QT_EMIT(resourcesChanged());
    
    return true;
}

NoteNagaAudioResource* NoteNagaAudioManager::getResource(int resourceId)
{
    auto it = resourceById_.find(resourceId);
    return (it != resourceById_.end()) ? it->second : nullptr;
}

const NoteNagaAudioResource* NoteNagaAudioManager::getResource(int resourceId) const
{
    auto it = resourceById_.find(resourceId);
    return (it != resourceById_.end()) ? it->second : nullptr;
}

NoteNagaAudioResource* NoteNagaAudioManager::getResourceByPath(const std::string& filePath)
{
    auto it = resourceByPath_.find(filePath);
    return (it != resourceByPath_.end()) ? it->second : nullptr;
}

void NoteNagaAudioManager::updateResourceId(NoteNagaAudioResource* resource, int newId)
{
    if (!resource) return;
    
    int oldId = resource->getId();
    if (oldId == newId) return;
    
    // Remove from map with old ID
    resourceById_.erase(oldId);
    
    // Update resource ID
    resource->setId(newId);
    
    // Add to map with new ID
    resourceById_[newId] = resource;
    
    // Update nextResourceId_ if necessary
    if (newId >= nextResourceId_) {
        nextResourceId_ = newId + 1;
    }
}

void NoteNagaAudioManager::clear()
{
    resources_.clear();
    resourceById_.clear();
    resourceByPath_.clear();
    nextResourceId_ = 1;
    
    NN_QT_EMIT(resourcesChanged());
}

void NoteNagaAudioManager::prepareForPlayback(int64_t tick, int ppq, int tempo)
{
    // Calculate sample position for each resource based on tick
    // This is a placeholder - actual implementation needs clip info
    double usPerTick = static_cast<double>(tempo) / ppq;
    double seconds = (tick * usPerTick) / 1'000'000.0;
    int64_t samplePos = static_cast<int64_t>(seconds * sampleRate_);
    
    for (auto& resource : resources_) {
        resource->prepareForPosition(samplePos);
    }
}
