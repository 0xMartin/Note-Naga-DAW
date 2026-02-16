#include "ai_command_parser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>

namespace NoteNagaAI {

/**
 * @brief Attempt to repair incomplete JSON by closing unclosed brackets.
 *        Useful when AI model response is truncated mid-generation.
 */
QString repairIncompleteJson(const QString &json) {
    QString result = json.trimmed();
    
    // Track unclosed brackets/braces
    QVector<QChar> stack;
    bool inString = false;
    bool escaped = false;
    
    for (int i = 0; i < result.length(); ++i) {
        QChar c = result[i];
        
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (c == '\\' && inString) {
            escaped = true;
            continue;
        }
        
        if (c == '"') {
            inString = !inString;
            continue;
        }
        
        if (inString) continue;
        
        if (c == '{' || c == '[') {
            stack.push_back(c);
        } else if (c == '}') {
            if (!stack.isEmpty() && stack.last() == '{') {
                stack.pop_back();
            }
        } else if (c == ']') {
            if (!stack.isEmpty() && stack.last() == '[') {
                stack.pop_back();
            }
        }
    }
    
    // If we're inside an unterminated string, close it
    if (inString) {
        result += '"';
    }
    
    // Remove trailing comma if present (common in truncated arrays)
    while (result.endsWith(',') || result.endsWith(' ') || result.endsWith('\n')) {
        result.chop(1);
    }
    
    // Close any unclosed brackets in reverse order
    while (!stack.isEmpty()) {
        QChar open = stack.takeLast();
        if (open == '{') {
            result += '}';
        } else if (open == '[') {
            result += ']';
        }
    }
    
    return result;
}

AiResponse AiCommandParser::parseResponse(const QString &jsonText) {
    AiResponse response;
    response.rawJson = jsonText;
    
    qDebug() << "parseResponse called, input length:" << jsonText.length();
    
    // Try to extract JSON from the text
    QString json = extractJson(jsonText);
    qDebug() << "extractJson result length:" << json.length();
    
    if (json.isEmpty()) {
        response.parseError = QObject::tr("No valid JSON found in response");
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Raw response:" << jsonText;
        qWarning() << "================================";
        return response;
    }
    
    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    
    // If parsing failed, try to repair incomplete JSON
    if (parseError.error != QJsonParseError::NoError) {
        QString repairedJson = repairIncompleteJson(json);
        if (repairedJson != json) {
            qDebug() << "Attempting to repair incomplete JSON...";
            doc = QJsonDocument::fromJson(repairedJson.toUtf8(), &parseError);
        }
    }
    
    if (parseError.error != QJsonParseError::NoError) {
        response.parseError = QObject::tr("JSON parse error: %1").arg(parseError.errorString());
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Extracted JSON:" << json;
        qWarning() << "Raw response:" << jsonText;
        qWarning() << "================================";
        return response;
    }
    
    if (!doc.isObject()) {
        response.parseError = QObject::tr("Expected JSON object at root");
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Raw response:" << jsonText;
        qWarning() << "================================";
        return response;
    }
    
    QJsonObject root = doc.object();
    
    // Extract optional message
    if (root.contains("message")) {
        response.message = root["message"].toString();
    }
    
    // Extract commands array
    if (!root.contains("commands")) {
        response.parseError = QObject::tr("Missing 'commands' array in response");
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Raw response:" << jsonText;
        qWarning() << "================================";
        return response;
    }
    
    QJsonValue commandsVal = root["commands"];
    if (!commandsVal.isArray()) {
        response.parseError = QObject::tr("'commands' must be an array");
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Raw response:" << jsonText;
        qWarning() << "================================";
        return response;
    }
    
    QJsonArray commandsArr = commandsVal.toArray();
    for (const QJsonValue &cmdVal : commandsArr) {
        if (!cmdVal.isObject()) {
            AiCommand invalidCmd;
            invalidCmd.error = QObject::tr("Command must be an object");
            response.commands.push_back(invalidCmd);
            continue;
        }
        
        response.commands.push_back(parseCommand(cmdVal.toObject()));
    }
    
    // Check if commands array was empty
    if (response.commands.empty()) {
        response.parseError = QObject::tr("No commands found in AI response");
        qWarning() << "=== AI RESPONSE PARSE FAILED ===";
        qWarning() << "Error:" << response.parseError;
        qWarning() << "Raw response:" << jsonText;
        qWarning() << "================================";
    }
    
    return response;
}

bool AiCommandParser::isAiResponse(const QString &text) {
    QString trimmed = text.trimmed();
    
    // Check if it starts with { or contains JSON code block
    if (trimmed.startsWith('{') || trimmed.startsWith("```")) {
        // Look for typical AI response structure
        static QRegularExpression cmdPattern(R"("commands"\s*:\s*\[)", 
            QRegularExpression::CaseInsensitiveOption);
        return cmdPattern.match(text).hasMatch();
    }
    
    return false;
}

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
        // Find matching closing brace
        int braceCount = 0;
        int endPos = -1;
        
        for (int i = 0; i < trimmed.length(); ++i) {
            QChar c = trimmed[i];
            if (c == '{') braceCount++;
            else if (c == '}') {
                braceCount--;
                if (braceCount == 0) {
                    endPos = i;
                    break;
                }
            }
        }
        
        if (endPos > 0) {
            return trimmed.left(endPos + 1);
        }
    }
    
    // Try to find JSON object anywhere in text
    static QRegularExpression jsonPattern(R"(\{[\s\S]*"commands"[\s\S]*\})");
    match = jsonPattern.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(0);
    }
    
    return QString();
}

AiCommand AiCommandParser::parseCommand(const QJsonObject &cmdObj) {
    AiCommand cmd;
    
    // Get command type
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
    
    // Get parameters
    if (cmdObj.contains("params")) {
        QJsonValue paramsVal = cmdObj["params"];
        if (paramsVal.isObject()) {
            cmd.params = paramsVal.toObject();
        } else {
            cmd.error = QObject::tr("'params' must be an object");
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
