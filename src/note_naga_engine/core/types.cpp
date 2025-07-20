#include "types.h"

#include <algorithm>
#include <iostream>
#include <QFileInfo>
#include <atomic>

static std::atomic<int> next_id = 1;

// ---------- Note Naga Note ----------

double note_time_ms(const NoteNagaNote &note, int ppq, int tempo)
{
    if (!note.length.has_value() || note.length.value() <= 0)
        return 0.0;
    double us_per_tick = static_cast<double>(tempo) / ppq;
    double total_us = note.length.value() * us_per_tick;
    return total_us / 1000.0;
}

int generate_random_id() {
    int id = next_id;
    next_id++;
    return id;
}

// ---------- Note Naga Track ----------

NoteNagaTrack::NoteNagaTrack()
{
    this->track_id = 0;
    this->name = name.isEmpty() ? QString("Track %1").arg(track_id + 1) : name;
    this->instrument = std::nullopt;
    this->channel = std::nullopt;
    this->color = DEFAULT_CHANNEL_COLORS[0];
    this->visible = true;
    this->muted = false;
    this->solo = false;
    this->volume = 1.0f;
}

NoteNagaTrack::NoteNagaTrack(int track_id,
                             NoteNagaMIDISeq *parent,
                             const QString &name,
                             const std::optional<int> &instrument,
                             const std::optional<int> &channel)
{
    this->track_id = track_id;
    this->parent = parent;
    this->name = name.isEmpty() ? QString("Track %1").arg(track_id + 1) : name;
    this->instrument = instrument;
    this->channel = channel;
    this->color = DEFAULT_CHANNEL_COLORS[track_id % DEFAULT_CHANNEL_COLORS.size()];
    this->visible = true;
    this->muted = false;
    this->solo = false;
    this->volume = 1.0f;
}

void NoteNagaTrack::set_notes(const std::vector<NoteNagaNote> &notes)
{
    this->midi_notes = notes;
}

void NoteNagaTrack::set_instrument(std::optional<int> instrument)
{
    if (this->instrument == instrument)
        return;
    this->instrument = instrument;
    NN_QT_EMIT(meta_changed_signal(this, "instrument"));
}

void NoteNagaTrack::set_channel(std::optional<int> channel)
{
    if (this->channel == channel)
        return;
    this->channel = channel;
    NN_QT_EMIT(meta_changed_signal(this, "channel"));
}

void NoteNagaTrack::set_id(int new_id)
{
    if (this->track_id == new_id)
        return;
    this->track_id = new_id;
    NN_QT_EMIT(meta_changed_signal(this, "id"));
}

void NoteNagaTrack::set_name(const QString &new_name)
{
    if (this->name == new_name)
        return;
    this->name = new_name;
    NN_QT_EMIT(meta_changed_signal(this, "name"));
}

void NoteNagaTrack::set_color(const QColor &new_color)
{
    if (this->color == new_color)
        return;
    this->color = new_color;
    NN_QT_EMIT(meta_changed_signal(this, "color"));
}

void NoteNagaTrack::set_visible(bool is_visible)
{
    if (this->visible == is_visible)
        return;
    this->visible = is_visible;
    NN_QT_EMIT(meta_changed_signal(this, "visible"));
}

void NoteNagaTrack::set_muted(bool is_muted)
{
    if (this->muted == is_muted)
        return;
    this->muted = is_muted;
    NN_QT_EMIT(meta_changed_signal(this, "muted"));
}

void NoteNagaTrack::set_solo(bool is_solo)
{
    if (this->solo == is_solo)
        return;
    this->solo = is_solo;
    NN_QT_EMIT(meta_changed_signal(this, "solo"));
}

void NoteNagaTrack::set_volume(float new_volume)
{
    if (this->volume == new_volume)
        return;
    this->volume = new_volume;
    NN_QT_EMIT(meta_changed_signal(this, "volume"));
}

// ---------- Note Naga MIDI file Sequence ----------

NoteNagaMIDISeq::NoteNagaMIDISeq()
{
    this->sequence_id = rand();
    this->clear();
}

NoteNagaMIDISeq::NoteNagaMIDISeq(int sequence_id)
{
    this->sequence_id = sequence_id;
    this->clear();
}

NoteNagaMIDISeq::NoteNagaMIDISeq(int sequence_id, std::vector<NoteNagaTrack*> tracks)
{
    this->sequence_id = sequence_id;
    this->clear();
    this->tracks = std::move(tracks);
}

NoteNagaMIDISeq::~NoteNagaMIDISeq()
{
    clear();
}

