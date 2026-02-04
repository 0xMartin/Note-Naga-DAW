#include <note_naga_engine/core/types.h>
#include <note_naga_engine/core/soundfont_finder.h>
#include <note_naga_engine/synth/synth_fluidsynth.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <cmath>

#include <note_naga_engine/logger.h>
#include <string>

/*******************************************************************************************************/
// Channel Colors
/*******************************************************************************************************/

const std::vector<NN_Color_t> DEFAULT_CHANNEL_COLORS = {
    NN_Color_t(0, 180, 255),   NN_Color_t(255, 100, 100),
    NN_Color_t(250, 200, 75),  NN_Color_t(90, 230, 120),
    NN_Color_t(180, 110, 255), NN_Color_t(170, 180, 70),
    NN_Color_t(95, 220, 210),  NN_Color_t(230, 90, 210),
    NN_Color_t(70, 180, 90),   NN_Color_t(255, 180, 60),
    NN_Color_t(210, 80, 80),   NN_Color_t(80, 120, 255),
    NN_Color_t(255, 230, 80),  NN_Color_t(110, 255, 120),
    NN_Color_t(220, 160, 255), NN_Color_t(100, 180, 160)};

NN_Color_t nn_color_blend(const NN_Color_t &fg, const NN_Color_t &bg,
                          double opacity) {
  // opacity: 0.0 = jen bg, 1.0 = jen fg
  double a = opacity;
  int r = int(a * fg.red + (1 - a) * bg.red);
  int g = int(a * fg.green + (1 - a) * bg.green);
  int b = int(a * fg.blue + (1 - a) * bg.blue);
  return NN_Color_t(r, g, b);
}

double nn_yiq_luminance(const NN_Color_t &color) {
  return (double)(color.red * 299 + color.green * 587 + color.blue * 114) /
         1000.0;
}

/*******************************************************************************************************/
// Unique ID generation
/*******************************************************************************************************/

static std::atomic<unsigned long> next_note_id = 1;
static std::atomic<unsigned long> next_seq_id = 1;

unsigned long nn_generate_unique_note_id() {
  return static_cast<int>(next_note_id++);
}

int nn_generate_unique_seq_id() { return static_cast<int>(next_seq_id++); }

/*******************************************************************************************************/
// Note Naga Note
/*******************************************************************************************************/

double note_time_ms(const NN_Note_t &note, int ppq, int tempo) {
  if (!note.length.has_value() || note.length.value() <= 0)
    return 0.0;
  double us_per_tick = static_cast<double>(tempo) / ppq;
  double total_us = note.length.value() * us_per_tick;
  return total_us / 1000.0;
}

/*******************************************************************************************************/
// Note Naga Track
/*******************************************************************************************************/

NoteNagaTrack::NoteNagaTrack()
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
  this->track_id = 0;
  this->name = name.empty() ? "Track " + std::to_string(track_id + 1) : name;
  this->instrument = std::nullopt;
  this->channel = std::nullopt;
  this->color = DEFAULT_CHANNEL_COLORS[0];
  this->visible = true;
  this->muted = false;
  this->solo = false;
  this->volume = 1.0f;
  this->is_tempo_track = false;
  this->tempo_track_active = false;
  // Per-track synth members
  this->synth_ = nullptr;
  this->audio_volume_db_ = 0.0f;
  this->midi_pan_offset_ = 0;
  NOTE_NAGA_LOG_INFO("Created default Track with ID: " +
                     std::to_string(track_id));
}

NoteNagaTrack::NoteNagaTrack(int track_id, NoteNagaMidiSeq *parent,
                             const std::string &name,
                             const std::optional<int> &instrument,
                             const std::optional<int> &channel)
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
  this->track_id = track_id;
  this->parent = parent;
  this->name = name.empty() ? "Track " + std::to_string(track_id + 1) : name;
  this->instrument = instrument;
  this->channel = channel;
  this->color =
      DEFAULT_CHANNEL_COLORS[track_id % DEFAULT_CHANNEL_COLORS.size()];
  this->is_tempo_track = false;
  this->tempo_track_active = false;
  this->visible = true;
  this->muted = false;
  this->solo = false;
  this->volume = 1.0f;
  // Per-track synth members
  this->synth_ = nullptr;
  this->audio_volume_db_ = 0.0f;
  this->midi_pan_offset_ = 0;
  NOTE_NAGA_LOG_INFO("Created Track with ID: " + std::to_string(track_id) +
                     " and name: " + this->name);
}

void NoteNagaTrack::addNote(const NN_Note_t &note) {
    // Vložíme notu na správnou pozici podle start času (binary search)
    // aby noty byly vždy seřazené pro playback worker
    int noteStart = note.start.value_or(0);
    auto it = std::lower_bound(
        this->midi_notes.begin(), this->midi_notes.end(), noteStart,
        [](const NN_Note_t &n, int start) {
            return n.start.value_or(0) < start;
        }
    );
    this->midi_notes.insert(it, note);
    NN_QT_EMIT(metadataChanged(this, "notes"));
}

void NoteNagaTrack::removeNote(const NN_Note_t &note) {
    auto it = std::find_if(midi_notes.begin(), midi_notes.end(),
                           [&note](const NN_Note_t &n) { return n.id == note.id; });
    if (it != midi_notes.end()) {
        midi_notes.erase(it);
    }
    NN_QT_EMIT(metadataChanged(this, "notes"));
}

void NoteNagaTrack::setInstrument(std::optional<int> instrument) {
  if (this->instrument == instrument)
    return;
  this->instrument = instrument;
  NOTE_NAGA_LOG_INFO(
      "Instrument changed for Track ID: " + std::to_string(track_id) + " to: " +
      (instrument.has_value() ? std::to_string(instrument.value()) : "None"));
  NN_QT_EMIT(metadataChanged(this, "instrument"));
}

void NoteNagaTrack::setChannel(std::optional<int> channel) {
  if (this->channel == channel)
    return;
  this->channel = channel;
  NOTE_NAGA_LOG_INFO(
      "Channel changed for Track ID: " + std::to_string(track_id) + " to: " +
      (channel.has_value() ? std::to_string(channel.value()) : "None"));
  NN_QT_EMIT(metadataChanged(this, "channel"));
}

void NoteNagaTrack::setId(int new_id) {
  if (this->track_id == new_id)
    return;
  NOTE_NAGA_LOG_INFO("ID of Track changed from: " + std::to_string(track_id) +
                     " to: " + std::to_string(new_id));
  this->track_id = new_id;
  NN_QT_EMIT(metadataChanged(this, "id"));
}

void NoteNagaTrack::setName(const std::string &new_name) {
  if (this->name == new_name)
    return;
  NOTE_NAGA_LOG_INFO("Name of Track changed from: " + this->name +
                     " to: " + new_name);
  this->name = new_name;
  NN_QT_EMIT(metadataChanged(this, "name"));
}

void NoteNagaTrack::setColor(const NN_Color_t &new_color) {
  if (this->color == new_color)
    return;
  this->color = new_color;
  NN_QT_EMIT(metadataChanged(this, "color"));
}

void NoteNagaTrack::setVisible(bool is_visible) {
  if (this->visible == is_visible)
    return;
  this->visible = is_visible;
  NN_QT_EMIT(metadataChanged(this, "visible"));
}

void NoteNagaTrack::setMuted(bool is_muted) {
  if (this->muted == is_muted)
    return;
  this->muted = is_muted;
  NN_QT_EMIT(metadataChanged(this, "muted"));
}

void NoteNagaTrack::setSolo(bool is_solo) {
  if (this->solo == is_solo)
    return;
  this->solo = is_solo;
  NN_QT_EMIT(metadataChanged(this, "solo"));
}

