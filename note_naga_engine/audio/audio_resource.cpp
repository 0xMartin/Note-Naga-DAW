#include "note_naga_engine/audio/audio_resource.h"
#include "note_naga_engine/logger.h"

#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <filesystem>

// WAV file header structures
#pragma pack(push, 1)
struct WavHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // File size - 8
    char wave[4];           // "WAVE"
};

struct WavChunkHeader {
    char id[4];
    uint32_t size;
};

struct WavFmtChunk {
    uint16_t audioFormat;   // 1 = PCM, 3 = IEEE float
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

NoteNagaAudioResource::NoteNagaAudioResource(const std::string &filePath)
    : filePath_(filePath)
{
    // Extract filename from path
    std::filesystem::path p(filePath);
    fileName_ = p.filename().string();
}

NoteNagaAudioResource::~NoteNagaAudioResource()
{
    // Stop streaming thread
    if (loadThreadRunning_) {
        loadThreadRunning_ = false;
        loadCondition_.notify_all();
        if (loadThread_.joinable()) {
            loadThread_.join();
        }
    }
}

bool NoteNagaAudioResource::load(int targetSampleRate)
{
    sampleRate_ = targetSampleRate;
    
    if (!loadWavFile(targetSampleRate)) {
        return false;
    }
    
    generateWaveformPeaks();
    loaded_ = true;
    
    NOTE_NAGA_LOG_INFO("Loaded audio resource: " + fileName_ + 
                        " (" + std::to_string(totalSamples_) + " samples, " +
                        std::to_string(durationSeconds_) + "s)");
    
    return true;
}

bool NoteNagaAudioResource::loadWavFile(int targetSampleRate)
{
    std::ifstream file(filePath_, std::ios::binary);
    if (!file.is_open()) {
        hasError_ = true;
        errorMessage_ = "Cannot open file: " + filePath_;
        NOTE_NAGA_LOG_ERROR(errorMessage_);
        return false;
    }
    
    // Read RIFF header
    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
        std::strncmp(header.wave, "WAVE", 4) != 0) {
        hasError_ = true;
        errorMessage_ = "Not a valid WAV file: " + filePath_;
        NOTE_NAGA_LOG_ERROR(errorMessage_);
        return false;
    }
    
    // Find fmt and data chunks
    WavFmtChunk fmt = {};
    std::vector<uint8_t> rawData;
    bool foundFmt = false;
    bool foundData = false;
    
