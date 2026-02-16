#pragma once

#include "ai_types.h"
#include <QString>

namespace NoteNagaAI {

/**
 * @brief Parses AI responses from JSON format into executable commands.
 */
class AiCommandParser {
public:
    /**
     * @brief Parses a JSON string into an AI response.
     * @param jsonText The JSON text to parse.
     * @return Parsed response with commands.
     */
    static AiResponse parseResponse(const QString &jsonText);
    
    /**
     * @brief Checks if text looks like an AI response (vs user prompt).
     *        Detects JSON-like structure with commands array.
     * @param text Text to check.
     * @return True if it appears to be an AI response.
     */
    static bool isAiResponse(const QString &text);
    
    /**
     * @brief Extracts JSON from mixed text (handles markdown code blocks, etc).
     * @param text Text potentially containing JSON.
     * @return Extracted JSON string.
     */
    static QString extractJson(const QString &text);

private:
    static AiCommand parseCommand(const QJsonObject &cmdObj);
    static AiCommandType parseCommandType(const QString &typeStr);
};

} // namespace NoteNagaAI
