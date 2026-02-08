#pragma once

#include "undo_manager.h"
#include <note_naga_engine/core/types.h>
#include <QList>
#include <functional>

class ArrangementTimelineWidget;
class NoteNagaArrangement;
class NoteNagaArrangementTrack;

/**
 * @brief Base class for arrangement clip commands with common functionality.
 */
class ArrangementClipCommandBase : public UndoCommand {
protected:
    ArrangementTimelineWidget *m_timeline;
    
    // Helper to refresh timeline after execute/undo
    void refreshTimeline();
    
    // Get arrangement pointer
    NoteNagaArrangement* getArrangement() const;
    
    // Check if a MIDI sequence exists
    bool sequenceExists(int sequenceId) const;
    
    // Check if an audio resource exists
    bool audioResourceExists(int resourceId) const;
    
public:
    explicit ArrangementClipCommandBase(ArrangementTimelineWidget *timeline) 
        : m_timeline(timeline) {}
};

/**
 * @brief Command for adding a MIDI clip.
 */
class AddClipCommand : public ArrangementClipCommandBase {
public:
    AddClipCommand(ArrangementTimelineWidget *timeline, const NN_MidiClip_t &clip, int trackIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Add Clip"); }
    bool isValid() const override;
    
private:
    NN_MidiClip_t m_clip;
    int m_trackIndex;
};

/**
 * @brief Command for deleting MIDI clips.
 */
class DeleteClipsCommand : public ArrangementClipCommandBase {
public:
    struct ClipData {
        NN_MidiClip_t clip;
        int trackIndex;
    };
    
    DeleteClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipData> &clips);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    bool isValid() const override;
    
private:
    QList<ClipData> m_clips;
};

/**
 * @brief Command for moving MIDI clips.
 */
class MoveClipsCommand : public ArrangementClipCommandBase {
public:
    struct ClipMoveData {
        int clipId;
        int oldTrackIndex;
        int newTrackIndex;
        int64_t oldStartTick;
        int64_t newStartTick;
        int sequenceId = -1;  // For validity check
    };
    
    MoveClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipMoveData> &moves);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Move Clips"); }
    bool isValid() const override;
    
private:
    QList<ClipMoveData> m_moves;
};

/**
 * @brief Command for resizing a MIDI clip.
 */
class ResizeClipCommand : public ArrangementClipCommandBase {
public:
    ResizeClipCommand(ArrangementTimelineWidget *timeline, int clipId,
                      int64_t oldStartTick, int64_t oldDuration,
                      int64_t newStartTick, int64_t newDuration,
                      int sequenceId = -1);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Resize Clip"); }
    bool isValid() const override;
    
private:
    int m_clipId;
    int64_t m_oldStartTick;
    int64_t m_oldDuration;
    int64_t m_newStartTick;
    int64_t m_newDuration;
    int m_sequenceId = -1;
};

/**
 * @brief Command for duplicating MIDI clips.
 */
class DuplicateClipsCommand : public ArrangementClipCommandBase {
public:
    struct ClipData {
        NN_MidiClip_t clip;  // Original clip data
        int trackIndex;
        int64_t newStartTick;  // Computed safe position
    };
    
    DuplicateClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipData> &clips);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Duplicate Clips"); }
    bool isValid() const override;
    
private:
    QList<ClipData> m_clips;
    QList<int> m_createdClipIds;  // IDs of created clips for undo
};

/**
 * @brief Command for pasting MIDI clips.
 */
class PasteClipsCommand : public ArrangementClipCommandBase {
public:
    struct ClipData {
        NN_MidiClip_t clip;
        int trackIndex;
    };
    
    PasteClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipData> &clips);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Paste Clips"); }
    bool isValid() const override;
    
private:
    QList<ClipData> m_clips;
};

/**
 * @brief Command for adding an audio clip.
 */
class AddAudioClipCommand : public ArrangementClipCommandBase {
public:
    AddAudioClipCommand(ArrangementTimelineWidget *timeline, const NN_AudioClip_t &clip, int trackIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Add Audio Clip"); }
    bool isValid() const override;
    
private:
    NN_AudioClip_t m_clip;
    int m_trackIndex;
};

/**
 * @brief Command for deleting audio clips.
 */
class DeleteAudioClipsCommand : public ArrangementClipCommandBase {
public:
    struct AudioClipData {
        NN_AudioClip_t clip;
        int trackIndex;
    };
    
    DeleteAudioClipsCommand(ArrangementTimelineWidget *timeline, const QList<AudioClipData> &clips);
    
    void execute() override;
    void undo() override;
    QString description() const override;
    bool isValid() const override;
    
private:
    QList<AudioClipData> m_clips;
};

/**
 * @brief Command for moving audio clips.
 */
class MoveAudioClipsCommand : public ArrangementClipCommandBase {
public:
    struct AudioClipMoveData {
        int clipId;
        int oldTrackIndex;
        int newTrackIndex;
        int64_t oldStartTick;
        int64_t newStartTick;
        int resourceId = -1;  // For validity check
    };
    
    MoveAudioClipsCommand(ArrangementTimelineWidget *timeline, const QList<AudioClipMoveData> &moves);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Move Audio Clips"); }
    bool isValid() const override;
    
private:
    QList<AudioClipMoveData> m_moves;
};
/**
 * @brief Command for resizing an audio clip.
 */
class ResizeAudioClipCommand : public ArrangementClipCommandBase {
public:
    ResizeAudioClipCommand(ArrangementTimelineWidget *timeline, int clipId,
                           int64_t oldStartTick, int64_t oldDuration,
                           int64_t newStartTick, int64_t newDuration,
                           int resourceId = -1);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Resize Audio Clip"); }
    bool isValid() const override;
    
private:
    int m_clipId;
    int64_t m_oldStartTick;
    int64_t m_oldDuration;
    int64_t m_newStartTick;
    int64_t m_newDuration;
    int m_resourceId = -1;
};

/**
 * @brief Command for adding a track.
 */
class AddTrackCommand : public ArrangementClipCommandBase {
public:
    AddTrackCommand(ArrangementTimelineWidget *timeline, const QString &name);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Add Track"); }
    
private:
    QString m_name;
    int m_createdTrackIndex = -1;
};

/**
 * @brief Command for deleting a track (including all its clips).
 */
class DeleteTrackCommand : public ArrangementClipCommandBase {
public:
    DeleteTrackCommand(ArrangementTimelineWidget *timeline, int trackIndex);
    
    void execute() override;
    void undo() override;
    QString description() const override { return QObject::tr("Delete Track"); }
    
private:
    int m_trackIndex;
    QString m_trackName;
    NN_Color_t m_trackColor;
    bool m_muted = false;
    bool m_solo = false;
    float m_volume = 1.0f;
    float m_pan = 0.0f;
    QList<NN_MidiClip_t> m_midiClips;
    QList<NN_AudioClip_t> m_audioClips;
};