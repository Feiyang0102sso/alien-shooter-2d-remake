/** @file midi.h
 * @brief Parse standard MIDI events into deterministic sample timestamps.
 */
#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace alien {
inline constexpr int AudioSampleRate = 44100;

// Convert SMF time directly to sample positions and preserve same-tick file order.
struct MidiEvent {
    std::uint64_t sample = 0;
    std::uint8_t status = 0, data1 = 0, data2 = 0;
};
struct MidiSequence {
    std::vector<MidiEvent> events;
    std::uint64_t endSample = 0;
    int noteOnCount = 0;
    int tempoChanges = 0;
    static MidiSequence load(const std::filesystem::path& path);
    static MidiSequence parse(const std::vector<std::uint8_t>& bytes);
};
}
