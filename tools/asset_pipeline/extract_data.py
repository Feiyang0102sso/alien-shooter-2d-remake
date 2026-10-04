"""Export numeric tables, English text, readable rich text and lossless MIDI events."""

import logging
import shutil
import struct

import mido

from .asset_common import OUTPUT, SOURCE, save_json, signed_byte, xor_decode
from .rich_text import decode_rich_text, encode_rich_text

ENGLISH_ROWS = [
    ["ENABLE", "SOUND?", "YES", "NO", "LEVEL ", "RESTARTING...", "PLEASE WAIT...", "EXIT"],
    ["SELECT", "BACK", "NEW GAME", "CONTINUE", "SELECT LEVEL", "SETTINGS", "HELP", "ABOUT", "EXIT", "MENU", "SELECT LEVEL", "DIFFICULTY", "EASY", "NORMAL", "HARD", "SOUND ON", "SOUND OFF", "VIBRATION ON", "VIBRATION OFF", "LEVEL 1", "LEVEL 2", "LEVEL 3", "LEVEL 4", "LEVEL 5", "LEVEL 6", "LEVEL 7", "LEVEL 8", "BACK"],
    ["", "", "CONTINUE", "RESTART", "MENU", "EXIT", "", "", "", ""],
    ["TERMINAL ", "  -ACTIVATED-", "GENERATOR:", "  -RUNNING-", "SECTOR EXIT:", "  -OPEN-"],
    ["FIND", "A PLACE", "TO PLANT", "THE DYNAMITE"],
    ["PRESS", '"DOWN" OR "8"', "TO TRAVEL", "THROUGH THE TERMINAL"],
    ['PRESS "0"', "TO SWITCH", "YOUR CURRENT", "WEAPON"],
    ["YOU PICKED UP", "AMMO", "FOR YOUR WEAPON"],
    ["PLANT", "DYNAMITE", "TO DESTROY", "THE SOURCE"],
    ["ARMOR", "REDUCES", "INCOMING", "DAMAGE"],
    ["ARMOR AND HELMET", "REDUCE", "INCOMING", "DAMAGE"],
    ["AREA CLEARED!", "PROCEED", "TO THE EXIT"],
    ["", "YOU LOST!", "START THE LEVEL", "AGAIN...", ""],
    ["", "", "LEVEL", "COMPLETE!!!", "", ""],
    ["", "AREA NOT CLEARED!", "EXIT UNAVAILABLE!", ""],
    ["", " ::CONGRATULATIONS!::", "::GAME COMPLETE!::", ""],
]

# Preserve icon references and layout controls while translating natural language.
ENGLISH_ABOUT = (
    "About\r\n{|(c) NET Lizard 2006{{::: SUPPORT :::{www.netlizard.net{info@netlizard.net"
    "{{::: PRODUCER :::{Alexey Reshetilo{{::: PROGRAMMERS :::{Igor Darkov{Steve Bely"
    "{Alexey Reshetilo{{::: DESIGN :::{Dmitry Gromov{Svetlana Reshetilo{Ksenia Rybnitskaya"
    "{{::: SOUND :::{Anton Zozulya{}\r\n"
)
ENGLISH_HELP = (
    "Help\r\n|* GAME *{{[The endless dark corridors of the military complex have become a lair of evil. "
    "Thousands of bloodthirsty monsters have overrun its offices, warehouses and laboratories."
    "{{Your mission is simple: clear the base of monsters by any means necessary. "
    "Use explosives to destroy the teleporters through which the creatures enter."
    "{{You will gain access to advanced weapons and equipment:"
    "{^  - Machine gun^! - Flamethrower^\" - Rocket launcher^# - Blaster"
    "^+ - Light armor reduces damage by 30%^, - Improved armor reduces damage by 50%"
    "^/ - Machine gun ammo^0 - Flamethrower fuel^1 - Rockets^2 - Blaster ammo"
    "^- - Medkit restores your health^% - Explosives for teleporters^$ - Explosive placement site^. - Money"
    "{{|* CONTROLS *{[LEFT (4) - Move left{RIGHT (6) - Move right{UP (2) - Jump"
    "{DOWN (8) - Look down{(0) - Switch weapon{SOFT1 (5) - Fire{SOFT2 (#) - Open menu"
    "{{|* SCORE *{[Earn points by killing monsters. The score depends on the monster type."
    "{|* * *{{|The invasion has begun!{You have one chance!{The fate of humanity is in your hands!}\r\n"
)


