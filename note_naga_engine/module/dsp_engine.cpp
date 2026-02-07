#include <note_naga_engine/module/dsp_engine.h>

#include <note_naga_engine/core/types.h>

#include <cmath>
#include <algorithm>
#include <cstring>

NoteNagaDSPEngine::NoteNagaDSPEngine(NoteNagaMetronome* metronome, NoteNagaSpectrumAnalyzer * spectrum_analyzer, NoteNagaPanAnalyzer* pan_analyzer) {
    this->metronome_ = metronome;
    this->spectrum_analyzer_ = spectrum_analyzer;
    this->pan_analyzer_ = pan_analyzer;
    this->enable_dsp_ = true;
    NOTE_NAGA_LOG_INFO("DSP Engine initialized");
}

void NoteNagaDSPEngine::render(float *output, size_t num_frames, bool compute_rms) {
    // Prepare mix buffers
    if (mix_left_.size() < num_frames) mix_left_.resize(num_frames, 0.0f);
    if (mix_right_.size() < num_frames) mix_right_.resize(num_frames, 0.0f);
    if (temp_left_.size() < num_frames) temp_left_.resize(num_frames, 0.0f);
    if (temp_right_.size() < num_frames) temp_right_.resize(num_frames, 0.0f);
    if (track_left_.size() < num_frames) track_left_.resize(num_frames, 0.0f);
    if (track_right_.size() < num_frames) track_right_.resize(num_frames, 0.0f);
    
    std::fill(mix_left_.begin(), mix_left_.begin() + num_frames, 0.0f);
    std::fill(mix_right_.begin(), mix_right_.begin() + num_frames, 0.0f);

    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    
    // Render audio from tracks based on playback mode
    if (runtime_data_) {
        std::vector<NoteNagaMidiSeq*> sequencesToRender;
        
        if (playback_mode_ == PlaybackMode::Arrangement) {
            // In Arrangement mode, render ALL sequences
            sequencesToRender = runtime_data_->getSequences();
        } else {
            // In Sequence mode, only render the active sequence
            NoteNagaMidiSeq* activeSeq = runtime_data_->getActiveSequence();
            if (activeSeq) {
                sequencesToRender.push_back(activeSeq);
            }
        }
        
        for (NoteNagaMidiSeq* seq : sequencesToRender) {
            if (!seq) continue;
            
            for (NoteNagaTrack* track : seq->getTracks()) {
                if (!track || track->isMuted() || track->isTempoTrack()) continue;
                
                INoteNagaSoftSynth* softSynth = track->getSoftSynth();
                if (!softSynth) continue;
                
                // Clear track buffers
                std::fill(track_left_.begin(), track_left_.begin() + num_frames, 0.0f);
                std::fill(track_right_.begin(), track_right_.begin() + num_frames, 0.0f);
                
                // Render this track (applies its own volume internally)
                track->renderAudio(track_left_.data(), track_right_.data(), num_frames);
                
                // Apply track's synth DSP blocks if DSP is enabled
                if (this->enable_dsp_) {
                    auto it = synth_dsp_blocks_.find(softSynth);
                    if (it != synth_dsp_blocks_.end()) {
                        for (NoteNagaDSPBlockBase *block : it->second) {
                            if (block->isActive()) {
                                block->process(track_left_.data(), track_right_.data(), num_frames);
                            }
                        }
                    }
                }
                
                // Calculate and store per-track RMS
                track_rms_values_[track] = calculateTrackRMS(track_left_.data(), track_right_.data(), num_frames);
                
                // Add to mix buffers
                for (size_t i = 0; i < num_frames; i++) {
                    mix_left_[i] += track_left_[i];
                    mix_right_[i] += track_right_[i];
                }
            }
        }
    }
    
    // Also render global synths (for backwards compatibility / master synth)
    for (INoteNagaSoftSynth *synth : this->synths_) {
        // Clear temporary buffers
        std::fill(temp_left_.begin(), temp_left_.begin() + num_frames, 0.0f);
        std::fill(temp_right_.begin(), temp_right_.begin() + num_frames, 0.0f);
        
        // Render this synth to temporary buffers
        synth->renderAudio(temp_left_.data(), temp_right_.data(), num_frames);
        
        // Apply synth-specific DSP blocks if DSP is enabled
        if (this->enable_dsp_) {
            auto it = synth_dsp_blocks_.find(synth);
            if (it != synth_dsp_blocks_.end()) {
                for (NoteNagaDSPBlockBase *block : it->second) {
                    if (block->isActive()) {
                        block->process(temp_left_.data(), temp_right_.data(), num_frames);
                    }
                }
            }
        }
        
        // Add to mix buffers
        for (size_t i = 0; i < num_frames; i++) {
            mix_left_[i] += temp_left_[i];
            mix_right_[i] += temp_right_[i];
        }
    }

    // Render audio clips from arrangement tracks (in Arrangement mode)
    renderAudioClips(num_frames);

    // Master DSP blocks processing
    if (this->enable_dsp_) {
        for (NoteNagaDSPBlockBase *block : this->dsp_blocks_) {
            if (block->isActive()) {
                block->process(mix_left_.data(), mix_right_.data(), num_frames);
            }
        }
    }

    // Metronome rendering
    if (this->metronome_) {
        this->metronome_->render(mix_left_.data(), mix_right_.data(), num_frames);
    }

    // apply master volume with logarithmic effect
    if (output_volume_ < 1.0f) {
        // Use a simple logarithmic curve for perceptual loudness
        float log_volume = powf(output_volume_, 2.0f); // or use another exponent for desired curve
        for (size_t i = 0; i < num_frames; ++i) {
            mix_left_[i] *= log_volume;
            mix_right_[i] *= log_volume;
        }
    }

    // Calculate RMS for visualization
    if (compute_rms) {
        this->calculateRMS(mix_left_.data(), mix_right_.data(), num_frames);
    } else {
        this->last_rms_left_ = -100.0f;
        this->last_rms_right_ = -100.0f;
    }

    // push to spectrum analyzer
    if (this->spectrum_analyzer_) {
        this->spectrum_analyzer_->pushSamplesToLeftBuffer(mix_left_.data(), num_frames);
        this->spectrum_analyzer_->pushSamplesToRightBuffer(mix_right_.data(), num_frames);
    }

    // push to pan analyzer
    if (this->pan_analyzer_) {
        this->pan_analyzer_->pushSamplesToLeftBuffer(mix_left_.data(), num_frames);
        this->pan_analyzer_->pushSamplesToRightBuffer(mix_right_.data(), num_frames);
    }

    // Interleave left and right channels using pointer arithmetic for efficiency
    float *left = mix_left_.data();
    float *right = mix_right_.data();
    float *out = output;
    size_t i = 0;
    size_t n = num_frames;
    // Unroll loop for better performance
    for (; i + 3 < n; i += 4) {
        out[0] = left[i];
        out[1] = right[i];
        out[2] = left[i + 1];
        out[3] = right[i + 1];
        out[4] = left[i + 2];
        out[5] = right[i + 2];
        out[6] = left[i + 3];
        out[7] = right[i + 3];
        out += 8;
    }
    // Handle remaining frames
    for (; i < n; ++i) {
        *out++ = left[i];
        *out++ = right[i];
    }
}