void NoteNagaTrack::setVolume(float new_volume) {
  if (this->volume == new_volume)
    return;
  this->volume = new_volume;
  NN_QT_EMIT(metadataChanged(this, "volume"));
}

/*******************************************************************************************************/
// Note Naga Track - Per-Track Synth Methods
/*******************************************************************************************************/

INoteNagaSoftSynth* NoteNagaTrack::getSoftSynth() const {
  return dynamic_cast<INoteNagaSoftSynth*>(synth_);
}

float NoteNagaTrack::getAudioVolumeLinear() const {
  // Convert dB to linear gain: gain = 10^(dB/20)
  return std::pow(10.0f, audio_volume_db_ / 20.0f);
}

void NoteNagaTrack::setSynth(NoteNagaSynthesizer* synth) {
  if (synth_ == synth)
    return;
  // Delete old synth if we own it
  if (synth_ != nullptr) {
    delete synth_;
  }
  synth_ = synth;
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " synth set to: " + (synth ? synth->getName() : "nullptr"));
  NN_QT_EMIT(metadataChanged(this, "synth"));
}

bool NoteNagaTrack::initDefaultSynth() {
  // Find a soundfont
  std::string sf2_path = SoundFontFinder::findSoundFont();
  if (sf2_path.empty()) {
    NOTE_NAGA_LOG_WARNING("Track ID: " + std::to_string(track_id) + 
                          " - No soundfont found for default synth");
    return false;
  }
  
  // Create FluidSynth with the found soundfont - use async loading to not block GUI
  std::string synth_name = "Track " + std::to_string(track_id + 1) + " Synth";
  auto* fluidSynth = new NoteNagaSynthFluidSynth(synth_name, sf2_path, true /* loadAsync */);
  
  // Set callback to log when loading completes
  fluidSynth->setLoadCompletedCallback([this, sf2_path](bool success) {
    if (success) {
      NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                         " - SoundFont loaded successfully: " + sf2_path);
      NN_QT_EMIT(metadataChanged(this, "synth_loaded"));
    } else {
      NOTE_NAGA_LOG_ERROR("Track ID: " + std::to_string(track_id) + 
                          " - Failed to load SoundFont: " + sf2_path);
    }
  });
  
  setSynth(fluidSynth);
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " - Default synth initializing with soundfont: " + sf2_path);
  return true;
}

void NoteNagaTrack::setAudioVolumeDb(float db) {
  // Clamp to valid range
  db = std::max(-24.0f, std::min(6.0f, db));
  if (audio_volume_db_ == db)
    return;
  audio_volume_db_ = db;
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " audio_volume_db set to: " + std::to_string(db));
  NN_QT_EMIT(metadataChanged(this, "audio_volume_db"));
}

void NoteNagaTrack::setMidiPanOffset(int offset) {
  // Clamp to valid range
  offset = std::max(-64, std::min(64, offset));
  if (midi_pan_offset_ == offset)
    return;
  midi_pan_offset_ = offset;
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " midi_pan_offset set to: " + std::to_string(offset));
  NN_QT_EMIT(metadataChanged(this, "midi_pan_offset"));
}

void NoteNagaTrack::playNote(const NN_Note_t& note) {
  if (!synth_ || muted)
    return;
  
  // Get channel from track or use default
  int chan = channel.value_or(0);
  
  // Pan is applied in renderAudio() now, not per-note
  // Pass center pan (0.0) to synth - actual panning happens at audio level
  synth_->playNote(note, chan, 0.0f);
}

void NoteNagaTrack::stopNote(const NN_Note_t& note) {
  if (!synth_)
    return;
  synth_->stopNote(note);
}

void NoteNagaTrack::stopAllNotes() {
  if (!synth_)
    return;
  synth_->stopAllNotes(nullptr, this);
}

void NoteNagaTrack::renderAudio(float* left, float* right, size_t num_frames) {
  auto* softSynth = getSoftSynth();
  if (!softSynth || muted)
    return;
  
  // Render audio from synth (centered - pan applied below)
  softSynth->renderAudio(left, right, num_frames);
  
  // Apply audio volume gain and pan
  float gain = getAudioVolumeLinear();
  float pan = getPanNormalized();  // -1.0 (left) to +1.0 (right)
  
  // Equal power panning for hard pan:
  // pan = -1.0: left_gain = 1.0, right_gain = 0.0
  // pan =  0.0: left_gain = 0.707, right_gain = 0.707 (but we want 1.0/1.0 for center)
  // pan = +1.0: left_gain = 0.0, right_gain = 1.0
  // Using linear panning for true hard pan (0% on opposite channel)
  float left_gain = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
  float right_gain = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
  
  left_gain *= gain;
  right_gain *= gain;
  
  for (size_t i = 0; i < num_frames; ++i) {
    // Mix stereo to mono first, then apply pan
    float mono = (left[i] + right[i]) * 0.5f;
    left[i] = mono * left_gain;
    right[i] = mono * right_gain;
  }
}

/*******************************************************************************************************/
// Note Naga Track - Tempo Track Methods
/*******************************************************************************************************/

void NoteNagaTrack::setTempoTrackActive(bool active) {
  if (this->tempo_track_active == active)
    return;
  this->tempo_track_active = active;
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " tempo_track_active set to: " + (active ? "true" : "false"));
  NN_QT_EMIT(metadataChanged(this, "tempo_track_active"));
}

void NoteNagaTrack::setTempoTrack(bool is_tempo) {
  if (this->is_tempo_track == is_tempo)
    return;
  this->is_tempo_track = is_tempo;
  this->tempo_track_active = is_tempo;  // Activate by default when setting as tempo track
  if (is_tempo && tempo_events.empty()) {
    // Initialize with default tempo at tick 0
    tempo_events.push_back(NN_TempoEvent_t(0, 120.0, TempoInterpolation::Step));
  }
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " is_tempo_track set to: " + (is_tempo ? "true" : "false"));
  NN_QT_EMIT(metadataChanged(this, "is_tempo_track"));
}

void NoteNagaTrack::setTempoEvents(const std::vector<NN_TempoEvent_t>& events) {
  tempo_events = events;
  // Sort by tick
  std::sort(tempo_events.begin(), tempo_events.end());
  NN_QT_EMIT(tempoEventsChanged(this));
}

void NoteNagaTrack::addTempoEvent(const NN_TempoEvent_t& event) {
  // Remove any existing event at the same tick
  tempo_events.erase(
    std::remove_if(tempo_events.begin(), tempo_events.end(),
                   [&event](const NN_TempoEvent_t& e) { return e.tick == event.tick; }),
    tempo_events.end());
  
  // Insert and sort
  tempo_events.push_back(event);
  std::sort(tempo_events.begin(), tempo_events.end());
  
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " tempo event added at tick: " + std::to_string(event.tick) +
                     " BPM: " + std::to_string(event.bpm));
  NN_QT_EMIT(tempoEventsChanged(this));
}

bool NoteNagaTrack::removeTempoEventAtTick(int tick) {
  // Don't allow removing the event at tick 0 if it's the only one
  if (tempo_events.size() <= 1 && !tempo_events.empty() && tempo_events[0].tick == tick) {
    NOTE_NAGA_LOG_WARNING("Cannot remove the only tempo event");
    return false;
  }
  
  auto it = std::find_if(tempo_events.begin(), tempo_events.end(),
                         [tick](const NN_TempoEvent_t& e) { return e.tick == tick; });
  if (it != tempo_events.end()) {
    tempo_events.erase(it);
    NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                       " tempo event removed at tick: " + std::to_string(tick));
    NN_QT_EMIT(tempoEventsChanged(this));
    return true;
  }
  return false;
}

