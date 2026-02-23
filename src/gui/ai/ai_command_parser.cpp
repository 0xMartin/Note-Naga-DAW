#include "ai_command_parser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>

namespace NoteNagaAI {

/**
 * @brief Parse a single NNC (NoteNaga Commands) line into an AiCommand
 */
AiCommand parseNncLine(const QString &line) {
    AiCommand cmd;
    QString trimmed = line.trimmed();
    
    // Skip empty lines and message lines
    if (trimmed.isEmpty() || trimmed.startsWith("M:")) {
        cmd.type = AiCommandType::Unknown;
        return cmd;
    }
    
    // N+ : Add note - N+trackId,note,start,length,velocity[,pan]
    if (trimmed.startsWith("N+")) {
        QString params = trimmed.mid(2);
        QStringList parts = params.split(',');
        if (parts.size() >= 5) {
            cmd.type = AiCommandType::AddNote;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["note"] = parts[1].toInt();
            cmd.params["start"] = parts[2].toInt();
            cmd.params["length"] = parts[3].toInt();
            cmd.params["velocity"] = parts[4].toInt();
            if (parts.size() >= 6) {
                cmd.params["pan"] = parts[5].toInt();
            }
        } else {
            cmd.error = QObject::tr("N+ requires: trackId,note,start,length,velocity");
        }
        return cmd;
    }
    
    // N- : Remove note - N-trackId,note,start
    if (trimmed.startsWith("N-")) {
        QString params = trimmed.mid(2);
        QStringList parts = params.split(',');
        if (parts.size() >= 3) {
            cmd.type = AiCommandType::RemoveNote;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["note"] = parts[1].toInt();
            cmd.params["start"] = parts[2].toInt();
        } else {
            cmd.error = QObject::tr("N- requires: trackId,note,start");
        }
        return cmd;
    }
    
    // N* or NM: : Modify note - N*trackId,oNote,oStart>nNote,nStart,nLen,nVel or NM:...
    if (trimmed.startsWith("N*") || trimmed.startsWith("NM:")) {
        int prefixLen = trimmed.startsWith("NM:") ? 3 : 2;
        QString params = trimmed.mid(prefixLen);
        int arrowPos = params.indexOf('>');
        if (arrowPos > 0) {
            QString oldPart = params.left(arrowPos);
            QString newPart = params.mid(arrowPos + 1);
            QStringList oldParts = oldPart.split(',');
            QStringList newParts = newPart.split(',');
            
            if (oldParts.size() >= 3 && newParts.size() >= 4) {
                cmd.type = AiCommandType::ModifyNote;
                cmd.params["trackId"] = oldParts[0].toInt();
                cmd.params["originalNote"] = oldParts[1].toInt();
                cmd.params["originalStart"] = oldParts[2].toInt();
                cmd.params["newNote"] = newParts[0].toInt();
                cmd.params["newStart"] = newParts[1].toInt();
                cmd.params["newLength"] = newParts[2].toInt();
                cmd.params["newVelocity"] = newParts[3].toInt();
            } else {
                cmd.error = QObject::tr("NM: format: trackId,oNote,oStart>nNote,nStart,nLen,nVel");
            }
        } else {
            cmd.error = QObject::tr("NM: requires '>' separator");
        }
        return cmd;
    }
    
    // T+ : Add track - T+name|instrument|channel
    if (trimmed.startsWith("T+")) {
        QString params = trimmed.mid(2);
        QStringList parts = params.split('|');
        if (parts.size() >= 3) {
            cmd.type = AiCommandType::AddTrack;
            cmd.params["name"] = parts[0];
            cmd.params["instrument"] = parts[1].toInt();
            cmd.params["channel"] = parts[2].toInt();
        } else {
            cmd.error = QObject::tr("T+ requires: name|instrument|channel");
        }
        return cmd;
    }
    
    // T- : Remove track - T-trackId
    if (trimmed.startsWith("T-")) {
        QString params = trimmed.mid(2);
        cmd.type = AiCommandType::RemoveTrack;
        cmd.params["trackId"] = params.toInt();
        return cmd;
    }
    
    // T* or TM: : Modify track - T*trackId prop=val or TM:trackId prop=val
    if (trimmed.startsWith("T*") || trimmed.startsWith("TM:")) {
        int prefixLen = trimmed.startsWith("TM:") ? 3 : 2;
        QString params = trimmed.mid(prefixLen);
        int spacePos = params.indexOf(' ');
        if (spacePos > 0) {
            cmd.type = AiCommandType::ModifyTrack;
            cmd.params["trackId"] = params.left(spacePos).toInt();
            
            QString propsStr = params.mid(spacePos + 1);
            // Parse key=value pairs
            static QRegularExpression propRegex(R"((\w+)=([^\s]+))");
            QRegularExpressionMatchIterator it = propRegex.globalMatch(propsStr);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                QString key = match.captured(1);
                QString val = match.captured(2);
                
                if (key == "n") cmd.params["name"] = val;
                else if (key == "i") cmd.params["instrument"] = val.toInt();
                else if (key == "c") cmd.params["channel"] = val.toInt();
                else if (key == "m") cmd.params["mute"] = (val == "1");
                else if (key == "s") cmd.params["solo"] = (val == "1");
                else if (key == "v") cmd.params["volume"] = val.toInt();
                else if (key == "p") cmd.params["pan"] = val.toInt();
                else if (key == "col") cmd.params["color"] = val;
            }
        } else {
            cmd.error = QObject::tr("TM: requires: trackId prop=val...");
        }
        return cmd;
    }
    