    while (!file.eof() && (!foundFmt || !foundData)) {
        WavChunkHeader chunk;
        file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));
        
        if (file.eof()) break;
        
        if (std::strncmp(chunk.id, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&fmt), std::min((uint32_t)sizeof(fmt), chunk.size));
            // Skip any extra format bytes
            if (chunk.size > sizeof(fmt)) {
                file.seekg(chunk.size - sizeof(fmt), std::ios::cur);
            }
            foundFmt = true;
        }
        else if (std::strncmp(chunk.id, "data", 4) == 0) {
            rawData.resize(chunk.size);
            file.read(reinterpret_cast<char*>(rawData.data()), chunk.size);
            foundData = true;
        }
        else {
            // Skip unknown chunks
            file.seekg(chunk.size, std::ios::cur);
        }
    }
    
    if (!foundFmt || !foundData) {
        hasError_ = true;
        errorMessage_ = "Invalid WAV file structure: " + filePath_;
        NOTE_NAGA_LOG_ERROR(errorMessage_);
        return false;
    }
    
    // Validate format
    if (fmt.audioFormat != 1 && fmt.audioFormat != 3) {
        hasError_ = true;
        errorMessage_ = "Unsupported WAV format (only PCM and IEEE float): " + filePath_;
        NOTE_NAGA_LOG_ERROR(errorMessage_);
        return false;
    }
    
    if (fmt.bitsPerSample != 16 && fmt.bitsPerSample != 24 && 
        fmt.bitsPerSample != 32 && fmt.bitsPerSample != 8) {
        hasError_ = true;
        errorMessage_ = "Unsupported bit depth: " + std::to_string(fmt.bitsPerSample);
        NOTE_NAGA_LOG_ERROR(errorMessage_);
        return false;
    }
    
    originalSampleRate_ = fmt.sampleRate;
    originalChannels_ = fmt.numChannels;
    
    // Calculate number of samples
    int bytesPerSample = fmt.bitsPerSample / 8;
    int64_t numSamples = rawData.size() / (bytesPerSample * fmt.numChannels);
    originalTotalSamples_ = numSamples;
    
    // Convert raw data to float samples
    std::vector<float> leftChannel(numSamples);
    std::vector<float> rightChannel(numSamples);
    
    for (int64_t i = 0; i < numSamples; ++i) {
        float left = 0.0f, right = 0.0f;
        
        for (int ch = 0; ch < fmt.numChannels; ++ch) {
            float sample = 0.0f;
            int64_t offset = i * fmt.numChannels * bytesPerSample + ch * bytesPerSample;
            
            if (fmt.audioFormat == 1) { // PCM
                if (fmt.bitsPerSample == 8) {
                    uint8_t val = rawData[offset];
                    sample = (val - 128) / 128.0f;
                }
                else if (fmt.bitsPerSample == 16) {
                    int16_t val = *reinterpret_cast<int16_t*>(&rawData[offset]);
                    sample = val / 32768.0f;
                }
                else if (fmt.bitsPerSample == 24) {
                    int32_t val = (rawData[offset] | (rawData[offset+1] << 8) | 
                                   (rawData[offset+2] << 16));
                    if (val & 0x800000) val |= 0xFF000000; // Sign extend
                    sample = val / 8388608.0f;
                }
                else if (fmt.bitsPerSample == 32) {
                    int32_t val = *reinterpret_cast<int32_t*>(&rawData[offset]);
                    sample = val / 2147483648.0f;
                }
            }
            else if (fmt.audioFormat == 3) { // IEEE float
                if (fmt.bitsPerSample == 32) {
                    sample = *reinterpret_cast<float*>(&rawData[offset]);
                }
            }
            
            if (ch == 0) left = sample;
            else if (ch == 1) right = sample;
        }
        
        // Convert mono to stereo
        if (fmt.numChannels == 1) {
            right = left;
        }
        
        leftChannel[i] = left;
        rightChannel[i] = right;
    }
    
    channels_ = 2; // Always output stereo
    
    // Resample if needed
    if (originalSampleRate_ != targetSampleRate) {
        NOTE_NAGA_LOG_INFO("Resampling from " + std::to_string(originalSampleRate_) + 
                           " to " + std::to_string(targetSampleRate));
        
        std::vector<float> resampledLeft, resampledRight;
        resampleAudio(leftChannel, resampledLeft, originalSampleRate_, targetSampleRate);
        resampleAudio(rightChannel, resampledRight, originalSampleRate_, targetSampleRate);
        
        fullAudioLeft_ = std::move(resampledLeft);
        fullAudioRight_ = std::move(resampledRight);
    }
    else {
        fullAudioLeft_ = std::move(leftChannel);
        fullAudioRight_ = std::move(rightChannel);
    }
    
    totalSamples_ = fullAudioLeft_.size();
    durationSeconds_ = static_cast<double>(totalSamples_) / sampleRate_;
    
    // Decide whether to use full cache or streaming
    // Use full cache for files < 30 seconds (at 44100 Hz stereo = ~10 MB)
    constexpr double MAX_CACHE_SECONDS = 30.0;
    useFullAudioCache_ = (durationSeconds_ <= MAX_CACHE_SECONDS);
    
    if (!useFullAudioCache_) {
        // For large files, we keep the full audio in memory but use streaming buffer
        // for the actual playback to reduce memory bandwidth
        // In a production system, we'd read from disk in chunks
        
        int bufferSize = BUFFER_SECONDS * sampleRate_;
        streamBufferLeft_.resize(bufferSize);
        streamBufferRight_.resize(bufferSize);
        
        // Start streaming thread
        loadThreadRunning_ = true;
        loadThread_ = std::thread(&NoteNagaAudioResource::streamingThreadFunc, this);
    }
    
    return true;
}

void NoteNagaAudioResource::resampleAudio(const std::vector<float>& input, 
                                           std::vector<float>& output,
                                           int inputRate, int outputRate)
{
    if (inputRate == outputRate) {
        output = input;
        return;
    }
    
    double ratio = static_cast<double>(outputRate) / inputRate;
    int64_t outputSize = static_cast<int64_t>(input.size() * ratio);
    output.resize(outputSize);
    
    // Linear interpolation resampling
    for (int64_t i = 0; i < outputSize; ++i) {
        double srcPos = i / ratio;
        int64_t srcIndex = static_cast<int64_t>(srcPos);
        double frac = srcPos - srcIndex;
        
        if (srcIndex + 1 < static_cast<int64_t>(input.size())) {
            output[i] = static_cast<float>(input[srcIndex] * (1.0 - frac) + 
                                           input[srcIndex + 1] * frac);
        }
        else if (srcIndex < static_cast<int64_t>(input.size())) {
            output[i] = input[srcIndex];
        }
        else {
            output[i] = 0.0f;
        }
    }
}