void NoteNagaDSPEngine::setEnableDSP(bool enable) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    this->enable_dsp_ = enable;
}

void NoteNagaDSPEngine::addSynth(INoteNagaSoftSynth *synth) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    synths_.push_back(synth);
}

void NoteNagaDSPEngine::removeSynth(INoteNagaSoftSynth *synth) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    synths_.erase(std::remove(synths_.begin(), synths_.end(), synth), synths_.end());
    
    // Also remove any DSP blocks for this synth
    synth_dsp_blocks_.erase(synth);
}

void NoteNagaDSPEngine::addDSPBlock(NoteNagaDSPBlockBase *block) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    dsp_blocks_.push_back(block);
}

void NoteNagaDSPEngine::removeDSPBlock(NoteNagaDSPBlockBase *block) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    dsp_blocks_.erase(std::remove(dsp_blocks_.begin(), dsp_blocks_.end(), block),
                      dsp_blocks_.end());
}

void NoteNagaDSPEngine::reorderDSPBlock(int from_idx, int to_idx) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    if (from_idx < 0 || from_idx >= int(dsp_blocks_.size()) || to_idx < 0 ||
        to_idx >= int(dsp_blocks_.size()) || from_idx == to_idx)
        return;
    auto it_from = dsp_blocks_.begin() + from_idx;
    auto block = *it_from;
    dsp_blocks_.erase(it_from);
    dsp_blocks_.insert(dsp_blocks_.begin() + to_idx, block);
}