double NoteNagaTrack::getTempoAtTick(int tick) const {
  if (tempo_events.empty()) {
    return 120.0;  // Default tempo
  }
  
  // Find the tempo event at or before this tick
  const NN_TempoEvent_t* prevEvent = nullptr;
  const NN_TempoEvent_t* nextEvent = nullptr;
  
  for (size_t i = 0; i < tempo_events.size(); ++i) {
    if (tempo_events[i].tick <= tick) {
      prevEvent = &tempo_events[i];
      if (i + 1 < tempo_events.size()) {
        nextEvent = &tempo_events[i + 1];
      } else {
        nextEvent = nullptr;
      }
    } else {
      if (!prevEvent) {
        // tick is before first event - use that next event info
        nextEvent = &tempo_events[i];
      }
      break;
    }
  }
  
  if (!prevEvent) {
    // Before first tempo event, use first event's tempo
    return tempo_events[0].bpm;
  }
  
  // Check interpolation mode
  if (prevEvent->interpolation == TempoInterpolation::Linear && nextEvent && nextEvent->tick > prevEvent->tick) {
    // Linear interpolation between prevEvent and nextEvent
    int tickRange = nextEvent->tick - prevEvent->tick;
    double t = static_cast<double>(tick - prevEvent->tick) / tickRange;
    t = std::clamp(t, 0.0, 1.0);
    return prevEvent->bpm + t * (nextEvent->bpm - prevEvent->bpm);
  }
  
  // Step interpolation or no next event
  return prevEvent->bpm;
}

void NoteNagaTrack::resetTempoEvents(double bpm) {
  tempo_events.clear();
  tempo_events.push_back(NN_TempoEvent_t(0, bpm, TempoInterpolation::Step));
  NOTE_NAGA_LOG_INFO("Track ID: " + std::to_string(track_id) + 
                     " tempo events reset to: " + std::to_string(bpm) + " BPM");
  NN_QT_EMIT(tempoEventsChanged(this));
}

/*******************************************************************************************************/
// Note Naga MIDI Sequence
/*******************************************************************************************************/

NoteNagaMidiSeq::NoteNagaMidiSeq()
#ifndef QT_DEACTIVATED
    : QObject(nullptr)
#endif
{
  this->sequence_id = nn_generate_unique_seq_id();
  this->clear();
}

NoteNagaMidiSeq::NoteNagaMidiSeq(int sequence_id) {
  this->sequence_id = nn_generate_unique_seq_id();
  this->clear();
}

NoteNagaMidiSeq::NoteNagaMidiSeq(int sequence_id,
                                 std::vector<NoteNagaTrack *> tracks) {
  this->sequence_id = nn_generate_unique_seq_id();
  this->clear();
  this->tracks = std::move(tracks);
  NOTE_NAGA_LOG_INFO("Created MIDI sequence with ID: " +
                     std::to_string(sequence_id));
}

NoteNagaMidiSeq::~NoteNagaMidiSeq() { clear(); }

void NoteNagaMidiSeq::clear() {
  NOTE_NAGA_LOG_INFO("Clearing MIDI sequence with ID: " +
                     std::to_string(sequence_id));

  // Dealokace všech tracků
  for (NoteNagaTrack *track : this->tracks) {
    if (track)
      delete track;
  }
  this->tracks.clear();

  // Dealokace midi_file
  if (midi_file) {
    delete midi_file;
    midi_file = nullptr;
  }

  this->ppq = 480;
  this->tempo = 600000;  // 100 BPM (microseconds per beat)
  this->max_tick = 0;
  this->active_track = nullptr;
  this->solo_track = nullptr;

  NN_QT_EMIT(trackListChanged());
}

void NoteNagaMidiSeq::setId(int new_id) {
  if (this->sequence_id == new_id)
    return;
  NOTE_NAGA_LOG_INFO(
      "ID of MIDI sequence changed from: " + std::to_string(sequence_id) +
      " to: " + std::to_string(new_id));
  sequence_id = new_id;
  NN_QT_EMIT(metadataChanged(this, "id"));
}

void NoteNagaMidiSeq::setPPQ(int ppq) {
  if (this->ppq == ppq)
    return;
  this->ppq = ppq;
  NOTE_NAGA_LOG_INFO("PPQ changed to: " + std::to_string(ppq) +
                     " for MIDI sequence ID: " + std::to_string(sequence_id));
  NN_QT_EMIT(metadataChanged(this, "ppq"));
}

void NoteNagaMidiSeq::setTempo(int tempo) {
  if (this->tempo == tempo)
    return;
  this->tempo = tempo;
  NOTE_NAGA_LOG_INFO(
      "Tempo changed to: " + std::to_string(60'000'000.0 / tempo) +
      " for MIDI sequence ID: " + std::to_string(sequence_id));
  NN_QT_EMIT(metadataChanged(this, "tempo"));
}

void NoteNagaMidiSeq::setSoloTrack(NoteNagaTrack *track) {
  NoteNagaTrack *current = this->active_track;

  if (track) {
    for (NoteNagaTrack *tr : this->tracks) {
      if (tr == track) {
        this->solo_track = track;
        NOTE_NAGA_LOG_INFO("Track with ID: " + std::to_string(track->getId()) +
                           " set as solo track for MIDI sequence ID: " +
                           std::to_string(sequence_id));
        break;
      }
    }
  } else {
    this->solo_track = nullptr;
    NOTE_NAGA_LOG_INFO("Solo track cleared for MIDI sequence ID: " +
                       std::to_string(sequence_id));
  }

  if (current != track) {
    NN_QT_EMIT(metadataChanged(this, "solo_track"));
  }
}

void NoteNagaMidiSeq::setActiveTrack(NoteNagaTrack *track) {
  NoteNagaTrack *current = this->active_track;

  if (track) {
    for (NoteNagaTrack *tr : this->tracks) {
      if (tr == track) {
        this->active_track = track;
        NOTE_NAGA_LOG_INFO("Track with ID: " + std::to_string(track->getId()) +
                           " set as active track for MIDI sequence ID: " +
                           std::to_string(sequence_id));
        break;
      }
    }
  } else {
    this->active_track = nullptr;
    NOTE_NAGA_LOG_INFO("Active track cleared for MIDI sequence ID: " +
                       std::to_string(sequence_id));
  }

  if (current != track) {
    NN_QT_EMIT(metadataChanged(this, "active_track"));
    NN_QT_EMIT(activeTrackChanged(this->active_track));
  }
}

/*******************************************************************************************************/
// Note Naga MIDI Sequence - Tempo Track Methods
/*******************************************************************************************************/

bool NoteNagaMidiSeq::hasTempoTrack() const {
  for (const auto* track : tracks) {
    if (track && track->isTempoTrack()) {
      return true;
    }
  }
  return false;
}

NoteNagaTrack* NoteNagaMidiSeq::getTempoTrack() const {
  for (auto* track : tracks) {
    if (track && track->isTempoTrack()) {
      return track;
    }
  }
  return nullptr;
}

