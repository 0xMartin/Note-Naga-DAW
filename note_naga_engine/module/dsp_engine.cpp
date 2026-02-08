#include <note_naga_engine/module/dsp_engine.h>

#include <note_naga_engine/core/types.h>

#include <cmath>
#include <algorithm>
#include <cstring>
#include <set>
#include <map>

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
        if (playback_mode_ == PlaybackMode::Arrangement) {
            // In Arrangement mode, render based on arrangement tracks
            // Each arrangement track has its own volume/pan settings
            renderArrangementTracks(num_frames);
        } else {
            // In Sequence mode, only render the active sequence
            NoteNagaMidiSeq* activeSeq = runtime_data_->getActiveSequence();
            if (activeSeq) {
                for (NoteNagaTrack* track : activeSeq->getTracks()) {
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
    
    // Clear fade out tracking state
    synthFadeOutState_.clear();
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

void NoteNagaDSPEngine::renderArrangementTracks(size_t numFrames) {
    if (!runtime_data_) return;
    
    NoteNagaArrangement* arrangement = runtime_data_->getArrangement();
    if (!arrangement) return;
    
    int currentTick = runtime_data_->getCurrentArrangementTick();
    
    // Check if any arrangement track has solo enabled
    bool hasSoloTrack = false;
    for (size_t i = 0; i < arrangement->getTrackCount(); ++i) {
        NoteNagaArrangementTrack* arrTrack = arrangement->getTracks()[i];
        if (arrTrack && arrTrack->isSolo()) {
            hasSoloTrack = true;
            break;
        }
    }
    
    // Build a map of synth -> arrangement track based on ACTIVE clips at current tick
    // This ensures the synth uses the volume/pan of the track where the clip is currently playing
    std::map<INoteNagaSoftSynth*, NoteNagaArrangementTrack*> synthToArrTrack;
    std::map<INoteNagaSoftSynth*, const NN_MidiClip_t*> synthToClip;  // For fade in/out
    
    for (size_t trackIdx = 0; trackIdx < arrangement->getTrackCount(); ++trackIdx) {
        NoteNagaArrangementTrack* arrTrack = arrangement->getTracks()[trackIdx];
        if (!arrTrack) continue;
        
        // Skip muted tracks
        if (arrTrack->isMuted()) continue;
        if (hasSoloTrack && !arrTrack->isSolo()) continue;
        
        // Check only ACTIVE clips at current tick
        std::vector<const NN_MidiClip_t*> activeClips = arrTrack->getClipsAtTick(currentTick);
        for (const auto* clip : activeClips) {
            if (!clip || clip->muted) continue;
            
            NoteNagaMidiSeq* seq = runtime_data_->getSequenceById(clip->sequenceId);
            if (!seq) continue;
            
            for (NoteNagaTrack* midiTrack : seq->getTracks()) {
                if (!midiTrack || midiTrack->isTempoTrack()) continue;
                
                INoteNagaSoftSynth* softSynth = midiTrack->getSoftSynth();
                if (softSynth) {
                    // Override previous assignment - the currently active clip wins
                    synthToArrTrack[softSynth] = arrTrack;
                    synthToClip[softSynth] = clip;
                }
            }
        }
    }
    
    // Also collect all synths that may have notes still playing (from sequences)
    // even if no clip is currently active (for note release/sustain)
    std::set<INoteNagaSoftSynth*> allSynths;
    for (NoteNagaMidiSeq* seq : runtime_data_->getSequences()) {
        if (!seq) continue;
        for (NoteNagaTrack* midiTrack : seq->getTracks()) {
            if (!midiTrack || midiTrack->isTempoTrack()) continue;
            INoteNagaSoftSynth* softSynth = midiTrack->getSoftSynth();
            if (softSynth) {
                allSynths.insert(softSynth);
            }
        }
    }
    
    // Reset RMS for all arrangement tracks first
    for (size_t trackIdx = 0; trackIdx < arrangement->getTrackCount(); ++trackIdx) {
        NoteNagaArrangementTrack* arrTrack = arrangement->getTracks()[trackIdx];
        if (arrTrack) {
            arr_track_rms_values_[arrTrack] = {-100.0f, -100.0f};
        }
    }
    
    // Track RMS accumulation per arrangement track
    std::map<NoteNagaArrangementTrack*, std::pair<float, float>> arrTrackRmsSum;
    std::map<NoteNagaArrangementTrack*, int> arrTrackSampleCount;
    
    // Render all synths
    for (INoteNagaSoftSynth* synth : allSynths) {
        if (!synth) continue;
        
        // Find the arrangement track for this synth (if any)
        NoteNagaArrangementTrack* arrTrack = nullptr;
        const NN_MidiClip_t* activeClip = nullptr;
        auto it = synthToArrTrack.find(synth);
        if (it != synthToArrTrack.end()) {
            arrTrack = it->second;
        }
        auto clipIt = synthToClip.find(synth);
        if (clipIt != synthToClip.end()) {
            activeClip = clipIt->second;
        }
        
        // Default volume/pan if no arrangement track owns this synth
        float arrVolume = arrTrack ? arrTrack->getVolume() : 1.0f;
        float arrPan = arrTrack ? arrTrack->getPan() : 0.0f;
        
        // Check mute/solo state
        if (arrTrack) {
            if (arrTrack->isMuted()) continue;
            if (hasSoloTrack && !arrTrack->isSolo()) continue;
        }
        
        // Calculate pan gains (constant power)
        float panAngle = (arrPan + 1.0f) * 0.25f * 3.14159265f; // 0 to pi/2
        float panL = cosf(panAngle);
        float panR = sinf(panAngle);
        
        // Clear track buffers
        std::fill(track_left_.begin(), track_left_.begin() + numFrames, 0.0f);
        std::fill(track_right_.begin(), track_right_.begin() + numFrames, 0.0f);
        
        // Render synth directly (no MIDI track volume/pan - we apply arr track settings)
        synth->renderAudio(track_left_.data(), track_right_.data(), numFrames);
        
        // Apply synth-specific DSP blocks if DSP is enabled
        if (this->enable_dsp_) {
            auto dspIt = synth_dsp_blocks_.find(synth);
            if (dspIt != synth_dsp_blocks_.end()) {
                for (NoteNagaDSPBlockBase *block : dspIt->second) {
                    if (block->isActive()) {
                        block->process(track_left_.data(), track_right_.data(), numFrames);
                    }
                }
            }
        }
        
        // Apply arrangement track volume and pan, then add to mix
        // Pan crossfeed: pan affects how much of each channel goes to L/R output
        float sumL = 0.0f, sumR = 0.0f;
        
        // Calculate fade parameters for MIDI clip (if any active clip)
        int64_t clipFadeInSamples = 0;
        int64_t clipFadeOutSamples = 0;
        int64_t clipStartSample = 0;
        int64_t clipEndSample = 0;
        bool hasFadeOut = false;
        
        int tempo = runtime_data_->getTempo();
        int ppq = runtime_data_->getPPQ();
        
        if (activeClip && (activeClip->fadeInTicks > 0 || activeClip->fadeOutTicks > 0)) {
            clipFadeInSamples = tickToSamples(activeClip->fadeInTicks, tempo, ppq);
            clipFadeOutSamples = tickToSamples(activeClip->fadeOutTicks, tempo, ppq);
            clipStartSample = tickToSamples(activeClip->startTick, tempo, ppq);
            clipEndSample = tickToSamples(activeClip->startTick + activeClip->durationTicks, tempo, ppq);
            
            // Store fade out state for this synth so we can continue fading after clip ends
            if (activeClip->fadeOutTicks > 0) {
                synthFadeOutState_[synth] = {clipEndSample, clipFadeOutSamples};
            }
            hasFadeOut = clipFadeOutSamples > 0;
        } else if (!activeClip) {
            // No active clip - check if this synth was playing a clip with fade out
            auto fadeIt = synthFadeOutState_.find(synth);
            if (fadeIt != synthFadeOutState_.end()) {
                clipEndSample = fadeIt->second.first;
                clipFadeOutSamples = fadeIt->second.second;
                hasFadeOut = true;
            }
        }
        
        // Get current sample position for fade calculation
        int64_t currentSamplePos = audioSamplePosition_.load(std::memory_order_relaxed);
        
        for (size_t i = 0; i < numFrames; i++) {
            // Calculate fade gain for MIDI clip
            float fadeGain = 1.0f;
            int64_t absSamplePos = currentSamplePos + static_cast<int64_t>(i);
            
            if (activeClip && clipFadeInSamples > 0) {
                // Fade in (only when inside clip)
                if (absSamplePos < clipStartSample + clipFadeInSamples) {
                    int64_t fadePos = absSamplePos - clipStartSample;
                    fadeGain = static_cast<float>(fadePos) / static_cast<float>(clipFadeInSamples);
                    fadeGain = std::max(0.0f, std::min(1.0f, fadeGain));
                }
            }
            
            // Fade out - apply both during clip and after clip ends (for note release)
            if (hasFadeOut && clipFadeOutSamples > 0) {
                int64_t fadeOutStartSample = clipEndSample - clipFadeOutSamples;
                if (absSamplePos >= fadeOutStartSample) {
                    int64_t fadePos = clipEndSample - absSamplePos;
                    float fadeOutGain = static_cast<float>(fadePos) / static_cast<float>(clipFadeOutSamples);
                    // Clamp: after clip end, fadeOutGain becomes negative, so clamp to 0
                    fadeOutGain = std::max(0.0f, std::min(1.0f, fadeOutGain));
                    fadeGain *= fadeOutGain;
                }
            }
            
            // Apply volume first, then pan crossfeed for stereo
            float srcL = track_left_[i] * arrVolume * fadeGain;
            float srcR = track_right_[i] * arrVolume * fadeGain;
            
            // Pan crossfeed: at pan=0, L goes 100% left, R goes 100% right
            // At pan=-1, both go left; at pan=+1, both go right
            float sampleL = srcL * panL + srcR * (1.0f - panR);
            float sampleR = srcR * panR + srcL * (1.0f - panL);
            
            mix_left_[i] += sampleL;
            mix_right_[i] += sampleR;
            
            // Accumulate for RMS calculation
            sumL += sampleL * sampleL;
            sumR += sampleR * sampleR;
        }
        
        // Accumulate RMS for this arrangement track (if any)
        if (arrTrack) {
            arrTrackRmsSum[arrTrack].first += sumL;
            arrTrackRmsSum[arrTrack].second += sumR;
            arrTrackSampleCount[arrTrack] += static_cast<int>(numFrames);
        }
    }
    
    // Calculate and store RMS for each arrangement track
    for (const auto& [arrTrack, sums] : arrTrackRmsSum) {
        if (!arrTrack) continue;
        int count = arrTrackSampleCount[arrTrack];
        if (count > 0) {
            float rmsLeft = sqrtf(sums.first / count);
            float rmsRight = sqrtf(sums.second / count);
            float dbLeft = (rmsLeft > 0.0f) ? 20.0f * log10f(rmsLeft) : -100.0f;
            float dbRight = (rmsRight > 0.0f) ? 20.0f * log10f(rmsRight) : -100.0f;
            arr_track_rms_values_[arrTrack] = {dbLeft, dbRight};
        }
    }
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
            
            if (!resource || !resource->isLoaded()) continue;
            
            // Calculate clip's start and end in samples
            int64_t clipStartSample = tickToSamples(clip.startTick, tempo, ppq);
            int64_t clipDurationSamples = tickToSamples(clip.durationTicks, tempo, ppq);
            int64_t clipEndSample = clipStartSample + clipDurationSamples;
            
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
            // offsetTicks represents how much of the start is trimmed (in ticks)
            // offsetSamples is an additional sample-level offset
            int64_t offsetFromTicks = tickToSamples(clip.offsetTicks, tempo, ppq);
            int64_t resourceOffset = clip.offsetSamples + offsetFromTicks + (renderStart - clipStartSample);
            
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
            
            // Calculate fade in/out regions in samples
            int64_t fadeInSamples = tickToSamples(clip.fadeInTicks, tempo, ppq);
            int64_t fadeOutSamples = tickToSamples(clip.fadeOutTicks, tempo, ppq);
            int64_t fadeOutStartSample = clipEndSample - fadeOutSamples;
            
            // Apply clip gain, track volume, pan, and fade, then add to mix
            float combinedGain = clip.gain * trackVolume;
            float gainL = combinedGain * panL;
            float gainR = combinedGain * panR;
            
            for (int i = 0; i < gotSamples; ++i) {
                // Calculate absolute sample position for this sample
                int64_t absSamplePos = renderStart + i;
                
                // Calculate fade multiplier
                float fadeGain = 1.0f;
                
                // Fade in: from clipStart to clipStart + fadeInSamples
                if (fadeInSamples > 0 && absSamplePos < clipStartSample + fadeInSamples) {
                    int64_t fadePos = absSamplePos - clipStartSample;
                    fadeGain = static_cast<float>(fadePos) / static_cast<float>(fadeInSamples);
                    fadeGain = std::max(0.0f, std::min(1.0f, fadeGain));
                }
                
                // Fade out: from fadeOutStartSample to clipEndSample
                if (fadeOutSamples > 0 && absSamplePos >= fadeOutStartSample) {
                    int64_t fadePos = clipEndSample - absSamplePos;
                    float fadeOutGain = static_cast<float>(fadePos) / static_cast<float>(fadeOutSamples);
                    fadeOutGain = std::max(0.0f, std::min(1.0f, fadeOutGain));
                    fadeGain *= fadeOutGain;  // Combine with fade in if overlapping
                }
                
                float sampleL = clipLeft[i] * gainL * fadeGain;
                float sampleR = clipRight[i] * gainR * fadeGain;
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