"""Validate PNG packages and restore resource IDs, glyphs and composite frames."""

import io
import logging
import struct
import zlib

from PIL import Image

from .asset_common import OUTPUT, SOURCE, PNG_SIGNATURE, compose_parts, contact_sheet, save_image, save_json, signed_byte, xor_decode


def split_png_stream(data):
    """Split PNG streams at actual chunk boundaries and verify each CRC."""
    cursor = 0
    segments = []
    while cursor < len(data):
        start = cursor
        if data[cursor:cursor + 8] != PNG_SIGNATURE:
            raise ValueError(f"Invalid PNG signature at {cursor}")
        cursor += 8
        while True:
            length = int.from_bytes(data[cursor:cursor + 4], "big")
            chunk_type = data[cursor + 4:cursor + 8]
            end = cursor + 12 + length
            if end > len(data):
                raise ValueError(f"PNG chunk exceeds file at {cursor}")
            expected_crc = int.from_bytes(data[end - 4:end], "big")
            actual_crc = zlib.crc32(data[cursor + 4:end - 4]) & 0xFFFFFFFF
            if expected_crc != actual_crc:
                raise ValueError(f"PNG CRC mismatch at {cursor}")
            cursor = end
            if chunk_type == b"IEND":
                break
        encoded = data[start:cursor]
        with Image.open(io.BytesIO(encoded)) as source_image:
            source_image.load()
            image = source_image.convert("RGBA")
        segments.append({"offset": start, "length": len(encoded), "bytes": encoded, "image": image})
    return segments


def extract_packages():
    catalog = {}
    inventory = []
    for path in sorted(SOURCE.rglob("*.png")):
        relative = path.relative_to(SOURCE).as_posix()
        data = path.read_bytes()
        item = {"source": relative, "size": len(data)}
        if path.stem == "lz":
            payload_length = int.from_bytes(data[:2], "big")
            if payload_length != len(data) - 3:
                raise ValueError("Invalid lz payload length")
            data = xor_decode(data[3:], data[2])
            item["decoding"] = "length_u16_be + key_u8 + incremental_xor"
        if data.startswith(PNG_SIGNATURE):
            segments = split_png_stream(data)
            item["image_count"] = len(segments)
            entries = []
            records = []
            for index, segment in enumerate(segments):
                if len(segments) == 1:
                    destination = OUTPUT / "images" / relative
                else:
                    destination = OUTPUT / "packages" / path.stem / f"{index:03d}.png"
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(segment["bytes"])
                segment["path"] = destination.relative_to(OUTPUT).as_posix()
                record = {"index": index, "offset": segment["offset"], "length": segment["length"], "path": segment["path"], "size": list(segment["image"].size)}
                records.append(record)
                entries.append((f"{path.stem}:{index:03d}", segment["image"]))
            catalog[relative] = segments
            item["images"] = records
            if len(segments) > 1:
                contact_sheet(entries, OUTPUT / "previews" / f"package_{path.stem}.png")
        else:
            item["kind"] = "binary_or_text"
            destination = OUTPUT / "binary" / path.with_suffix(".bin").name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        inventory.append(item)
    save_json(OUTPUT / "source_inventory.json", inventory)
    logging.info("PNG chunks and pixels validated for %d resource files", len(inventory))
    return catalog, inventory


def resolve_segment(catalog, filename, offset, length):
    for segment in catalog[filename]:
        if segment["offset"] == offset and segment["length"] == length:
            return segment
    raise ValueError(f"Image index does not match PNG boundary: {filename} {offset} {length}")