void NoteNagaMIDISeq::clear()
{
    std::cout << "AppContext: Clearing context" << std::endl;
    // Dealokace všech tracků
    for (NoteNagaTrack *track : this->tracks) {
        if (track) delete track;
    }
    this->tracks.clear();
    // Dealokace midi_file
    if (midi_file) {
        delete midi_file;
        midi_file = nullptr;
    }
    ppq = 480;
    tempo = 500000;
    active_track_id.reset();
    max_tick = 0;
}

void NoteNagaMIDISeq::set_active_track_id(std::optional<int> track_id)
{
    if (!track_id.has_value())
    {
        this->active_track_id.reset();
        return;
    }
    if (track_id < 0 || track_id >= tracks.size())
    {
        std::cerr << "Invalid track ID: " << track_id.value() << std::endl;
        return;
    }
    this->active_track_id = track_id;
    NN_QT_EMIT(active_track_changed_signal(get_track_by_id(track_id.value())));
}

NoteNagaTrack* NoteNagaMIDISeq::get_active_track()
{
    if (active_track_id.has_value())
    {
        return get_track_by_id(*active_track_id);
    }
    return nullptr;
}

NoteNagaTrack* NoteNagaMIDISeq::get_track_by_id(int track_id)
{
    for (NoteNagaTrack* tr : tracks) {
        if (tr && tr->get_id() == track_id)
            return tr;
    }
    return nullptr;
}

int NoteNagaMIDISeq::compute_max_tick()
{
    this->max_tick = 0;
    for (const auto &track : tracks)
    {
        for (const auto &note : track->get_notes())
        {
            if (note.start.has_value() && note.length.has_value())
                this->max_tick = std::max(this->max_tick, note.start.value() + note.length.value());
        }
    }
    return this->max_tick;
}

void NoteNagaMIDISeq::load_from_midi(const QString &midi_file_path)
{
    // Check for empty path
    if (midi_file_path.isEmpty())
    {
        std::cout << "AppContext: No MIDI file path provided." << std::endl;
        return;
    }
    QFileInfo fi(midi_file_path);
    if (!fi.exists())
    {
        std::cout << "AppContext: MIDI file " << midi_file_path.toStdString() << " does not exist." << std::endl;
        return;
    }

    std::cout << "AppContext: Loading MIDI file from " << midi_file_path.toStdString() << std::endl;
    clear();

    MidiFile* midiFile = new MidiFile();
    if (!midiFile->load(midi_file_path.toStdString()))
    {
        std::cout << "AppContext: Failed to load MIDI file." << std::endl;
        delete midiFile;
        return;
    }
    this->midi_file = midiFile;
    this->ppq = midiFile->header.division;

    // Split logic for type 0 and type 1 into helper methods
    std::vector<NoteNagaTrack*> tracks_tmp;
    if (midiFile->header.format == 0 && midiFile->getNumTracks() == 1)
    {
        tracks_tmp = load_type0_tracks(midiFile);
    }
    else
    {
        tracks_tmp = load_type1_tracks(midiFile);
    }

    // Set the tracks
    this->tracks = tracks_tmp;
    compute_max_tick();

    // Set the active track
    if (!tracks.empty())
    {
        active_track_id = tracks[0]->get_id();
    }
    else
    {
        active_track_id.reset();
    }

    // signals
    for (NoteNagaTrack* track : this->tracks)
    {
        if (!track)
            continue;
        connect(track, &NoteNagaTrack::meta_changed_signal, this, &NoteNagaMIDISeq::track_meta_changed_signal);
    }
}

