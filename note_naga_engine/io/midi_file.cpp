#include "midi_file.h"

#include <fstream>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>

MidiFile::MidiFile() {
    header.format = 1;
    header.nTracks = 0;
    header.division = 480;
}

void MidiFile::clear() {
    tracks.clear();
    header.nTracks = 0;
}

int MidiFile::getNumTracks() const {
    return static_cast<int>(tracks.size());
}

const MidiTrack& MidiFile::getTrack(int idx) const {
    return tracks.at(idx);
}

MidiTrack& MidiFile::getTrack(int idx) {
    return tracks.at(idx);
}

static void read_exact(std::istream& in, char* buf, size_t len) {
    in.read(buf, len);
    if (in.gcount() != static_cast<std::streamsize>(len))
        throw std::runtime_error("Unexpected EOF");
}

uint16_t MidiFile::readBE16(std::istream &in) {
    char buf[2];
    read_exact(in, buf, 2);
    return (static_cast<uint8_t>(buf[0]) << 8) | static_cast<uint8_t>(buf[1]);
}

uint32_t MidiFile::readBE32(std::istream &in) {
    char buf[4];
    read_exact(in, buf, 4);
    return (static_cast<uint8_t>(buf[0]) << 24) | (static_cast<uint8_t>(buf[1]) << 16)
         | (static_cast<uint8_t>(buf[2]) << 8)  | static_cast<uint8_t>(buf[3]);
}

void MidiFile::writeBE16(std::ostream &out, uint16_t value) {
    char buf[2] = {
        static_cast<char>((value >> 8) & 0xFF),
        static_cast<char>(value & 0xFF)
    };
    out.write(buf, 2);
}

void MidiFile::writeBE32(std::ostream &out, uint32_t value) {
    char buf[4] = {
        static_cast<char>((value >> 24) & 0xFF),
        static_cast<char>((value >> 16) & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
        static_cast<char>(value & 0xFF)
    };
    out.write(buf, 4);
}

uint32_t MidiFile::readVarLen(std::istream &in) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t b = in.get();
        value = (value << 7) | (b & 0x7F);
        if ((b & 0x80) == 0)
            break;
    }
    return value;
}

void MidiFile::writeVarLen(std::ostream &out, uint32_t value) {
    uint8_t buf[5];
    int idx = 4;
    buf[idx] = value & 0x7F;
    while ((value >>= 7)) {
        buf[--idx] = 0x80 | (value & 0x7F);
    }
    out.write(reinterpret_cast<char*>(buf + idx), 5 - idx);
}

bool MidiFile::parseHeader(std::istream &in) {
    char hdr[4];
    read_exact(in, hdr, 4);
    if (std::memcmp(hdr, "MThd", 4) != 0) return false;
    uint32_t header_len = readBE32(in);
    if (header_len != 6) return false;
    header.format = readBE16(in);
    header.nTracks = readBE16(in);
    header.division = readBE16(in);
    return true;
}

bool MidiFile::parseTrack(std::istream &in, MidiTrack& track) {
    char hdr[4];
    read_exact(in, hdr, 4);
    if (std::memcmp(hdr, "MTrk", 4) != 0) return false;
    uint32_t trk_len = readBE32(in);

    std::streampos track_start = in.tellg();
    uint8_t running_status = 0;
    while (in.tellg() - track_start < static_cast<std::streamoff>(trk_len)) {
        MidiEvent ev;
        ev.delta_time = readVarLen(in);
        uint8_t status = in.peek();
        if (status & 0x80) {
            status = in.get();
            if (status == 0xFF) { // meta
                ev.type = MidiEventType::Meta;
                ev.meta_type = in.get();
                uint32_t len = readVarLen(in);
                ev.meta_data.resize(len);
                in.read(reinterpret_cast<char*>(ev.meta_data.data()), len);
            } else if (status == 0xF0 || status == 0xF7) { // sysex
                ev.type = MidiEventType::SysEx;
                uint32_t len = readVarLen(in);
                ev.sysex_data.resize(len);
                in.read(reinterpret_cast<char*>(ev.sysex_data.data()), len);
            } else if ((status & 0xF0) >= 0x80 && (status & 0xF0) <= 0xE0) { // channel voice
                running_status = status;
                ev.channel = running_status & 0x0F;
                switch (running_status & 0xF0) {
                    case 0x80: ev.type = MidiEventType::NoteOff; break;
                    case 0x90: ev.type = MidiEventType::NoteOn; break;
                    case 0xA0: ev.type = MidiEventType::PolyAftertouch; break;
                    case 0xB0: ev.type = MidiEventType::ControlChange; break;
                    case 0xC0: ev.type = MidiEventType::ProgramChange; break;
                    case 0xD0: ev.type = MidiEventType::ChannelAftertouch; break;
                    case 0xE0: ev.type = MidiEventType::PitchBend; break;
                    default: ev.type = MidiEventType::Unknown; break;
                }
                int datalen = 0;
                if ((running_status & 0xF0) == 0xC0 || (running_status & 0xF0) == 0xD0)
                    datalen = 1;
                else
                    datalen = 2;
                ev.data.resize(datalen);
                in.read(reinterpret_cast<char*>(ev.data.data()), datalen);
            } else {
                // Unsupported event
                ev.type = MidiEventType::Unknown;
                in.get(); // skip
            }
        } else {
            // running status
            if (running_status == 0) return false;
            ev.channel = running_status & 0x0F;
            switch (running_status & 0xF0) {
                case 0x80: ev.type = MidiEventType::NoteOff; break;
                case 0x90: ev.type = MidiEventType::NoteOn; break;
                case 0xA0: ev.type = MidiEventType::PolyAftertouch; break;
                case 0xB0: ev.type = MidiEventType::ControlChange; break;
                case 0xC0: ev.type = MidiEventType::ProgramChange; break;
                case 0xD0: ev.type = MidiEventType::ChannelAftertouch; break;
                case 0xE0: ev.type = MidiEventType::PitchBend; break;
                default: ev.type = MidiEventType::Unknown; break;
            }
            int datalen = 0;
            if ((running_status & 0xF0) == 0xC0 || (running_status & 0xF0) == 0xD0)
                datalen = 1;
            else
                datalen = 2;
            ev.data.resize(datalen);
            in.read(reinterpret_cast<char*>(ev.data.data()), datalen);
        }
        track.events.push_back(std::move(ev));
    }
    return true;
}