NoteNagaTrack* NoteNagaMidiSeq::createTempoTrack() {
  // Check if tempo track already exists
  if (hasTempoTrack()) {
    NOTE_NAGA_LOG_WARNING("Tempo track already exists");
    return getTempoTrack();
  }
  
  // Create new tempo track at position 0
  int track_id = this->tracks.size();
  NoteNagaTrack* tempoTrack = new NoteNagaTrack(track_id, this, "Tempo Track");
  tempoTrack->setTempoTrack(true);
  
  // Initialize with current fixed tempo
  double currentBpm = 60'000'000.0 / this->tempo;
  tempoTrack->resetTempoEvents(currentBpm);
  
  // Insert at beginning of track list
  this->tracks.insert(this->tracks.begin(), tempoTrack);
  
  // Update track IDs
  for (size_t i = 0; i < this->tracks.size(); ++i) {
    this->tracks[i]->setId(static_cast<int>(i));
  }
  
#ifndef QT_DEACTIVATED
  connect(tempoTrack, &NoteNagaTrack::metadataChanged, this,
          &NoteNagaMidiSeq::trackMetadataChanged);
  connect(tempoTrack, &NoteNagaTrack::tempoEventsChanged, this,
          [this](NoteNagaTrack*) { NN_QT_EMIT(tempoTrackChanged()); });
#endif
  
  NOTE_NAGA_LOG_INFO("Created tempo track for MIDI sequence ID: " + 
                     std::to_string(sequence_id));
  NN_QT_EMIT(trackListChanged());
  NN_QT_EMIT(tempoTrackChanged());
  return tempoTrack;
}

bool NoteNagaMidiSeq::setTempoTrack(NoteNagaTrack* track) {
  if (!track) return false;
  
  // Check if track belongs to this sequence
  bool found = false;
  for (auto* t : tracks) {
    if (t == track) {
      found = true;
      break;
    }
  }
  if (!found) {
    NOTE_NAGA_LOG_ERROR("Track does not belong to this sequence");
    return false;
  }
  
  // Remove existing tempo track designation
  for (auto* t : tracks) {
    if (t && t->isTempoTrack() && t != track) {
      t->setTempoTrack(false);
    }
  }
  
  // Set new tempo track
  track->setTempoTrack(true);
  
  // Initialize with current fixed tempo if no events
  if (track->getTempoEvents().empty()) {
    double currentBpm = 60'000'000.0 / this->tempo;
    track->resetTempoEvents(currentBpm);
  }
  
#ifndef QT_DEACTIVATED
  connect(track, &NoteNagaTrack::tempoEventsChanged, this,
          [this](NoteNagaTrack*) { NN_QT_EMIT(tempoTrackChanged()); });
#endif
  
  NOTE_NAGA_LOG_INFO("Set track ID: " + std::to_string(track->getId()) + 
                     " as tempo track for MIDI sequence ID: " + std::to_string(sequence_id));
  NN_QT_EMIT(tempoTrackChanged());
  return true;
}

bool NoteNagaMidiSeq::removeTempoTrack() {
  NoteNagaTrack* tempoTrack = getTempoTrack();
  if (!tempoTrack) {
    return false;
  }
  
  tempoTrack->setTempoTrack(false);
  NOTE_NAGA_LOG_INFO("Removed tempo track designation for MIDI sequence ID: " + 
                     std::to_string(sequence_id));
  NN_QT_EMIT(tempoTrackChanged());
  return true;
}

int NoteNagaMidiSeq::getEffectiveTempoAtTick(int tick) const {
  NoteNagaTrack* tempoTrack = getTempoTrack();
  if (tempoTrack && tempoTrack->isTempoTrackActive()) {
    double bpm = tempoTrack->getTempoAtTick(tick);
    return static_cast<int>(60'000'000.0 / bpm);
  }
  return tempo;  // Fixed tempo
}

double NoteNagaMidiSeq::getEffectiveBPMAtTick(int tick) const {
  NoteNagaTrack* tempoTrack = getTempoTrack();
  if (tempoTrack && tempoTrack->isTempoTrackActive()) {
    return tempoTrack->getTempoAtTick(tick);
  }
  return 60'000'000.0 / tempo;  // Fixed tempo to BPM
}

double NoteNagaMidiSeq::ticksToSeconds(int tick) const {
  NoteNagaTrack* tempoTrack = getTempoTrack();
  
  if (!tempoTrack || !tempoTrack->isTempoTrackActive() || tempoTrack->getTempoEvents().empty()) {
    // Use fixed tempo
    double us_per_tick = static_cast<double>(tempo) / ppq;
    return tick * us_per_tick / 1'000'000.0;
  }
  
  // Integrate over tempo changes
  const auto& events = tempoTrack->getTempoEvents();
  double totalSeconds = 0.0;
  int currentTick = 0;
  
  for (size_t i = 0; i < events.size(); ++i) {
    const auto& event = events[i];
    int nextEventTick = (i + 1 < events.size()) ? events[i + 1].tick : tick + 1;
    
    if (event.tick >= tick) break;
    
    int startTick = std::max(currentTick, event.tick);
    int endTick = std::min(tick, nextEventTick);
    
    if (startTick < endTick) {
      if (event.interpolation == TempoInterpolation::Linear && i + 1 < events.size() && events[i + 1].tick <= tick) {
        // Linear interpolation - need to integrate
        double startBpm = event.bpm;
        double endBpm = events[i + 1].bpm;
        int segmentTicks = events[i + 1].tick - event.tick;
        
        // For linear tempo change, use average tempo
        int ticksInSegment = endTick - startTick;
        double t1 = static_cast<double>(startTick - event.tick) / segmentTicks;
        double t2 = static_cast<double>(endTick - event.tick) / segmentTicks;
        double avgBpm = (startBpm + t1 * (endBpm - startBpm) + startBpm + t2 * (endBpm - startBpm)) / 2.0;
        double us_per_beat = 60'000'000.0 / avgBpm;
        double us_per_tick_avg = us_per_beat / ppq;
        totalSeconds += ticksInSegment * us_per_tick_avg / 1'000'000.0;
      } else {
        // Step interpolation
        double us_per_beat = 60'000'000.0 / event.bpm;
        double us_per_tick_step = us_per_beat / ppq;
        totalSeconds += (endTick - startTick) * us_per_tick_step / 1'000'000.0;
      }
    }
    currentTick = endTick;
  }
  
  // Handle remaining ticks after last event
  if (currentTick < tick && !events.empty()) {
    double lastBpm = events.back().bpm;
    double us_per_beat = 60'000'000.0 / lastBpm;
    double us_per_tick_last = us_per_beat / ppq;
    totalSeconds += (tick - currentTick) * us_per_tick_last / 1'000'000.0;
  }
  
  return totalSeconds;
}

int NoteNagaMidiSeq::secondsToTicks(double seconds) const {
  NoteNagaTrack* tempoTrack = getTempoTrack();
  
  if (!tempoTrack || !tempoTrack->isTempoTrackActive() || tempoTrack->getTempoEvents().empty()) {
    // Use fixed tempo
    double us_per_tick = static_cast<double>(tempo) / ppq;
    return static_cast<int>(seconds * 1'000'000.0 / us_per_tick);
  }
  
  // Binary search approach - find tick that gives us the target seconds
  // This is an approximation for efficiency
  const auto& events = tempoTrack->getTempoEvents();
  double totalSeconds = 0.0;
  int currentTick = 0;
  
  for (size_t i = 0; i < events.size(); ++i) {
    const auto& event = events[i];
    int nextEventTick = (i + 1 < events.size()) ? events[i + 1].tick : max_tick;
    
    double us_per_beat = 60'000'000.0 / event.bpm;
    double us_per_tick_step = us_per_beat / ppq;
    double segmentMaxSeconds = (nextEventTick - event.tick) * us_per_tick_step / 1'000'000.0;
    
    if (totalSeconds + segmentMaxSeconds >= seconds) {
      // Target is in this segment
      double remainingSeconds = seconds - totalSeconds;
      int ticksInSegment = static_cast<int>(remainingSeconds * 1'000'000.0 / us_per_tick_step);
      return event.tick + ticksInSegment;
    }
    
    totalSeconds += segmentMaxSeconds;
    currentTick = nextEventTick;
  }
  
  // Past last event
  if (!events.empty()) {
    double lastBpm = events.back().bpm;
    double us_per_beat = 60'000'000.0 / lastBpm;
    double us_per_tick_last = us_per_beat / ppq;
    double remainingSeconds = seconds - totalSeconds;
    int additionalTicks = static_cast<int>(remainingSeconds * 1'000'000.0 / us_per_tick_last);
    return currentTick + additionalTicks;
  }
  
  // Fallback to fixed tempo
  double us_per_tick = static_cast<double>(tempo) / ppq;
  return static_cast<int>(seconds * 1'000'000.0 / us_per_tick);
}

