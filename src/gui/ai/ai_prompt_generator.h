#pragma once

#include "ai_types.h"
#include <QString>
#include <QList>

class NoteNagaMidiSeq;
class NoteNagaTrack;

namespace NoteNagaAI {

/**
 * @brief Generates AI prompts with MIDI sequence data and instructions.
 */
class AiPromptGenerator {
public:
    /**
     * @brief Generates a full AI prompt including sequence data and instructions.
     * @param userPrompt The user's input text.
     * @param sequence The MIDI sequence to include data from.
     * @param chatHistory Previous conversation messages for context.
     * @param targetDurationSec Target duration in seconds (0 = not specified).
     * @return Complete prompt string ready for AI.
     */
    static QString generateFullPrompt(const QString &userPrompt, NoteNagaMidiSeq *sequence,
                                       const QList<ChatMessage> &chatHistory = {},
                                       int targetDurationSec = 0);
    
    /**
     * @brief Creates a compact representation of the MIDI sequence.
     * @param sequence The MIDI sequence to convert.
     * @return Compact sequence data structure.
     */
    static CompactSequence createCompactSequence(NoteNagaMidiSeq *sequence);
    
    /**
     * @brief Creates a compact representation of a track.
     * @param track The track to convert.
     * @return Compact track data structure.
     */
    static CompactTrack createCompactTrack(NoteNagaTrack *track);
    
    /**
     * @brief Gets the system instructions for the AI.
     * @return Instructions string explaining available commands.
     */
    static QString getSystemInstructions();
    
    /**
     * @brief Gets the JSON schema for AI responses.
     * @return JSON schema string.
     */
    static QString getResponseSchema();
    
    /**
     * @brief Gets a list of available GM instruments as compact string.
     * @return Instruments list string.
     */
    static QString getInstrumentsList();
};

} // namespace NoteNagaAI
