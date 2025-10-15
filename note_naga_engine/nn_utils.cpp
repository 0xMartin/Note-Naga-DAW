#include <note_naga_engine/nn_utils.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

void NN_Utils::quantize(NoteNagaMidiSeq &seq, int grid_divisor)
{
    if (grid_divisor == 0)
        return;

    const int grid_ticks = seq.getPPQ() / (grid_divisor / 4.0);
    if (grid_ticks <= 0)
        return;

    bool changed = false;

    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            if (note.start.has_value())
            {
                int old_start = *note.start;
                int new_start = static_cast<int>(std::round(static_cast<double>(old_start) / grid_ticks)) * grid_ticks;
                if (old_start != new_start)
                {
                    note.start = new_start;
                    changed = true;
                }
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::humanize(NoteNagaMidiSeq &seq, int time_strength, int vel_strength)
{
    if (time_strength == 0 && vel_strength == 0)
        return;

    // Použití moderního C++ pro generování náhodných čísel
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> time_dist(-time_strength, time_strength);
    std::uniform_int_distribution<> vel_dist(-vel_strength, vel_strength);

    bool changed = false;

    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            if (note.start.has_value() && time_strength > 0)
            {
                int new_start = *note.start + time_dist(gen);
                note.start = std::max(0, new_start);
                changed = true;
            }
            if (note.velocity.has_value() && vel_strength > 0)
            {
                int new_vel = *note.velocity + vel_dist(gen);
                note.velocity = std::clamp(new_vel, 0, 127);
                changed = true;
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::transpose(NoteNagaMidiSeq &seq, int semitones)
{
    if (semitones == 0)
        return;

    bool changed = false;
    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            int new_pitch = note.note + semitones;
            int clamped_pitch = std::clamp(new_pitch, 0, 127);
            if (clamped_pitch != note.note)
            {
                note.note = clamped_pitch;
                changed = true;
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::changeVelocity(NoteNagaMidiSeq &seq, int value, bool relative)
{
    bool changed = false;
    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            if (note.velocity.has_value())
            {
                int old_vel = *note.velocity;
                int new_vel;
                if (relative)
                {
                    new_vel = (old_vel * value) / 100;
                }
                else
                {
                    new_vel = value;
                }
                new_vel = std::clamp(new_vel, 0, 127);
                if (new_vel != old_vel)
                {
                    note.velocity = new_vel;
                    changed = true;
                }
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::changeDuration(NoteNagaMidiSeq &seq, int value, bool relative)
{
    bool changed = false;
    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            if (note.length.has_value())
            {
                int old_len = *note.length;
                int new_len;
                if (relative)
                {
                    new_len = (old_len * value) / 100;
                }
                else
                {
                    new_len = value;
                }
                new_len = std::max(1, new_len); // Délka musí být alespoň 1 tick
                if (new_len != old_len)
                {
                    note.length = new_len;
                    changed = true;
                }
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::legato(NoteNagaMidiSeq &seq, int strength_percent)
{
    if (strength_percent <= 0)
        return;
    double factor = strength_percent / 100.0;
    bool changed = false;

    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        if (notes.size() < 2)
            continue;

        // Seřadit noty podle času začátku
        std::sort(notes.begin(), notes.end(), [](const NN_Note_t &a, const NN_Note_t &b)
                  { return a.start.value_or(0) < b.start.value_or(0); });

        for (size_t i = 0; i < notes.size() - 1; ++i)
        {
            if (notes[i].start.has_value() && notes[i].length.has_value() && notes[i + 1].start.has_value())
            {
                int current_end = *notes[i].start + *notes[i].length;
                int next_start = *notes[i + 1].start;
                if (next_start > *notes[i].start)
                {
                    int ideal_length = next_start - *notes[i].start;
                    int new_length = *notes[i].length + static_cast<int>((ideal_length - *notes[i].length) * factor);
                    new_length = std::max(1, new_length);

                    if (new_length != *notes[i].length)
                    {
                        notes[i].length = new_length;
                        changed = true;
                    }
                }
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::staccato(NoteNagaMidiSeq &seq, int strength_percent)
{
    if (strength_percent < 0 || strength_percent > 100)
        return;

    bool changed = false;
    double factor = strength_percent / 100.0;

    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            if (note.length.has_value())
            {
                int old_len = *note.length;
                int new_len = static_cast<int>(old_len * factor);
                new_len = std::max(1, new_len);
                if (new_len != old_len)
                {
                    note.length = new_len;
                    changed = true;
                }
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::invert(NoteNagaMidiSeq &seq, int axis_note)
{
    bool changed = false;
    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            int distance = note.note - axis_note;
            int new_pitch = axis_note - distance;
            int clamped_pitch = std::clamp(new_pitch, 0, 127);
            if (clamped_pitch != note.note)
            {
                note.note = clamped_pitch;
                changed = true;
            }
        }
        track->setNotes(notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::retrograde(NoteNagaMidiSeq &seq)
{
    bool changed = false;
    int max_tick = seq.computeMaxTick();

    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        if (notes.empty())
            continue;

        changed = true;
        std::vector<NN_Note_t> reversed_notes;
        reversed_notes.reserve(notes.size());

        for (const auto &note : notes)
        {
            if (note.start.has_value() && note.length.has_value())
            {
                NN_Note_t new_note = note;
                int note_end = *note.start + *note.length;
                new_note.start = max_tick - note_end;
                // Délka zůstává stejná, mění se jen pozice
                reversed_notes.push_back(new_note);
            }
        }
        track->setNotes(reversed_notes);
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::deleteOverlappingNotes(NoteNagaMidiSeq &seq)
{
    bool changed = false;
    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        if (notes.size() < 2)
            continue;

        // Seřadit noty podle výšky a pak podle času
        std::sort(notes.begin(), notes.end(), [](const NN_Note_t &a, const NN_Note_t &b)
                  {
            if (a.note != b.note) return a.note < b.note;
            return a.start.value_or(0) < b.start.value_or(0); });

        std::vector<NN_Note_t> cleaned_notes;
        cleaned_notes.push_back(notes[0]);

        for (size_t i = 1; i < notes.size(); ++i)
        {
            NN_Note_t &last_note = cleaned_notes.back();
            NN_Note_t &current_note = notes[i];

            if (last_note.note == current_note.note && last_note.start.has_value() && last_note.length.has_value() && current_note.start.has_value())
            {
                int last_note_end = *last_note.start + *last_note.length;
                // Pokud aktuální nota začíná před koncem poslední noty stejné výšky, přeskočíme ji
                if (*current_note.start < last_note_end)
                {
                    continue;
                }
            }
            cleaned_notes.push_back(current_note);
        }

        if (cleaned_notes.size() != notes.size())
        {
            changed = true;
            track->setNotes(cleaned_notes);
        }
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}

void NN_Utils::scaleTiming(NoteNagaMidiSeq &seq, double factor)
{
    if (factor <= 0.0 || factor == 1.0)
        return;

    bool changed = false;
    for (auto &track : seq.getTracks())
    {
        std::vector<NN_Note_t> notes = track->getNotes();
        for (auto &note : notes)
        {
            if (note.start.has_value())
            {
                note.start = static_cast<int>(*note.start * factor);
            }
            if (note.length.has_value())
            {
                note.length = std::max(1, static_cast<int>(*note.length * factor));
            }
        }
        track->setNotes(notes);
        changed = true;
    }

    if (changed)
    {
        NN_QT_EMIT(seq.metadataChanged(&seq, "notes"));
    }
}