NoteNagaTrack *NoteNagaMidiSeq::getTrackById(int track_id) {
  for (NoteNagaTrack *tr : this->tracks) {
    if (tr && tr->getId() == track_id)
      return tr;
  }
  return nullptr;
}

int NoteNagaMidiSeq::computeMaxTick() {
  this->max_tick = 0;
  for (const auto &track : this->tracks) {
    for (const auto &note : track->getNotes()) {
      if (note.start.has_value() && note.length.has_value())
        this->max_tick =
            std::max(this->max_tick, note.start.value() + note.length.value());
    }
  }
  NN_QT_EMIT(metadataChanged(this, "max_tick"));
  return this->max_tick;
}

NoteNagaTrack* NoteNagaMidiSeq::addTrack(int instrument_index) {
  if (instrument_index < 0)
    return nullptr;
  if (instrument_index > 127)
    return nullptr;
  int track_id = this->tracks.size();
  NoteNagaTrack* newTrack = new NoteNagaTrack(
      track_id, this, "Track " + std::to_string(track_id + 1),
      instrument_index >= 0 ? std::optional<int>(instrument_index)
                            : std::nullopt,
      0);
  this->tracks.push_back(newTrack);

  // Initialize default synthesizer for the new track (Per-Track Synth architecture)
  newTrack->initDefaultSynth();

#ifndef QT_DEACTIVATED
  // Connect metadata signal so name/color/etc changes are propagated
  connect(newTrack, &NoteNagaTrack::metadataChanged, this,
          &NoteNagaMidiSeq::trackMetadataChanged);
#endif

  NN_QT_EMIT(trackListChanged());
  return newTrack;
}

bool NoteNagaMidiSeq::removeTrack(int track_index) {
  if (track_index < 0 || track_index >= this->tracks.size())
    return false;

  delete this->tracks[track_index];
  this->tracks.erase(this->tracks.begin() + track_index);
  NN_QT_EMIT(trackListChanged());
  return true;
}

bool NoteNagaMidiSeq::moveTrack(int from_index, int to_index) {
  if (from_index < 0 || from_index >= static_cast<int>(this->tracks.size()))
    return false;
  if (to_index < 0 || to_index >= static_cast<int>(this->tracks.size()))
    return false;
  if (from_index == to_index)
    return true;  // No-op but not an error

  NoteNagaTrack *track = this->tracks[from_index];
  this->tracks.erase(this->tracks.begin() + from_index);
  this->tracks.insert(this->tracks.begin() + to_index, track);
  
  NN_QT_EMIT(trackListChanged());
  return true;
}

void NoteNagaMidiSeq::loadFromMidi(const std::string &midi_file_path) {
  // Check for empty path
  if (midi_file_path.empty()) {
    NOTE_NAGA_LOG_ERROR("No MIDI file path provided");
    return;
  }

  NOTE_NAGA_LOG_INFO("Loading MIDI file from: " + midi_file_path);
  clear();

  MidiFile *midiFile = new MidiFile();
  if (!midiFile->load(midi_file_path)) {
    NOTE_NAGA_LOG_ERROR("Failed to load MIDI file: " + midi_file_path);
    delete midiFile;
    return;
  }
  this->midi_file = midiFile;
  this->ppq = midiFile->header.division;

  // Split logic for type 0 and type 1 into helper methods
  std::vector<NoteNagaTrack *> tracks_tmp;
  if (midiFile->header.format == 0 && midiFile->getNumTracks() == 1) {
    tracks_tmp = loadType0Tracks(midiFile);
  } else {
    tracks_tmp = loadType1Tracks(midiFile);
  }

  // Set the tracks
  this->tracks = tracks_tmp;
  this->computeMaxTick();

  // Initialize default synthesizer for each track (Per-Track Synth architecture)
  for (NoteNagaTrack *track : this->tracks) {
    if (track && !track->isTempoTrack()) {
      track->initDefaultSynth();
    }
  }

  // Set the active track
  if (!tracks.empty())
    this->active_track = tracks[0];

  // signals
  for (NoteNagaTrack *track : this->tracks) {
    if (!track)
      continue;
#ifndef QT_DEACTIVATED
    connect(track, &NoteNagaTrack::metadataChanged, this,
            &NoteNagaMidiSeq::trackMetadataChanged);
#endif
  }

  // Set the file path
  this->file_path = midi_file_path;

  // emit signal (track list changed)
  NN_QT_EMIT(this->trackListChanged());

  NOTE_NAGA_LOG_INFO("MIDI file loaded successfully. Num tracks: " +
                     std::to_string(this->tracks.size()));
}