// --- Helper: load type 0 MIDI file (split channels) ---
std::vector<NoteNagaTrack*> NoteNagaMIDISeq::load_type0_tracks(const MidiFile *midiFile)
{
    qDebug() << "NoteNagaMIDISequence: Loading type 0 MIDI file with " << midiFile->getNumTracks() << " tracks.";
    std::vector<NoteNagaTrack*> tracks_tmp;

    // Only one track - need to split by MIDI channel
    const MidiTrack &track = midiFile->getTrack(0);
    int abs_time = 0;
    std::map<std::pair<int, int>, std::pair<int, int>> notes_on; // (note, channel) -> (start, velocity)
    std::map<int, std::vector<NoteNagaNote>> channel_note_buffers;
    std::map<int, int> channel_instruments;
    std::map<int, QString> channel_names;

    int tempo = 500000;

    // Parse all events and group notes per channel
    for (const auto &evt : track.events)
    {
        abs_time += evt.delta_time;
        // Track name: store for all channels
        if (evt.type == MidiEventType::Meta && evt.meta_type == MIDI_META_TRACK_NAME)
        {
            std::string track_name(evt.meta_data.begin(), evt.meta_data.end());
            size_t endpos = track_name.find_last_not_of('\0');
            if (endpos != std::string::npos)
                track_name = track_name.substr(0, endpos + 1);
            for (int ch = 0; ch < 16; ++ch)
            {
                channel_names[ch] = QString::fromStdString(track_name);
            }
        }
        // Program change: store instrument per channel
        if (evt.type == MidiEventType::ProgramChange && !evt.data.empty())
        {
            channel_instruments[evt.channel] = evt.data[0];
        }
        // Tempo change: only once
        if (evt.type == MidiEventType::Meta && evt.meta_type == MIDI_META_SET_TEMPO)
        {
            if (evt.meta_data.size() == 3)
            {
                tempo = (evt.meta_data[0] << 16) | (evt.meta_data[1] << 8) | evt.meta_data[2];
            }
        }
        // Note on: register note start per channel
        if (evt.type == MidiEventType::NoteOn && !evt.data.empty() && evt.data[1] > 0)
        {
            int note = evt.data[0];
            int velocity = evt.data[1];
            int channel = evt.channel;
            notes_on[std::make_pair(note, channel)] = std::make_pair(abs_time, velocity);
        }
        // Note off: finish note per channel
        else if ((evt.type == MidiEventType::NoteOff) || (evt.type == MidiEventType::NoteOn && !evt.data.empty() && evt.data[1] == 0))
        {
            int note = evt.data[0];
            int channel = evt.channel;
            auto key = std::make_pair(note, channel);
            auto it = notes_on.find(key);
            if (it != notes_on.end())
            {
                int start = it->second.first;
                int velocity = it->second.second;
                channel_note_buffers[channel].push_back(
                    NoteNagaNote(note, nullptr, start, abs_time - start, velocity)
                );
                notes_on.erase(it);
            }
        }
    }

    // Create Track for each used channel
    int t_id = 0;
    for (auto &pair : channel_note_buffers)
    {
        int channel = pair.first;
        std::vector<NoteNagaNote> &note_buffer = pair.second;
        if (note_buffer.empty())
            continue;

        QString name = channel_names.count(channel) ? channel_names[channel] : QString("Channel %1").arg(channel + 1);
        int instrument = channel_instruments.count(channel) ? channel_instruments[channel] : 0;

        NoteNagaTrack *nn_track = new NoteNagaTrack(
            t_id,
            this,
            name,
            instrument,
            channel);
        std::sort(note_buffer.begin(), note_buffer.end(), [](const NoteNagaNote &a, const NoteNagaNote &b)
                  { return a.start < b.start; });
        for (auto &note : note_buffer)
        {
            note.parent = nn_track; 
        }
        nn_track->set_notes(note_buffer);
        tracks_tmp.push_back(nn_track);
        ++t_id;
    }
    this->tempo = tempo;
    return tracks_tmp;
}

// --- Helper: load type 1 MIDI file (one track per chunk) ---
std::vector<NoteNagaTrack*> NoteNagaMIDISeq::load_type1_tracks(const MidiFile *midiFile)
{
    qDebug() << "NoteNagaMIDISequence: Loading type 1 MIDI file with " << midiFile->getNumTracks() << " tracks.";
    std::vector<NoteNagaTrack*> tracks_tmp;

    int tempo = 500000;

    for (int track_idx = 0; track_idx < midiFile->getNumTracks(); ++track_idx)
    {
        const MidiTrack &track = midiFile->getTrack(track_idx);

        std::map<std::pair<int, int>, std::pair<int, int>> notes_on; // (note, channel) -> (start, velocity)
        int abs_time = 0;
        int instrument = 0;
        std::optional<int> channel_used;
        QString name;
        std::vector<NoteNagaNote> note_buffer;

        // create instance of track
        NoteNagaTrack *nn_track = new NoteNagaTrack(
            track_idx,
            this
        );

        // Parse events for this track
        for (const auto &evt : track.events)
        {
            abs_time += evt.delta_time;

            // Track name
            if (evt.type == MidiEventType::Meta && evt.meta_type == MIDI_META_TRACK_NAME)
            {
                std::string track_name(evt.meta_data.begin(), evt.meta_data.end());
                size_t endpos = track_name.find_last_not_of('\0');
                if (endpos != std::string::npos)
                    track_name = track_name.substr(0, endpos + 1);
                name = QString::fromStdString(track_name);
            }
            // Program change: store instrument
            if (evt.type == MidiEventType::ProgramChange)
            {
                if (!evt.data.empty())
                {
                    instrument = evt.data[0];
                    if (!channel_used.has_value())
                        channel_used = evt.channel;
                }
            }
            // Tempo change: only from first track
            if (evt.type == MidiEventType::Meta && evt.meta_type == MIDI_META_SET_TEMPO && track_idx == 0)
            {
                if (evt.meta_data.size() == 3)
                {
                    tempo = (evt.meta_data[0] << 16) | (evt.meta_data[1] << 8) | evt.meta_data[2];
                }
            }
            // Note on
            if (evt.type == MidiEventType::NoteOn && !evt.data.empty() && evt.data[1] > 0)
            {
                int note = evt.data[0];
                int velocity = evt.data[1];
                int channel = evt.channel;
                if (!channel_used.has_value())
                    channel_used = channel;
                notes_on[std::make_pair(note, channel)] = std::make_pair(abs_time, velocity);
            }
            // Note off
            else if ((evt.type == MidiEventType::NoteOff) || (evt.type == MidiEventType::NoteOn && !evt.data.empty() && evt.data[1] == 0))
            {
                int note = evt.data[0];
                int channel = evt.channel;
                auto key = std::make_pair(note, channel);
                auto it = notes_on.find(key);
                if (it != notes_on.end())
                {
                    int start = it->second.first;
                    int velocity = it->second.second;
                    note_buffer.push_back(
                        NoteNagaNote(note, nn_track, start, abs_time - start, velocity));
                    notes_on.erase(it);
                }
            }
        }

        // sort notes by start time and store in track
        std::sort(note_buffer.begin(), note_buffer.end(), [](const NoteNagaNote &a, const NoteNagaNote &b)
                  { return a.start < b.start; });
        nn_track->set_notes(note_buffer);
        // set channel and instrument
        nn_track->set_channel(channel_used);
        nn_track->set_instrument(instrument);

        // push track to result
        tracks_tmp.push_back(nn_track);
    }
    this->tempo = tempo;
    return tracks_tmp;
}