bool MidiFile::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return false;
    if (!parseHeader(in)) return false;
    tracks.resize(header.nTracks);
    for (int i = 0; i < header.nTracks; ++i) {
        if (!parseTrack(in, tracks[i])) return false;
    }
    return true;
}

bool MidiFile::writeHeader(std::ostream &out) const {
    out.write("MThd", 4);
    writeBE32(out, 6);
    writeBE16(out, header.format);
    writeBE16(out, static_cast<uint16_t>(tracks.size()));
    writeBE16(out, header.division);
    return true;
}

bool MidiFile::writeTrack(std::ostream &out, const MidiTrack& track) const {
    std::ostringstream track_data(std::ios::binary);
    uint8_t last_status = 0;
    for (const MidiEvent& ev : track.events) {
        writeVarLen(track_data, ev.delta_time);
        switch (ev.type) {
            case MidiEventType::Meta:
                track_data.put(0xFF);
                track_data.put(ev.meta_type);
                writeVarLen(track_data, static_cast<uint32_t>(ev.meta_data.size()));
                if (!ev.meta_data.empty())
                    track_data.write(reinterpret_cast<const char*>(ev.meta_data.data()), ev.meta_data.size());
                break;
            case MidiEventType::SysEx:
                track_data.put(0xF0);
                writeVarLen(track_data, static_cast<uint32_t>(ev.sysex_data.size()));
                if (!ev.sysex_data.empty())
                    track_data.write(reinterpret_cast<const char*>(ev.sysex_data.data()), ev.sysex_data.size());
                break;
            default:
                uint8_t status = 0;
                switch (ev.type) {
                    case MidiEventType::NoteOff: status = 0x80 | ev.channel; break;
                    case MidiEventType::NoteOn: status = 0x90 | ev.channel; break;
                    case MidiEventType::PolyAftertouch: status = 0xA0 | ev.channel; break;
                    case MidiEventType::ControlChange: status = 0xB0 | ev.channel; break;
                    case MidiEventType::ProgramChange: status = 0xC0 | ev.channel; break;
                    case MidiEventType::ChannelAftertouch: status = 0xD0 | ev.channel; break;
                    case MidiEventType::PitchBend: status = 0xE0 | ev.channel; break;
                    default: status = 0; break;
                }
                if (status != last_status || ev.type == MidiEventType::ProgramChange || ev.type == MidiEventType::ChannelAftertouch) {
                    track_data.put(status);
                    last_status = status;
                }
                if (!ev.data.empty())
                    track_data.write(reinterpret_cast<const char*>(ev.data.data()), ev.data.size());
                break;
        }
    }
    // Write actual chunk
    std::string trk = track_data.str();
    out.write("MTrk", 4);
    writeBE32(out, static_cast<uint32_t>(trk.size()));
    out.write(trk.data(), trk.size());
    return true;
}

bool MidiFile::save(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    if (!writeHeader(out)) return false;
    for (const auto& track : tracks) {
        if (!writeTrack(out, track)) return false;
    }
    return true;
}

// Example/test creator
MidiFile MidiFile::createTestFile() {
    MidiFile file;
    file.header.format = 1;
    file.header.division = 480;
    file.header.nTracks = 1;
    MidiTrack trk;
    // Simple C major scale
    for (int i = 0; i < 8; ++i) {
        MidiEvent ev_on;
        ev_on.delta_time = (i == 0 ? 0 : 480);
        ev_on.type = MidiEventType::NoteOn;
        ev_on.channel = 0;
        ev_on.data = { static_cast<uint8_t>(60 + i), 100 };
        trk.events.push_back(ev_on);

        MidiEvent ev_off;
        ev_off.delta_time = 240;
        ev_off.type = MidiEventType::NoteOff;
        ev_off.channel = 0;
        ev_off.data = { static_cast<uint8_t>(60 + i), 0 };
        trk.events.push_back(ev_off);
    }
    // End of track
    MidiEvent eot;
    eot.delta_time = 0;
    eot.type = MidiEventType::Meta;
    eot.meta_type = 0x2F;
    eot.meta_data = {};
    trk.events.push_back(eot);
    file.tracks.push_back(trk);
    return file;
}