// --- Helper: load type 0 MIDI file (split channels) ---
std::vector<NoteNagaTrack *>
NoteNagaMidiSeq::loadType0Tracks(const MidiFile *midiFile) {
  NOTE_NAGA_LOG_INFO("Loading Type 0 MIDI tracks");

  std::vector<NoteNagaTrack *> tracks_tmp;

  // Only one track - need to split by MIDI channel
  const MidiTrack &track = midiFile->getTrack(0);
  int abs_time = 0;
  std::map<std::pair<int, int>, std::pair<int, int>>
      notes_on; // (note, channel) -> (start, velocity)
  std::map<int, std::vector<NN_Note_t>> channel_note_buffers;
  std::map<int, int> channel_instruments;
  std::map<int, std::string> channel_names;

  int tempo = 500000;
  std::vector<NN_TempoEvent_t> tempoEvents;  // Collect all tempo events

  // Parse all events and group notes per channel
  for (const auto &evt : track.events) {
    abs_time += evt.delta_time;
    // Track name: store for all channels
    if (evt.type == MidiEventType::Meta &&
        evt.meta_type == MIDI_META_TRACK_NAME) {
      std::string track_name(evt.meta_data.begin(), evt.meta_data.end());
      size_t endpos = track_name.find_last_not_of('\0');
      if (endpos != std::string::npos)
        track_name = track_name.substr(0, endpos + 1);
      for (int ch = 0; ch < 16; ++ch) {
        channel_names[ch] = track_name;
      }
    }
    // Program change: store instrument per channel
    if (evt.type == MidiEventType::ProgramChange && !evt.data.empty()) {
      channel_instruments[evt.channel] = evt.data[0];
    }
    // Tempo change: collect all tempo events
    if (evt.type == MidiEventType::Meta &&
        evt.meta_type == MIDI_META_SET_TEMPO) {
      if (evt.meta_data.size() == 3) {
        int tempoUs = (evt.meta_data[0] << 16) | (evt.meta_data[1] << 8) |
                evt.meta_data[2];
        double bpm = 60'000'000.0 / tempoUs;
        tempoEvents.push_back(NN_TempoEvent_t(abs_time, bpm, TempoInterpolation::Step));
        
        // Use first tempo as the fixed tempo
        if (tempo == 500000 || tempoEvents.size() == 1) {
          tempo = tempoUs;
        }
      }
    }
    // Note on: register note start per channel
    if (evt.type == MidiEventType::NoteOn && !evt.data.empty() &&
        evt.data[1] > 0) {
      int note = evt.data[0];
      int velocity = evt.data[1];
      int channel = evt.channel;
      notes_on[std::make_pair(note, channel)] =
          std::make_pair(abs_time, velocity);
    }
    // Note off: finish note per channel
    else if ((evt.type == MidiEventType::NoteOff) ||
             (evt.type == MidiEventType::NoteOn && !evt.data.empty() &&
              evt.data[1] == 0)) {
      int note = evt.data[0];
      int channel = evt.channel;
      auto key = std::make_pair(note, channel);
      auto it = notes_on.find(key);
      if (it != notes_on.end()) {
        int start = it->second.first;
        int velocity = it->second.second;
        channel_note_buffers[channel].push_back(
            NN_Note_t(note, nullptr, start, abs_time - start, velocity));
        notes_on.erase(it);
      }
    }
  }

  // Create tempo track first (at position 0)
  if (!tempoEvents.empty()) {
    NoteNagaTrack* tempoTrack = new NoteNagaTrack(0, this, "Tempo Track");
    tempoTrack->setTempoTrack(true);
    tempoTrack->setTempoEvents(tempoEvents);
    tracks_tmp.push_back(tempoTrack);
    NOTE_NAGA_LOG_INFO("Created tempo track with " + std::to_string(tempoEvents.size()) + " tempo events from Type 0 MIDI");
  }

  // Create Track for each used channel
  int t_id = tracks_tmp.size();  // Start after tempo track
  for (auto &pair : channel_note_buffers) {
    int channel = pair.first;
    std::vector<NN_Note_t> &note_buffer = pair.second;
    if (note_buffer.empty())
      continue;

    std::string name = channel_names.count(channel)
                           ? channel_names[channel]
                           : "Channel " + std::to_string(channel + 1);
    int instrument =
        channel_instruments.count(channel) ? channel_instruments[channel] : 0;

    NoteNagaTrack *nn_track =
        new NoteNagaTrack(t_id, this, name, instrument, channel);
    std::sort(note_buffer.begin(), note_buffer.end(),
              [](const NN_Note_t &a, const NN_Note_t &b) {
                return a.start < b.start;
              });
    for (auto &note : note_buffer) {
      note.parent = nn_track;
    }
    nn_track->setNotes(note_buffer);
    tracks_tmp.push_back(nn_track);
    ++t_id;
  }
  this->tempo = tempo;
  return tracks_tmp;
}

// --- Helper: load type 1 MIDI file (one track per chunk) ---
std::vector<NoteNagaTrack *>
NoteNagaMidiSeq::loadType1Tracks(const MidiFile *midiFile) {
  NOTE_NAGA_LOG_INFO("Loading Type 1 MIDI tracks");

  std::vector<NoteNagaTrack *> tracks_tmp;

  int tempo = 500000;
  std::vector<NN_TempoEvent_t> tempoEvents;  // Collect all tempo events

  for (int track_idx = 0; track_idx < midiFile->getNumTracks(); ++track_idx) {
    const MidiTrack &track = midiFile->getTrack(track_idx);

    std::map<std::pair<int, int>, std::pair<int, int>>
        notes_on; // (note, channel) -> (start, velocity)
    int abs_time = 0;
    int instrument = 0;
    std::optional<int> channel_used;
    std::string name;
    std::vector<NN_Note_t> note_buffer;

    // create instance of track
    NoteNagaTrack *nn_track = new NoteNagaTrack(track_idx, this);

    // Parse events for this track
    for (const auto &evt : track.events) {
      abs_time += evt.delta_time;

      // Track name
      if (evt.type == MidiEventType::Meta &&
          evt.meta_type == MIDI_META_TRACK_NAME) {
        std::string track_name(evt.meta_data.begin(), evt.meta_data.end());
        size_t endpos = track_name.find_last_not_of('\0');
        if (endpos != std::string::npos)
          track_name = track_name.substr(0, endpos + 1);
        name = track_name;
      }
      // Program change: store instrument
      if (evt.type == MidiEventType::ProgramChange) {
        if (!evt.data.empty()) {
          instrument = evt.data[0];
          if (!channel_used.has_value())
            channel_used = evt.channel;
        }
      }
      // Tempo change: collect from first track (track 0 is usually tempo/conductor track)
      if (evt.type == MidiEventType::Meta &&
          evt.meta_type == MIDI_META_SET_TEMPO && track_idx == 0) {
        if (evt.meta_data.size() == 3) {
          int tempoUs = (evt.meta_data[0] << 16) | (evt.meta_data[1] << 8) |
                        evt.meta_data[2];
          double bpm = 60'000'000.0 / tempoUs;
          tempoEvents.push_back(NN_TempoEvent_t(abs_time, bpm, TempoInterpolation::Step));
          
          // Use first tempo as the fixed tempo
          if (tempo == 500000 || tempoEvents.size() == 1) {
            tempo = tempoUs;
          }
        }
      }
      // Note on
      if (evt.type == MidiEventType::NoteOn && !evt.data.empty() &&
          evt.data[1] > 0) {
        int note = evt.data[0];
        int velocity = evt.data[1];
        int channel = evt.channel;
        if (!channel_used.has_value())
          channel_used = channel;
        notes_on[std::make_pair(note, channel)] =
            std::make_pair(abs_time, velocity);
      }
      // Note off
      else if ((evt.type == MidiEventType::NoteOff) ||
               (evt.type == MidiEventType::NoteOn && !evt.data.empty() &&
                evt.data[1] == 0)) {
        int note = evt.data[0];
        int channel = evt.channel;
        auto key = std::make_pair(note, channel);
        auto it = notes_on.find(key);
        if (it != notes_on.end()) {
          int start = it->second.first;
          int velocity = it->second.second;
          note_buffer.push_back(
              NN_Note_t(note, nn_track, start, abs_time - start, velocity));
          notes_on.erase(it);
        }
      }
    }

    // sort notes by start time and store in track
    std::sort(note_buffer.begin(), note_buffer.end(),
              [](const NN_Note_t &a, const NN_Note_t &b) {
                return a.start < b.start;
              });
    nn_track->setNotes(note_buffer);
    // set channel and instrument
    nn_track->setChannel(channel_used);
    nn_track->setInstrument(instrument);

    // push track to result
    tracks_tmp.push_back(nn_track);
  }
  this->tempo = tempo;
  
  // If we collected multiple tempo events, create a tempo track
  if (tempoEvents.size() > 1) {
    // Create tempo track and insert at beginning
    NoteNagaTrack* tempoTrack = new NoteNagaTrack(0, this, "Tempo Track");
    tempoTrack->setTempoTrack(true);
    tempoTrack->setTempoEvents(tempoEvents);
    
    // Update track IDs
    for (size_t i = 0; i < tracks_tmp.size(); ++i) {
      tracks_tmp[i]->setId(static_cast<int>(i + 1));
    }
    
    // Insert tempo track at beginning
    tracks_tmp.insert(tracks_tmp.begin(), tempoTrack);
    
    NOTE_NAGA_LOG_INFO("Created tempo track with " + std::to_string(tempoEvents.size()) + " tempo events");
  }
  
  return tracks_tmp;
}

/*******************************************************************************************************/
// MIDI Export
/*******************************************************************************************************/