    // TC: : Clear track - TC:trackId
    if (trimmed.startsWith("TC:")) {
        QString params = trimmed.mid(3);
        cmd.type = AiCommandType::ClearTrack;
        cmd.params["trackId"] = params.toInt();
        return cmd;
    }
    
    // TCA : Clear all tracks
    if (trimmed == "TCA") {
        cmd.type = AiCommandType::ClearAllTracks;
        return cmd;
    }
    
    // BPM: : Set tempo
    if (trimmed.startsWith("BPM:")) {
        QString params = trimmed.mid(4);
        cmd.type = AiCommandType::SetTempo;
        cmd.params["bpm"] = params.toInt();
        return cmd;
    }
    
    // TE+ : Add tempo event - TE+tick,bpm
    if (trimmed.startsWith("TE+")) {
        QString params = trimmed.mid(3);
        QStringList parts = params.split(',');
        if (parts.size() >= 2) {
            cmd.type = AiCommandType::AddTempoEvent;
            cmd.params["tick"] = parts[0].toInt();
            cmd.params["bpm"] = parts[1].toInt();
        } else {
            cmd.error = QObject::tr("TE+ requires: tick,bpm");
        }
        return cmd;
    }
    
    // TE- : Remove tempo event - TE-tick
    if (trimmed.startsWith("TE-")) {
        QString params = trimmed.mid(3);
        cmd.type = AiCommandType::RemoveTempoEvent;
        cmd.params["tick"] = params.toInt();
        return cmd;
    }
    
    // CHORD: Add chord - CHORD:trackId,root,type,start,length,vel
    if (trimmed.startsWith("CHORD:")) {
        QString params = trimmed.mid(6);
        QStringList parts = params.split(',');
        if (parts.size() >= 6) {
            cmd.type = AiCommandType::AddChord;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["root"] = parts[1].toInt();
            cmd.params["chordType"] = parts[2].trimmed();
            cmd.params["start"] = parts[3].toInt();
            cmd.params["length"] = parts[4].toInt();
            cmd.params["velocity"] = parts[5].toInt();
        } else {
            cmd.error = QObject::tr("CHORD requires: trackId,root,type,start,length,vel");
        }
        return cmd;
    }
    
