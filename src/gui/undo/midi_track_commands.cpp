#include "midi_track_commands.h"
#include <note_naga_engine/core/types.h>
#include <set>

// ============================================================================
// MidiAddTrackCommand
// ============================================================================

MidiAddTrackCommand::MidiAddTrackCommand(NoteNagaMidiSeq *sequence, int instrument,
                                 const QString &name, int channel)
    : m_sequence(sequence), m_instrument(instrument), m_name(name), m_channel(channel)
{
}

void MidiAddTrackCommand::execute() {
    if (!m_sequence) return;
    
    // Find available channel if not specified
    int channel = m_channel;
    if (channel < 0) {
        std::set<int> usedChannels;
        for (auto *t : m_sequence->getTracks()) {
            if (t->getChannel().has_value()) {
                usedChannels.insert(t->getChannel().value());
            }
        }
        for (int ch = 0; ch < 16; ++ch) {
            if (ch != 9 && usedChannels.find(ch) == usedChannels.end()) {  // Skip drum channel
                channel = ch;
                break;
            }
        }
        if (channel < 0) channel = 0;
    }
    
    // Create or reinsert track
    if (m_createdTrack) {
        // Redo: reinsert the previously created track
        m_sequence->insertTrack(m_createdTrackIndex, m_createdTrack);
    } else {
        // First execute: create new track
        m_createdTrack = m_sequence->addTrack(m_instrument);
        if (m_createdTrack) {
            if (!m_name.isEmpty()) {
                m_createdTrack->setName(m_name.toStdString());
            }
            m_createdTrack->setChannel(channel);
            m_createdTrackIndex = static_cast<int>(m_sequence->getTracks().size() - 1);
        }
    }
    
    if (m_refreshCallback) m_refreshCallback();
}

void MidiAddTrackCommand::undo() {
    if (!m_sequence || !m_createdTrack) return;
    
    // Find and extract the track (don't delete it)
    auto tracks = m_sequence->getTracks();
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i) {
        if (tracks[i] == m_createdTrack) {
            m_createdTrackIndex = i;
            m_sequence->extractTrack(i);
            break;
        }
    }
    
    if (m_refreshCallback) m_refreshCallback();
}

// ============================================================================
// MidiRemoveTrackCommand
// ============================================================================

MidiRemoveTrackCommand::MidiRemoveTrackCommand(NoteNagaMidiSeq *sequence, int trackIndex)
    : m_sequence(sequence), m_trackIndex(trackIndex)
{
}

void MidiRemoveTrackCommand::execute() {
    if (!m_sequence) return;
    
    auto tracks = m_sequence->getTracks();
    if (m_trackIndex < 0 || m_trackIndex >= static_cast<int>(tracks.size())) return;
    
    // Store track for undo
    m_removedTrack = m_sequence->extractTrack(m_trackIndex);
    
    if (m_refreshCallback) m_refreshCallback();
}

void MidiRemoveTrackCommand::undo() {
    if (!m_sequence || !m_removedTrack) return;
    
    // Reinsert at original position
    m_sequence->insertTrack(m_trackIndex, m_removedTrack);
    
    if (m_refreshCallback) m_refreshCallback();
}

// ============================================================================
// MidiDuplicateTrackCommand
// ============================================================================

MidiDuplicateTrackCommand::MidiDuplicateTrackCommand(NoteNagaMidiSeq *sequence, int trackIndex)
    : m_sequence(sequence), m_sourceTrackIndex(trackIndex)
{
}

void MidiDuplicateTrackCommand::execute() {
    if (!m_sequence) return;
    
    auto tracks = m_sequence->getTracks();
    if (m_sourceTrackIndex < 0 || m_sourceTrackIndex >= static_cast<int>(tracks.size())) return;
    
    if (m_duplicatedTrack) {
        // Redo: reinsert the duplicated track
        m_sequence->insertTrack(m_duplicatedTrackIndex, m_duplicatedTrack);
    } else {
        // First execute: create duplicate
        NoteNagaTrack *sourceTrack = tracks[m_sourceTrackIndex];
        m_duplicatedTrack = m_sequence->addTrack(sourceTrack->getInstrument().value_or(0));
        
        if (m_duplicatedTrack) {
            // Copy properties
            m_duplicatedTrack->setName(sourceTrack->getName() + " (Copy)");
            if (sourceTrack->getChannel().has_value()) {
                m_duplicatedTrack->setChannel(sourceTrack->getChannel().value());
            }
            m_duplicatedTrack->setVolume(sourceTrack->getVolume());
            m_duplicatedTrack->setMidiPanOffset(sourceTrack->getMidiPanOffset());
            m_duplicatedTrack->setMuted(sourceTrack->isMuted());
            m_duplicatedTrack->setColor(sourceTrack->getColor());
            
            // Copy notes
            for (const auto &note : sourceTrack->getNotes()) {
                m_duplicatedTrack->addNote(note);
            }
            
            m_duplicatedTrackIndex = static_cast<int>(m_sequence->getTracks().size() - 1);
        }
    }
    
    if (m_refreshCallback) m_refreshCallback();
}

