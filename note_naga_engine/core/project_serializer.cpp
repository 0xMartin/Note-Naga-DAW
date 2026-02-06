#include <note_naga_engine/core/project_serializer.h>
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/dsp/dsp_factory.h>
#include <note_naga_engine/synth/synth_fluidsynth.h>
#include <note_naga_engine/synth/synth_external_midi.h>
#include <note_naga_engine/logger.h>

#include <fstream>
#include <cstring>

NoteNagaProjectSerializer::NoteNagaProjectSerializer(NoteNagaEngine *engine)
    : m_engine(engine)
{
}

/*******************************************************************************************************/
// Binary I/O helpers
/*******************************************************************************************************/

void NoteNagaProjectSerializer::writeString(std::ofstream &out, const std::string &str) {
    uint32_t len = static_cast<uint32_t>(str.size());
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        out.write(str.data(), len);
    }
}

std::string NoteNagaProjectSerializer::readString(std::ifstream &in) {
    uint32_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (len == 0 || len > 1000000) return "";  // Sanity check
    std::string str(len, '\0');
    in.read(&str[0], len);
    return str;
}

void NoteNagaProjectSerializer::writeInt32(std::ofstream &out, int32_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

int32_t NoteNagaProjectSerializer::readInt32(std::ifstream &in) {
    int32_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

void NoteNagaProjectSerializer::writeInt64(std::ofstream &out, int64_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

int64_t NoteNagaProjectSerializer::readInt64(std::ifstream &in) {
    int64_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

void NoteNagaProjectSerializer::writeUInt64(std::ofstream &out, uint64_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

uint64_t NoteNagaProjectSerializer::readUInt64(std::ifstream &in) {
    uint64_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

void NoteNagaProjectSerializer::writeFloat(std::ofstream &out, float value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

float NoteNagaProjectSerializer::readFloat(std::ifstream &in) {
    float value = 0.0f;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

void NoteNagaProjectSerializer::writeBool(std::ofstream &out, bool value) {
    uint8_t byte = value ? 1 : 0;
    out.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
}

bool NoteNagaProjectSerializer::readBool(std::ifstream &in) {
    uint8_t byte = 0;
    in.read(reinterpret_cast<char*>(&byte), sizeof(byte));
    return byte != 0;
}

void NoteNagaProjectSerializer::writeUInt8(std::ofstream &out, uint8_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

uint8_t NoteNagaProjectSerializer::readUInt8(std::ifstream &in) {
    uint8_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

/*******************************************************************************************************/
// Metadata serialization
/*******************************************************************************************************/

void NoteNagaProjectSerializer::serializeMetadata(std::ofstream &out, const NoteNagaProjectMetadata &metadata) {
    writeString(out, metadata.name);
    writeString(out, metadata.author);
    writeString(out, metadata.description);
    writeString(out, metadata.copyright);
    writeInt64(out, metadata.createdAt);
    writeInt64(out, NoteNagaProjectMetadata::currentTimestamp());  // modifiedAt = now
    writeInt32(out, metadata.projectVersion);
}

bool NoteNagaProjectSerializer::deserializeMetadata(std::ifstream &in, NoteNagaProjectMetadata &metadata) {
    metadata.name = readString(in);
    metadata.author = readString(in);
    metadata.description = readString(in);
    metadata.copyright = readString(in);
    metadata.createdAt = readInt64(in);
    metadata.modifiedAt = readInt64(in);
    metadata.projectVersion = readInt32(in);
    return in.good();
}

/*******************************************************************************************************/
// Save Project
/*******************************************************************************************************/

bool NoteNagaProjectSerializer::saveProject(const std::string &filePath, const NoteNagaProjectMetadata &metadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        m_lastError = "Cannot open file for writing: " + filePath;
        return false;
    }

    // Write magic number and version
    writeInt32(file, static_cast<int32_t>(NNPROJ_MAGIC));
    writeInt32(file, static_cast<int32_t>(NNPROJ_VERSION));
    
    // Write metadata
    serializeMetadata(file, metadata);
    
    // Write MIDI sequences
    NoteNagaRuntimeData *runtime = m_engine->getRuntimeData();
    if (runtime) {
        std::vector<NoteNagaMidiSeq*> sequences = runtime->getSequences();
        writeInt32(file, static_cast<int32_t>(sequences.size()));
        for (NoteNagaMidiSeq *seq : sequences) {
            if (seq) {
                serializeSequence(file, seq);
            }
        }
    } else {
        writeInt32(file, 0);  // No sequences
    }
    
    // Write master DSP blocks
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    if (dspEngine) {
        std::vector<NoteNagaDSPBlockBase*> blocks = dspEngine->getDSPBlocks();
        writeInt32(file, static_cast<int32_t>(blocks.size()));
        for (NoteNagaDSPBlockBase *block : blocks) {
            if (block) {
                serializeDSPBlock(file, block);
            }
        }
        writeBool(file, dspEngine->isDSPEnabled());
    } else {
        writeInt32(file, 0);
        writeBool(file, true);
    }
    
    // Write Arrangement (v6+)
    if (runtime && runtime->getArrangement()) {
        serializeArrangement(file, runtime->getArrangement());
    } else {
        writeInt32(file, 0);  // No arrangement tracks
    }
    
    file.close();
    NOTE_NAGA_LOG_INFO("Project saved: " + filePath);
    return true;
}

/*******************************************************************************************************/
// Load Project
/*******************************************************************************************************/

bool NoteNagaProjectSerializer::loadProject(const std::string &filePath, NoteNagaProjectMetadata &outMetadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        m_lastError = "Cannot open file: " + filePath;
        return false;
    }
    
    // Read and verify magic number
    uint32_t magic = static_cast<uint32_t>(readInt32(file));
    if (magic != NNPROJ_MAGIC) {
        m_lastError = "Invalid file format (bad magic number)";
        return false;
    }
    
    // Read version - support loading older versions
    uint32_t version = static_cast<uint32_t>(readInt32(file));
    if (version > NNPROJ_VERSION) {
        m_lastError = "Project was created with a newer version (" + std::to_string(version) + "). Please update the application.";
        return false;
    }
    if (version < 3) {
        m_lastError = "Project version too old (" + std::to_string(version) + "). Versions before 3 are not supported.";
        return false;
    }
    m_loadingVersion = version;  // Store for use in deserialize methods
    
    // Read metadata
    if (!deserializeMetadata(file, outMetadata)) {
        m_lastError = "Failed to read metadata";
        return false;
    }
    
    // Clear existing project data
    NoteNagaRuntimeData *runtime = m_engine->getRuntimeData();
    if (runtime) {
        std::vector<NoteNagaMidiSeq*> seqsCopy = runtime->getSequences();
        for (NoteNagaMidiSeq *seq : seqsCopy) {
            runtime->removeSequence(seq);
            delete seq;
        }
    }
    
    // Read MIDI sequences
    int32_t numSequences = readInt32(file);
    for (int32_t i = 0; i < numSequences; ++i) {
        NoteNagaMidiSeq *seq = new NoteNagaMidiSeq();
        if (!deserializeSequence(file, seq)) {
            delete seq;
            m_lastError = "Failed to read sequence " + std::to_string(i);
            return false;
        }
        seq->computeMaxTick();
        runtime->addSequence(seq);
    }
    
    // Set first sequence as active
    if (!runtime->getSequences().empty()) {
        runtime->setActiveSequence(runtime->getSequences().front());
    }
    
    // Read master DSP blocks
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    if (dspEngine) {
        // Clear existing blocks
        std::vector<NoteNagaDSPBlockBase*> blocksCopy = dspEngine->getDSPBlocks();
        for (NoteNagaDSPBlockBase *block : blocksCopy) {
            dspEngine->removeDSPBlock(block);
            delete block;
        }
        
        // Read new blocks
        int32_t numBlocks = readInt32(file);
        for (int32_t i = 0; i < numBlocks; ++i) {
            NoteNagaDSPBlockBase *block = deserializeDSPBlock(file);
            if (block) {
                dspEngine->addDSPBlock(block);
            }
        }
        
        bool dspEnabled = readBool(file);
        dspEngine->setEnableDSP(dspEnabled);
    }
    
    // Read Arrangement (v6+)
    if (m_loadingVersion >= 6 && runtime && runtime->getArrangement()) {
        if (!deserializeArrangement(file, runtime->getArrangement())) {
            NOTE_NAGA_LOG_WARNING("Failed to load arrangement data, continuing with empty arrangement");
        }
    }
    
    file.close();
    NOTE_NAGA_LOG_INFO("Project loaded: " + filePath);
    return true;
}

/*******************************************************************************************************/
// Import MIDI and Create Empty Project
/*******************************************************************************************************/

bool NoteNagaProjectSerializer::importMidiAsProject(const std::string &midiFilePath, const NoteNagaProjectMetadata &metadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }
    
    if (!m_engine->loadProject(midiFilePath)) {
        m_lastError = "Failed to load MIDI file";
        return false;
    }
    
    return true;
}

bool NoteNagaProjectSerializer::createEmptyProject(const NoteNagaProjectMetadata &metadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }
    
    NoteNagaRuntimeData *runtime = m_engine->getRuntimeData();
    if (!runtime) {
        m_lastError = "Runtime data is null";
        return false;
    }
    
    // Clear existing sequences
    std::vector<NoteNagaMidiSeq*> seqsCopy = runtime->getSequences();
    for (NoteNagaMidiSeq *seq : seqsCopy) {
        runtime->removeSequence(seq);
        delete seq;
    }
    
    // Create new sequence
    NoteNagaMidiSeq *seq = new NoteNagaMidiSeq(1);
    seq->setPPQ(480);
    seq->setTempo(600000);  // 100 BPM in microseconds
    
    // Create one default track with default synth
    // Note: addTrack() already calls initDefaultSynth() internally
    NoteNagaTrack *track = seq->addTrack(0);  // Piano
    if (track) {
        track->setName("Track 1");
        // initDefaultSynth is already called in addTrack()
    }
    
    runtime->addSequence(seq);
    runtime->setActiveSequence(seq);
    
    return true;
}

/*******************************************************************************************************/
// Sequence Serialization
/*******************************************************************************************************/

void NoteNagaProjectSerializer::serializeSequence(std::ofstream &out, NoteNagaMidiSeq *seq)
{
    writeInt32(out, seq->getId());
    writeInt32(out, seq->getPPQ());
    writeInt32(out, seq->getTempo());
    writeInt32(out, seq->getMaxTick());
    
    const std::vector<NoteNagaTrack*> &tracks = seq->getTracks();
    writeInt32(out, static_cast<int32_t>(tracks.size()));
    for (NoteNagaTrack *track : tracks) {
        if (track) {
            serializeTrack(out, track);
        }
    }
}

bool NoteNagaProjectSerializer::deserializeSequence(std::ifstream &in, NoteNagaMidiSeq *seq)
{
    int32_t id = readInt32(in);
    (void)id;  // ID is set by constructor
    seq->setPPQ(readInt32(in));
    seq->setTempo(readInt32(in));
    int32_t maxTick = readInt32(in);
    (void)maxTick;  // Will be computed from notes
    
    int32_t numTracks = readInt32(in);
    for (int32_t i = 0; i < numTracks; ++i) {
        // Read track data
        int32_t trackId = readInt32(in);
        std::string name = readString(in);
        int32_t instrument = readInt32(in);
        int32_t channel = readInt32(in);
        uint8_t colorR = readUInt8(in);
        uint8_t colorG = readUInt8(in);
        uint8_t colorB = readUInt8(in);
        bool visible = readBool(in);
        bool muted = readBool(in);
        bool solo = readBool(in);
        float volume = readFloat(in);
        
        // Read per-track synth configuration (Version 3+)
        float audioVolumeDb = readFloat(in);
        int32_t midiPanOffset = readInt32(in);
        int32_t midiVelocityOffset = readInt32(in);  // Read but ignore (deprecated)
        (void)midiVelocityOffset;  // Suppress unused warning
        
        // Read synth type and configuration
        std::string synthType = readString(in);
        std::string soundFontPath = readString(in);
        
        // Read tempo track flag and events
        bool isTempoTrack = readBool(in);
        bool tempoTrackActive = true;  // Default to active
        std::vector<NN_TempoEvent_t> tempoEvents;
        if (isTempoTrack) {
            if (m_loadingVersion >= 4) {
                tempoTrackActive = readBool(in);  // Version 4+
            }
            int32_t numTempoEvents = readInt32(in);
            for (int32_t j = 0; j < numTempoEvents; ++j) {
                int32_t tick = readInt32(in);
                float bpm = readFloat(in);
                int32_t interp = readInt32(in);
                tempoEvents.push_back(NN_TempoEvent_t(
                    tick, static_cast<double>(bpm),
                    interp == 1 ? TempoInterpolation::Linear : TempoInterpolation::Step));
            }
        }
        
        NoteNagaTrack *track = seq->addTrack(instrument);
        if (!track) continue;
        
        track->setName(name);
        track->setChannel(channel);
        track->setColor(NN_Color_t(colorR, colorG, colorB));
        track->setVisible(visible);
        track->setMuted(muted);
        track->setSolo(solo);
        track->setVolume(volume);
        
        // Set per-track synth parameters
        track->setAudioVolumeDb(audioVolumeDb);
        track->setMidiPanOffset(midiPanOffset);
        
        // Initialize synth asynchronously
        if (synthType == "fluidsynth" && !soundFontPath.empty()) {
            NoteNagaSynthFluidSynth *fluidSynth = new NoteNagaSynthFluidSynth("Track Synth", soundFontPath, true /* loadAsync */);
            track->setSynth(fluidSynth);
        }
        // Note: addTrack() already calls initDefaultSynth() which loads async
        
        // Set tempo track state
        if (isTempoTrack) {
            track->setTempoTrack(true);
            track->setTempoTrackActive(tempoTrackActive);
            track->setTempoEvents(tempoEvents);
        }
        
        // Read notes
        int32_t numNotes = readInt32(in);
        for (int32_t j = 0; j < numNotes; ++j) {
            NN_Note_t note;
            note.id = readUInt64(in);
            note.note = readInt32(in);
            note.start = readInt32(in);
            note.length = readInt32(in);
            note.velocity = readInt32(in);
            note.pan = readInt32(in);
            note.parent = track;
            track->addNote(note);
        }
        
        // Read per-synth DSP blocks (Version 5+)
        if (m_loadingVersion >= 5) {
            NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
            INoteNagaSoftSynth* softSynth = track->getSoftSynth();
            int32_t numSynthBlocks = readInt32(in);
            for (int32_t j = 0; j < numSynthBlocks; ++j) {
                NoteNagaDSPBlockBase *block = deserializeDSPBlock(in);
                if (block && dspEngine && softSynth) {
                    dspEngine->addSynthDSPBlock(softSynth, block);
                }
            }
        }
    }
    
    return in.good();
}

void NoteNagaProjectSerializer::serializeTrack(std::ofstream &out, NoteNagaTrack *track)
{
    writeInt32(out, track->getId());
    writeString(out, track->getName());
    writeInt32(out, track->getInstrument().value_or(0));
    writeInt32(out, track->getChannel().value_or(0));
    
    NN_Color_t color = track->getColor();
    writeUInt8(out, color.red);
    writeUInt8(out, color.green);
    writeUInt8(out, color.blue);
    
    writeBool(out, track->isVisible());
    writeBool(out, track->isMuted());
    writeBool(out, track->isSolo());
    writeFloat(out, track->getVolume());
    
    // Per-track synth configuration (Version 3+)
    writeFloat(out, track->getAudioVolumeDb());
    writeInt32(out, track->getMidiPanOffset());
    writeInt32(out, 0);  // midiVelocityOffset - deprecated, write 0 for compatibility
    
    // Synth type and configuration
    INoteNagaSoftSynth* synth = track->getSoftSynth();
    NoteNagaSynthFluidSynth* fluidSynth = dynamic_cast<NoteNagaSynthFluidSynth*>(synth);
    if (fluidSynth) {
        writeString(out, "fluidsynth");
        writeString(out, fluidSynth->getSoundFontPath());
    } else {
        writeString(out, "none");
        writeString(out, "");
    }
    
    // Tempo track flag and events
    writeBool(out, track->isTempoTrack());
    if (track->isTempoTrack()) {
        writeBool(out, track->isTempoTrackActive());  // Version 4+
        const std::vector<NN_TempoEvent_t>& tempoEvents = track->getTempoEvents();
        writeInt32(out, static_cast<int32_t>(tempoEvents.size()));
        for (const NN_TempoEvent_t& te : tempoEvents) {
            writeInt32(out, te.tick);
            writeFloat(out, static_cast<float>(te.bpm));
            writeInt32(out, static_cast<int32_t>(te.interpolation));
        }
    }
    
    const std::vector<NN_Note_t> &notes = track->getNotes();
    writeInt32(out, static_cast<int32_t>(notes.size()));
    for (const NN_Note_t &note : notes) {
        writeUInt64(out, note.id);
        writeInt32(out, note.note);
        writeInt32(out, note.start.value_or(0));
        writeInt32(out, note.length.value_or(480));
        writeInt32(out, note.velocity.value_or(100));
        writeInt32(out, note.pan.value_or(64));
    }
    
    // Per-synth DSP blocks (Version 5+)
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    INoteNagaSoftSynth* softSynth = track->getSoftSynth();
    if (dspEngine && softSynth) {
        std::vector<NoteNagaDSPBlockBase*> synthBlocks = dspEngine->getSynthDSPBlocks(softSynth);
        writeInt32(out, static_cast<int32_t>(synthBlocks.size()));
        for (NoteNagaDSPBlockBase *block : synthBlocks) {
            if (block) {
                serializeDSPBlock(out, block);
            }
        }
    } else {
        writeInt32(out, 0);  // No synth DSP blocks
    }
}

bool NoteNagaProjectSerializer::deserializeTrack(std::ifstream &in, NoteNagaTrack *track)
{
    // This is handled inline in deserializeSequence
    return true;
}

/*******************************************************************************************************/
// DSP Block Serialization
/*******************************************************************************************************/

void NoteNagaProjectSerializer::serializeDSPBlock(std::ofstream &out, NoteNagaDSPBlockBase *block)
{
    if (!block) return;
    
    writeString(out, block->getBlockName());
    writeBool(out, block->isActive());
    
    std::vector<DSPParamDescriptor> descriptors = block->getParamDescriptors();
    writeInt32(out, static_cast<int32_t>(descriptors.size()));
    for (size_t i = 0; i < descriptors.size(); ++i) {
        writeString(out, descriptors[i].name);
        writeFloat(out, block->getParamValue(i));
    }
}

NoteNagaDSPBlockBase *NoteNagaProjectSerializer::deserializeDSPBlock(std::ifstream &in)
{
    std::string blockType = readString(in);
    bool active = readBool(in);
    
    NoteNagaDSPBlockBase *block = createDSPBlockByName(blockType);
    if (!block) {
        // Skip parameters if block type unknown
        int32_t numParams = readInt32(in);
        for (int32_t i = 0; i < numParams; ++i) {
            readString(in);
            readFloat(in);
        }
        return nullptr;
    }
    
    block->setActive(active);
    
    int32_t numParams = readInt32(in);
    for (int32_t i = 0; i < numParams; ++i) {
        std::string name = readString(in);
        float value = readFloat(in);
        if (i < static_cast<int32_t>(block->getParamDescriptors().size())) {
            block->setParamValue(i, value);
        }
    }
    
    return block;
}

/*******************************************************************************************************/
// DSP Block Factory
/*******************************************************************************************************/

NoteNagaDSPBlockBase *NoteNagaProjectSerializer::createDSPBlockByName(const std::string &name)
{
    if (name == "Gain") {
        return nn_create_audio_gain_block();
    } else if (name == "Pan") {
        return nn_create_audio_pan_block();
    } else if (name == "Single EQ" || name == "Equalizer" || name == "Single Band EQ") {
        return nn_create_single_band_eq_block();
    } else if (name == "Compressor") {
        return nn_create_compressor_block();
    } else if (name == "Multi EQ" || name == "Multi Band EQ") {
        return nn_create_multi_band_eq_block();
    } else if (name == "Limiter") {
        return nn_create_limiter_block();
    } else if (name == "Delay") {
        return nn_create_delay_block();
    } else if (name == "Reverb") {
        return nn_create_reverb_block();
    } else if (name == "Bitcrusher") {
        return nn_create_bitcrusher_block();
    } else if (name == "Tremolo") {
        return nn_create_tremolo_block();
    } else if (name == "Filter") {
        return nn_create_filter_block();
    } else if (name == "Chorus") {
        return nn_create_chorus_block();
    } else if (name == "Phaser") {
        return nn_create_phaser_block();
    } else if (name == "Flanger") {
        return nn_create_flanger_block();
    } else if (name == "Noise Gate") {
        return nn_create_noise_gate_block();
    } else if (name == "Saturator") {
        return nn_create_saturator_block();
    } else if (name == "Exciter") {
        return nn_create_exciter_block();
    } else if (name == "Stereo Imager") {
        return nn_create_stereo_imager_block();
    } else if (name == "Distortion") {
        return nn_create_distortion_block();
    } else if (name == "Ring Modulator") {
        return nn_create_ring_mod_block();
    } else if (name == "Vibrato") {
        return nn_create_vibrato_block();
    } else if (name == "Pitch Shifter") {
        return nn_create_pitch_shifter_block();
    } else if (name == "Auto Wah") {
        return nn_create_auto_wah_block();
    } else if (name == "De-Esser") {
        return nn_create_deesser_block();
    } else if (name == "Transient Shaper") {
        return nn_create_transient_shaper_block();
    } else if (name == "Sub Bass") {
        return nn_create_sub_bass_block();
    } else if (name == "Tape Saturation") {
        return nn_create_tape_saturation_block();
    } else if (name == "Ducker") {
        return nn_create_ducker_block();
    }
    
    return nullptr;
}

/*******************************************************************************************************/
// Arrangement Serialization (v6+)
/*******************************************************************************************************/

void NoteNagaProjectSerializer::serializeArrangement(std::ofstream &out, NoteNagaArrangement *arrangement)
{
    if (!arrangement) {
        writeInt32(out, 0);
        writeBool(out, false);  // No tempo track
        return;
    }
    
    const auto& tracks = arrangement->getTracks();
    writeInt32(out, static_cast<int32_t>(tracks.size()));
    
    for (auto* track : tracks) {
        serializeArrangementTrack(out, track);
    }
    
    // Serialize tempo track (v7+)
    bool hasTempoTrack = arrangement->hasTempoTrack();
    writeBool(out, hasTempoTrack);
    if (hasTempoTrack) {
        NoteNagaTrack* tempoTrack = arrangement->getTempoTrack();
        writeBool(out, tempoTrack->isTempoTrackActive());
        
        // Write tempo events
        const std::vector<NN_TempoEvent_t>& tempoEvents = tempoTrack->getTempoEvents();
        writeInt32(out, static_cast<int32_t>(tempoEvents.size()));
        for (const NN_TempoEvent_t& te : tempoEvents) {
            writeInt32(out, te.tick);
            writeFloat(out, static_cast<float>(te.bpm));
            writeInt32(out, static_cast<int32_t>(te.interpolation));
        }
    }
}

bool NoteNagaProjectSerializer::deserializeArrangement(std::ifstream &in, NoteNagaArrangement *arrangement)
{
    if (!arrangement) return false;
    
    // Clear existing arrangement
    arrangement->clear();
    
    int32_t numTracks = readInt32(in);
    if (numTracks < 0 || numTracks > 10000) {
        NOTE_NAGA_LOG_ERROR("Invalid arrangement track count: " + std::to_string(numTracks));
        return false;
    }
    
    for (int32_t i = 0; i < numTracks; ++i) {
        auto* track = arrangement->addTrack();
        if (!deserializeArrangementTrack(in, track)) {
            NOTE_NAGA_LOG_ERROR("Failed to deserialize arrangement track " + std::to_string(i));
            return false;
        }
    }
    
    // Deserialize tempo track (v7+)
    if (m_loadingVersion >= 7) {
        bool hasTempoTrack = readBool(in);
        if (hasTempoTrack) {
            bool tempoTrackActive = readBool(in);
            
            // Read tempo events
            int32_t numEvents = readInt32(in);
            if (numEvents < 0 || numEvents > 100000) {
                NOTE_NAGA_LOG_ERROR("Invalid tempo event count: " + std::to_string(numEvents));
                return false;
            }
            
            std::vector<NN_TempoEvent_t> tempoEvents;
            tempoEvents.reserve(numEvents);
            for (int32_t i = 0; i < numEvents; ++i) {
                NN_TempoEvent_t te;
                te.tick = readInt32(in);
                te.bpm = static_cast<double>(readFloat(in));
                te.interpolation = static_cast<TempoInterpolation>(readInt32(in));
                tempoEvents.push_back(te);
            }
            
            // Create tempo track with first BPM or 120 default
            double defaultBpm = tempoEvents.empty() ? 120.0 : tempoEvents[0].bpm;
            NoteNagaTrack* tempoTrack = arrangement->createTempoTrack(defaultBpm);
            if (tempoTrack) {
                tempoTrack->setTempoEvents(tempoEvents);
                tempoTrack->setTempoTrackActive(tempoTrackActive);
            }
        }
    }
    
    arrangement->updateMaxTick();
    return true;
}

void NoteNagaProjectSerializer::serializeArrangementTrack(std::ofstream &out, NoteNagaArrangementTrack *track)
{
    if (!track) return;
    
    writeInt32(out, track->getId());
    writeString(out, track->getName());
    
    // Color
    writeUInt8(out, track->getColor().red);
    writeUInt8(out, track->getColor().green);
    writeUInt8(out, track->getColor().blue);
    
    writeBool(out, track->isMuted());
    writeBool(out, track->isSolo());
    writeFloat(out, track->getVolume());
    writeFloat(out, track->getPan());  // Version 8+
    writeInt32(out, track->getChannelOffset());
    
    // Clips
    const auto& clips = track->getClips();
    writeInt32(out, static_cast<int32_t>(clips.size()));
    for (const auto& clip : clips) {
        serializeMidiClip(out, clip);
    }
}

bool NoteNagaProjectSerializer::deserializeArrangementTrack(std::ifstream &in, NoteNagaArrangementTrack *track)
{
    if (!track) return false;
    
    int32_t id = readInt32(in);
    track->setId(id);
    
    std::string name = readString(in);
    track->setName(name);
    
    // Color
    uint8_t r = readUInt8(in);
    uint8_t g = readUInt8(in);
    uint8_t b = readUInt8(in);
    track->setColor(NN_Color_t(r, g, b));
    
    track->setMuted(readBool(in));
    track->setSolo(readBool(in));
    track->setVolume(readFloat(in));
    
    // Version 8+: Pan support
    if (m_loadingVersion >= 8) {
        track->setPan(readFloat(in));
    } else {
        track->setPan(0.0f);  // Default to center
    }
    
    track->setChannelOffset(readInt32(in));
    
    // Clips
    int32_t numClips = readInt32(in);
    if (numClips < 0 || numClips > 100000) {
        NOTE_NAGA_LOG_ERROR("Invalid clip count: " + std::to_string(numClips));
        return false;
    }
    
    for (int32_t i = 0; i < numClips; ++i) {
        NN_MidiClip_t clip;
        if (!deserializeMidiClip(in, clip)) {
            NOTE_NAGA_LOG_ERROR("Failed to deserialize clip " + std::to_string(i));
            return false;
        }
        track->addClip(clip);
    }
    
    return true;
}

void NoteNagaProjectSerializer::serializeMidiClip(std::ofstream &out, const NN_MidiClip_t &clip)
{
    writeInt32(out, clip.id);
    writeInt32(out, clip.sequenceId);
    writeInt32(out, clip.startTick);
    writeInt32(out, clip.durationTicks);
    writeInt32(out, clip.offsetTicks);
    writeBool(out, clip.muted);
    writeString(out, clip.name);
    
    // Color
    writeUInt8(out, clip.color.red);
    writeUInt8(out, clip.color.green);
    writeUInt8(out, clip.color.blue);
}

bool NoteNagaProjectSerializer::deserializeMidiClip(std::ifstream &in, NN_MidiClip_t &clip)
{
    clip.id = readInt32(in);
    clip.sequenceId = readInt32(in);
    clip.startTick = readInt32(in);
    clip.durationTicks = readInt32(in);
    clip.offsetTicks = readInt32(in);
    clip.muted = readBool(in);
    clip.name = readString(in);
    
    // Color
    uint8_t r = readUInt8(in);
    uint8_t g = readUInt8(in);
    uint8_t b = readUInt8(in);
    clip.color = NN_Color_t(r, g, b);
    
    return in.good();
}

