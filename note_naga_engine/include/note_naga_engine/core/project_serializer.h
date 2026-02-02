#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/project_file_types.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <note_naga_engine/module/mixer.h>
#include <note_naga_engine/module/dsp_engine.h>

#include <string>
#include <fstream>
#include <cstdint>

class NoteNagaEngine;

/**
 * @brief Binary file format magic number and version.
 */
constexpr uint32_t NNPROJ_MAGIC = 0x4E4E5052;  // "NNPR" in little endian
constexpr uint32_t NNPROJ_VERSION = 2;

/**
 * @brief Handles serialization and deserialization of Note Naga project files.
 * 
 * Project file format (.nnproj) is binary and contains:
 * - Project metadata (name, author, timestamps, etc.)
 * - MIDI sequences with all tracks and notes
 * - DSP block chain configuration
 * - Mixer routing table
 * 
 * Uses only standard C++ types for Qt-independent compilation.
 */
class NOTE_NAGA_ENGINE_API NoteNagaProjectSerializer {
public:
    /**
     * @brief Construct a new serializer for the given engine.
     * @param engine Pointer to the Note Naga engine.
     */
    explicit NoteNagaProjectSerializer(NoteNagaEngine *engine);
    
    /**
     * @brief Save the current project to a file.
     * @param filePath Path to save the project file.
     * @param metadata Project metadata.
     * @return True on success, false on failure.
     */
    bool saveProject(const std::string &filePath, const NoteNagaProjectMetadata &metadata);
    
    /**
     * @brief Load a project from a file.
     * @param filePath Path to the project file.
     * @param outMetadata Output parameter for loaded metadata.
     * @return True on success, false on failure.
     */
    bool loadProject(const std::string &filePath, NoteNagaProjectMetadata &outMetadata);
    
    /**
     * @brief Import a MIDI file as a new project.
     * @param midiFilePath Path to the MIDI file.
     * @param metadata Project metadata to use.
     * @return True on success, false on failure.
     */
    bool importMidiAsProject(const std::string &midiFilePath, const NoteNagaProjectMetadata &metadata);
    
    /**
     * @brief Create a new empty project with one sequence and one track.
     * @param metadata Project metadata.
     * @return True on success, false on failure.
     */
    bool createEmptyProject(const NoteNagaProjectMetadata &metadata);
    
    /**
     * @brief Get the last error message.
     * @return Error message string.
     */
    std::string lastError() const { return m_lastError; }
    
private:
    NoteNagaEngine *m_engine;
    std::string m_lastError;
    
    // Binary serialization helpers
    void writeString(std::ofstream &out, const std::string &str);
    std::string readString(std::ifstream &in);
    
    void writeInt32(std::ofstream &out, int32_t value);
    int32_t readInt32(std::ifstream &in);
    
    void writeInt64(std::ofstream &out, int64_t value);
    int64_t readInt64(std::ifstream &in);
    
    void writeUInt64(std::ofstream &out, uint64_t value);
    uint64_t readUInt64(std::ifstream &in);
    
    void writeFloat(std::ofstream &out, float value);
    float readFloat(std::ifstream &in);
    
    void writeBool(std::ofstream &out, bool value);
    bool readBool(std::ifstream &in);
    
    void writeUInt8(std::ofstream &out, uint8_t value);
    uint8_t readUInt8(std::ifstream &in);
    
    // Serialization helpers
    void serializeMetadata(std::ofstream &out, const NoteNagaProjectMetadata &metadata);
    bool deserializeMetadata(std::ifstream &in, NoteNagaProjectMetadata &metadata);
    
    void serializeSequence(std::ofstream &out, NoteNagaMidiSeq *seq);
    bool deserializeSequence(std::ifstream &in, NoteNagaMidiSeq *seq);
    
    void serializeTrack(std::ofstream &out, NoteNagaTrack *track);
    bool deserializeTrack(std::ifstream &in, NoteNagaTrack *track);
    
    void serializeDSPBlock(std::ofstream &out, NoteNagaDSPBlockBase *block);
    NoteNagaDSPBlockBase *deserializeDSPBlock(std::ifstream &in);
    
    void serializeSynthesizers(std::ofstream &out);
    bool deserializeSynthesizers(std::ifstream &in);
    
    void serializeRoutingTable(std::ofstream &out);
    bool deserializeRoutingTable(std::ifstream &in);
    
    // DSP block factory
    NoteNagaDSPBlockBase *createDSPBlockByName(const std::string &name);
};

