/** @file audio_tests.cpp
 * @brief Verify audio tests against runtime and original-game contracts.
 */
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}
// Tests use the real-time callback's sample entry point without requiring an audio device.
std::vector<float> render(alien::Audio& audio, unsigned int frames) {
    std::vector<float> result(frames * 2);
    audio.render(result.data(), frames);
    return result;
}
void requireSilence(const std::vector<float>& samples) {
    for (float sample : samples) { require(sample == 0.0f, "Paused MIDI produced audio."); }
}

void compareBanks(const alien::Assets& assets, const char* subset, const char* original) {
    // Compare the same runtime callback with independent synth states and exact float equality.
    alien::Audio actual(assets, false, subset);
    alien::Audio expected(assets, false, original);
    std::uint64_t comparedFrames = 0;
    const auto compare = [&](unsigned int frames) {
        const auto actualSamples = render(actual, frames);
        const auto expectedSamples = render(expected, frames);
        for (std::size_t index = 0; index < actualSamples.size(); ++index) {
            require(std::isfinite(actualSamples[index]), "Non-finite subset output.");
            require(actualSamples[index] == expectedSamples[index],
                "Subset PCM differs at frame " + std::to_string(comparedFrames + index / 2));
        }
        comparedFrames += frames;
    };
    for (int sound = 0; sound < 17; ++sound) {
        const auto sequence = alien::MidiSequence::load(assets.root() / assets.midiPaths[sound]);
        actual.play(sound);
        expected.play(sound);
        std::uint64_t remaining = sequence.endSample + alien::AudioSampleRate * 10;
        while (remaining > 0) {
            const auto frames = static_cast<unsigned int>(std::min<std::uint64_t>(remaining, 4096));
            compare(frames);
            remaining -= frames;
        }
        std::cout << "Subset PCM identical: sound=" << sound << '\n';
    }
    // Use irregular callback lengths, interruptions, mute, pause and replay on completed voices.
    for (int step = 0; step < 80; ++step) {
        const int sound = (step * 7) % 17;
        actual.play(sound);
        expected.play(sound);
        compare(37 + step * 193);
        actual.pause(true);
        expected.pause(true);
        compare(127);
        actual.pause(false);
        expected.pause(false);
        actual.setEnabled(false);
        expected.setEnabled(false);
        compare(311);
        actual.setEnabled(true);
        expected.setEnabled(true);
    }
    std::cout << "Subset comparison passed: " << comparedFrames << " stereo frames identical.\n";
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3 || argc == 4, "Expected asset root, SoundFont and optional original bank.");
        alien::Assets assets(argv[1]); assets.load();
        if (argc == 4) { compareBanks(assets, argv[2], argv[3]); }
        alien::Audio audio(assets, false, argv[2]);
        requireSilence(render(audio, 512));
        audio.play(16);
        render(audio, 1000);
        audio.play(3);
        render(audio, 997);
        require(audio.samplePosition(16) == 1000, "Interrupted music advanced.");
        audio.play(16);
        render(audio, 313);
        require(audio.samplePosition(16) == 1313, "Music restarted instead of resuming.");
        require(audio.samplePosition(3) == 997, "Interrupted effect advanced.");
        audio.play(16);
        render(audio, 17);
        require(audio.samplePosition(16) == 1330, "Active sound retrigger changed the timeline.");
        audio.pause(true);
        requireSilence(render(audio, 999));
        require(audio.samplePosition(16) == 1330, "Pause advanced music.");
        audio.pause(false);
        audio.setEnabled(false);
        audio.play(4);
        requireSilence(render(audio, 999));
        require(audio.samplePosition(16) == 1330, "Muted playback advanced music.");
        audio.setEnabled(true);
        render(audio, 100);
        require(audio.samplePosition(16) == 1430, "Muted play request replaced the active sound.");

        for (int sound = 0; sound < 17; ++sound) {
            const auto sequence = alien::MidiSequence::load(assets.root() / assets.midiPaths[sound]);
            audio.play(sound);
            const auto samples = render(audio, static_cast<unsigned int>(sequence.endSample + 512));
            float peak = 0.0f;
            for (float sample : samples) {
                require(std::isfinite(sample), "Non-finite MIDI output.");
                peak = std::max(peak, std::abs(sample));
            }
            require(peak > 0.0001f, "MIDI rendered silence: " + std::to_string(sound));
            audio.play(sound);
            require(audio.samplePosition(sound) == 0, "Finished MIDI did not restart.");
            render(audio, 37);
            require(audio.samplePosition(sound) == 37, "Restart lost sample accuracy.");
            std::cout << "MIDI render passed: sound=" << sound << ", peak=" << peak << '\n';
        }
        std::cout << "Audio interruption, resume, pause and restart contracts passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Audio test failure: " << error.what() << '\n';
        return 1;
    }
}
