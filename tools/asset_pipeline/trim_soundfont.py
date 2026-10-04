"""Trim the complete SoundFont to the MIDI files extracted from the package."""

import json
import struct

import mido


TABLE_FORMATS = {
    b"phdr": "<20sHHHIII",
    b"pbag": "<HH",
    b"pmod": "<HHhHH",
    b"pgen": "<HH",
    b"inst": "<20sH",
    b"ibag": "<HH",
    b"imod": "<HHhHH",
    b"igen": "<HH",
    b"shdr": "<20sIIIIIBbHH",
}
INSTRUMENT = 41
SAMPLE_ID = 53
KEY_RANGE = 43
VELOCITY_RANGE = 44
GUARD_SAMPLES = 46


def chunks(data):
    """Read RIFF children, including the padding of odd-sized chunks."""
    offset = 0
    result = {}
    while offset < len(data):
        tag, size = struct.unpack_from("<4sI", data, offset)
        end = offset + 8 + size
        if end > len(data) or tag in result:
            raise ValueError("Invalid or repeated SoundFont chunk")
        result[tag] = data[offset + 8:end]
        offset = end + size % 2
    return result


def chunk(tag, payload):
    """Write a RIFF chunk without modifying its payload."""
    padding = b""
    if len(payload) % 2:
        padding = b"\0"
    return struct.pack("<4sI", tag, len(payload)) + payload + padding


def midi_presets(directory):
    """Include default programs and silent program changes to retain channel setup."""
    selected = {(0, 0), (128, 0)}
    sources = []
    notes = {}
    for path in sorted(directory.glob("*.mid")):
        midi = mido.MidiFile(path)
        if midi.type != 0:
            raise ValueError(f"Expected original format-0 MIDI: {path}")
        sources.append(path.name)
        programs = [0] * 16
        for event in midi.tracks[0]:
            if event.type == "sysex" or (event.type == "control_change" and event.control in (0, 32)):
                raise ValueError(f"Bank selection requires a new subset audit: {path}")
            if event.type == "program_change":
                programs[event.channel] = event.program
                bank = 0
                if event.channel == 9:
                    bank = 128
                selected.add((bank, event.program))
            if event.type == "note_on" and event.velocity > 0:
                bank = 0
                if event.channel == 9:
                    bank = 128
                identity = (bank, programs[event.channel])
                notes.setdefault(identity, set()).add((event.note, event.velocity))
    if not sources:
        raise ValueError("No MIDI files found")
    for identity in selected:
        if identity not in notes:
            # Preserve a valid preset for default channel setup and silent program changes.
            notes[identity] = {(60, 100)}
    return selected, sources, notes


def zones(tables, header_tag, bag_tag, generator_tag, modulator_tag, index):
    """Resolve a preset or instrument into ordered generator/modulator zones."""
    headers = tables[header_tag]
    pointer = 1
    if header_tag == b"phdr":
        pointer = 3
    begin = headers[index][pointer]
    end = headers[index + 1][pointer]
    result = []
    for bag_index in range(begin, end):
        generators, modulators = tables[bag_tag][bag_index]
        next_generators, next_modulators = tables[bag_tag][bag_index + 1]
        result.append((tables[generator_tag][generators:next_generators],
                       tables[modulator_tag][modulators:next_modulators]))
    return result


def references(zone_list, kind):
    """Collect dependencies while leaving all global zones and parameters intact."""
    result = set()
    for generators, modulators in zone_list:
        for generator, value in generators:
            if generator == kind:
                result.add(value)
    return result


def matching_notes(generators, notes):
    """Keep every note/velocity that can trigger this local zone."""
    key_min, key_max = 0, 127
    velocity_min, velocity_max = 0, 127
    for kind, value in generators:
        if kind == KEY_RANGE:
            key_min, key_max = value & 255, value >> 8
        if kind == VELOCITY_RANGE:
            velocity_min, velocity_max = value & 255, value >> 8
    matched = set()
    for key, velocity in notes:
        if key_min <= key <= key_max and velocity_min <= velocity <= velocity_max:
            matched.add((key, velocity))
    return matched


def keep_zones(zone_list, reference_kind, notes, dependency_notes):
    """Keep global zones and local zones reached by the fixed MIDI note events."""
    retained = []
    for generators, modulators in zone_list:
        targets = references([(generators, modulators)], reference_kind)
        if not targets:
            retained.append((generators, modulators))
            continue
        matched = matching_notes(generators, notes)
        if matched:
            retained.append((generators, modulators))
            for target in targets:
                dependency_notes.setdefault(target, set()).update(matched)
    return retained


def write_zones(zone_lists, reference_kind, mapping):
    """Rebuild indices only; preserve zone order and original synthesis parameters."""
    bags = []
    generators = []
    modulators = []
    starts = []
    for zone_list in zone_lists:
        starts.append(len(bags))
        for original_generators, original_modulators in zone_list:
            bags.append((len(generators), len(modulators)))
            for kind, value in original_generators:
                if kind == reference_kind:
                    value = mapping[value]
                generators.append((kind, value))
            modulators.extend(original_modulators)
    starts.append(len(bags))
    bags.append((len(generators), len(modulators)))
    generators.append((0, 0))
    modulators.append((0, 0, 0, 0, 0))
    return starts, bags, generators, modulators