void NoteNagaMIDISeq::set_id(int new_id)
{
    if (this->sequence_id == new_id)
        return;
    sequence_id = new_id;
    NN_QT_EMIT(meta_changed_signal(this, "id"));
}

void NoteNagaMIDISeq::set_ppq(int ppq)
{
    if (this->ppq == ppq)
        return;
    this->ppq = ppq;
    NN_QT_EMIT(meta_changed_signal(this, "ppq"));
}

void NoteNagaMIDISeq::set_tempo(int tempo)
{
    if (this->tempo == tempo)
        return;
    this->tempo = tempo;
    NN_QT_EMIT(meta_changed_signal(this, "tempo"));
}

void NoteNagaMIDISeq::set_active_track_id(std::optional<int> track_id)
{
    if (this->active_track_id == track_id)
        return;
    active_track_id = track_id;
    NN_QT_EMIT(meta_changed_signal(this, "active_track_id"));
}

void NoteNagaMIDISeq::set_solo_track_id(std::optional<int> track_id)
{
    if (this->solo_track_id == track_id)
        return;
    solo_track_id = track_id;
    NN_QT_EMIT(meta_changed_signal(this, "solo_track_id"));
}

// ---------- Channel colors ----------

const std::vector<QColor> DEFAULT_CHANNEL_COLORS = {
    QColor(0, 180, 255), QColor(255, 100, 100), QColor(250, 200, 75), QColor(90, 230, 120),
    QColor(180, 110, 255), QColor(170, 180, 70), QColor(95, 220, 210), QColor(230, 90, 210),
    QColor(70, 180, 90), QColor(255, 180, 60), QColor(210, 80, 80), QColor(80, 120, 255),
    QColor(255, 230, 80), QColor(110, 255, 120), QColor(220, 160, 255), QColor(100, 180, 160)};

QColor color_blend(const QColor &fg, const QColor &bg, double opacity)
{
    // opacity: 0.0 = jen bg, 1.0 = jen fg
    double a = opacity;
    int r = int(a * fg.red() + (1 - a) * bg.red());
    int g = int(a * fg.green() + (1 - a) * bg.green());
    int b = int(a * fg.blue() + (1 - a) * bg.blue());
    return QColor(r, g, b);
}

// ---------- GM Instruments ----------

const std::vector<GMInstrument> GM_INSTRUMENTS = {
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

std::optional<GMInstrument> find_instrument_by_name(const QString &name)
{
    auto it = std::find_if(GM_INSTRUMENTS.begin(), GM_INSTRUMENTS.end(),
                           [&](const GMInstrument &instr)
                           { return instr.name.compare(name, Qt::CaseInsensitive) == 0; });
    if (it != GM_INSTRUMENTS.end())
        return *it;
    return std::nullopt;
}

std::optional<GMInstrument> find_instrument_by_index(int index)
{
    auto it = std::find_if(GM_INSTRUMENTS.begin(), GM_INSTRUMENTS.end(),
                           [&](const GMInstrument &instr)
                           { return instr.index == index; });
    if (it != GM_INSTRUMENTS.end())
        return *it;
    return std::nullopt;
}

// ---------- Note names ----------

const std::vector<QString> NOTE_NAMES = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

QString note_name(int n)
{
    return NOTE_NAMES.at(n % 12) + QString::number(n / 12 - 1);
}

int index_in_octave(int n)
{
    return n % 12;
}