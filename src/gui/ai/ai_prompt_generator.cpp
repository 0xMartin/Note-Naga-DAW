#include "ai_prompt_generator.h"

#include <note_naga_engine/core/types.h>
#include <QJsonDocument>

namespace NoteNagaAI {

QString AiPromptGenerator::generateFullPrompt(const QString &userPrompt, NoteNagaMidiSeq *sequence,
                                               const QList<ChatMessage> &chatHistory) {
    if (!sequence) {
        return QString();
    }
    
    QString prompt;
    prompt.reserve(12000);
    
    // System instructions
    prompt += getSystemInstructions();
    prompt += "\n\n";
    
    // Response format
    prompt += "=== RESPONSE FORMAT ===\n";
    prompt += getResponseSchema();
    prompt += "\n\n";
    
    // Current sequence data
    prompt += "=== CURRENT MIDI SEQUENCE DATA ===\n";
    CompactSequence compactSeq = createCompactSequence(sequence);
    QJsonDocument doc(compactSeq.toJson());
    prompt += QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    prompt += "\n\n";
    
    // Important guidance for extending content
    int maxTick = sequence->getMaxTick();
    if (maxTick > 0) {
        prompt += "=== IMPORTANT CONTEXT ===\n";
        prompt += QString("The composition currently ends at tick %1.\n").arg(maxTick);
        prompt += "When extending or adding more content:\n";
        prompt += QString("- Add new notes with start positions >= %1 (after existing content)\n").arg(maxTick);
        prompt += "- Do NOT overwrite existing notes by using the same start positions\n";
        prompt += "- Extend each track appropriately to maintain the musical style\n";
        prompt += "\n";
    }
    
    // Available instruments (condensed)
    prompt += "=== AVAILABLE GM INSTRUMENTS (index: name) ===\n";
    prompt += getInstrumentsList();
    prompt += "\n\n";
    
    // Include conversation history for context (last 4 turns max)
    if (!chatHistory.isEmpty()) {
        prompt += "=== CONVERSATION HISTORY ===\n";
        int startIdx = qMax(0, chatHistory.size() - 8);  // Last 8 messages (4 turns)
        for (int i = startIdx; i < chatHistory.size(); ++i) {
            const ChatMessage &msg = chatHistory[i];
            if (msg.role == ChatMessage::Role::User) {
                prompt += QString("User: %1\n").arg(msg.displayText);
            } else if (msg.role == ChatMessage::Role::Assistant) {
                // Just indicate that assistant responded, don't include full response
                if (msg.executed) {
                    prompt += QString("Assistant: [Executed changes successfully]\n");
                } else {
                    prompt += QString("Assistant: [Failed to execute]\n");
                }
            }
        }
        prompt += "\n";
    }
    
    // User's current request
    prompt += "=== USER REQUEST ===\n";
    prompt += userPrompt;
    prompt += "\n\n";
    
    prompt += "Please respond ONLY with valid JSON following the response schema above. Do not include any explanatory text outside the JSON.";
    
    return prompt;
}

CompactSequence AiPromptGenerator::createCompactSequence(NoteNagaMidiSeq *sequence) {
    CompactSequence cs;
    if (!sequence) return cs;
    
    cs.id = sequence->getId();
    cs.ppq = sequence->getPPQ();
    cs.tempo = sequence->getTempo();
    cs.bpm = static_cast<int>(60000000.0 / sequence->getTempo());
    cs.maxTick = sequence->getMaxTick();
    
    for (NoteNagaTrack *track : sequence->getTracks()) {
        cs.tracks.push_back(createCompactTrack(track));
    }
    
    return cs;
}

CompactTrack AiPromptGenerator::createCompactTrack(NoteNagaTrack *track) {
    CompactTrack ct;
    if (!track) return ct;
    
    ct.id = track->getId();
    ct.name = QString::fromStdString(track->getName());
    ct.instrument = track->getInstrument().value_or(0);
    ct.channel = track->getChannel().value_or(0);
    ct.muted = track->isMuted();
    ct.solo = track->isSolo();
    ct.volume = track->getVolume();
    ct.pan = track->getMidiPanOffset();
    ct.colorR = track->getColor().red;
    ct.colorG = track->getColor().green;
    ct.colorB = track->getColor().blue;
    
    // Add notes
    for (const auto &note : track->getNotes()) {
        CompactNote cn;
        cn.note = note.note;
        cn.start = note.start.value_or(0);
        cn.length = note.length.value_or(480);
        cn.velocity = note.velocity.value_or(100);
        if (note.pan.has_value()) {
            cn.pan = note.pan.value();
        }
        ct.notes.push_back(cn);
    }
    
    return ct;
}

QString AiPromptGenerator::getSystemInstructions() {
    return R"PROMPT(You are a MIDI composition assistant for NoteNaga music software.
You can create and modify MIDI melodies, manage tracks, adjust track properties, and control tempo.

=== AVAILABLE COMMANDS ===

1. ADD_NOTE - Add a new note to a track
   Parameters: trackId (int), note (int 0-127), start (int ticks), length (int ticks), velocity (int 0-127), pan (int 0-127, optional)

2. REMOVE_NOTE - Remove a note from a track
   Parameters: trackId (int), note (int), start (int) - matches note by pitch and start position

3. MODIFY_NOTE - Modify an existing note
   Parameters: trackId (int), originalNote (int), originalStart (int), newNote (int), newStart (int), newLength (int), newVelocity (int)

4. ADD_TRACK - Create a new track
   Parameters: name (string), instrument (int 0-127 GM instrument index), channel (int 0-15)

5. REMOVE_TRACK - Delete a track
   Parameters: trackId (int)

6. MODIFY_TRACK - Change track properties
   Parameters: trackId (int), and any of: name (string), instrument (int), channel (int), mute (bool), solo (bool), volume (int 0-100), pan (int -64 to +64), color (string "R,G,B")

7. CLEAR_TRACK - Remove all notes from a track
   Parameters: trackId (int)

8. SET_TEMPO - Set the global tempo
   Parameters: bpm (int, beats per minute, typically 40-240)

9. ADD_TEMPO_EVENT - Add a tempo change at a specific tick position
   Parameters: tick (int), bpm (int)

10. REMOVE_TEMPO_EVENT - Remove a tempo change at a specific tick
    Parameters: tick (int)

=== MUSIC THEORY REFERENCE ===
- MIDI notes: C4 = 60, each semitone = +1
- Common scales: Major (W-W-H-W-W-W-H), Minor (W-H-W-W-H-W-W)
- PPQ (pulses per quarter): typically 480, so quarter note = 480 ticks, eighth = 240, sixteenth = 120
- Velocity: 0-127 (0=silent, 64=mf, 100=f, 127=fff)
- Channel 9 (index 9) is reserved for drums
- Tempo: 60 BPM = 1 beat per second, 120 BPM = 2 beats per second)PROMPT";
}

QString AiPromptGenerator::getResponseSchema() {
    return R"SCHEMA({
  "message": "Optional text message to show user (string)",
  "commands": [
    {
      "type": "ADD_NOTE|REMOVE_NOTE|MODIFY_NOTE|ADD_TRACK|REMOVE_TRACK|MODIFY_TRACK|CLEAR_TRACK",
      "params": { ... command-specific parameters ... }
    }
  ]
})SCHEMA";
}

QString AiPromptGenerator::getInstrumentsList() {
    QString list;
    bool first = true;
    
    for (const auto &instr : GM_INSTRUMENTS) {
        if (!first) list += ", ";
        first = false;
        list += QString("%1:%2").arg(instr.index).arg(QString::fromStdString(instr.name));
    }
    
    return list;
}

} // namespace NoteNagaAI