def trim(source, midi_directory, output, report):
    """Remove unreachable zones and samples without changing retained audio parameters."""
    data = source.read_bytes()
    riff = chunks(data)[b"RIFF"]
    if riff[:4] != b"sfbk":
        raise ValueError("Expected a SoundFont bank")
    # LIST chunks repeat their tag, so walk the three top-level lists separately.
    lists = {}
    offset = 4
    while offset < len(riff):
        tag, size = struct.unpack_from("<4sI", riff, offset)
        if tag != b"LIST":
            raise ValueError("Unexpected SoundFont top-level chunk")
        name = riff[offset + 8:offset + 12]
        lists[name] = riff[offset + 12:offset + 8 + size]
        offset += 8 + size + size % 2
    sample_data = chunks(lists[b"sdta"])
    if set(sample_data) != {b"smpl"}:
        raise ValueError("This tool expects the approved 16-bit PCM bank")
    tables = {}
    for name, payload in chunks(lists[b"pdta"]).items():
        tables[name] = list(struct.iter_unpack(TABLE_FORMATS[name], payload))
    selected, sources, notes = midi_presets(midi_directory)
    preset_indices = []
    preset_zones = []
    instrument_notes = {}
    found = set()
    for index, header in enumerate(tables[b"phdr"][:-1]):
        identity = (header[2], header[1])
        if identity in selected:
            preset_indices.append(index)
            found.add(identity)
            zone_list = zones(tables, b"phdr", b"pbag", b"pgen", b"pmod", index)
            preset_zones.append(keep_zones(zone_list, INSTRUMENT, notes[identity], instrument_notes))
    if found != selected:
        raise ValueError(f"Missing MIDI presets: {selected - found}")
    instrument_indices = sorted(instrument_notes)
    instrument_map = {}
    instrument_zones = []
    sample_notes = {}
    for new_index, old_index in enumerate(instrument_indices):
        instrument_map[old_index] = new_index
        zone_list = zones(tables, b"inst", b"ibag", b"igen", b"imod", old_index)
        instrument_zones.append(keep_zones(zone_list, SAMPLE_ID, instrument_notes[old_index], sample_notes))
    sample_indices = sorted(sample_notes)
    sample_map = {}
    new_samples = []
    pcm = bytearray()
    for new_index, old_index in enumerate(sample_indices):
        sample_map[old_index] = new_index
        header = list(tables[b"shdr"][old_index])
        if header[9] != 1:
            raise ValueError("This pinned bank is expected to use mono sample records")
        start, end = header[1], header[2]
        shift = len(pcm) // 2 - start
        # Keep the entire sample plus its original interpolation guard values.
        pcm.extend(sample_data[b"smpl"][start * 2:(end + GUARD_SAMPLES) * 2])
        for field in range(1, 5):
            header[field] += shift
        new_samples.append(tuple(header))
    new_samples.append(tables[b"shdr"][-1])
    new_tables = {b"shdr": new_samples}
    starts, bags, generators, modulators = write_zones(preset_zones, INSTRUMENT, instrument_map)
    new_tables[b"phdr"] = []
    for new_index, old_index in enumerate(preset_indices):
        header = list(tables[b"phdr"][old_index])
        header[3] = starts[new_index]
        new_tables[b"phdr"].append(tuple(header))
    terminal = list(tables[b"phdr"][-1])
    terminal[3] = starts[-1]
    new_tables[b"phdr"].append(tuple(terminal))
    new_tables[b"pbag"], new_tables[b"pgen"], new_tables[b"pmod"] = bags, generators, modulators
    starts, bags, generators, modulators = write_zones(instrument_zones, SAMPLE_ID, sample_map)
    new_tables[b"inst"] = []
    for new_index, old_index in enumerate(instrument_indices):
        new_tables[b"inst"].append((tables[b"inst"][old_index][0], starts[new_index]))
    new_tables[b"inst"].append((tables[b"inst"][-1][0], starts[-1]))
    new_tables[b"ibag"], new_tables[b"igen"], new_tables[b"imod"] = bags, generators, modulators
    pdta = bytearray()
    for name, format_string in TABLE_FORMATS.items():
        payload = bytearray()
        for record in new_tables[name]:
            payload.extend(struct.pack(format_string, *record))
        pdta.extend(chunk(name, payload))
    result = chunk(b"RIFF", b"sfbk" + chunk(b"LIST", b"INFO" + lists[b"INFO"])
                   + chunk(b"LIST", b"sdta" + chunk(b"smpl", pcm))
                   + chunk(b"LIST", b"pdta" + pdta))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result)
    metadata = {
        "source_bytes": len(data), "subset_bytes": len(result),
        "presets": len(preset_indices), "instruments": len(instrument_indices), "samples": len(sample_indices),
        "policy": "Keep zones reached by MIDI note/velocity events; retain global zones, modulators and complete original samples. Silent/default presets retain middle C at velocity 100 for channel setup.",
        "bank_programs_zero_based": sorted(selected), "midi_files": sources,
    }
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"SoundFont reduced: {len(data):,} -> {len(result):,} bytes ({len(preset_indices)} presets, {len(sample_indices)} samples).")