bool NoteNagaMidiSeq::exportToMidi(const std::string &midi_file_path,
                                    const std::set<int> &trackIds) const {
  if (midi_file_path.empty()) {
    NOTE_NAGA_LOG_ERROR("No MIDI file path provided for export");
    return false;
  }

  NOTE_NAGA_LOG_INFO("Exporting MIDI file to: " + midi_file_path);

  MidiFile midiFile;
  midiFile.header.format = 1;  // Type 1: multiple tracks
  midiFile.header.division = ppq;

  // Create tempo track (track 0) with tempo meta events
  MidiTrack tempoTrack;
  
  // Check if we have a tempo track with events
  NoteNagaTrack* tempoTrackPtr = getTempoTrack();
  if (tempoTrackPtr && !tempoTrackPtr->getTempoEvents().empty()) {
    // Export all tempo events from tempo track
    const auto& tempoEvents = tempoTrackPtr->getTempoEvents();
    int lastTick = 0;
    
    for (const auto& te : tempoEvents) {
      MidiEvent tempoEvent;
      tempoEvent.delta_time = te.tick - lastTick;
      tempoEvent.type = MidiEventType::Meta;
      tempoEvent.meta_type = MIDI_META_SET_TEMPO;
      
      // Convert BPM to microseconds per quarter note
      int tempoUs = static_cast<int>(60'000'000.0 / te.bpm);
      tempoEvent.meta_data = {
        static_cast<uint8_t>((tempoUs >> 16) & 0xFF),
        static_cast<uint8_t>((tempoUs >> 8) & 0xFF),
        static_cast<uint8_t>(tempoUs & 0xFF)
      };
      tempoTrack.events.push_back(tempoEvent);
      lastTick = te.tick;
    }
    NOTE_NAGA_LOG_INFO("Exported " + std::to_string(tempoEvents.size()) + " tempo events");
  } else {
    // Use fixed tempo
    MidiEvent tempoEvent;
    tempoEvent.delta_time = 0;
    tempoEvent.type = MidiEventType::Meta;
    tempoEvent.meta_type = MIDI_META_SET_TEMPO;
    // Tempo is stored as 3 bytes (microseconds per quarter note)
    tempoEvent.meta_data = {
      static_cast<uint8_t>((tempo >> 16) & 0xFF),
      static_cast<uint8_t>((tempo >> 8) & 0xFF),
      static_cast<uint8_t>(tempo & 0xFF)
    };
    tempoTrack.events.push_back(tempoEvent);
  }

  // End of track
  MidiEvent eotTempo;
  eotTempo.delta_time = 0;
  eotTempo.type = MidiEventType::Meta;
  eotTempo.meta_type = MIDI_META_END_OF_TRACK;
  tempoTrack.events.push_back(eotTempo);
  
  midiFile.tracks.push_back(tempoTrack);

  // Export each track (filter by trackIds if provided)
  for (const NoteNagaTrack *track : tracks) {
    if (!track) continue;
    
    // Skip tempo track - already exported as MIDI tempo track 0
    if (track->isTempoTrack()) continue;
    
    // Skip tracks not in the filter set (if filter is provided)
    if (!trackIds.empty() && trackIds.find(track->getId()) == trackIds.end()) {
      continue;
    }

    MidiTrack midiTrack;
    uint8_t channel = track->getChannel().value_or(0) & 0x0F;

    // Track name meta event
    MidiEvent nameEvent;
    nameEvent.delta_time = 0;
    nameEvent.type = MidiEventType::Meta;
    nameEvent.meta_type = MIDI_META_TRACK_NAME;
    std::string trackName = track->getName();
    nameEvent.meta_data = std::vector<uint8_t>(trackName.begin(), trackName.end());
    midiTrack.events.push_back(nameEvent);

    // Program change (instrument)
    if (track->getInstrument().has_value()) {
      MidiEvent pcEvent;
      pcEvent.delta_time = 0;
      pcEvent.type = MidiEventType::ProgramChange;
      pcEvent.channel = channel;
      pcEvent.data = {static_cast<uint8_t>(track->getInstrument().value() & 0x7F)};
      midiTrack.events.push_back(pcEvent);
    }

    // Collect all note events (note on/off) with absolute times
    struct NoteEvent {
      int abs_time;
      bool is_note_on;
      uint8_t note;
      uint8_t velocity;
    };
    std::vector<NoteEvent> noteEvents;

    for (const NN_Note_t &note : track->getNotes()) {
      int start = note.start.value_or(0);
      int length = note.length.value_or(ppq);
      uint8_t velocity = static_cast<uint8_t>(note.velocity.value_or(100) & 0x7F);
      uint8_t noteNum = static_cast<uint8_t>(note.note & 0x7F);

      // Note on
      noteEvents.push_back({start, true, noteNum, velocity});
      // Note off
      noteEvents.push_back({start + length, false, noteNum, 0});
    }

    // Sort by absolute time (stable sort to keep note-off before note-on at same time)
    std::stable_sort(noteEvents.begin(), noteEvents.end(),
      [](const NoteEvent &a, const NoteEvent &b) {
        if (a.abs_time != b.abs_time) return a.abs_time < b.abs_time;
        // Note-off before note-on at same time
        return !a.is_note_on && b.is_note_on;
      });

    // Convert to MIDI events with delta times
    int lastTime = 0;
    for (const NoteEvent &ne : noteEvents) {
      MidiEvent evt;
      evt.delta_time = ne.abs_time - lastTime;
      lastTime = ne.abs_time;
      evt.type = ne.is_note_on ? MidiEventType::NoteOn : MidiEventType::NoteOff;
      evt.channel = channel;
      evt.data = {ne.note, ne.velocity};
      midiTrack.events.push_back(evt);
    }

    // End of track
    MidiEvent eot;
    eot.delta_time = 0;
    eot.type = MidiEventType::Meta;
    eot.meta_type = MIDI_META_END_OF_TRACK;
    midiTrack.events.push_back(eot);

    midiFile.tracks.push_back(midiTrack);
  }

  // Save the file
  if (!midiFile.save(midi_file_path)) {
    NOTE_NAGA_LOG_ERROR("Failed to save MIDI file: " + midi_file_path);
    return false;
  }

  NOTE_NAGA_LOG_INFO("MIDI file exported successfully: " + midi_file_path);
  return true;
}

/*******************************************************************************************************/
// General MIDI Instruments Utils
/*******************************************************************************************************/

