#include "ai_prompt_generator.h"

#include <note_naga_engine/core/types.h>
#include <QJsonDocument>

namespace NoteNagaAI {

QString AiPromptGenerator::generateFullPrompt(const QString &userPrompt, NoteNagaMidiSeq *sequence,
                                               const QList<ChatMessage> &chatHistory,
                                               int targetDurationSec) {
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
        prompt += QString("=== CONTEXT: Composition ends at tick %1. Add new content after this position. ===\n\n").arg(maxTick);
    }
    
    // Target duration guidance
    if (targetDurationSec > 0) {
        int bpm = compactSeq.bpm;
        int ppq = compactSeq.ppq;
        
        // Calculate target ticks based on duration and tempo
        // ticks = seconds * (bpm / 60) * ppq
        int targetTicks = static_cast<int>(targetDurationSec * (bpm / 60.0) * ppq);
        int targetBars = targetTicks / (ppq * 4); // Assuming 4/4 time
        int startTick = maxTick > 0 ? maxTick : 0;
        int endTick = startTick + targetTicks;
        
        // Estimate notes needed (rough: 2-4 notes per beat per track)
        int beatsNeeded = targetDurationSec * bpm / 60;
        int minNotes = beatsNeeded * 2;
        int maxNotes = beatsNeeded * 8;
        
        prompt += "=== TARGET DURATION ===\n";
        prompt += QString("User wants approximately %1 seconds of music.\n").arg(targetDurationSec);
        prompt += QString("At %1 BPM with PPQ=%2:\n").arg(bpm).arg(ppq);
        prompt += QString("- Generate content from tick %1 to ~%2\n").arg(startTick).arg(endTick);
        prompt += QString("- This equals roughly %1 bars (4/4 time)\n").arg(targetBars);
        prompt += QString("- Create %1-%2 notes total across all tracks\n").arg(minNotes).arg(maxNotes);
        prompt += "- Fill the ENTIRE duration, don't stop early!\n";
        prompt += "- Create complete musical phrases and sections\n\n";
    }
    
    // Available instruments (condensed)
    prompt += "=== AVAILABLE GM INSTRUMENTS (index: name) ===\n";
    prompt += getInstrumentsList();
    prompt += "\n\n";
    
    // Include conversation history for context - dynamically determined by size
    // Reserve approx 6000 chars for history to stay within reasonable prompt limits
    // (Full prompt ~8000 chars base + sequence data which varies)
    const int MAX_HISTORY_CHARS = 6000;
    
    if (!chatHistory.isEmpty()) {
        QString historySection;
        int totalChars = 0;
        
        // Build history from most recent backwards
        QStringList historyLines;
        for (int i = chatHistory.size() - 1; i >= 0; --i) {
            const ChatMessage &msg = chatHistory[i];
            QString line;
            
            if (msg.role == ChatMessage::Role::User) {
                // Include user message - truncate if very long
                QString userText = msg.displayText;
                if (userText.length() > 200) {
                    userText = userText.left(200) + "...";
                }
                line = QString("User: %1\n").arg(userText);
            } else if (msg.role == ChatMessage::Role::Assistant) {
                // Just indicate status, don't include full response
                if (msg.executed) {
                    line = QString("Assistant: [Executed changes successfully]\n");
                } else {
                    line = QString("Assistant: [Failed to execute]\n");
                }
            }
            
            // Check if adding this line would exceed limit
            if (totalChars + line.length() > MAX_HISTORY_CHARS) {
                break;  // Stop adding more history
            }
            
            historyLines.prepend(line);  // Prepend to maintain chronological order
            totalChars += line.length();
        }
        
        if (!historyLines.isEmpty()) {
            prompt += "=== CONVERSATION HISTORY ===\n";
            for (const QString &line : historyLines) {
                prompt += line;
            }
            prompt += "\n";
        }
    }
    
    // User's current request
    prompt += "=== REQUEST ===\n";
    prompt += userPrompt;
    prompt += "\n\nRespond with NNC commands only:";
    
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
    return R"PROMPT(MIDI composition assistant for NoteNaga. Create/modify melodies, tracks, tempo.

=== RULES ===
- NEVER overlap notes with same pitch on same track (causes stuck notes)
- When EXTENDING existing music: add notes AFTER maxTick, don't overwrite
- When user asks for COMPLETELY DIFFERENT/NEW music: use TC:tid to clear tracks FIRST, then generate new notes from tick 0
- Keywords meaning "replace all": "different", "new", "change style", "don't like", "something else", "start over", "jiny", "novy", "uplne", "nelibi"
- Channel 9 is drums only
- Note pitch: 0-127, velocity: 0-127

=== INPUT FORMAT (JSON) ===
Sequence: {bpm, ppq, maxTick, tracks[]}
Track: {id, name, inst, ch, vol, pan, mute, solo, notes[]}
Note: {n=pitch, s=start, l=length, v=velocity}

=== OUTPUT FORMAT (NNC) ===
One command per line. NO JSON, NO markdown!

M:message                                     Optional message (first line)

NOTES:
N+tid,pitch,start,len,vel                     Add note
N-tid,pitch,start                             Remove note
N*tid,oPitch,oStart>nPitch,nStart,nLen,nVel   Modify note

CHORDS/ARPEGGIOS:
CHORD:tid,root,type,start,len,vel             maj/min/dim/aug/7/maj7/min7/sus2/sus4
ARP:tid,root,type,start,len,vel,dir           dir: up/down/updown
SCALE:tid,root,type,start,len,vel             major/minor/penta/blues/dorian

BATCH:
PAT:tid,notes,start,len,vel                   notes: 60-64-67
DUP:tid,srcStart,srcEnd,destStart             Copy range
TRANS:tid,start,end,semitones                 Transpose
QUANT:tid,start,end,grid                      Quantize to grid

DRUMS:
DRUM:tid,pattern,start,bars                   rock/pop/hiphop/jazz/metal/shuffle

TRACKS:
T+name|inst|ch                                Add track
T-tid                                         Remove track
T*tid i=inst v=vol p=pan m=0/1 s=0/1          Modify track
TC:tid                                        Clear ALL notes from track
TCA                                           Clear ALL tracks (use when replacing entire composition)

TEMPO:
BPM:value                                     Set tempo
TE+tick,bpm                                   Add tempo event

=== REFERENCE ===
Notes: C4=60 | Octave=12 semitones | PPQ=480 (quarter=480, 8th=240, 16th=120)
Velocity: pp=32 mp=64 mf=80 f=100 ff=120
Chords: maj[0,4,7] min[0,3,7] 7[0,4,7,10] maj7[0,4,7,11]
Drums(ch9): 36=kick 38=snare 42=hihat 46=open 49=crash 51=ride
Progressions: I-V-vi-IV | ii-V-I | I-IV-V-I)PROMPT";
}

QString AiPromptGenerator::getResponseSchema() {
    return R"SCHEMA(Output NNC commands only. Example:
M:Added 4-bar melody
N+0,60,0,480,100
N+0,64,480,480,90
CHORD:0,60,maj,1920,960,85)SCHEMA";
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
