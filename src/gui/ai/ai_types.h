#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include <optional>

namespace NoteNagaAI {

/**
 * @brief Represents a chat message in the AI conversation.
 */
struct ChatMessage {
    enum class Role {
        User,       ///< User's prompt text (displayed in chat)
        System,     ///< Generated full prompt with instructions (hidden, for copy)
        Assistant   ///< AI's response (executed commands)
    };
    
    Role role;
    QString displayText;        ///< Text shown in chat UI
    QString fullPrompt;         ///< Full prompt with system instructions (for copy button)
    QDateTime timestamp;
    bool executed = false;      ///< Whether AI response was successfully executed
    
    ChatMessage(Role r = Role::User, const QString &display = QString(), 
                const QString &full = QString())
        : role(r), displayText(display), fullPrompt(full), 
          timestamp(QDateTime::currentDateTime()) {}
};

/**
 * @brief Types of AI commands that can be executed.
 */
enum class AiCommandType {
    // Note operations
    AddNote,
    RemoveNote,
    ModifyNote,
    
    // Track operations
    AddTrack,
    RemoveTrack,
    ModifyTrack,
    
    // Bulk operations
    ClearTrack,
    ClearAllTracks,
    
    // Tempo operations
    SetTempo,           ///< Set global tempo (BPM)
    AddTempoEvent,      ///< Add tempo change at specific tick
    RemoveTempoEvent,   ///< Remove tempo event at specific tick
    
    // Unknown/invalid command
    Unknown
};

/**
 * @brief Represents a single AI command to be executed.
 */
struct AiCommand {
    AiCommandType type = AiCommandType::Unknown;
    QJsonObject params;
    QString error;  ///< Error message if parsing failed
    
    bool isValid() const { return type != AiCommandType::Unknown && error.isEmpty(); }
};

/**
 * @brief Represents the complete AI response containing multiple commands.
 */
struct AiResponse {
    std::vector<AiCommand> commands;
    QString message;        ///< Optional message from AI to display
    QString rawJson;        ///< Original JSON for debugging
    QString parseError;     ///< Error if parsing failed
    
    bool isValid() const { return parseError.isEmpty() && !commands.empty(); }
    int commandCount() const { return static_cast<int>(commands.size()); }
};

/**
 * @brief Compact representation of a note for AI prompt.
 */
struct CompactNote {
    int note;           ///< MIDI note number (0-127)
    int start;          ///< Start tick
    int length;         ///< Duration in ticks
    int velocity;       ///< Velocity (0-127)
    std::optional<int> pan;  ///< Pan (0-127, optional)
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["n"] = note;
        obj["s"] = start;
        obj["l"] = length;
        obj["v"] = velocity;
        if (pan.has_value()) {
            obj["p"] = pan.value();
        }
        return obj;
    }
    
    static CompactNote fromJson(const QJsonObject &obj) {
        CompactNote cn;
        cn.note = obj["n"].toInt();
        cn.start = obj["s"].toInt();
        cn.length = obj["l"].toInt();
        cn.velocity = obj["v"].toInt(100);
        if (obj.contains("p")) {
            cn.pan = obj["p"].toInt();
        }
        return cn;
    }
};

/**
 * @brief Compact representation of a track for AI prompt.
 */
struct CompactTrack {
    int id;                     ///< Track ID
    QString name;               ///< Track name
    int instrument;             ///< GM instrument index (0-127)
    int channel;                ///< MIDI channel (0-15)
    bool muted;                 ///< Is track muted
    bool solo;                  ///< Is track soloed
    float volume;               ///< Volume (0.0-1.0)
    int pan;                    ///< Pan offset (-64 to +64)
    int colorR, colorG, colorB; ///< Track color
    std::vector<CompactNote> notes;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["inst"] = instrument;
        obj["ch"] = channel;
        obj["mute"] = muted;
        obj["solo"] = solo;
        obj["vol"] = static_cast<int>(volume * 100);
        obj["pan"] = pan;
        obj["color"] = QString("%1,%2,%3").arg(colorR).arg(colorG).arg(colorB);
        
        QJsonArray notesArr;
        for (const auto &note : notes) {
            notesArr.append(note.toJson());
        }
        obj["notes"] = notesArr;
        
        return obj;
    }
};

/**
 * @brief Compact representation of MIDI sequence for AI prompt.
 */
struct CompactSequence {
    int id;
    int ppq;                    ///< Pulses per quarter note
    int tempo;                  ///< Tempo in microseconds per quarter
    int bpm;                    ///< Tempo in BPM
    int maxTick;                ///< Maximum tick in sequence
    std::vector<CompactTrack> tracks;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["ppq"] = ppq;
        obj["bpm"] = bpm;
        obj["maxTick"] = maxTick;
        
        QJsonArray tracksArr;
        for (const auto &track : tracks) {
            tracksArr.append(track.toJson());
        }
        obj["tracks"] = tracksArr;
        
        return obj;
    }
};

} // namespace NoteNagaAI