const std::vector<NN_GMInstrument_t> GM_INSTRUMENTS = {
    {0, "Acoustic Grand Piano", "grand_piano"},
    {1, "Bright Acoustic Piano", "grand_piano"},
    {2, "Electric Grand Piano", "grand_piano"},
    {3, "Honky-tonk Piano", "grand_piano"},
    {4, "Electric Piano 1", "keyboard"},
    {5, "Electric Piano 2", "keyboard"},
    {6, "Harpsichord", "harp"},
    {7, "Clavinet", "keyboard"},
    {8, "Celesta", "keyboard"},
    {9, "Glockenspiel", "xylophone"},
    {10, "Music Box", "keyboard"},
    {11, "Vibraphone", "xylophone"},
    {12, "Marimba", "xylophone"},
    {13, "Xylophone", "xylophone"},
    {14, "Tubular Bells", "xylophone"},
    {15, "Dulcimer", "lyre"},
    {16, "Drawbar Organ", "keyboard"},
    {17, "Percussive Organ", "keyboard"},
    {18, "Rock Organ", "keyboard"},
    {19, "Church Organ", "keyboard"},
    {20, "Reed Organ", "keyboard"},
    {21, "Accordion", "accordion"},
    {22, "Harmonica", "accordion"},
    {23, "Tango Accordion", "accordion"},
    {24, "Acoustic Guitar (nylon)", "acoustic_guitar"},
    {25, "Acoustic Guitar (steel)", "acoustic_guitar"},
    {26, "Electric Guitar (jazz)", "electric_guitar"},
    {27, "Electric Guitar (clean)", "electric_guitar"},
    {28, "Electric Guitar (muted)", "electric_guitar"},
    {29, "Overdriven Guitar", "electric_guitar"},
    {30, "Distortion Guitar", "electric_guitar"},
    {31, "Guitar harmonics", "electric_guitar"},
    {32, "Acoustic Bass", "contrabass"},
    {33, "Electric Bass (finger)", "contrabass"},
    {34, "Electric Bass (pick)", "contrabass"},
    {35, "Fretless Bass", "contrabass"},
    {36, "Slap Bass 1", "contrabass"},
    {37, "Slap Bass 2", "contrabass"},
    {38, "Synth Bass 1", "contrabass"},
    {39, "Synth Bass 2", "contrabass"},
    {40, "Violin", "violin"},
    {41, "Viola", "violin"},
    {42, "Cello", "contrabass"},
    {43, "Contrabass", "contrabass"},
    {44, "Tremolo Strings", "violin"},
    {45, "Pizzicato Strings", "violin"},
    {46, "Orchestral Harp", "harp"},
    {47, "Timpani", "drum"},
    {48, "String Ensemble 1", "lyre"},
    {49, "String Ensemble 2", "lyre"},
    {50, "SynthStrings 1", "lyre"},
    {51, "SynthStrings 2", "lyre"},
    {52, "Choir Aahs", "lyre"},
    {53, "Voice Oohs", "lyre"},
    {54, "Synth Voice", "lyre"},
    {55, "Orchestra Hit", "lyre"},
    {56, "Trumpet", "trumpet"},
    {57, "Trombone", "trombone"},
    {58, "Tuba", "trombone"},
    {59, "Muted Trumpet", "trumpet"},
    {60, "French Horn", "trumpet"},
    {61, "Brass Section", "trumpet"},
    {62, "SynthBrass 1", "trumpet"},
    {63, "SynthBrass 2", "trumpet"},
    {64, "Soprano Sax", "clarinet"},
    {65, "Alto Sax", "clarinet"},
    {66, "Tenor Sax", "clarinet"},
    {67, "Baritone Sax", "clarinet"},
    {68, "Oboe", "clarinet"},
    {69, "English Horn", "clarinet"},
    {70, "Bassoon", "clarinet"},
    {71, "Clarinet", "clarinet"},
    {72, "Piccolo", "recorder"},
    {73, "Flute", "recorder"},
    {74, "Recorder", "recorder"},
    {75, "Pan Flute", "pan_flute"},
    {76, "Blown Bottle", "recorder"},
    {77, "Shakuhachi", "recorder"},
    {78, "Whistle", "recorder"},
    {79, "Ocarina", "recorder"},
    {80, "Lead 1 (square)", "keyboard"},
    {81, "Lead 2 (sawtooth)", "keyboard"},
    {82, "Lead 3 (calliope)", "keyboard"},
    {83, "Lead 4 (chiff)", "keyboard"},
    {84, "Lead 5 (charang)", "keyboard"},
    {85, "Lead 6 (voice)", "keyboard"},
    {86, "Lead 7 (fifths)", "keyboard"},
    {87, "Lead 8 (bass + lead)", "keyboard"},
    {88, "Pad 1 (new age)", "keyboard"},
    {89, "Pad 2 (warm)", "keyboard"},
    {90, "Pad 3 (polysynth)", "keyboard"},
    {91, "Pad 4 (choir)", "keyboard"},
    {92, "Pad 5 (bowed)", "keyboard"},
    {93, "Pad 6 (metallic)", "keyboard"},
    {94, "Pad 7 (halo)", "keyboard"},
    {95, "Pad 8 (sweep)", "keyboard"},
    {96, "FX 1 (rain)", "vinyl"},
    {97, "FX 2 (soundtrack)", "vinyl"},
    {98, "FX 3 (crystal)", "vinyl"},
    {99, "FX 4 (atmosphere)", "vinyl"},
    {100, "FX 5 (brightness)", "vinyl"},
    {101, "FX 6 (goblins)", "vinyl"},
    {102, "FX 7 (echoes)", "vinyl"},
    {103, "FX 8 (sci-fi)", "vinyl"},
    {104, "Sitar", "acoustic_guitar"},
    {105, "Banjo", "banjo"},
    {106, "Shamisen", "acoustic_guitar"},
    {107, "Koto", "lyre"},
    {108, "Kalimba", "lyre"},
    {109, "Bag pipe", "bagpipes"},
    {110, "Fiddle", "violin"},
    {111, "Shanai", "clarinet"},
    {112, "Tinkle Bell", "xylophone"},
    {113, "Agogo", "drum"},
    {114, "Steel Drums", "drum"},
    {115, "Woodblock", "snare_drum"},
    {116, "Taiko Drum", "drum"},
    {117, "Melodic Tom", "drum"},
    {118, "Synth Drum", "drum"},
    {119, "Reverse Cymbal", "cymbal"},
    {120, "Guitar Fret Noise", "electric_guitar"},
    {121, "Breath Noise", "vinyl"},
    {122, "Seashore", "vinyl"},
    {123, "Bird Tweet", "vinyl"},
    {124, "Telephone Ring", "vinyl"},
    {125, "Helicopter", "vinyl"},
    {126, "Applause", "vinyl"},
    {127, "Gunshot", "vinyl"}};

std::optional<NN_GMInstrument_t>
nn_find_instrument_by_name(const std::string &name) {
  auto it = std::find_if(GM_INSTRUMENTS.begin(), GM_INSTRUMENTS.end(),
                         [&](const NN_GMInstrument_t &instr) {
                           return instr.name.compare(name) == 0;
                         });
  if (it != GM_INSTRUMENTS.end())
    return *it;
  return std::nullopt;
}

std::optional<NN_GMInstrument_t> nn_find_instrument_by_index(int index) {
  auto it = std::find_if(
      GM_INSTRUMENTS.begin(), GM_INSTRUMENTS.end(),
      [&](const NN_GMInstrument_t &instr) { return instr.index == index; });
  if (it != GM_INSTRUMENTS.end())
    return *it;
  return std::nullopt;
}

/*******************************************************************************************************/
// Note Names Utils
/*******************************************************************************************************/

const std::vector<std::string> NOTE_NAMES = {"C",  "C#", "D",  "D#", "E",  "F",
                                             "F#", "G",  "G#", "A",  "A#", "B"};

std::string nn_note_name(int n) {
  return NOTE_NAMES.at(n % 12) + std::to_string(n / 12 - 1);
}

int nn_index_in_octave(int n) { return n % 12; }

/*******************************************************************************************************/
// Time / Tick Utils
/*******************************************************************************************************/

double nn_seconds_to_ticks(double seconds, int ppq, int tempo) {
  double us_per_tick = double(tempo) / double(ppq);
  return seconds * 1'000'000.0 / us_per_tick;
}

double nn_ticks_to_seconds(int ticks, int ppq, int tempo) {
  double us_per_tick = double(tempo) / double(ppq);
  return ticks * us_per_tick / 1'000'000.0;
}

/*******************************************************************************************************/
// Audio Analysis Utils
/*******************************************************************************************************/

void nn_fft(std::vector<std::complex<float>> &a) {
  const size_t N = a.size();
  size_t j = 0;
  for (size_t i = 1; i < N; ++i) {
    size_t bit = N >> 1;
    while (j & bit) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= N; len <<= 1) {
    float ang = -2 * M_PI / len;
    std::complex<float> wlen(cosf(ang), sinf(ang));
    for (size_t i = 0; i < N; i += len) {
      std::complex<float> w(1);
      for (size_t j = 0; j < len / 2; ++j) {
        auto u = a[i + j];
        auto v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
}