def extract_tiles(catalog):
    table = (SOURCE / "cc.png").read_bytes()
    index_data = (SOURCE / "hd.png").read_bytes()
    package_lengths = []
    for package in ("pck0.png", "pck1.png", "pck2.png"):
        package_lengths.append((SOURCE / package).stat().st_size)
    all_themes = []
    metadata = []
    for theme in range(3):
        tiles = {}
        entries = []
        for tile_id in range(181):
            values = list(table[tile_id * 6:tile_id * 6 + 6])
            record = {"id": tile_id, "theme": theme, "raw": values, "offset_x": signed_byte(values[4]), "offset_y": signed_byte(values[5])}
            image = None
            if 9 <= tile_id <= 166 and values[2] > 0 and values[3] > 0:
                lookup_id = tile_id
                if 160 <= tile_id <= 166:
                    lookup_id += theme * 7
                offset, length = struct.unpack_from(">HH", index_data, lookup_id * 4)
                if length == 0:
                    record["kind"] = "no_encoded_image"
                    tiles[tile_id] = {"image": None, "record": record}
                    metadata.append(record)
                    continue
                package_index = 0
                package_base = 0
                if tile_id >= 73:
                    package_index = 1
                    package_base += package_lengths[0]
                if tile_id >= 126:
                    package_index = 2
                    package_base += package_lengths[1]
                filename = f"pck{package_index}.png"
                segment = resolve_segment(catalog, filename, offset - package_base, length)
                image = segment["image"]
                record["source_image"] = segment["path"]
                record["size"] = list(image.size)
                record["kind"] = "image"
                record["path"] = f"tiles/theme_{theme}/{tile_id:03d}.png"
                save_image(OUTPUT / record["path"], image)
            elif values[2] == 0 and values[3] == 0 and values[4] and values[5]:
                record["kind"] = "composite"
                record["children"] = [values[4], values[5]]
            elif tile_id >= 167:
                record["kind"] = "theme_metadata_not_runtime_id"
            else:
                record["kind"] = "empty"
            tiles[tile_id] = {"image": image, "record": record}
            metadata.append(record)
        # Tiles 102-104 combine terrain and debris; do not offset the base image.
        for tile_id in (102, 103, 104):
            record = tiles[tile_id]["record"]
            base_id, overlay_id = record["children"]
            result = tiles[base_id]["image"].copy()
            overlay = tiles[overlay_id]
            result.alpha_composite(overlay["image"], (overlay["record"]["offset_x"], overlay["record"]["offset_y"]))
            tiles[tile_id]["image"] = result
            record["path"] = f"tiles/theme_{theme}/{tile_id:03d}.png"
            save_image(OUTPUT / record["path"], result)
        for tile_id in range(167):
            entries.append((f"{tile_id:03d}", tiles[tile_id]["image"]))
        contact_sheet(entries, OUTPUT / "previews" / f"tiles_theme_{theme}.png", columns=10)
        all_themes.append(tiles)
    save_json(OUTPUT / "data" / "tiles.json", metadata)
    return all_themes


def extract_sprites(catalog):
    table = (SOURCE / "cu.png").read_bytes()
    index_data = (SOURCE / "hd.png").read_bytes()
    sprites = {}
    metadata = []
    package_number = None
    for sprite_id in range(155):
        values = list(table[sprite_id * 6:sprite_id * 6 + 6])
        record = {"id": sprite_id, "raw": values, "offset_x": signed_byte(values[4]), "baseline_offset_y": signed_byte(values[5])}
        images = {"right": None, "left": None}
        if values[:5] == [255, 255, 255, 255, 255]:
            package_number = values[5]
            record["kind"] = "package_marker"
            record["package"] = package_number
        elif values[:3] == [255, 255, 255]:
            record["kind"] = "alias"
            record["alias_of"] = values[3]
            images = sprites[values[3]]["images"].copy()
        elif values[2] and values[3]:
            record["kind"] = "image"
            record["sources"] = {}
            for direction, prefix, index_offset in (("right", "up", 0), ("left", "zp", 155)):
                # The original skips reverse images 64-101; do not apply those indices to zp9.
                if direction == "left" and 64 <= sprite_id <= 101:
                    continue
                offset, length = struct.unpack_from(">HH", index_data, 724 + (sprite_id + index_offset) * 4)
                if length == 0:
                    continue
                filename = f"{prefix}{package_number}.png"
                segment = resolve_segment(catalog, filename, offset, length)
                images[direction] = segment["image"]
                record["sources"][direction] = segment["path"]
        else:
            record["kind"] = "empty"
        record["paths"] = {}
        for direction, image in images.items():
            if image is not None:
                relative = f"sprites/{direction}/{sprite_id:03d}.png"
                save_image(OUTPUT / relative, image)
                record["paths"][direction] = relative
        sprites[sprite_id] = {"record": record, "images": images}
        metadata.append(record)
    for direction in ("right", "left"):
        entries = []
        for sprite_id, sprite in sprites.items():
            entries.append((f"{sprite_id:03d}", sprite["images"][direction]))
        contact_sheet(entries, OUTPUT / "previews" / f"sprites_{direction}.png", columns=10)
    save_json(OUTPUT / "data" / "sprites.json", metadata)
    return sprites