    // ARP: Add arpeggio - ARP:trackId,root,type,start,noteLen,vel,dir
    if (trimmed.startsWith("ARP:")) {
        QString params = trimmed.mid(4);
        QStringList parts = params.split(',');
        if (parts.size() >= 7) {
            cmd.type = AiCommandType::AddArpeggio;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["root"] = parts[1].toInt();
            cmd.params["chordType"] = parts[2].trimmed();
            cmd.params["start"] = parts[3].toInt();
            cmd.params["noteLength"] = parts[4].toInt();
            cmd.params["velocity"] = parts[5].toInt();
            cmd.params["direction"] = parts[6].trimmed();
        } else {
            cmd.error = QObject::tr("ARP requires: trackId,root,type,start,noteLen,vel,dir");
        }
        return cmd;
    }
    
    // SCALE: Add scale - SCALE:trackId,root,type,start,noteLen,vel
    if (trimmed.startsWith("SCALE:")) {
        QString params = trimmed.mid(6);
        QStringList parts = params.split(',');
        if (parts.size() >= 6) {
            cmd.type = AiCommandType::AddScale;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["root"] = parts[1].toInt();
            cmd.params["scaleType"] = parts[2].trimmed();
            cmd.params["start"] = parts[3].toInt();
            cmd.params["noteLength"] = parts[4].toInt();
            cmd.params["velocity"] = parts[5].toInt();
        } else {
            cmd.error = QObject::tr("SCALE requires: trackId,root,type,start,noteLen,vel");
        }
        return cmd;
    }
    
    // PAT: Add pattern - PAT:trackId,pattern,start,noteLen,vel
    if (trimmed.startsWith("PAT:")) {
        QString params = trimmed.mid(4);
        // Find first comma to get trackId, rest is pattern data
        int firstComma = params.indexOf(',');
        if (firstComma > 0) {
            int trackId = params.left(firstComma).toInt();
            QString rest = params.mid(firstComma + 1);
            
            // Find pattern (notes in parentheses or until next params)
            // Format: trackId,(60,64,67),start,noteLen,vel or trackId,60-64-67,start,noteLen,vel
            QStringList parts = rest.split(',');
            if (parts.size() >= 4) {
                cmd.type = AiCommandType::AddPattern;
                cmd.params["trackId"] = trackId;
                cmd.params["pattern"] = parts[0].trimmed(); // e.g., "60-64-67"
                cmd.params["start"] = parts[1].toInt();
                cmd.params["noteLength"] = parts[2].toInt();
                cmd.params["velocity"] = parts[3].toInt();
            } else {
                cmd.error = QObject::tr("PAT requires: trackId,pattern,start,noteLen,vel");
            }
        } else {
            cmd.error = QObject::tr("PAT requires: trackId,pattern,start,noteLen,vel");
        }
        return cmd;
    }
    
    // DUP: Duplicate notes - DUP:trackId,srcStart,srcEnd,destStart
    if (trimmed.startsWith("DUP:")) {
        QString params = trimmed.mid(4);
        QStringList parts = params.split(',');
        if (parts.size() >= 4) {
            cmd.type = AiCommandType::DuplicateNotes;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["srcStart"] = parts[1].toInt();
            cmd.params["srcEnd"] = parts[2].toInt();
            cmd.params["destStart"] = parts[3].toInt();
        } else {
            cmd.error = QObject::tr("DUP requires: trackId,srcStart,srcEnd,destStart");
        }
        return cmd;
    }
    
    // TRANS: Transpose notes - TRANS:trackId,start,end,semitones
    if (trimmed.startsWith("TRANS:")) {
        QString params = trimmed.mid(6);
        QStringList parts = params.split(',');
        if (parts.size() >= 4) {
            cmd.type = AiCommandType::TransposeNotes;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["start"] = parts[1].toInt();
            cmd.params["end"] = parts[2].toInt();
            cmd.params["semitones"] = parts[3].toInt();
        } else {
            cmd.error = QObject::tr("TRANS requires: trackId,start,end,semitones");
        }
        return cmd;
    }
    