void NoteNagaDSPEngine::addSynthDSPBlock(INoteNagaSoftSynth *synth, NoteNagaDSPBlockBase *block) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    synth_dsp_blocks_[synth].push_back(block);
}

void NoteNagaDSPEngine::removeSynthDSPBlock(INoteNagaSoftSynth *synth, NoteNagaDSPBlockBase *block) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    auto it = synth_dsp_blocks_.find(synth);
    if (it != synth_dsp_blocks_.end()) {
        auto &blocks = it->second;
        blocks.erase(std::remove(blocks.begin(), blocks.end(), block), blocks.end());
    }
}

void NoteNagaDSPEngine::reorderSynthDSPBlock(INoteNagaSoftSynth *synth, int from_idx, int to_idx) {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    auto it = synth_dsp_blocks_.find(synth);
    if (it == synth_dsp_blocks_.end()) return;
    
    auto &blocks = it->second;
    if (from_idx < 0 || from_idx >= int(blocks.size()) || to_idx < 0 ||
        to_idx >= int(blocks.size()) || from_idx == to_idx)
        return;
        
    auto it_from = blocks.begin() + from_idx;
    auto block = *it_from;
    blocks.erase(it_from);
    blocks.insert(blocks.begin() + to_idx, block);
}

std::vector<NoteNagaDSPBlockBase*> NoteNagaDSPEngine::getSynthDSPBlocks(INoteNagaSoftSynth *synth) const {
    auto it = synth_dsp_blocks_.find(synth);
    if (it != synth_dsp_blocks_.end()) {
        return it->second;
    }
    return {};
}