def extract_frames(sprites):
    table = (SOURCE / "ca.png").read_bytes()
    frames = {}
    metadata = []
    for frame_id in range(85):
        component_ids = list(table[frame_id * 8:frame_id * 8 + 8])
        frame = {"id": frame_id, "components": component_ids, "directions": {}}
        frames[frame_id] = {}
        for direction in ("right", "left"):
            parts = []
            for sprite_id in component_ids:
                if sprite_id == 0:
                    continue
                sprite = sprites[sprite_id]
                image = sprite["images"][direction]
                if image is None:
                    raise ValueError(f"Missing frame part {frame_id}/{sprite_id}/{direction}")
                x = sprite["record"]["offset_x"]
                y = -sprite["record"]["baseline_offset_y"]
                if direction == "left":
                    x = -image.width - x
                parts.append((image, x, y))
            image, anchor = compose_parts(parts)
            path = f"frames/{direction}/{frame_id:03d}.png"
            save_image(OUTPUT / path, image)
            frame["directions"][direction] = {"path": path, "anchor": anchor, "empty": not bool(parts)}
            frames[frame_id][direction] = {"image": image, "anchor": anchor}
        metadata.append(frame)
    for direction in ("right", "left"):
        entries = []
        for frame_id in range(85):
            entries.append((f"{frame_id:03d}", frames[frame_id][direction]["image"]))
        contact_sheet(entries, OUTPUT / "previews" / f"frames_{direction}.png", columns=10)
    save_json(OUTPUT / "data" / "frames.json", metadata)
    return frames


def extract_font(catalog):
    atlas = catalog["fnt.png"][0]["image"]
    table = (SOURCE / "cf.png").read_bytes()
    entries = []
    records = []
    for glyph_id in range(100):
        fields = []
        for value in table[glyph_id * 6:glyph_id * 6 + 6]:
            fields.append(signed_byte(value))
        x, y, width, height, offset_x, offset_y = fields
        if x < 0 or y < 0 or x + width > atlas.width or y + height > atlas.height:
            raise ValueError(f"Font rectangle outside atlas: {glyph_id}")
        glyph = Image.new("RGBA", (max(1, width), max(1, height)))
        if width > 0 and height > 0:
            glyph = atlas.crop((x, y, x + width, y + height))
        path = f"font/glyphs/{glyph_id:03d}.png"
        save_image(OUTPUT / path, glyph)
        character = ""
        if glyph_id < 64:
            character = chr(glyph_id + 32)
        elif glyph_id < 96:
            character = chr(glyph_id + 976)
        else:
            character = ("↑", "↓", "←", "→")[glyph_id - 96]
        records.append({"id": glyph_id, "character": character, "raw": fields, "rectangle": [x, y, width, height], "offset_x": offset_x, "baseline_offset_y": offset_y, "advance": width + 1, "path": path})
        label = f"{glyph_id:03d}"
        if 0 <= glyph_id < 64:
            label += " " + character
        elif glyph_id >= 96:
            label += " " + ("UP", "DOWN", "LEFT", "RIGHT")[glyph_id - 96]
        entries.append((label, glyph))
    contact_sheet(entries, OUTPUT / "previews" / "font_000_099.png", columns=10, scale=5)
    save_json(OUTPUT / "font" / "font.json", {"glyphs": records, "lowercase_maps_to_uppercase": True, "reference": "d.java:362-375,434-444,666-677"})
