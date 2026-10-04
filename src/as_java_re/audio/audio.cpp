/** @file audio.cpp
 * @brief Schedule original MIDI playback and retain interrupted synthesizer state.
 */
#include "audio/audio.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace alien {
namespace {
Audio* currentAudio = nullptr;
void audioCallback(void* buffer, unsigned int frames) {
    currentAudio->render(static_cast<float*>(buffer), frames);
}
}

Audio::Audio(const Assets& assets, bool deviceOutput, const std::filesystem::path& soundfont) {
    voices_.resize(assets.midiPaths.size());
    for (std::size_t id = 0; id < voices_.size(); ++id) {
        voices_[id].sequence = MidiSequence::load(assets.root() / assets.midiPaths[id]);
    }
    auto fontPath = soundfont;
    if (fontPath.empty()) { fontPath = assets.root() / assets.soundfontPath; }
    if (!std::filesystem::exists(fontPath)) { throw std::runtime_error("Missing runtime SoundFont."); }
    settings_.reset(new_fluid_settings());
    if (!settings_) { throw std::runtime_error("Cannot create FluidSynth settings."); }
    fluid_settings_setnum(settings_.get(), "synth.sample-rate", AudioSampleRate);
    fluid_settings_setnum(settings_.get(), "synth.gain", audio_settings::Gain);
    fluid_settings_setint(settings_.get(), "synth.reverb.active", 0);
    fluid_settings_setint(settings_.get(), "synth.chorus.active", 0);
    fluid_settings_setint(settings_.get(), "synth.polyphony", audio_settings::Polyphony);
    fluid_settings_setstr(settings_.get(), "synth.midi-bank-select", "gm");
    // Prepare all tracks at startup; gameplay and real-time callbacks never load the SoundFont.
    for (auto& voice : voices_) {
        voice.synth.reset(new_fluid_synth(settings_.get()));
        if (!voice.synth) { throw std::runtime_error("Cannot create MIDI synthesizer."); }
        if (fluid_synth_sfload(voice.synth.get(), fontPath.u8string().c_str(), 1) < 0) {
            throw std::runtime_error("Cannot load GeneralUser SoundFont.");
        }
    }
    if (!deviceOutput) { return; }
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        throw std::runtime_error("Audio device unavailable.");
    }
    SetAudioStreamBufferSizeDefault(audio_settings::StreamBufferFrames);
    stream_ = LoadAudioStream(AudioSampleRate, audio_settings::SampleBits, audio_settings::ChannelCount);
    if (!IsAudioStreamValid(stream_)) {
        CloseAudioDevice();
        throw std::runtime_error("Cannot create MIDI output stream.");
    }
    deviceOutput_ = true;
    currentAudio = this;
    SetAudioStreamCallback(stream_, audioCallback);
    PlayAudioStream(stream_);
    std::cout << "MIDI ready: sample clock, FluidSynth 2.6.1, GeneralUser GS 2.0.3.\n";
}

Audio::~Audio() {
    if (deviceOutput_) {
        StopAudioStream(stream_);
        UnloadAudioStream(stream_);
        CloseAudioDevice();
        currentAudio = nullptr;
    }
}

void Audio::play(int sound) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) { return; }
    Voice& voice = voices_.at(sound);
    if (voice.position >= voice.sequence.endSample) {
        fluid_synth_system_reset(voice.synth.get());
        voice.position = 0;
        voice.event = 0;
    }
    active_ = sound;
}

void Audio::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}
void Audio::pause(bool paused) {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = paused;
}
std::uint64_t Audio::samplePosition(int sound) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return voices_.at(sound).position;
}

void Audio::dispatch(Voice& voice, const MidiEvent& event) {
    const int channel = event.status & 15;
    switch (event.status & 0xf0) {
    case 0x80: fluid_synth_noteoff(voice.synth.get(), channel, event.data1); break;
    case 0x90:
        if (event.data2 == 0) { fluid_synth_noteoff(voice.synth.get(), channel, event.data1); }
        else { fluid_synth_noteon(voice.synth.get(), channel, event.data1, event.data2); }
        break;
    case 0xa0: fluid_synth_key_pressure(voice.synth.get(), channel, event.data1, event.data2); break;
    case 0xb0: fluid_synth_cc(voice.synth.get(), channel, event.data1, event.data2); break;
    case 0xc0: fluid_synth_program_change(voice.synth.get(), channel, event.data1); break;
    case 0xd0: fluid_synth_channel_pressure(voice.synth.get(), channel, event.data1); break;
    case 0xe0: fluid_synth_pitch_bend(voice.synth.get(), channel, event.data1 | (event.data2 << 7)); break;
    }
}

void Audio::render(float* samples, unsigned int frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int channels = audio_settings::ChannelCount;
    std::fill(samples, samples + frames * channels, 0.0f);
    if (!enabled_ || paused_ || active_ < 0) { return; }
    Voice& voice = voices_[active_];
    unsigned int written = 0;
    while (written < frames) {
        while (voice.event < voice.sequence.events.size() && voice.sequence.events[voice.event].sample <= voice.position) {
            dispatch(voice, voice.sequence.events[voice.event]);
            ++voice.event;
        }
        std::uint64_t count = frames - written;
        if (voice.event < voice.sequence.events.size()) {
            count = std::min(count, voice.sequence.events[voice.event].sample - voice.position);
        }
        // Write interleaved stereo; release tails may finish without extending MIDI EOT.
        fluid_synth_write_float(voice.synth.get(), static_cast<int>(count), samples + written * channels, 0, channels,
                               samples + written * channels, 1, channels);
        voice.position += count;
        written += static_cast<unsigned int>(count);
    }
}
}