    // QUANT: Quantize notes - QUANT:trackId,start,end,grid
    if (trimmed.startsWith("QUANT:")) {
        QString params = trimmed.mid(6);
        QStringList parts = params.split(',');
        if (parts.size() >= 4) {
            cmd.type = AiCommandType::QuantizeNotes;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["start"] = parts[1].toInt();
            cmd.params["end"] = parts[2].toInt();
            cmd.params["grid"] = parts[3].toInt();
        } else {
            cmd.error = QObject::tr("QUANT requires: trackId,start,end,grid");
        }
        return cmd;
    }
    
    // DRUM: Add drum pattern - DRUM:trackId,pattern,start,bars
    if (trimmed.startsWith("DRUM:")) {
        QString params = trimmed.mid(5);
        QStringList parts = params.split(',');
        if (parts.size() >= 4) {
            cmd.type = AiCommandType::AddDrumPattern;
            cmd.params["trackId"] = parts[0].toInt();
            cmd.params["pattern"] = parts[1].trimmed();
            cmd.params["start"] = parts[2].toInt();
            cmd.params["bars"] = parts[3].toInt();
        } else {
            cmd.error = QObject::tr("DRUM requires: trackId,pattern,start,bars");
        }
        return cmd;
    }
    
    // Unknown command
    cmd.error = QObject::tr("Unknown NNC command: %1").arg(trimmed.left(20));
    return cmd;
}

AiResponse AiCommandParser::parseResponse(const QString &responseText) {
    AiResponse response;
    response.rawJson = responseText; // Keep raw for debugging (renamed from rawJson for compat)
    
    qDebug() << "parseResponse (NNC) called, input length:" << responseText.length();
    
    // Extract text from potential markdown code blocks
    QString text = extractNncText(responseText);
    
    if (text.isEmpty()) {
        response.parseError = QObject::tr("No valid NNC commands found in response");
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Raw response:" << responseText;
        qWarning() << "================================";
        return response;
    }
    
    // Split into lines and parse each
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    QStringList messages; // Collect all M: messages
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        
        // Extract message if present - collect all of them
        if (trimmed.startsWith("M:")) {
            messages.append(trimmed.mid(2).trimmed());
            continue;
        }
        
        // Skip empty lines
        if (trimmed.isEmpty()) {
            continue;
        }
        
        // Skip comment lines (lines starting with #)
        if (trimmed.startsWith('#')) {
            continue;
        }
        
        // Parse command
        AiCommand cmd = parseNncLine(trimmed);
        if (cmd.type != AiCommandType::Unknown || !cmd.error.isEmpty()) {
            response.commands.push_back(cmd);
        }
    }
    
    // Join all messages with newline
    if (!messages.isEmpty()) {
        response.message = messages.join("\n");
    }
    
    // Check if no commands were found
    if (response.commands.empty()) {
        response.parseError = QObject::tr("No valid commands found in AI response");
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Raw response:" << responseText;
        qWarning() << "================================";
    }
    
    return response;
}

bool AiCommandParser::isAiResponse(const QString &text) {
    QString trimmed = text.trimmed();
    
    // Check for NNC command patterns
    static QRegularExpression nncPattern(R"(^(M:|N[+\-]|NM:|T[+\-]|TM:|TC:|TCA|BPM:|TE[+\-]|CHORD:|ARP:|SCALE:|PAT:|DUP:|TRANS:|QUANT:|DRUM:))", 
        QRegularExpression::MultilineOption);
    
    return nncPattern.match(trimmed).hasMatch();
}

