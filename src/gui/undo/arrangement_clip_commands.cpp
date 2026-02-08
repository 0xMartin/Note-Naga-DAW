#include "arrangement_clip_commands.h"
#include "../editor/arrangement_timeline_widget.h"
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/audio/audio_manager.h>

// ==== Base class helpers ====

void ArrangementClipCommandBase::refreshTimeline()
{
    if (m_timeline) {
        m_timeline->refreshFromArrangement();
        m_timeline->update();
    }
}

NoteNagaArrangement* ArrangementClipCommandBase::getArrangement() const
{
    if (!m_timeline) return nullptr;
    return m_timeline->getArrangement();
}

bool ArrangementClipCommandBase::sequenceExists(int sequenceId) const
{
    if (!m_timeline) return false;
    
    NoteNagaEngine *engine = m_timeline->getEngine();
    if (!engine || !engine->getRuntimeData()) return false;
    
    // Check if sequence with this ID exists in runtime data
    for (const auto *seq : engine->getRuntimeData()->getSequences()) {
        if (seq && seq->getId() == sequenceId) {
            return true;
        }
    }
    return false;
}

bool ArrangementClipCommandBase::audioResourceExists(int resourceId) const
{
    if (!m_timeline) return false;
    
    NoteNagaEngine *engine = m_timeline->getEngine();
    if (!engine || !engine->getRuntimeData()) return false;
    
    // Check if audio resource exists
    return engine->getRuntimeData()->getAudioManager().getResource(resourceId) != nullptr;
}

// ==== AddClipCommand ====

AddClipCommand::AddClipCommand(ArrangementTimelineWidget *timeline, const NN_MidiClip_t &clip, int trackIndex)
    : ArrangementClipCommandBase(timeline)
    , m_clip(clip)
    , m_trackIndex(trackIndex)
{
}

void AddClipCommand::execute()
{
    // Skip if the sequence no longer exists
    if (!sequenceExists(m_clip.sequenceId)) return;
    
    NoteNagaArrangement *arr = getArrangement();
    if (!arr || m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    track->addClip(m_clip);
    arr->updateMaxTick();
    refreshTimeline();
}

void AddClipCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr || m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    track->removeClip(m_clip.id);
    arr->updateMaxTick();
    refreshTimeline();
}

bool AddClipCommand::isValid() const
{
    return sequenceExists(m_clip.sequenceId);
}

// ==== DeleteClipsCommand ====

DeleteClipsCommand::DeleteClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipData> &clips)
    : ArrangementClipCommandBase(timeline)
    , m_clips(clips)
{
}

void DeleteClipsCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &data : m_clips) {
        if (data.trackIndex >= 0 && data.trackIndex < static_cast<int>(arr->getTrackCount())) {
            NoteNagaArrangementTrack *track = arr->getTracks()[data.trackIndex];
            if (track) {
                track->removeClip(data.clip.id);
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

void DeleteClipsCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &data : m_clips) {
        // Skip clips whose sequence no longer exists
        if (!sequenceExists(data.clip.sequenceId)) continue;
        
        if (data.trackIndex >= 0 && data.trackIndex < static_cast<int>(arr->getTrackCount())) {
            NoteNagaArrangementTrack *track = arr->getTracks()[data.trackIndex];
            if (track) {
                NN_MidiClip_t clip = data.clip;
                track->addClip(clip);
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

QString DeleteClipsCommand::description() const
{
    if (m_clips.size() == 1) {
        return QObject::tr("Delete Clip");
    }
    return QObject::tr("Delete %1 Clips").arg(m_clips.size());
}

bool DeleteClipsCommand::isValid() const
{
    // Command is valid if at least one clip's sequence still exists
    for (const auto &data : m_clips) {
        if (sequenceExists(data.clip.sequenceId)) {
            return true;
        }
    }
    return false;
}

// ==== MoveClipsCommand ====

MoveClipsCommand::MoveClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipMoveData> &moves)
    : ArrangementClipCommandBase(timeline)
    , m_moves(moves)
{
}

void MoveClipsCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &move : m_moves) {
        // Find the clip
        for (int tIdx = 0; tIdx < static_cast<int>(arr->getTrackCount()); ++tIdx) {
            NoteNagaArrangementTrack *track = arr->getTracks()[tIdx];
            if (!track) continue;
            
            for (auto &clip : track->getClips()) {
                if (clip.id == move.clipId) {
                    // If track changed, move clip to new track
                    if (tIdx != move.newTrackIndex && move.newTrackIndex >= 0 && 
                        move.newTrackIndex < static_cast<int>(arr->getTrackCount())) {
                        NN_MidiClip_t clipCopy = clip;
                        clipCopy.startTick = move.newStartTick;
                        track->removeClip(clip.id);
                        arr->getTracks()[move.newTrackIndex]->addClip(clipCopy);
                    } else {
                        clip.startTick = move.newStartTick;
                    }
                    break;
                }
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

void MoveClipsCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &move : m_moves) {
        // Find the clip
        for (int tIdx = 0; tIdx < static_cast<int>(arr->getTrackCount()); ++tIdx) {
            NoteNagaArrangementTrack *track = arr->getTracks()[tIdx];
            if (!track) continue;
            
            for (auto &clip : track->getClips()) {
                if (clip.id == move.clipId) {
                    // If track changed, move clip back
                    if (tIdx != move.oldTrackIndex && move.oldTrackIndex >= 0 &&
                        move.oldTrackIndex < static_cast<int>(arr->getTrackCount())) {
                        NN_MidiClip_t clipCopy = clip;
                        clipCopy.startTick = move.oldStartTick;
                        track->removeClip(clip.id);
                        arr->getTracks()[move.oldTrackIndex]->addClip(clipCopy);
                    } else {
                        clip.startTick = move.oldStartTick;
                    }
                    break;
                }
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

bool MoveClipsCommand::isValid() const
{
    // Command is valid if at least one clip's sequence still exists
    for (const auto &move : m_moves) {
        if (move.sequenceId >= 0 && sequenceExists(move.sequenceId)) {
            return true;
        }
    }
    // If no sequenceId stored, assume valid (backwards compatibility)
    for (const auto &move : m_moves) {
        if (move.sequenceId < 0) {
            return true;
        }
    }
    return false;
}

// ==== ResizeClipCommand ====

ResizeClipCommand::ResizeClipCommand(ArrangementTimelineWidget *timeline, int clipId,
                                     int64_t oldStartTick, int64_t oldDuration,
                                     int64_t newStartTick, int64_t newDuration,
                                     int sequenceId)
    : ArrangementClipCommandBase(timeline)
    , m_clipId(clipId)
    , m_oldStartTick(oldStartTick)
    , m_oldDuration(oldDuration)
    , m_newStartTick(newStartTick)
    , m_newDuration(newDuration)
    , m_sequenceId(sequenceId)
{
}

void ResizeClipCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getClips()) {
            if (clip.id == m_clipId) {
                clip.startTick = m_newStartTick;
                clip.durationTicks = m_newDuration;
                arr->updateMaxTick();
                refreshTimeline();
                return;
            }
        }
    }
}

void ResizeClipCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getClips()) {
            if (clip.id == m_clipId) {
                clip.startTick = m_oldStartTick;
                clip.durationTicks = m_oldDuration;
                arr->updateMaxTick();
                refreshTimeline();
                return;
            }
        }
    }
}

bool ResizeClipCommand::isValid() const
{
    // If sequenceId was not stored, assume valid (backwards compatibility)
    if (m_sequenceId < 0) return true;
    return sequenceExists(m_sequenceId);
}

// ==== DuplicateClipsCommand ====

DuplicateClipsCommand::DuplicateClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipData> &clips)
    : ArrangementClipCommandBase(timeline)
    , m_clips(clips)
{
}

void DuplicateClipsCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    m_createdClipIds.clear();
    
    for (const auto &data : m_clips) {
        // Skip clips whose sequence no longer exists
        if (!sequenceExists(data.clip.sequenceId)) continue;
        
        if (data.trackIndex >= 0 && data.trackIndex < static_cast<int>(arr->getTrackCount())) {
            NoteNagaArrangementTrack *track = arr->getTracks()[data.trackIndex];
            if (track) {
                NN_MidiClip_t newClip = data.clip;
                newClip.id = 0;  // Will be assigned
                newClip.startTick = data.newStartTick;
                track->addClip(newClip);
                m_createdClipIds.append(newClip.id);
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

void DuplicateClipsCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (int clipId : m_createdClipIds) {
        for (auto *track : arr->getTracks()) {
            if (track && track->removeClip(clipId)) {
                break;
            }
        }
    }
    m_createdClipIds.clear();
    arr->updateMaxTick();
    refreshTimeline();
}

bool DuplicateClipsCommand::isValid() const
{
    // Command is valid if at least one clip's sequence still exists
    for (const auto &data : m_clips) {
        if (sequenceExists(data.clip.sequenceId)) {
            return true;
        }
    }
    return false;
}

// ==== PasteClipsCommand ====

PasteClipsCommand::PasteClipsCommand(ArrangementTimelineWidget *timeline, const QList<ClipData> &clips)
    : ArrangementClipCommandBase(timeline)
    , m_clips(clips)
{
}

void PasteClipsCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &data : m_clips) {
        // Skip clips whose sequence no longer exists
        if (!sequenceExists(data.clip.sequenceId)) continue;
        
        if (data.trackIndex >= 0 && data.trackIndex < static_cast<int>(arr->getTrackCount())) {
            NoteNagaArrangementTrack *track = arr->getTracks()[data.trackIndex];
            if (track) {
                NN_MidiClip_t clip = data.clip;
                track->addClip(clip);
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

void PasteClipsCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &data : m_clips) {
        for (auto *track : arr->getTracks()) {
            if (track && track->removeClip(data.clip.id)) {
                break;
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

bool PasteClipsCommand::isValid() const
{
    // Command is valid if at least one clip's sequence still exists
    for (const auto &data : m_clips) {
        if (sequenceExists(data.clip.sequenceId)) {
            return true;
        }
    }
    return false;
}

// ==== AddAudioClipCommand ====

AddAudioClipCommand::AddAudioClipCommand(ArrangementTimelineWidget *timeline, const NN_AudioClip_t &clip, int trackIndex)
    : ArrangementClipCommandBase(timeline)
    , m_clip(clip)
    , m_trackIndex(trackIndex)
{
}

void AddAudioClipCommand::execute()
{
    // Skip if the audio resource no longer exists
    if (!audioResourceExists(m_clip.audioResourceId)) return;
    
    NoteNagaArrangement *arr = getArrangement();
    if (!arr || m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    track->addAudioClip(m_clip);
    arr->updateMaxTick();
    refreshTimeline();
}

void AddAudioClipCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr || m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    track->removeAudioClip(m_clip.id);
    arr->updateMaxTick();
    refreshTimeline();
}

bool AddAudioClipCommand::isValid() const
{
    return audioResourceExists(m_clip.audioResourceId);
}

// ==== DeleteAudioClipsCommand ====

DeleteAudioClipsCommand::DeleteAudioClipsCommand(ArrangementTimelineWidget *timeline, const QList<AudioClipData> &clips)
    : ArrangementClipCommandBase(timeline)
    , m_clips(clips)
{
}

void DeleteAudioClipsCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &data : m_clips) {
        if (data.trackIndex >= 0 && data.trackIndex < static_cast<int>(arr->getTrackCount())) {
            NoteNagaArrangementTrack *track = arr->getTracks()[data.trackIndex];
            if (track) {
                track->removeAudioClip(data.clip.id);
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

void DeleteAudioClipsCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &data : m_clips) {
        // Skip clips whose audio resource no longer exists
        if (!audioResourceExists(data.clip.audioResourceId)) continue;
        
        if (data.trackIndex >= 0 && data.trackIndex < static_cast<int>(arr->getTrackCount())) {
            NoteNagaArrangementTrack *track = arr->getTracks()[data.trackIndex];
            if (track) {
                NN_AudioClip_t clip = data.clip;
                track->addAudioClip(clip);
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

QString DeleteAudioClipsCommand::description() const
{
    if (m_clips.size() == 1) {
        return QObject::tr("Delete Audio Clip");
    }
    return QObject::tr("Delete %1 Audio Clips").arg(m_clips.size());
}

bool DeleteAudioClipsCommand::isValid() const
{
    // Command is valid if at least one clip's audio resource still exists
    for (const auto &data : m_clips) {
        if (audioResourceExists(data.clip.audioResourceId)) {
            return true;
        }
    }
    return false;
}

// ==== MoveAudioClipsCommand ====

MoveAudioClipsCommand::MoveAudioClipsCommand(ArrangementTimelineWidget *timeline, const QList<AudioClipMoveData> &moves)
    : ArrangementClipCommandBase(timeline)
    , m_moves(moves)
{
}

void MoveAudioClipsCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &move : m_moves) {
        for (int tIdx = 0; tIdx < static_cast<int>(arr->getTrackCount()); ++tIdx) {
            NoteNagaArrangementTrack *track = arr->getTracks()[tIdx];
            if (!track) continue;
            
            for (auto &clip : track->getAudioClips()) {
                if (clip.id == move.clipId) {
                    if (tIdx != move.newTrackIndex && move.newTrackIndex >= 0 && 
                        move.newTrackIndex < static_cast<int>(arr->getTrackCount())) {
                        NN_AudioClip_t clipCopy = clip;
                        clipCopy.startTick = move.newStartTick;
                        track->removeAudioClip(clip.id);
                        arr->getTracks()[move.newTrackIndex]->addAudioClip(clipCopy);
                    } else {
                        clip.startTick = move.newStartTick;
                    }
                    break;
                }
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

void MoveAudioClipsCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (const auto &move : m_moves) {
        for (int tIdx = 0; tIdx < static_cast<int>(arr->getTrackCount()); ++tIdx) {
            NoteNagaArrangementTrack *track = arr->getTracks()[tIdx];
            if (!track) continue;
            
            for (auto &clip : track->getAudioClips()) {
                if (clip.id == move.clipId) {
                    if (tIdx != move.oldTrackIndex && move.oldTrackIndex >= 0 &&
                        move.oldTrackIndex < static_cast<int>(arr->getTrackCount())) {
                        NN_AudioClip_t clipCopy = clip;
                        clipCopy.startTick = move.oldStartTick;
                        track->removeAudioClip(clip.id);
                        arr->getTracks()[move.oldTrackIndex]->addAudioClip(clipCopy);
                    } else {
                        clip.startTick = move.oldStartTick;
                    }
                    break;
                }
            }
        }
    }
    arr->updateMaxTick();
    refreshTimeline();
}

bool MoveAudioClipsCommand::isValid() const
{
    // Command is valid if at least one clip's audio resource still exists
    for (const auto &move : m_moves) {
        if (move.resourceId >= 0 && audioResourceExists(move.resourceId)) {
            return true;
        }
    }
    // If no resourceId stored, assume valid (backwards compatibility)
    for (const auto &move : m_moves) {
        if (move.resourceId < 0) {
            return true;
        }
    }
    return false;
}
// ==== ResizeAudioClipCommand ====

ResizeAudioClipCommand::ResizeAudioClipCommand(ArrangementTimelineWidget *timeline, int clipId,
                                               int64_t oldStartTick, int64_t oldDuration, int64_t oldOffsetTicks,
                                               int64_t newStartTick, int64_t newDuration, int64_t newOffsetTicks,
                                               int resourceId)
    : ArrangementClipCommandBase(timeline)
    , m_clipId(clipId)
    , m_oldStartTick(oldStartTick)
    , m_oldDuration(oldDuration)
    , m_oldOffsetTicks(oldOffsetTicks)
    , m_newStartTick(newStartTick)
    , m_newDuration(newDuration)
    , m_newOffsetTicks(newOffsetTicks)
    , m_resourceId(resourceId)
{
}

void ResizeAudioClipCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getAudioClips()) {
            if (clip.id == m_clipId) {
                clip.startTick = m_newStartTick;
                clip.durationTicks = m_newDuration;
                clip.offsetTicks = m_newOffsetTicks;
                arr->updateMaxTick();
                refreshTimeline();
                return;
            }
        }
    }
}

void ResizeAudioClipCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getAudioClips()) {
            if (clip.id == m_clipId) {
                clip.startTick = m_oldStartTick;
                clip.durationTicks = m_oldDuration;
                clip.offsetTicks = m_oldOffsetTicks;
                arr->updateMaxTick();
                refreshTimeline();
                return;
            }
        }
    }
}

bool ResizeAudioClipCommand::isValid() const
{
    // If resourceId was not stored, assume valid (backwards compatibility)
    if (m_resourceId < 0) return true;
    return audioResourceExists(m_resourceId);
}

// ==== AddTrackCommand ====

AddTrackCommand::AddTrackCommand(ArrangementTimelineWidget *timeline, const QString &name)
    : ArrangementClipCommandBase(timeline)
    , m_name(name)
{
}

void AddTrackCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    m_createdTrackIndex = static_cast<int>(arr->getTrackCount());
    arr->addTrack(m_name.toStdString());
    refreshTimeline();
}

void AddTrackCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    if (m_createdTrackIndex >= 0 && m_createdTrackIndex < static_cast<int>(arr->getTrackCount())) {
        arr->removeTrack(m_createdTrackIndex);
        refreshTimeline();
    }
}

// ==== DeleteTrackCommand ====

DeleteTrackCommand::DeleteTrackCommand(ArrangementTimelineWidget *timeline, int trackIndex)
    : ArrangementClipCommandBase(timeline)
    , m_trackIndex(trackIndex)
{
}

void DeleteTrackCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    if (m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    // Save track data for undo
    m_trackName = QString::fromStdString(track->getName());
    m_trackColor = track->getColor();
    m_muted = track->isMuted();
    m_solo = track->isSolo();
    m_volume = track->getVolume();
    m_pan = track->getPan();
    
    // Save all clips
    m_midiClips.clear();
    for (const auto &clip : track->getClips()) {
        m_midiClips.append(clip);
    }
    
    m_audioClips.clear();
    for (const auto &clip : track->getAudioClips()) {
        m_audioClips.append(clip);
    }
    
    // Delete the track by index
    arr->removeTrackByIndex(m_trackIndex);
    arr->updateMaxTick();
    refreshTimeline();
}

void DeleteTrackCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    // Re-insert track at original position
    arr->insertTrack(m_trackIndex, m_trackName.toStdString());
    
    if (m_trackIndex < static_cast<int>(arr->getTrackCount())) {
        NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
        if (track) {
            track->setColor(m_trackColor);
            track->setMuted(m_muted);
            track->setSolo(m_solo);
            track->setVolume(m_volume);
            track->setPan(m_pan);
            
            // Restore all clips
            for (const auto &clip : m_midiClips) {
                track->addClip(clip);
            }
            for (const auto &clip : m_audioClips) {
                track->addAudioClip(clip);
            }
        }
    }
    
    arr->updateMaxTick();
    refreshTimeline();
}

// ==== ChangeMidiClipFadeCommand ====

ChangeMidiClipFadeCommand::ChangeMidiClipFadeCommand(ArrangementTimelineWidget *timeline, int clipId,
                                                     int oldFadeIn, int oldFadeOut,
                                                     int newFadeIn, int newFadeOut,
                                                     int sequenceId)
    : ArrangementClipCommandBase(timeline)
    , m_clipId(clipId)
    , m_oldFadeIn(oldFadeIn)
    , m_oldFadeOut(oldFadeOut)
    , m_newFadeIn(newFadeIn)
    , m_newFadeOut(newFadeOut)
    , m_sequenceId(sequenceId)
{
}

void ChangeMidiClipFadeCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getClips()) {
            if (clip.id == m_clipId) {
                clip.fadeInTicks = m_newFadeIn;
                clip.fadeOutTicks = m_newFadeOut;
                refreshTimeline();
                return;
            }
        }
    }
}

void ChangeMidiClipFadeCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getClips()) {
            if (clip.id == m_clipId) {
                clip.fadeInTicks = m_oldFadeIn;
                clip.fadeOutTicks = m_oldFadeOut;
                refreshTimeline();
                return;
            }
        }
    }
}

bool ChangeMidiClipFadeCommand::isValid() const
{
    if (m_sequenceId < 0) return true;
    return sequenceExists(m_sequenceId);
}

// ==== ChangeAudioClipFadeCommand ====

ChangeAudioClipFadeCommand::ChangeAudioClipFadeCommand(ArrangementTimelineWidget *timeline, int clipId,
                                                       int oldFadeIn, int oldFadeOut,
                                                       int newFadeIn, int newFadeOut,
                                                       int resourceId)
    : ArrangementClipCommandBase(timeline)
    , m_clipId(clipId)
    , m_oldFadeIn(oldFadeIn)
    , m_oldFadeOut(oldFadeOut)
    , m_newFadeIn(newFadeIn)
    , m_newFadeOut(newFadeOut)
    , m_resourceId(resourceId)
{
}

void ChangeAudioClipFadeCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getAudioClips()) {
            if (clip.id == m_clipId) {
                clip.fadeInTicks = m_newFadeIn;
                clip.fadeOutTicks = m_newFadeOut;
                refreshTimeline();
                return;
            }
        }
    }
}

void ChangeAudioClipFadeCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    for (auto *track : arr->getTracks()) {
        if (!track) continue;
        for (auto &clip : track->getAudioClips()) {
            if (clip.id == m_clipId) {
                clip.fadeInTicks = m_oldFadeIn;
                clip.fadeOutTicks = m_oldFadeOut;
                refreshTimeline();
                return;
            }
        }
    }
}

bool ChangeAudioClipFadeCommand::isValid() const
{
    if (m_resourceId < 0) return true;
    return audioResourceExists(m_resourceId);
}

// ==== CutAudioClipCommand ====

CutAudioClipCommand::CutAudioClipCommand(ArrangementTimelineWidget *timeline, int clipId,
                                         int trackIndex, int64_t cutTick, int resourceId)
    : ArrangementClipCommandBase(timeline)
    , m_originalClipId(clipId)
    , m_trackIndex(trackIndex)
    , m_cutTick(cutTick)
    , m_resourceId(resourceId)
{
}

void CutAudioClipCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    if (m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    // Find the clip
    NN_AudioClip_t *clip = nullptr;
    for (auto &c : track->getAudioClips()) {
        if (c.id == m_originalClipId) {
            clip = &c;
            break;
        }
    }
    
    if (!clip) return;
    
    // Save original clip for undo
    m_originalClip = *clip;
    
    // Calculate split position
    int64_t localCutTick = m_cutTick - clip->startTick;
    if (localCutTick <= 0 || localCutTick >= clip->durationTicks) return;
    
    // Create second clip
    NN_AudioClip_t secondClip;
    secondClip.id = nn_generate_unique_clip_id();
    secondClip.audioResourceId = clip->audioResourceId;
    secondClip.startTick = m_cutTick;
    secondClip.durationTicks = clip->durationTicks - localCutTick;
    secondClip.offsetTicks = clip->offsetTicks + localCutTick;
    secondClip.offsetSamples = clip->offsetSamples;  // Will be calculated from offsetTicks
    secondClip.clipLengthSamples = clip->clipLengthSamples;
    secondClip.muted = clip->muted;
    secondClip.looping = clip->looping;
    secondClip.gain = clip->gain;
    secondClip.fadeInTicks = 0;
    secondClip.fadeOutTicks = clip->fadeOutTicks;
    
    m_secondClipId = secondClip.id;
    
    // Modify first clip
    clip->durationTicks = localCutTick;
    clip->fadeOutTicks = 0;
    
    // Add second clip
    track->addAudioClip(secondClip);
    
    arr->updateMaxTick();
    refreshTimeline();
}

void CutAudioClipCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    if (m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    // Remove second clip
    if (m_secondClipId >= 0) {
        track->removeAudioClip(m_secondClipId);
    }
    
    // Restore original clip
    for (auto &c : track->getAudioClips()) {
        if (c.id == m_originalClipId) {
            c = m_originalClip;
            break;
        }
    }
    
    arr->updateMaxTick();
    refreshTimeline();
}

bool CutAudioClipCommand::isValid() const
{
    if (m_resourceId < 0) return true;
    return audioResourceExists(m_resourceId);
}

// ==== CutMidiClipCommand ====

CutMidiClipCommand::CutMidiClipCommand(ArrangementTimelineWidget *timeline, int clipId,
                                       int trackIndex, int64_t cutTick, int sequenceId)
    : ArrangementClipCommandBase(timeline)
    , m_originalClipId(clipId)
    , m_trackIndex(trackIndex)
    , m_cutTick(cutTick)
    , m_sequenceId(sequenceId)
{
}

void CutMidiClipCommand::execute()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    if (m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    // Find the clip
    NN_MidiClip_t *clip = nullptr;
    for (auto &c : track->getClips()) {
        if (c.id == m_originalClipId) {
            clip = &c;
            break;
        }
    }
    
    if (!clip) return;
    
    // Save original clip for undo
    m_originalClip = *clip;
    
    // Calculate split position
    int64_t localCutTick = m_cutTick - clip->startTick;
    if (localCutTick <= 0 || localCutTick >= clip->durationTicks) return;
    
    // Create second clip
    NN_MidiClip_t secondClip;
    secondClip.id = nn_generate_unique_clip_id();
    secondClip.sequenceId = clip->sequenceId;
    secondClip.startTick = m_cutTick;
    secondClip.durationTicks = clip->durationTicks - localCutTick;
    secondClip.offsetTicks = clip->offsetTicks + localCutTick;
    secondClip.muted = clip->muted;
    secondClip.name = clip->name;
    secondClip.color = clip->color;
    secondClip.fadeInTicks = 0;
    secondClip.fadeOutTicks = clip->fadeOutTicks;
    
    m_secondClipId = secondClip.id;
    
    // Modify first clip
    clip->durationTicks = localCutTick;
    clip->fadeOutTicks = 0;
    
    // Add second clip
    track->addClip(secondClip);
    
    arr->updateMaxTick();
    refreshTimeline();
}

void CutMidiClipCommand::undo()
{
    NoteNagaArrangement *arr = getArrangement();
    if (!arr) return;
    
    if (m_trackIndex < 0 || m_trackIndex >= static_cast<int>(arr->getTrackCount())) return;
    
    NoteNagaArrangementTrack *track = arr->getTracks()[m_trackIndex];
    if (!track) return;
    
    // Remove second clip
    if (m_secondClipId >= 0) {
        track->removeClip(m_secondClipId);
    }
    
    // Restore original clip
    for (auto &c : track->getClips()) {
        if (c.id == m_originalClipId) {
            c = m_originalClip;
            break;
        }
    }
    
    arr->updateMaxTick();
    refreshTimeline();
}

bool CutMidiClipCommand::isValid() const
{
    if (m_sequenceId < 0) return true;
    return sequenceExists(m_sequenceId);
}