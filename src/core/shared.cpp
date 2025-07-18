#include "shared.h"
#include <algorithm>

// ---------- Channel colors ----------
const std::vector<QColor> DEFAULT_CHANNEL_COLORS = {
    QColor(0, 180, 255), QColor(255, 100, 100), QColor(250, 200, 75), QColor(90, 230, 120),
    QColor(180, 110, 255), QColor(170, 180, 70), QColor(95, 220, 210), QColor(230, 90, 210),
    QColor(70, 180, 90), QColor(255, 180, 60), QColor(210, 80, 80), QColor(80, 120, 255),
    QColor(255, 230, 80), QColor(110, 255, 120), QColor(220, 160, 255), QColor(100, 180, 160)};

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

// ---------- Track ----------

Track::Track()
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

Track::Track(int track_id,
          const QString &name,
          std::optional<int> instrument = std::nullopt,
          std::optional<int> channel = std::nullopt)
{
    this->track_id = track_id;
    this->name = name.isEmpty() ? QString("Track %1").arg(track_id + 1) : name;
    this->instrument = instrument;
    this->channel = channel;
    this->color = DEFAULT_CHANNEL_COLORS[track_id % DEFAULT_CHANNEL_COLORS.size()];
    this->visible = true;
    this->muted = false;
    this->solo = false;
    this->volume = 1.0f;
}

// ---------- Note names ----------
const std::vector<QString> NOTE_NAMES = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

QColor color_blend(const QColor &fg, const QColor &bg, double opacity)
{
    // opacity: 0.0 = jen bg, 1.0 = jen fg
    double a = opacity;
    int r = int(a * fg.red()   + (1 - a) * bg.red());
    int g = int(a * fg.green() + (1 - a) * bg.green());
    int b = int(a * fg.blue()  + (1 - a) * bg.blue());
    return QColor(r, g, b);
}

// ---------- Utility functions ----------
QString note_name(int n)
{
    return NOTE_NAMES.at(n % 12) + QString::number(n / 12 - 1);
}

int index_in_octave(int n)
{
    return n % 12;
}

double note_time_ms(const MidiNote &note, int ppq, int tempo)
{
    if (!note.length.has_value() || note.length.value() <= 0)
        return 0.0;
    double us_per_tick = static_cast<double>(tempo) / ppq;
    double total_us = note.length.value() * us_per_tick;
    return total_us / 1000.0;
}

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