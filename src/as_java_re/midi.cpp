/** @file midi.cpp
 * @brief Parse standard MIDI events into deterministic sample timestamps.
 */
#include "midi.h"
#include <fstream>
#include <stdexcept>
#include <string>

namespace alien {
namespace {
// Report parse errors at the asset boundary instead of treating damaged tracks as silence.
class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data) : bytes(data), limit(data.size()) {}
    std::uint8_t byte() {
        if (position >= limit) { throw std::runtime_error("Truncated MIDI event."); }
        return bytes[position++];
    }
    std::uint32_t bigEndian(int count) {
        std::uint32_t value = 0;
        for (int i = 0; i < count; ++i) { value = (value << 8) | byte(); }
        return value;
    }
    std::uint32_t variable() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const auto next = byte();
            value = (value << 7) | (next & 127);
            if ((next & 128) == 0) { return value; }
        }
        throw std::runtime_error("Invalid MIDI variable length quantity.");
    }
    void tag(const std::string& expected) {
        for (char character : expected) {
            if (byte() != static_cast<std::uint8_t>(character)) { throw std::runtime_error("Invalid MIDI chunk."); }
        }
    }
    void skip(std::size_t count) {
        if (count > limit - position) { throw std::runtime_error("Invalid MIDI payload length."); }
        position += count;
    }
    const std::vector<std::uint8_t>& bytes;
    std::size_t position = 0, limit;
};
}

MidiSequence MidiSequence::load(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { throw std::runtime_error("Cannot open MIDI: " + path.string()); }
    const std::vector<std::uint8_t> data(std::istreambuf_iterator<char>(stream), {});
    return parse(data);
}

MidiSequence MidiSequence::parse(const std::vector<std::uint8_t>& bytes) {
    Reader reader(bytes);
    reader.tag("MThd");
    const auto headerLength = reader.bigEndian(4);
    if (headerLength < 6) { throw std::runtime_error("Invalid MIDI header size."); }
    const auto format = reader.bigEndian(2);
    const auto tracks = reader.bigEndian(2);
    const auto division = reader.bigEndian(2);
    if (format != 0 || tracks != 1 || division == 0 || (division & 0x8000) != 0) {
        throw std::runtime_error("Expected original format-0 PPQN MIDI.");
    }
    reader.skip(headerLength - 6);
    reader.tag("MTrk");
    const auto trackLength = reader.bigEndian(4);
    if (trackLength != bytes.size() - reader.position) { throw std::runtime_error("MIDI track length mismatch."); }
    MidiSequence result;
    std::uint64_t tempo = 500000;
    std::uint64_t microsecondNumerator = 0;
    std::uint8_t runningStatus = 0;
    bool ended = false;
    while (reader.position < reader.limit) {
        microsecondNumerator += static_cast<std::uint64_t>(reader.variable()) * tempo;
        const std::uint64_t sample = microsecondNumerator * AudioSampleRate / (division * 1000000ULL);
        auto status = reader.byte();
        bool firstDataPresent = status < 128;
        std::uint8_t firstData = status;
        if (firstDataPresent) {
            if (runningStatus == 0) { throw std::runtime_error("MIDI running status without channel status."); }
            status = runningStatus;
        }
        if (status == 0xff) {
            const auto type = reader.byte();
            const auto length = reader.variable();
            if (type == 0x51) {
                if (length != 3) { throw std::runtime_error("Invalid MIDI tempo."); }
                tempo = reader.bigEndian(3);
                if (tempo == 0) { throw std::runtime_error("Zero MIDI tempo."); }
                ++result.tempoChanges;
            } else if (type == 0x2f) {
                if (length != 0 || reader.position != reader.limit) { throw std::runtime_error("Invalid MIDI end of track."); }
                result.endSample = sample;
                ended = true;
                break;
            } else {
                reader.skip(length);
            }
        } else if (status == 0xf0 || status == 0xf7) {
            // Original assets contain no SysEx; reject unsupported messages rather than silently dropping resets.
            throw std::runtime_error("SysEx is not present in the original game MIDI contract.");
        } else if (status < 0xf0 && status >= 0x80) {
            runningStatus = status;
            MidiEvent event;
            event.sample = sample; event.status = status;
            if (firstDataPresent) { event.data1 = firstData; }
            else { event.data1 = reader.byte(); }
            const int command = status & 0xf0;
            if (command != 0xc0 && command != 0xd0) { event.data2 = reader.byte(); }
            if (event.data1 >= 128 || event.data2 >= 128) { throw std::runtime_error("Invalid MIDI channel data."); }
            if (command == 0x90 && event.data2 != 0) { ++result.noteOnCount; }
            result.events.push_back(event);
        } else {
            throw std::runtime_error("Unsupported MIDI status.");
        }
    }
    if (!ended) { throw std::runtime_error("MIDI has no end of track."); }
    return result;
}
}