def extract_numeric_tables():
    """Read big-endian signed shorts in d0/d1; negative lengths reference prior rows."""
    result = {}
    for name in ("d0", "d1", "dm"):
        data = (SOURCE / f"{name}.png").read_bytes()
        row_count = data[0]
        cursor = 1
        rows = []
        aliases = {}
        for row_id in range(row_count):
            length = signed_byte(data[cursor])
            cursor += 1
            row = []
            if length < 0 and name != "dm":
                reference = data[cursor]
                cursor += 1
                aliases[str(row_id)] = reference
                row = rows[reference].copy()
            else:
                for column in range(length):
                    if name == "dm":
                        row.append(signed_byte(data[cursor]))
                        cursor += 1
                    else:
                        row.append(struct.unpack_from(">h", data, cursor)[0])
                        cursor += 2
            rows.append(row)
        if cursor != len(data):
            raise ValueError(f"Unconsumed bytes in {name}: {len(data) - cursor}")
        save_json(OUTPUT / "data" / f"{name}.json", {"rows": rows, "aliases": aliases, "consumed_bytes": cursor})
        result[name] = rows
    return result


def extract_text():
    decoded = {}
    for name in ("tt", "t0", "t1"):
        data = xor_decode((SOURCE / f"{name}.png").read_bytes(), 77)
        text = data.decode("cp1251")
        decoded[name] = text
        destination = OUTPUT / "text" / "reference" / f"{name}.txt"
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(text, encoding="utf-8")
    original_rows = []
    for line in decoded["tt"].split("\r\n"):
        if line:
            original_rows.append(line.split("="))
    if len(original_rows) != len(ENGLISH_ROWS):
        raise ValueError("English table row count mismatch")
    for row_id, original in enumerate(original_rows):
        if len(original) != len(ENGLISH_ROWS[row_id]):
            raise ValueError(f"English table column count mismatch: {row_id}")
    save_json(OUTPUT / "text" / "reference" / "strings_ru.json", original_rows)
    save_json(OUTPUT / "text" / "en" / "strings.json", {"language": "en", "rows": ENGLISH_ROWS})
    for name, english in (("t0", ENGLISH_ABOUT), ("t1", ENGLISH_HELP)):
        original_markup = encode_rich_text(decoded[name])
        english_markup = encode_rich_text(english)
        # Reconstruct every source control, including title separation and image line breaks.
        for source, markup in ((decoded[name], original_markup), (english, english_markup)):
            expected = source.partition("}")[0] + "}"
            if decode_rich_text(markup) != expected:
                raise ValueError(f"Rich text round-trip mismatch: {name}")
        original_icons = []
        english_icons = []
        for markup, icons in ((original_markup, original_icons), (english_markup, english_icons)):
            for line in markup.split("\n"):
                if line.startswith("<img:"):
                    icons.append(line.split(">", 1)[0])
        if original_icons != english_icons:
            raise ValueError(f"Translation changed icon references: {name}")
        (OUTPUT / "text" / "reference" / f"{name}_readable.txt").write_text(original_markup, encoding="utf-8", newline="\n")
        (OUTPUT / "text" / "en" / f"{name}.txt").write_text(english_markup, encoding="utf-8", newline="\n")
    logging.info("English translation exported with original row and icon references")


def extract_audio():
    """Preserve MIDI bytes and export all events without choosing a synthesizer or sound bank."""
    summaries = []
    for path in sorted(SOURCE.glob("*.mid")):
        midi = mido.MidiFile(path)
        tracks = []
        programs = []
        controllers = []
        tempos = []
        channels = set()
        for track in midi.tracks:
            events = []
            ticks = 0
            for event in track:
                ticks += event.time
                item = event.dict()
                item["absolute_tick"] = ticks
                events.append(item)
                if hasattr(event, "channel"):
                    channels.add(event.channel + 1)
                if event.type == "program_change":
                    programs.append({"channel": event.channel + 1, "program_zero_based": event.program, "tick": ticks})
                elif event.type == "control_change":
                    controllers.append({"channel": event.channel + 1, "controller": event.control, "value": event.value, "tick": ticks})
                elif event.type == "set_tempo":
                    tempos.append({"microseconds_per_beat": event.tempo, "tick": ticks})
            tracks.append(events)
        destination = OUTPUT / "audio" / "midi" / path.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
        summary = {"file": path.name, "format": midi.type, "tracks": len(midi.tracks), "ticks_per_beat": midi.ticks_per_beat, "duration_seconds": round(midi.length, 6), "channels_one_based": sorted(channels), "programs": programs, "controllers": controllers, "tempos": tempos}
        summaries.append(summary)
        save_json(OUTPUT / "audio" / "events" / f"{path.stem}.json", {"summary": summary, "tracks": tracks})
    save_json(OUTPUT / "audio" / "index.json", summaries)
    save_json(OUTPUT / "audio" / "playback.json", {"backend": "original_java_me_mmapi", "source": "c.java", "active_players": 1, "new_sound_stops_previous": True, "explicit_rewind": False, "loop_count_explicitly_set": False, "rendered_audio_status": "not_generated_reference_synthesizer_unknown", "notes": "Stopped players resume from their stopped media time; end-of-media restart semantics depend on MMAPI. Preserve MIDI until a reference synthesizer is identified."})
    logging.info("Preserved and parsed %d MIDI files", len(summaries))
    return summaries
