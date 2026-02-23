#pragma once

#include "undo_manager.h"
#include <note_naga_engine/core/types.h>
#include <QString>
#include <QVariant>
#include <functional>

class NoteNagaTrack;
class NoteNagaMidiSeq;

/**
 * @brief Callback type for refreshing UI after track operations.
 */
using TrackCommandRefreshCallback = std::function<void()>;

/**
 * @brief Command for adding a new track to a MIDI sequence.
 */
class MidiAddTrackCommand : public UndoCommand {
public:
    /**
     * @brief Construct command for adding a track.
     * @param sequence The sequence to add the track to.
     * @param instrument GM instrument index.
     * @param name Track name.
     * @param channel MIDI channel (-1 for auto-assign).
     */
    MidiAddTrackCommand(NoteNagaMidiSeq *sequence, int instrument, 
                    const QString &name = QString(), int channel = -1);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Add Track"); }
    
    void setRefreshCallback(TrackCommandRefreshCallback callback) { m_refreshCallback = callback; }
    
    /**
     * @brief Get the track that was created.
     * @return Pointer to the created track, or nullptr if not yet executed.
     */
    NoteNagaTrack* getCreatedTrack() const { return m_createdTrack; }

private:
    NoteNagaMidiSeq *m_sequence;
    int m_instrument;
    QString m_name;
    int m_channel;
    NoteNagaTrack *m_createdTrack = nullptr;
    int m_createdTrackIndex = -1;
    TrackCommandRefreshCallback m_refreshCallback;
};

/**
 * @brief Command for removing a track from a MIDI sequence.
 */
class MidiRemoveTrackCommand : public UndoCommand {
public:
    /**
     * @brief Construct command for removing a track.
     * @param sequence The sequence containing the track.
     * @param trackIndex Index of the track to remove.
     */
    MidiRemoveTrackCommand(NoteNagaMidiSeq *sequence, int trackIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Remove Track"); }
    
    void setRefreshCallback(TrackCommandRefreshCallback callback) { m_refreshCallback = callback; }

private:
    NoteNagaMidiSeq *m_sequence;
    int m_trackIndex;
    NoteNagaTrack *m_removedTrack = nullptr;
    TrackCommandRefreshCallback m_refreshCallback;
};

/**
 * @brief Command for duplicating a MIDI track.
 */
class MidiDuplicateTrackCommand : public UndoCommand {
public:
    /**
     * @brief Construct command for duplicating a track.
     * @param sequence The sequence containing the track.
     * @param trackIndex Index of the track to duplicate.
     */
    MidiDuplicateTrackCommand(NoteNagaMidiSeq *sequence, int trackIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Duplicate Track"); }
    
    void setRefreshCallback(TrackCommandRefreshCallback callback) { m_refreshCallback = callback; }

private:
    NoteNagaMidiSeq *m_sequence;
    int m_sourceTrackIndex;
    NoteNagaTrack *m_duplicatedTrack = nullptr;
    int m_duplicatedTrackIndex = -1;
    TrackCommandRefreshCallback m_refreshCallback;
};

/**
 * @brief Command for moving a MIDI track within the sequence.
 */
class MidiMoveTrackCommand : public UndoCommand {
public:
    /**
     * @brief Construct command for moving a track.
     * @param sequence The sequence containing the track.
     * @param fromIndex Original index of the track.
     * @param toIndex Target index for the track.
     */
    MidiMoveTrackCommand(NoteNagaMidiSeq *sequence, int fromIndex, int toIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Move Track"); }
    
    void setRefreshCallback(TrackCommandRefreshCallback callback) { m_refreshCallback = callback; }

private:
    NoteNagaMidiSeq *m_sequence;
    int m_fromIndex;
    int m_toIndex;
    TrackCommandRefreshCallback m_refreshCallback;
};

/**
 * @brief Command for modifying MIDI track properties (name, instrument, channel, etc.)
 */
class MidiModifyTrackPropertyCommand : public UndoCommand {
public:
    enum class Property {
        Name,
        Instrument,
        Channel,
        Muted,
        Volume,
        Pan
    };

    /**
     * @brief Construct command for modifying a track property.
     * @param track The track to modify.
     * @param property Which property to modify.
     * @param oldValue The original value.
     * @param newValue The new value.
     */
    MidiModifyTrackPropertyCommand(NoteNagaTrack *track, Property property,
                               const QVariant &oldValue, const QVariant &newValue);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    
    void setRefreshCallback(TrackCommandRefreshCallback callback) { m_refreshCallback = callback; }

private:
    void applyValue(const QVariant &value);
    
    NoteNagaTrack *m_track;
    Property m_property;
    QVariant m_oldValue;
    QVariant m_newValue;
    TrackCommandRefreshCallback m_refreshCallback;
};

/**
 * @brief Command for clearing all MIDI tracks and creating a new empty one.
 */
class MidiClearTracksCommand : public UndoCommand {
public:
    /**
     * @brief Construct command for clearing all tracks.
     * @param sequence The sequence to clear.
     */
    explicit MidiClearTracksCommand(NoteNagaMidiSeq *sequence);
    ~MidiClearTracksCommand() override;
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Clear All Tracks"); }
    
    void setRefreshCallback(TrackCommandRefreshCallback callback) { m_refreshCallback = callback; }

private:
    NoteNagaMidiSeq *m_sequence;
    std::vector<NoteNagaTrack*> m_removedTracks;  // Owned when undone
    NoteNagaTrack *m_newTrack = nullptr;
    bool m_ownsRemovedTracks = false;
    TrackCommandRefreshCallback m_refreshCallback;
};