QString AiCommandParser::extractNncText(const QString &text) {
    QString trimmed = text.trimmed();
    
    // Check for markdown code block: ```...``` or ```nnc...```
    static QRegularExpression codeBlockPattern(R"(```(?:\w*)?\s*\n?([\s\S]*?)\n?```)", 
        QRegularExpression::CaseInsensitiveOption);
    
    QRegularExpressionMatch match = codeBlockPattern.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    // Check if text contains NNC commands directly
    static QRegularExpression nncPattern(R"(^(M:|N[+\-]|NM:|T[+\-]|TM:|TC:|TCA|BPM:|TE[+\-]|CHORD:|ARP:|SCALE:|PAT:|DUP:|TRANS:|QUANT:|DRUM:))", 
        QRegularExpression::MultilineOption);
    
    if (nncPattern.match(trimmed).hasMatch()) {
        return trimmed;
    }
    
    // Try to find NNC commands anywhere in text
    QStringList validLines;
    QStringList lines = trimmed.split('\n');
    
    for (const QString &line : lines) {
        QString l = line.trimmed();
        if (l.startsWith("M:") || l.startsWith("N+") || l.startsWith("N-") || 
            l.startsWith("NM:") || l.startsWith("T+") || l.startsWith("T-") ||
            l.startsWith("TM:") || l.startsWith("TC:") || l == "TCA" ||
            l.startsWith("BPM:") || l.startsWith("TE+") || l.startsWith("TE-") ||
            l.startsWith("CHORD:") || l.startsWith("ARP:") || l.startsWith("SCALE:") ||
            l.startsWith("PAT:") || l.startsWith("DUP:") || l.startsWith("TRANS:") ||
            l.startsWith("QUANT:") || l.startsWith("DRUM:")) {
            validLines.append(l);
        }
    }
    
    if (!validLines.isEmpty()) {
        return validLines.join('\n');
    }
    
    return QString();
}

// Legacy JSON support for backwards compatibility
QString AiCommandParser::extractJson(const QString &text) {
    QString trimmed = text.trimmed();
    
    // Check for markdown code block: ```json ... ``` or ``` ... ```
    static QRegularExpression codeBlockPattern(R"(```(?:json)?\s*\n?([\s\S]*?)\n?```)", 
        QRegularExpression::CaseInsensitiveOption);
    
    QRegularExpressionMatch match = codeBlockPattern.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    // Check if it's raw JSON starting with {
    if (trimmed.startsWith('{')) {
        return trimmed;
    }
    
    return QString();
}

AiCommand AiCommandParser::parseCommand(const QJsonObject &cmdObj) {
    // Legacy JSON parsing - kept for potential backward compatibility
    AiCommand cmd;
    
    if (!cmdObj.contains("type")) {
        cmd.error = QObject::tr("Command missing 'type' field");
        return cmd;
    }
    
    QString typeStr = cmdObj["type"].toString();
    cmd.type = parseCommandType(typeStr);
    
    if (cmd.type == AiCommandType::Unknown) {
        cmd.error = QObject::tr("Unknown command type: %1").arg(typeStr);
        return cmd;
    }
    
    if (cmdObj.contains("params")) {
        QJsonValue paramsVal = cmdObj["params"];
        if (paramsVal.isObject()) {
            cmd.params = paramsVal.toObject();
        }
    }
    
    return cmd;
}

AiCommandType AiCommandParser::parseCommandType(const QString &typeStr) {
    QString upper = typeStr.toUpper().trimmed();
    
    if (upper == "ADD_NOTE") return AiCommandType::AddNote;
    if (upper == "REMOVE_NOTE") return AiCommandType::RemoveNote;
    if (upper == "MODIFY_NOTE") return AiCommandType::ModifyNote;
    if (upper == "ADD_TRACK") return AiCommandType::AddTrack;
    if (upper == "REMOVE_TRACK") return AiCommandType::RemoveTrack;
    if (upper == "MODIFY_TRACK") return AiCommandType::ModifyTrack;
    if (upper == "CLEAR_TRACK") return AiCommandType::ClearTrack;
    if (upper == "CLEAR_ALL_TRACKS") return AiCommandType::ClearAllTracks;
    if (upper == "SET_TEMPO") return AiCommandType::SetTempo;
    if (upper == "ADD_TEMPO_EVENT") return AiCommandType::AddTempoEvent;
    if (upper == "REMOVE_TEMPO_EVENT") return AiCommandType::RemoveTempoEvent;
    
    return AiCommandType::Unknown;
}

} // namespace NoteNagaAI
