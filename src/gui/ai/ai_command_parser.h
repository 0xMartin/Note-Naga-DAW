#pragma once

#include "ai_types.h"
#include <QString>

namespace NoteNagaAI {

/**
 * @brief Parses AI responses from NNC (NoteNaga Commands) format into executable commands.
 *        NNC is a compact line-based format more efficient than JSON.
 */
class AiCommandParser {
public:
    /**
     * @brief Parses an NNC formatted response into an AI response.
     * @param responseText The NNC text to parse.
     * @return Parsed response with commands.
     */
    static AiResponse parseResponse(const QString &responseText);
    
    /**
     * @brief Checks if text looks like an AI response (vs user prompt).
     *        Detects NNC command patterns.
     * @param text Text to check.
     * @return True if it appears to be an AI response.
     */
    static bool isAiResponse(const QString &text);
    
    /**
     * @brief Extracts NNC commands from mixed text (handles markdown code blocks, etc).
     * @param text Text potentially containing NNC commands.
     * @return Extracted NNC command lines.
     */
    static QString extractNncText(const QString &text);
    
    /**
     * @brief Extracts JSON from mixed text (legacy support).
     * @param text Text potentially containing JSON.
     * @return Extracted JSON string.
     */
    static QString extractJson(const QString &text);

private:
    static AiCommand parseCommand(const QJsonObject &cmdObj);
    static AiCommandType parseCommandType(const QString &typeStr);
};

} // namespace NoteNagaAI
