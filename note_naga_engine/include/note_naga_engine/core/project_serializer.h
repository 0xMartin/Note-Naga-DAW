#pragma once

// Project serializer requires Qt - only available when QT_DEACTIVATED is not defined
#ifndef QT_DEACTIVATED

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/project_file_types.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/dsp_block_base.h>
#include <note_naga_engine/module/mixer.h>
#include <note_naga_engine/module/dsp_engine.h>

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

class NoteNagaEngine;

/**
 * @brief Handles serialization and deserialization of Note Naga project files.
 * 
 * Project file format (.nnproj) is JSON-based and contains:
 * - Project metadata (name, author, timestamps, etc.)
 * - MIDI sequences with all tracks and notes
 * - DSP block chain configuration
 * - Mixer routing table
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
    bool saveProject(const QString &filePath, const NoteNagaProjectMetadata &metadata);
    
    /**
     * @brief Load a project from a file.
     * @param filePath Path to the project file.
     * @param outMetadata Output parameter for loaded metadata.
     * @return True on success, false on failure.
     */
    bool loadProject(const QString &filePath, NoteNagaProjectMetadata &outMetadata);
    
    /**
     * @brief Import a MIDI file as a new project.
     * @param midiFilePath Path to the MIDI file.
     * @param metadata Project metadata to use.
     * @return True on success, false on failure.
     */
    bool importMidiAsProject(const QString &midiFilePath, const NoteNagaProjectMetadata &metadata);
    
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
    QString lastError() const { return m_lastError; }
    
private:
    NoteNagaEngine *m_engine;
    QString m_lastError;
    
    // Serialization helpers
    QJsonObject serializeSequence(NoteNagaMidiSeq *seq);
    QJsonArray serializeTrack(NoteNagaTrack *track);
    QJsonArray serializeDSPBlocks();
    QJsonArray serializeSynthesizers();
    QJsonArray serializeRoutingTable();
    QJsonObject serializeDSPBlock(NoteNagaDSPBlockBase *block);
    
    // Deserialization helpers
    bool deserializeSequence(const QJsonObject &seqObj, NoteNagaMidiSeq *seq);
    bool deserializeTrack(const QJsonObject &trackObj, NoteNagaTrack *track);
    bool deserializeDSPBlocks(const QJsonArray &blocksArray);
    bool deserializeSynthesizers(const QJsonArray &synthsArray);
    bool deserializeRoutingTable(const QJsonArray &routingArray);
    
    // DSP block factory
    NoteNagaDSPBlockBase *createDSPBlockByName(const QString &name);
};

#endif // QT_DEACTIVATED