void NoteNagaAudioResource::generateWaveformPeaks()
{
    int64_t numPeaks = (totalSamples_ + samplesPerPeak_ - 1) / samplesPerPeak_;
    waveformPeaks_.resize(numPeaks);
    
    for (int64_t p = 0; p < numPeaks; ++p) {
        int64_t startSample = p * samplesPerPeak_;
        int64_t endSample = std::min(startSample + samplesPerPeak_, totalSamples_);
        
        float minL = 0.0f, maxL = 0.0f;
        float minR = 0.0f, maxR = 0.0f;
        
        for (int64_t s = startSample; s < endSample; ++s) {
            float left = fullAudioLeft_[s];
            float right = fullAudioRight_[s];
            
            minL = std::min(minL, left);
            maxL = std::max(maxL, left);
            minR = std::min(minR, right);
            maxR = std::max(maxR, right);
        }
        
        waveformPeaks_[p] = {minL, maxL, minR, maxR};
    }
}

int NoteNagaAudioResource::getSamples(int64_t startSample, int numSamples,
                                       float* outLeft, float* outRight)
{
    if (!loaded_ || startSample < 0) return 0;
    
    int samplesRead = 0;
    
    if (useFullAudioCache_) {
        // Direct read from full cache
        for (int i = 0; i < numSamples; ++i) {
            int64_t idx = startSample + i;
            if (idx >= totalSamples_) break;
            
            outLeft[i] = fullAudioLeft_[idx];
            outRight[i] = fullAudioRight_[idx];
            samplesRead++;
        }
    }
    else {
        // Read from streaming buffer
        std::lock_guard<std::mutex> lock(bufferMutex_);
        
        for (int i = 0; i < numSamples; ++i) {
            int64_t idx = startSample + i;
            if (idx >= totalSamples_) break;
            
            // Check if sample is in buffer
            if (idx >= bufferStartSample_ && idx < bufferEndSample_) {
                int bufIdx = static_cast<int>(idx - bufferStartSample_);
                outLeft[i] = streamBufferLeft_[bufIdx];
                outRight[i] = streamBufferRight_[bufIdx];
                samplesRead++;
            }
            else {
                // Sample not in buffer - read from full cache as fallback
                outLeft[i] = fullAudioLeft_[idx];
                outRight[i] = fullAudioRight_[idx];
                samplesRead++;
            }
        }
    }
    
    return samplesRead;
}

void NoteNagaAudioResource::prepareForPosition(int64_t startSample)
{
    if (useFullAudioCache_) return;
    
    requestedPosition_ = startSample;
    loadCondition_.notify_one();
}

void NoteNagaAudioResource::streamingThreadFunc()
{
    while (loadThreadRunning_) {
        std::unique_lock<std::mutex> lock(loadMutex_);
        loadCondition_.wait_for(lock, std::chrono::milliseconds(100));
        
        if (!loadThreadRunning_) break;
        
        int64_t reqPos = requestedPosition_.load();
        int bufferSize = static_cast<int>(streamBufferLeft_.size());
        
        // Check if we need to update buffer
        if (reqPos < bufferStartSample_ || reqPos >= bufferEndSample_ - bufferSize / 2) {
            loadBufferRange(reqPos, reqPos + bufferSize);
        }
    }
}

void NoteNagaAudioResource::loadBufferRange(int64_t startSample, int64_t endSample)
{
    startSample = std::max(int64_t(0), startSample);
    endSample = std::min(endSample, totalSamples_);
    
    int bufferSize = static_cast<int>(streamBufferLeft_.size());
    
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    for (int64_t i = startSample; i < endSample && (i - startSample) < bufferSize; ++i) {
        int bufIdx = static_cast<int>(i - startSample);
        streamBufferLeft_[bufIdx] = fullAudioLeft_[i];
        streamBufferRight_[bufIdx] = fullAudioRight_[i];
    }
    
    bufferStartSample_ = startSample;
    bufferEndSample_ = std::min(startSample + bufferSize, totalSamples_);
}
