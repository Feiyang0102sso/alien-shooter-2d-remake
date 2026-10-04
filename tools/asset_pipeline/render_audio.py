"""Render the package MIDI files and trim their SoundFont into output."""

import json
import logging
from pathlib import Path
import shutil
import subprocess
import urllib.request

import imageio_ffmpeg
import mido

from .asset_common import OUTPUT, save_json
from .trim_soundfont import trim

CACHE = OUTPUT / ".cache/audio"
PROFILE = Path(__file__).with_name("audio_profile.json")
SAMPLE_RATE = 44100
GAIN = 0.2
VORBIS_QUALITY = 6


def download(url, destination):
    """Download a missing dependency into output."""
    if destination.is_file():
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    logging.info("Downloading %s", destination.name)
    temporary = destination.with_suffix(destination.suffix + ".part")
    urllib.request.urlretrieve(url, temporary)
    temporary.replace(destination)


def prepare_audio():
    """Prepare the fixed audio tools without requiring paths from the caller."""
    from .pipeline import unpack_archive
    profile = json.loads(PROFILE.read_text(encoding="utf-8"))
    soundfont = CACHE / "generaluser/GeneralUser-GS.sf2"
    download(profile["soundfont"]["url"], soundfont)
    download(profile["soundfont"]["license_url"], soundfont.parent / "LICENSE.txt")
    synth = CACHE / "fluidsynth/fluidsynth-v2.6.1-win10-x64-cpp11/bin/fluidsynth.exe"
    if not synth.is_file():
        archive = CACHE / "fluidsynth.zip"
        download(profile["fluidsynth"]["url"], archive)
        unpack_archive(archive, CACHE / "fluidsynth")
    return soundfont, synth, imageio_ffmpeg.get_ffmpeg_exe()


def run(command):
    """Report conversion errors at the pipeline entry point."""
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout + result.stderr


def build_listening_page(destination, records):
    """Provide a local MIDI conversion playlist."""
    sections = ['<h1>Alien Shooter 音频试听</h1><p>FluidSynth · GeneralUser GS · 44.1 kHz 双声道</p>']
    for record in records:
        name = record["name"]
        sections.append(f'<section><h2>{name}</h2><audio controls preload="none" src="ogg/{name}.ogg"></audio><p><a href="wav/{name}.wav">WAV</a> · <a href="ogg/{name}.ogg">OGG</a></p></section>')
    page = '<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>音频试听</title><style>body{max-width:850px;margin:36px auto;background:#14201c;color:#e5eee7;font:16px system-ui}a{color:#a4e2b1}section{padding:12px 20px;margin:18px 0;background:#20332a}audio{width:100%}</style><body>' + "\n".join(sections) + '</body></html>'
    (destination / "index.html").write_text(page, encoding="utf-8")


def render():
    """Render each MIDI with fresh channel state and retain its original bytes."""
    soundfont, synthesizer, ffmpeg = prepare_audio()
    trim(soundfont, OUTPUT / "audio/midi", OUTPUT / "audio/synth/GeneralUser-GS.sf2",
         OUTPUT / "audio/synth/subset.json")
    shutil.copyfile(soundfont.parent / "LICENSE.txt", OUTPUT / "audio/synth/LICENSE.txt")
    shutil.copyfile(PROFILE, OUTPUT / "audio/profile.json")
    destination = OUTPUT / "audio/rendered"
    for folder in ("wav", "ogg", "logs"):
        (destination / folder).mkdir(parents=True, exist_ok=True)
    sounds = json.loads((OUTPUT / "audio/index.json").read_text(encoding="utf-8"))
    records = []
    for sound in sounds:
        midi = OUTPUT / "audio/midi" / sound["file"]
        sound_id = 16
        name = "main_menu"
        if midi.stem != "mm":
            sound_id = int(midi.stem)
            name = f"sound_{sound_id:02d}"
        logging.info("Rendering %s", name)
        wav = destination / "wav" / f"{name}.wav"
        ogg = destination / "ogg" / f"{name}.ogg"
        command = [str(synthesizer), "-ni", "-r", str(SAMPLE_RATE), "-g", str(GAIN),
                   "-R", "0", "-C", "0", "-o", "synth.midi-bank-select=gm",
                   "-o", "synth.polyphony=256", "-o", "player.timing-source=sample",
                   "-F", str(wav), "-T", "wav", "-O", "float", str(soundfont), str(midi)]
        (destination / "logs" / f"{name}.txt").write_text(run(command), encoding="utf-8")
        run([ffmpeg, "-v", "error", "-y", "-i", str(wav), "-map_metadata", "-1",
             "-c:a", "libvorbis", "-q:a", str(VORBIS_QUALITY),
             "-ar", str(SAMPLE_RATE), "-ac", "2", str(ogg)])
        records.append({"id": sound_id, "name": name, "midi_seconds": mido.MidiFile(midi).length})
    save_json(destination / "index.json", records)
    build_listening_page(destination, records)
    logging.info("Audio ready: %s", destination)