void MidiDuplicateTrackCommand::undo() {
    if (!m_sequence || !m_duplicatedTrack) return;
    
    // Find and extract the duplicated track
    auto tracks = m_sequence->getTracks();
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i) {
        if (tracks[i] == m_duplicatedTrack) {
            m_duplicatedTrackIndex = i;
            m_sequence->extractTrack(i);
            break;
        }
    }
    
    if (m_refreshCallback) m_refreshCallback();
}

// ============================================================================
// MidiMoveTrackCommand
// ============================================================================

MidiMoveTrackCommand::MidiMoveTrackCommand(NoteNagaMidiSeq *sequence, int fromIndex, int toIndex)
    : m_sequence(sequence), m_fromIndex(fromIndex), m_toIndex(toIndex)
{
}

void MidiMoveTrackCommand::execute() {
    if (!m_sequence) return;
    m_sequence->moveTrack(m_fromIndex, m_toIndex);
    if (m_refreshCallback) m_refreshCallback();
}

void MidiMoveTrackCommand::undo() {
    if (!m_sequence) return;
    m_sequence->moveTrack(m_toIndex, m_fromIndex);
    if (m_refreshCallback) m_refreshCallback();
}

// ============================================================================
// MidiModifyTrackPropertyCommand
// ============================================================================

MidiModifyTrackPropertyCommand::MidiModifyTrackPropertyCommand(NoteNagaTrack *track, Property property,
                                                       const QVariant &oldValue, const QVariant &newValue)
    : m_track(track), m_property(property), m_oldValue(oldValue), m_newValue(newValue)
{
}

void MidiModifyTrackPropertyCommand::execute() {
    applyValue(m_newValue);
    if (m_refreshCallback) m_refreshCallback();
}

void MidiModifyTrackPropertyCommand::undo() {
    applyValue(m_oldValue);
    if (m_refreshCallback) m_refreshCallback();
}

QString MidiModifyTrackPropertyCommand::description() const {
    switch (m_property) {
        case Property::Name: return QObject::tr("Rename Track");
        case Property::Instrument: return QObject::tr("Change Instrument");
        case Property::Channel: return QObject::tr("Change Channel");
        case Property::Muted: return QObject::tr("Toggle Mute");
        case Property::Volume: return QObject::tr("Change Volume");
        case Property::Pan: return QObject::tr("Change Pan");
        default: return QObject::tr("Modify Track");
    }
}

void MidiModifyTrackPropertyCommand::applyValue(const QVariant &value) {
    if (!m_track) return;
    
    switch (m_property) {
        case Property::Name:
            m_track->setName(value.toString().toStdString());
            break;
        case Property::Instrument:
            m_track->setInstrument(value.toInt());
            break;
        case Property::Channel:
            m_track->setChannel(value.toInt());
            break;
        case Property::Muted:
            m_track->setMuted(value.toBool());
            break;
        case Property::Volume:
            m_track->setVolume(value.toFloat());
            break;
        case Property::Pan:
            m_track->setMidiPanOffset(value.toInt());
            break;
    }
}

// ============================================================================
// MidiClearTracksCommand
// ============================================================================

MidiClearTracksCommand::MidiClearTracksCommand(NoteNagaMidiSeq *sequence)
    : m_sequence(sequence)
{
}

MidiClearTracksCommand::~MidiClearTracksCommand() {
    // Delete owned tracks when command is destroyed
    if (m_ownsRemovedTracks) {
        for (auto *track : m_removedTracks) {
            delete track;
        }
    }
}

void MidiClearTracksCommand::execute() {
    if (!m_sequence) return;
    
    // Extract all existing tracks (owner transfers to us)
    m_removedTracks.clear();
    while (!m_sequence->getTracks().empty()) {
        NoteNagaTrack *track = m_sequence->extractTrack(0);
        if (track) {
            m_removedTracks.push_back(track);
        }
    }
    m_ownsRemovedTracks = true;
    
    // Create new empty track
    m_newTrack = m_sequence->addTrack(0);  // Piano as default
    
    if (m_refreshCallback) m_refreshCallback();
}

void MidiClearTracksCommand::undo() {
    if (!m_sequence) return;
    
    // Remove the new track that was created
    if (m_newTrack) {
        auto tracks = m_sequence->getTracks();
        for (int i = 0; i < static_cast<int>(tracks.size()); ++i) {
            if (tracks[i] == m_newTrack) {
                m_sequence->removeTrack(i);  // Delete it - we don't need it anymore
                m_newTrack = nullptr;
                break;
            }
        }
    }
    
    // Restore all removed tracks
    for (size_t i = 0; i < m_removedTracks.size(); ++i) {
        m_sequence->insertTrack(static_cast<int>(i), m_removedTracks[i]);
    }
    m_removedTracks.clear();
    m_ownsRemovedTracks = false;
    
    if (m_refreshCallback) m_refreshCallback();
}
