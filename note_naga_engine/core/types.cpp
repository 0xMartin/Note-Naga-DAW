#include <note_naga_engine/core/types.h>

#include <algorithm>
#include <atomic>

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
  this->visible = true;
  this->muted = false;
  this->solo = false;
  this->volume = 1.0f;
  NOTE_NAGA_LOG_INFO("Created Track with ID: " + std::to_string(track_id) +
                     " and name: " + this->name);
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
  this->tempo = 500000;
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

bool NoteNagaMidiSeq::addTrack(int instrument_index) {
  if (instrument_index < 0)
    return false;
  if (instrument_index > 127)
    return false;
  int track_id = this->tracks.size();
  this->tracks.push_back(new NoteNagaTrack(
      track_id, this, "Track " + std::to_string(track_id),
      instrument_index >= 0 ? std::optional<int>(instrument_index)
                            : std::nullopt,
      0));

  NN_QT_EMIT(trackListChanged());
  return true;
}

bool NoteNagaMidiSeq::removeTrack(int track_id_or_index) {
  if (track_id_or_index < 0 || track_id_or_index >= this->tracks.size())
    return false;

  delete this->tracks[track_id_or_index];
  this->tracks.erase(this->tracks.begin() + track_id_or_index);
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
    // Tempo change: only once
    if (evt.type == MidiEventType::Meta &&
        evt.meta_type == MIDI_META_SET_TEMPO) {
      if (evt.meta_data.size() == 3) {
        tempo = (evt.meta_data[0] << 16) | (evt.meta_data[1] << 8) |
                evt.meta_data[2];
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

  // Create Track for each used channel
  int t_id = 0;
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
      // Tempo change: only from first track
      if (evt.type == MidiEventType::Meta &&
          evt.meta_type == MIDI_META_SET_TEMPO && track_idx == 0) {
        if (evt.meta_data.size() == 3) {
          tempo = (evt.meta_data[0] << 16) | (evt.meta_data[1] << 8) |
                  evt.meta_data[2];
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
  return tracks_tmp;
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