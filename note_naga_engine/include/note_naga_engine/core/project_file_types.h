#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

/*******************************************************************************************************/
// Project Metadata
/*******************************************************************************************************/

/**
 * @brief Project metadata structure containing all non-audio project information.
 *        Uses only standard C++ types for Qt-independent compilation.
 */
struct NOTE_NAGA_ENGINE_API NoteNagaProjectMetadata {
    std::string name;              ///< Project/song name
    std::string author;            ///< Author/composer name
    std::string description;       ///< Project description
    std::string copyright;         ///< Copyright information
    int64_t createdAt;             ///< Unix timestamp when the project was created
    int64_t modifiedAt;            ///< Unix timestamp when the project was last modified
    int projectVersion;            ///< Project file format version
    
    NoteNagaProjectMetadata()
        : name("Untitled Project"),
          author(""),
          description(""),
          copyright(""),
          createdAt(std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch()).count()),
          modifiedAt(std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch()).count()),
          projectVersion(2)  // Version 2 = binary format
    {}
    
    /**
     * @brief Get current timestamp as int64_t Unix seconds
     */
    static int64_t currentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

/*******************************************************************************************************/
// DSP Block Configuration
/*******************************************************************************************************/

/**
 * @brief Single DSP parameter for serialization.
 */
struct NOTE_NAGA_ENGINE_API DSPParamConfig {
    std::string name;              ///< Parameter name
    float value;                   ///< Parameter value
    
    DSPParamConfig() : name(""), value(0.0f) {}
    DSPParamConfig(const std::string &n, float v) : name(n), value(v) {}
};

/**
 * @brief DSP block configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API DSPBlockConfig {
    std::string blockType;                 ///< Type name of the DSP block (e.g., "Bitcrusher", "Reverb")
    bool active;                           ///< Whether the block is active
    std::vector<DSPParamConfig> parameters; ///< Array of parameter values
    
    DSPBlockConfig() : blockType(""), active(true) {}
};

/*******************************************************************************************************/
// Routing Entry Configuration
/*******************************************************************************************************/

/**
 * @brief Routing entry configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API RoutingEntryConfig {
    int trackId;               ///< Track ID
    std::string output;        ///< Output device name
    int channel;               ///< MIDI channel
    float volume;              ///< Volume multiplier
    int noteOffset;            ///< Note offset
    float pan;                 ///< Pan position
    
    RoutingEntryConfig()
        : trackId(0), output("any"), channel(0), volume(1.0f), noteOffset(0), pan(0.0f) {}
};

/*******************************************************************************************************/
// Note Configuration
/*******************************************************************************************************/

/**
 * @brief Note configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API NoteConfig {
    uint64_t id;
    int note;
    int start;
    int length;
    int velocity;
    int pan;
    
    NoteConfig()
        : id(0), note(60), start(0), length(480), velocity(100), pan(64) {}
};

/*******************************************************************************************************/
// Track Configuration
/*******************************************************************************************************/

/**
 * @brief Track configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API TrackConfig {
    int id;
    std::string name;
    int instrument;
    int channel;
    uint8_t colorR, colorG, colorB;  ///< Color as RGB components
    bool visible;
    bool muted;
    bool solo;
    float volume;
    std::vector<NoteConfig> notes;   ///< Notes in this track
    
    TrackConfig()
        : id(0), name("Track"), instrument(0), channel(0),
          colorR(0x50), colorG(0x80), colorB(0xc0),
          visible(true), muted(false), solo(false), volume(1.0f) {}
};

/*******************************************************************************************************/
// MIDI Sequence Configuration
/*******************************************************************************************************/

/**
 * @brief MIDI sequence configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API MidiSequenceConfig {
    int id;
    int ppq;
    int tempo;
    int maxTick;
    std::vector<TrackConfig> tracks;   ///< Tracks in this sequence
    
    MidiSequenceConfig()
        : id(1), ppq(480), tempo(600000), maxTick(0) {}
};

/*******************************************************************************************************/
// Synthesizer Configuration
/*******************************************************************************************************/

/**
 * @brief Synthesizer configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API SynthesizerConfig {
    std::string name;
    std::string type;                      ///< "fluidsynth" or "external_midi"
    std::string soundFontPath;             ///< For FluidSynth
    std::string midiPort;                  ///< For external MIDI
    std::vector<DSPBlockConfig> dspBlocks; ///< DSP blocks for this synth
    
    SynthesizerConfig() : name(""), type(""), soundFontPath(""), midiPort("") {}
};

/*******************************************************************************************************/
// Complete Project Data
/*******************************************************************************************************/

/**
 * @brief Complete project data structure for serialization.
 */
struct NOTE_NAGA_ENGINE_API NoteNagaProjectData {
    NoteNagaProjectMetadata metadata;
    std::vector<MidiSequenceConfig> sequences;
    std::vector<SynthesizerConfig> synthesizers;
    std::vector<DSPBlockConfig> masterDspBlocks;
    std::vector<RoutingEntryConfig> routingTable;
    bool dspEnabled;
    
    NoteNagaProjectData() : dspEnabled(true) {}
};

