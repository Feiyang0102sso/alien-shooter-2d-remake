/** @file audio.h
 * @brief Schedule original MIDI playback and retain interrupted synthesizer state.
 */
#pragma once
#include "assets.h"
#include "midi.h"
#include <fluidsynth.h>
#include <raylib.h>
#include <memory>
#include <mutex>

namespace alien {
// Each track retains its synthesizer envelopes; paused tracks do not advance sample time.
class Audio {
public:
    explicit Audio(const Assets& assets, bool deviceOutput = true, const std::filesystem::path& soundfont = {});
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    void play(int sound);
    void setEnabled(bool enabled);
    void pause(bool paused);
    bool enabled() const { return enabled_; }
    void render(float* samples, unsigned int frames);
    std::uint64_t samplePosition(int sound) const;
private:
    struct Voice {
        MidiSequence sequence;
        std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)> synth{nullptr, delete_fluid_synth};
        std::uint64_t position = 0;
        std::size_t event = 0;
    };
    void dispatch(Voice& voice, const MidiEvent& event);
    // Settings outlive synthesizers because members are destroyed in reverse declaration order.
    std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)> settings_{nullptr, delete_fluid_settings};
    std::array<Voice, 17> voices_;
    AudioStream stream_{};
    mutable std::mutex mutex_;
    int active_ = -1;
    bool enabled_ = true, paused_ = false, deviceOutput_ = false;
};
}
