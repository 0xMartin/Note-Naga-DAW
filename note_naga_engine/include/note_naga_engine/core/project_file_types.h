#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Project metadata structure containing all non-audio project information.
 */
struct NOTE_NAGA_ENGINE_API NoteNagaProjectMetadata {
    QString name;              ///< Project/song name
    QString author;            ///< Author/composer name
    QString description;       ///< Project description
    QString copyright;         ///< Copyright information
    QDateTime createdAt;       ///< When the project was created
    QDateTime modifiedAt;      ///< When the project was last modified
    int projectVersion;        ///< Project file format version
    
    NoteNagaProjectMetadata()
        : name("Untitled Project"),
          author(""),
          description(""),
          copyright(""),
          createdAt(QDateTime::currentDateTime()),
          modifiedAt(QDateTime::currentDateTime()),
          projectVersion(1)
    {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["author"] = author;
        obj["description"] = description;
        obj["copyright"] = copyright;
        obj["createdAt"] = createdAt.toString(Qt::ISODate);
        obj["modifiedAt"] = modifiedAt.toString(Qt::ISODate);
        obj["projectVersion"] = projectVersion;
        return obj;
    }
    
    static NoteNagaProjectMetadata fromJson(const QJsonObject &obj) {
        NoteNagaProjectMetadata meta;
        meta.name = obj["name"].toString("Untitled Project");
        meta.author = obj["author"].toString("");
        meta.description = obj["description"].toString("");
        meta.copyright = obj["copyright"].toString("");
        meta.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        meta.modifiedAt = QDateTime::fromString(obj["modifiedAt"].toString(), Qt::ISODate);
        meta.projectVersion = obj["projectVersion"].toInt(1);
        
        if (!meta.createdAt.isValid()) {
            meta.createdAt = QDateTime::currentDateTime();
        }
        if (!meta.modifiedAt.isValid()) {
            meta.modifiedAt = QDateTime::currentDateTime();
        }
        return meta;
    }
};

/**
 * @brief DSP block configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API DSPBlockConfig {
    QString blockType;         ///< Type name of the DSP block (e.g., "Bitcrusher", "Reverb")
    bool active;               ///< Whether the block is active
    QJsonArray parameters;     ///< Array of parameter values
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["type"] = blockType;
        obj["active"] = active;
        obj["parameters"] = parameters;
        return obj;
    }
    
    static DSPBlockConfig fromJson(const QJsonObject &obj) {
        DSPBlockConfig config;
        config.blockType = obj["type"].toString();
        config.active = obj["active"].toBool(true);
        config.parameters = obj["parameters"].toArray();
        return config;
    }
};

/**
 * @brief Routing entry configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API RoutingEntryConfig {
    int trackId;               ///< Track ID
    QString output;            ///< Output device name
    int channel;               ///< MIDI channel
    float volume;              ///< Volume multiplier
    int noteOffset;            ///< Note offset
    float pan;                 ///< Pan position
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["trackId"] = trackId;
        obj["output"] = output;
        obj["channel"] = channel;
        obj["volume"] = static_cast<double>(volume);
        obj["noteOffset"] = noteOffset;
        obj["pan"] = static_cast<double>(pan);
        return obj;
    }
    
    static RoutingEntryConfig fromJson(const QJsonObject &obj) {
        RoutingEntryConfig config;
        config.trackId = obj["trackId"].toInt(0);
        config.output = obj["output"].toString("any");
        config.channel = obj["channel"].toInt(0);
        config.volume = static_cast<float>(obj["volume"].toDouble(1.0));
        config.noteOffset = obj["noteOffset"].toInt(0);
        config.pan = static_cast<float>(obj["pan"].toDouble(0.0));
        return config;
    }
};

/**
 * @brief Track configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API TrackConfig {
    int id;
    QString name;
    int instrument;
    int channel;
    QString color;             ///< Color as hex string
    bool visible;
    bool muted;
    bool solo;
    float volume;
    QJsonArray notes;          ///< Array of note objects
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["instrument"] = instrument;
        obj["channel"] = channel;
        obj["color"] = color;
        obj["visible"] = visible;
        obj["muted"] = muted;
        obj["solo"] = solo;
        obj["volume"] = static_cast<double>(volume);
        obj["notes"] = notes;
        return obj;
    }
    
    static TrackConfig fromJson(const QJsonObject &obj) {
        TrackConfig config;
        config.id = obj["id"].toInt(0);
        config.name = obj["name"].toString("Track");
        config.instrument = obj["instrument"].toInt(0);
        config.channel = obj["channel"].toInt(0);
        config.color = obj["color"].toString("#5080c0");
        config.visible = obj["visible"].toBool(true);
        config.muted = obj["muted"].toBool(false);
        config.solo = obj["solo"].toBool(false);
        config.volume = static_cast<float>(obj["volume"].toDouble(1.0));
        config.notes = obj["notes"].toArray();
        return config;
    }
};

/**
 * @brief MIDI sequence configuration for serialization.
 */
struct NOTE_NAGA_ENGINE_API MidiSequenceConfig {
    int id;
    int ppq;
    int tempo;
    int maxTick;
    QJsonArray tracks;         ///< Array of TrackConfig objects
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["ppq"] = ppq;
        obj["tempo"] = tempo;
        obj["maxTick"] = maxTick;
        obj["tracks"] = tracks;
        return obj;
    }
    
    static MidiSequenceConfig fromJson(const QJsonObject &obj) {
        MidiSequenceConfig config;
        config.id = obj["id"].toInt(1);
        config.ppq = obj["ppq"].toInt(480);
        config.tempo = obj["tempo"].toInt(120);
        config.maxTick = obj["maxTick"].toInt(0);
        config.tracks = obj["tracks"].toArray();
        return config;
    }
};
