#pragma once

#include "ai_types.h"
#include "../undo/undo_manager.h"

#include <QString>
#include <QList>
#include <QPair>
#include <tuple>
#include <functional>

class MidiEditorWidget;
class NoteNagaMidiSeq;
class NoteNagaTrack;
struct NN_Note_t;

namespace NoteNagaAI {

// Forward declaration
class AiModificationCommand;

/**
 * @brief Result of executing AI commands.
 */
struct ExecutionResult {
    bool success = false;
    int commandsExecuted = 0;
    int commandsFailed = 0;
    QString errorMessage;
    QStringList warnings;
    
    bool hasWarnings() const { return !warnings.isEmpty(); }
};

/**
 * @brief Executes AI commands on MIDI sequence with undo support.
 */
class AiCommandExecutor {
public:
    /**
     * @brief Constructs executor with required dependencies.
     * @param editor MIDI editor widget (for refresh and undo manager).
     * @param sequence Active MIDI sequence.
     */
    AiCommandExecutor(MidiEditorWidget *editor, NoteNagaMidiSeq *sequence);
    
    /**
     * @brief Executes all commands in the AI response.
     *        Creates a single compound undo command for all changes.
     * @param response Parsed AI response.
     * @return Execution result with success/failure info.
     */
    ExecutionResult execute(const AiResponse &response);
    
    /**
     * @brief Sets callback for track list changes (add/remove track).
     */
    void setTrackListChangedCallback(std::function<void()> callback) {
        m_trackListChangedCallback = callback;
    }

private:
    bool executeCommand(const AiCommand &cmd, AiModificationCommand *compound, QString &error);
    
    bool executeAddNote(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeRemoveNote(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeModifyNote(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeAddTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeRemoveTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeModifyTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeClearTrack(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeSetTempo(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeAddTempoEvent(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    bool executeRemoveTempoEvent(const QJsonObject &params, AiModificationCommand *compound, QString &error);
    
    NoteNagaTrack* findTrackById(int trackId);
    NN_Note_t* findNote(NoteNagaTrack *track, int note, int start);
    
    MidiEditorWidget *m_editor;
    NoteNagaMidiSeq *m_sequence;
    std::function<void()> m_trackListChangedCallback;
};

/**
 * @brief Compound undo command for AI modifications.
 *        Groups multiple operations into a single undo step.
 */
class AiModificationCommand : public UndoCommand {
public:
    AiModificationCommand();
    ~AiModificationCommand() override;
    
    void addAddedNote(NoteNagaTrack *track, const NN_Note_t &note);
    void addRemovedNote(NoteNagaTrack *track, const NN_Note_t &note);
    void addModifiedNote(NoteNagaTrack *track, const NN_Note_t &oldNote, const NN_Note_t &newNote);
    void addAddedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track);
    void addRemovedTrack(NoteNagaMidiSeq *seq, NoteNagaTrack *track, int index);
    void addTrackPropertyChange(NoteNagaTrack *track, const QString &property, 
                                const QVariant &oldValue, const QVariant &newValue);
    void addTempoChange(NoteNagaMidiSeq *seq, int oldTempo, int newTempo);
    void addTempoEventAdd(NoteNagaMidiSeq *seq, int tick, int tempo);
    void addTempoEventRemove(NoteNagaMidiSeq *seq, int tick, int tempo);
    
    void setRefreshCallback(std::function<void()> callback) { m_refreshCallback = callback; }
    void setTrackListCallback(std::function<void()> callback) { m_trackListCallback = callback; }
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
    bool isEmpty() const { return m_isEmpty; }
    int operationCount() const { return m_operationCount; }

private:
    void refreshAll();
    
    // Added notes: <track, note>
    QList<QPair<NoteNagaTrack*, NN_Note_t>> m_addedNotes;
    
    // Removed notes: <track, note>
    QList<QPair<NoteNagaTrack*, NN_Note_t>> m_removedNotes;
    
    // Modified notes: <track, oldNote, newNote>
    QList<std::tuple<NoteNagaTrack*, NN_Note_t, NN_Note_t>> m_modifiedNotes;
    
    // Added tracks: <sequence, track>
    QList<QPair<NoteNagaMidiSeq*, NoteNagaTrack*>> m_addedTracks;
    
    // Removed tracks: <sequence, track, originalIndex>
    QList<std::tuple<NoteNagaMidiSeq*, NoteNagaTrack*, int>> m_removedTracks;
    
    // Track property changes: <track, property, oldValue, newValue>
    QList<std::tuple<NoteNagaTrack*, QString, QVariant, QVariant>> m_trackPropertyChanges;
    
    // Tempo changes: <sequence, oldTempo, newTempo>
    QList<std::tuple<NoteNagaMidiSeq*, int, int>> m_tempoChanges;
    
    // Added tempo events: <sequence, tick, tempo>
    QList<std::tuple<NoteNagaMidiSeq*, int, int>> m_addedTempoEvents;
    
    // Removed tempo events: <sequence, tick, tempo>
    QList<std::tuple<NoteNagaMidiSeq*, int, int>> m_removedTempoEvents;
    
    std::function<void()> m_refreshCallback;
    std::function<void()> m_trackListCallback;
    
    bool m_isEmpty = true;
    int m_operationCount = 0;
};

} // namespace NoteNagaAI