void NoteNagaDSPEngine::setOutputVolume(float volume) {
    // Ensure volume is within [0.0, 1.0] range
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    this->output_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

std::pair<float, float> NoteNagaDSPEngine::getCurrentVolumeDb() const {
    return {last_rms_left_, last_rms_right_};
}

void NoteNagaDSPEngine::calculateRMS(float *left, float *right, size_t numFrames) {
    // Výpočet RMS pro left/right
    double sum_left = 0.0, sum_right = 0.0;
    for (size_t i = 0; i < numFrames; ++i) {
        sum_left += left[i] * left[i];
        sum_right += right[i] * right[i];
    }
    float rms_left = sqrt(sum_left / numFrames);
    float rms_right = sqrt(sum_right / numFrames);

    // Konverze na dBFS (0 dB = max, typicky -60 až 0)
    this->last_rms_left_ = (rms_left > 0.000001f) ? 20.0f * log10(rms_left) : -100.0f;
    this->last_rms_right_ = (rms_right > 0.000001f) ? 20.0f * log10(rms_right) : -100.0f;
}

std::pair<float, float> NoteNagaDSPEngine::calculateTrackRMS(float *left, float *right, size_t numFrames) {
    double sum_left = 0.0, sum_right = 0.0;
    for (size_t i = 0; i < numFrames; ++i) {
        sum_left += left[i] * left[i];
        sum_right += right[i] * right[i];
    }
    float rms_left = sqrt(sum_left / numFrames);
    float rms_right = sqrt(sum_right / numFrames);
    
    float db_left = (rms_left > 0.000001f) ? 20.0f * log10(rms_left) : -100.0f;
    float db_right = (rms_right > 0.000001f) ? 20.0f * log10(rms_right) : -100.0f;
    return {db_left, db_right};
}

std::pair<float, float> NoteNagaDSPEngine::getTrackVolumeDb(NoteNagaTrack* track) const {
    auto it = track_rms_values_.find(track);
    if (it != track_rms_values_.end()) {
        return it->second;
    }
    return {-100.0f, -100.0f};
}

std::pair<float, float> NoteNagaDSPEngine::getArrangementTrackVolumeDb(NoteNagaArrangementTrack* track) const {
    auto it = arr_track_rms_values_.find(track);
    if (it != arr_track_rms_values_.end()) {
        return it->second;
    }
    return {-100.0f, -100.0f};
}

void NoteNagaDSPEngine::resetAllBlocks() {
    std::lock_guard<std::mutex> lock(dsp_engine_mutex_);
    
    // Reset master DSP blocks
    for (NoteNagaDSPBlockBase *block : dsp_blocks_) {
        if (block) block->resetState();
    }
    
    // Reset synth-specific DSP blocks
    for (auto &pair : synth_dsp_blocks_) {
        for (NoteNagaDSPBlockBase *block : pair.second) {
            if (block) block->resetState();
        }
    }
    
    // Reset audio sample position
    audioSamplePosition_.store(0, std::memory_order_relaxed);
}

int64_t NoteNagaDSPEngine::tickToSamples(int tick, int tempo, int ppq) const {
    // tempo is in microseconds per quarter note
    // samples = ticks * (samples_per_quarter_note)
    // samples_per_quarter_note = sampleRate * (tempo_us / 1000000)
    double usPerTick = static_cast<double>(tempo) / ppq;
    double secondsPerTick = usPerTick / 1000000.0;
    return static_cast<int64_t>(tick * secondsPerTick * sampleRate_);
}

int NoteNagaDSPEngine::sampleToTicks(int64_t sample, int tempo, int ppq) const {
    double usPerTick = static_cast<double>(tempo) / ppq;
    double secondsPerTick = usPerTick / 1000000.0;
    double samplesPerTick = secondsPerTick * sampleRate_;
    return static_cast<int>(sample / samplesPerTick);
}

void NoteNagaDSPEngine::renderAudioClips(size_t numFrames) {
    if (!runtime_data_ || playback_mode_ != PlaybackMode::Arrangement) {
        return;
    }
    
    // Don't render or advance if playback is not active
    if (!audioPlaybackActive_.load(std::memory_order_relaxed)) {
        return;
    }
    
    NoteNagaArrangement* arrangement = runtime_data_->getArrangement();
    if (!arrangement) {
        return;
    }
    
    // Get current sample position and advance it for next callback
    int64_t currentSamplePos = audioSamplePosition_.fetch_add(static_cast<int64_t>(numFrames), 
                                                               std::memory_order_relaxed);
    
    // Get tempo and PPQ for tick-to-sample conversion
    // Use getTempo() which gets tempo from active sequence, not getProjectTempo()
    int tempo = runtime_data_->getTempo();
    int ppq = runtime_data_->getPPQ();
    
    // Fallback to default tempo if not set (120 BPM = 500000 us per quarter)
    if (tempo <= 0) {
        tempo = 500000;
    }
    
    NoteNagaAudioManager& audioManager = runtime_data_->getAudioManager();
    
    // DEBUG: Log basic info periodically
    static int debugCounter = 0;
    int totalAudioClips = 0;
    for (size_t i = 0; i < arrangement->getTrackCount(); ++i) {
        if (arrangement->getTracks()[i]) {
            totalAudioClips += static_cast<int>(arrangement->getTracks()[i]->getAudioClips().size());
        }
    }
    if (++debugCounter % 500 == 1) {
        NOTE_NAGA_LOG_INFO("[AudioClip DEBUG] samplePos=" + std::to_string(currentSamplePos) +
                          " tempo=" + std::to_string(tempo) +
                          " ppq=" + std::to_string(ppq) +
                          " tracks=" + std::to_string(arrangement->getTrackCount()) +
                          " totalAudioClips=" + std::to_string(totalAudioClips) +
                          " resourceCount=" + std::to_string(audioManager.getResourceCount()));
    }
    
    // Check if any track has solo enabled
    bool hasSoloTrack = false;
    for (size_t i = 0; i < arrangement->getTrackCount(); ++i) {
        NoteNagaArrangementTrack* track = arrangement->getTracks()[i];
        if (track && track->isSolo()) {
            hasSoloTrack = true;
            break;
        }
    }
    
    // Ensure audio clip buffer is sized
    if (audioClipBuffer_.size() < numFrames * 2) {
        audioClipBuffer_.resize(numFrames * 2);
    }
    
    // Iterate all arrangement tracks
    for (size_t trackIdx = 0; trackIdx < arrangement->getTrackCount(); ++trackIdx) {
        NoteNagaArrangementTrack* arrTrack = arrangement->getTracks()[trackIdx];
        if (!arrTrack) continue;
        
        // Track audio accumulation for RMS calculation
        float trackSumLeft = 0.0f;
        float trackSumRight = 0.0f;
        int trackSampleCount = 0;
        
        if (arrTrack->isMuted() || (hasSoloTrack && !arrTrack->isSolo())) {
            // Reset RMS for muted/non-solo tracks
            arr_track_rms_values_[arrTrack] = {-100.0f, -100.0f};
            continue;
        }
        
        // Get track's volume and pan
        float trackVolume = arrTrack->getVolume();
        float trackPan = arrTrack->getPan();
        
        // Calculate pan gains (constant power)
        float panAngle = (trackPan + 1.0f) * 0.25f * 3.14159265f; // 0 to pi/2
        float panL = cosf(panAngle);
        float panR = sinf(panAngle);
        
        // Iterate audio clips on this track
        for (const auto& clip : arrTrack->getAudioClips()) {
            if (clip.muted) continue;
            
            // Get audio resource
            NoteNagaAudioResource* resource = audioManager.getResource(clip.audioResourceId);
            
            // DEBUG: Log clip info
            if (debugCounter % 500 == 1) {
                NOTE_NAGA_LOG_INFO("[AudioClip DEBUG] clip id=" + std::to_string(clip.id) +
                                  " resourceId=" + std::to_string(clip.audioResourceId) +
                                  " startTick=" + std::to_string(clip.startTick) +
                                  " durationTicks=" + std::to_string(clip.durationTicks) +
                                  " resource=" + (resource ? "found" : "NULL") +
                                  " loaded=" + (resource && resource->isLoaded() ? "yes" : "no"));
            }
            
            if (!resource || !resource->isLoaded()) continue;
            
            // Calculate clip's start and end in samples
            int64_t clipStartSample = tickToSamples(clip.startTick, tempo, ppq);
            int64_t clipDurationSamples = tickToSamples(clip.durationTicks, tempo, ppq);
            int64_t clipEndSample = clipStartSample + clipDurationSamples;
            
            // DEBUG: Log sample positions
            if (debugCounter % 500 == 1) {
                NOTE_NAGA_LOG_INFO("[AudioClip DEBUG] clipStartSample=" + std::to_string(clipStartSample) +
                                  " clipEndSample=" + std::to_string(clipEndSample) +
                                  " currentSamplePos=" + std::to_string(currentSamplePos) +
                                  " inRange=" + ((currentSamplePos >= clipStartSample && currentSamplePos < clipEndSample) ? "YES" : "NO"));
            }
            
            // Skip if completely outside current render window
            if (currentSamplePos + static_cast<int64_t>(numFrames) <= clipStartSample ||
                currentSamplePos >= clipEndSample) {
                continue;
            }
            
            // Calculate overlap with current render window
            int64_t renderStart = std::max(currentSamplePos, clipStartSample);
            int64_t renderEnd = std::min(currentSamplePos + static_cast<int64_t>(numFrames), clipEndSample);
            int64_t samplesToRender = renderEnd - renderStart;
            
            if (samplesToRender <= 0) continue;
            
            // Calculate offset in the output buffer
            size_t bufferOffset = static_cast<size_t>(renderStart - currentSamplePos);
            
            // Calculate position in the audio resource
            int64_t resourceOffset = clip.offsetSamples + (renderStart - clipStartSample);
            
            // Handle looping
            if (clip.looping) {
                int64_t resourceLength = resource->getTotalSamples();
                if (resourceLength > 0) {
                    resourceOffset = resourceOffset % resourceLength;
                }
            }
            
            // Prepare separate L/R buffers for audio clip samples
            if (audioClipBuffer_.size() < numFrames * 2) {
                audioClipBuffer_.resize(numFrames * 2);
            }
            float* clipLeft = audioClipBuffer_.data();
            float* clipRight = audioClipBuffer_.data() + numFrames;
            std::fill(clipLeft, clipLeft + samplesToRender, 0.0f);
            std::fill(clipRight, clipRight + samplesToRender, 0.0f);
            
            // Get samples from resource (returns number of samples read)
            int gotSamples = resource->getSamples(resourceOffset, static_cast<int>(samplesToRender), 
                                                   clipLeft, clipRight);
            
            // DEBUG: Log rendering info
            if (debugCounter % 500 == 1 || gotSamples > 0) {
                static int renderLogCounter = 0;
                if (++renderLogCounter % 500 == 1) {
                    NOTE_NAGA_LOG_INFO("[AudioClip DEBUG] RENDERING! resourceOffset=" + std::to_string(resourceOffset) +
                                      " samplesToRender=" + std::to_string(samplesToRender) +
                                      " gotSamples=" + std::to_string(gotSamples) +
                                      " bufferOffset=" + std::to_string(bufferOffset));
                }
            }
            
            // Apply clip gain, track volume, and pan, then add to mix
            float combinedGain = clip.gain * trackVolume;
            float gainL = combinedGain * panL;
            float gainR = combinedGain * panR;
            
            for (int i = 0; i < gotSamples; ++i) {
                float sampleL = clipLeft[i] * gainL;
                float sampleR = clipRight[i] * gainR;
                mix_left_[bufferOffset + i] += sampleL;
                mix_right_[bufferOffset + i] += sampleR;
                
                // Accumulate for RMS calculation
                trackSumLeft += sampleL * sampleL;
                trackSumRight += sampleR * sampleR;
            }
            trackSampleCount += gotSamples;
        }
        
        // Calculate and store RMS for this arrangement track
        if (trackSampleCount > 0) {
            float rmsLeft = sqrtf(trackSumLeft / trackSampleCount);
            float rmsRight = sqrtf(trackSumRight / trackSampleCount);
            float dbLeft = (rmsLeft > 0.000001f) ? 20.0f * log10f(rmsLeft) : -100.0f;
            float dbRight = (rmsRight > 0.000001f) ? 20.0f * log10f(rmsRight) : -100.0f;
            arr_track_rms_values_[arrTrack] = {dbLeft, dbRight};
        } else {
            // No audio rendered - decay RMS
            auto& rms = arr_track_rms_values_[arrTrack];
            rms.first = std::max(-100.0f, rms.first - 1.0f);
            rms.second = std::max(-100.0f, rms.second - 1.0f);
        }
    }
}