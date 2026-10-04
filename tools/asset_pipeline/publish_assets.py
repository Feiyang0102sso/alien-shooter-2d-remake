"""Pack extracted images and data into output/runtime."""

import json
import logging
import shutil
from pathlib import Path

from PIL import Image

from .asset_common import ASSETS, OUTPUT, save_image, save_json

ATLAS_WIDTH = 1024
ATLAS_PADDING = 1
INTERFACE_NAMES = {
    "images/or.png": "menu_background",
    "images/or_f.png": "menu_overlay",
    "images/lo.png": "text_background",
    "images/ldr.png": "loading_indicator",
    "images/la.png": "intro_logo",
    "images/lz.png": "publisher_logo",
    "images/ico.png": "game_icon",
    "images/icons/ico.png": "application_icon",
}


def load_output(relative):
    return json.loads((OUTPUT / relative).read_text(encoding="utf-8"))


def copy_asset(destination, source, target):
    path = destination / target
    path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(OUTPUT / source, path)


def build_atlas(destination, name, sources):
    """Pack in stable order and deduplicate images with identical dimensions and pixels."""
    placements = {}
    images = []
    known_pixels = {}
    x = ATLAS_PADDING
    y = ATLAS_PADDING
    row_height = 0
    texture = f"textures/{name}.png"
    for source in sorted(set(sources)):
        with Image.open(OUTPUT / source) as source_image:
            image = source_image.convert("RGBA")
        pixel_key = (image.size, image.tobytes())
        if pixel_key in known_pixels:
            rectangle = known_pixels[pixel_key]
        else:
            if image.width + 2 * ATLAS_PADDING > ATLAS_WIDTH:
                raise ValueError(f"Image too wide for atlas: {source}")
            if x + image.width + ATLAS_PADDING > ATLAS_WIDTH:
                x = ATLAS_PADDING
                y += row_height + ATLAS_PADDING
                row_height = 0
            rectangle = [x, y, image.width, image.height]
            known_pixels[pixel_key] = rectangle
            images.append((image, x, y))
            x += image.width + ATLAS_PADDING
            row_height = max(row_height, image.height)
        placements[source] = {"texture": texture, "region": rectangle}
    atlas = Image.new("RGBA", (ATLAS_WIDTH, y + row_height + ATLAS_PADDING))
    for image, left, top in images:
        # Copy RGBA directly, preserving RGB values even in transparent pixels.
        atlas.paste(image, (left, top))
    save_image(destination / texture, atlas)
    return placements


def publish_graphics(destination):
    sprites = load_output("data/sprites.json")
    tiles = load_output("data/tiles.json")
    icons = load_output("text/icons.json")
    sprite_sources = []
    tile_sources = []
    interface_sources = list(INTERFACE_NAMES)
    for sprite in sprites:
        sprite_sources.extend(sprite["paths"].values())
    for tile in tiles:
        if tile["kind"] == "image":
            tile_sources.append(tile["path"])
    for icon in icons:
        interface_sources.append(icon["path"])
    character_atlas = build_atlas(destination, "characters", sprite_sources)
    world_atlas = build_atlas(destination, "world", tile_sources)
    interface_atlas = build_atlas(destination, "interface", interface_sources)

    parts = []
    for sprite in sprites:
        directions = {}
        for direction, path in sprite["paths"].items():
            image = character_atlas[path].copy()
            offset_x = sprite["offset_x"]
            if direction == "left":
                offset_x = -image["region"][2] - offset_x
            image["offset"] = [offset_x, -sprite["baseline_offset_y"]]
            directions[direction] = image
        parts.append({"id": sprite["id"], "directions": directions})
    save_json(destination / "definitions" / "character_parts.json", {"parts": parts})

    themes = []
    for theme_id in range(3):
        records = []
        for tile in tiles:
            if tile["theme"] != theme_id or tile["id"] >= 167:
                continue
            record = {"id": tile["id"], "offset": [tile["offset_x"], tile["offset_y"]]}
            if tile["kind"] == "image":
                record["image"] = world_atlas[tile["path"]]
            elif tile["kind"] == "composite":
                record["children"] = tile["children"]
            elif tile["id"] in (78, 79, 80):
                # The game reuses three sprite parts as runtime debris tiles.
                aliases = {78: 65, 79: 64, 80: 61}
                sprite = sprites[aliases[tile["id"]]]
                record["image"] = character_atlas[sprite["paths"]["right"]]
                record["offset"] = [sprite["offset_x"], sprite["baseline_offset_y"]]
            else:
                record["image"] = None
            records.append(record)
        themes.append({"id": theme_id, "tiles": records})
    save_json(destination / "definitions" / "world_tiles.json", {"tile_size": 16, "themes": themes})
    interface_images = {}
    for path, name in INTERFACE_NAMES.items():
        interface_images[name] = interface_atlas[path]
    help_icons = []
    for icon in icons:
        help_icons.append({"id": icon["id"], "image": interface_atlas[icon["path"]]})
    save_json(destination / "definitions" / "interface.json", {"images": interface_images, "help_icons": help_icons})

    font = load_output("font/font.json")
    glyphs = []
    for glyph in font["glyphs"]:
        glyphs.append({"id": glyph["id"], "character": glyph["character"], "region": glyph["rectangle"], "offset_x": glyph["offset_x"], "baseline_offset_y": glyph["baseline_offset_y"], "advance": glyph["advance"]})
    copy_asset(destination, "images/fnt.png", "fonts/pixel_green.png")
    save_json(destination / "fonts" / "pixel_green.json", {"texture": "fonts/pixel_green.png", "lowercase_maps_to_uppercase": True, "glyphs": glyphs})


def publish_definitions(destination):
    frames = []
    for frame in load_output("data/frames.json"):
        # Keep all eight slots because armor insertion and crouching depend on slot indices.
        frames.append({"id": frame["id"], "part_slots": frame["components"]})
    save_json(destination / "definitions" / "character_frames.json", {"frames": frames})
    animations = []
    for animation in load_output("data/animations.json"):
        animations.append({"name": animation["name"], "frames": animation["frame_ids"]})
    save_json(destination / "definitions" / "character_animations.json", {"nominal_tick_ms": 80, "animations": animations})
    weapons = []
    for variant in load_output("data/weapon_frame_variants.json"):
        if variant["direction"] == "right":
            weapons.append({"weapon_id": variant["weapon"], "frame_id": variant["base_frame"], "part_slots": variant["components"]})
    save_json(destination / "definitions" / "player_poses.json", {
        "weapon_frames": weapons,
        "crouch": {"frame_ids": [0, 1, 2, 15, 16, 17, 18], "slot": 0, "standing_part": 31, "crouching_part": 35, "height_delta": -8},
        "armor": {"frame_range": [0, 34], "find_part": 5, "search_slots": [1, 2, 3], "replace_next_slot": True, "armor_parts": {"1": 33, "2": 34}},
    })
    # Preserve parameter order where column semantics have not been verified.
    for source, name in (("d0", "entity_parameters"), ("d1", "projectile_parameters"), ("dm", "menu_layouts")):
        table = load_output(f"data/{source}.json")
        records = []
        for index, row in enumerate(table["rows"]):
            record = {"id": index, "parameters": row}
            alias = table["aliases"].get(str(index))
            if alias is not None:
                record["shared_parameters_with"] = alias
            records.append(record)
        save_json(destination / "definitions" / f"{name}.json", {"records": records})
    recipe = load_output("effects/recipes.json")
    generator = recipe["generator"].copy()
    generator.pop("source")
    save_json(destination / "definitions" / "visual_effects.json", {
        "generator": generator, "shadows": recipe["shadows"],
        "screen_shake": {"axis": "y", "damping_per_tick": 1, "alternate_sign": True},
    })


def publish_levels(destination):
    paths = []
    for level_id in range(1, 9):
        level = load_output(f"maps/level_{level_id:02d}/level.json")
        entities = []
        for obj in level["objects"]:
            if obj["kind"] == "empty":
                continue
            entity = {"id": obj["record_id"], "type": obj["type"], "spawn_cell": obj["tile_position_from_top"]}
            if "spawn_count" in obj:
                entity["spawn_count"] = obj["spawn_count"]
                entity["spawn_types"] = obj["spawn_types"]
            entities.append(entity)
        path = f"levels/level_{level_id:02d}.json"
        save_json(destination / path, {
            "id": level_id, "theme": level["theme"], "tile_size": 16,
            "width": level["width_tiles"], "height": level["height_tiles"],
            "tile_grid_origin": "bottom_left", "spawn_cell_origin": "top_left",
            "tiles": level["tiles"], "base_collision_heights": level["base_collision_heights"],
            "player_spawn_cell": level["player_spawn"]["raw"][:2], "entities": entities,
        })
        paths.append(path)
    return paths


def publish_text_audio_config(destination):
    copy_asset(destination, "text/en/strings.json", "localization/en/interface.json")
    for source, name in (("t0", "about"), ("t1", "help")):
        copy_asset(destination, f"text/en/{source}.txt", f"localization/en/{name}.txt")
    sounds = []
    for item in load_output("audio/index.json"):
        stem = Path(item["file"]).stem
        if stem == "mm":
            sound_id = 16
            filename = "main_menu.mid"
        else:
            sound_id = int(stem)
            filename = f"sound_{sound_id:02d}.mid"
        target = f"audio/midi/{filename}"
        copy_asset(destination, "audio/midi/" + item["file"], target)
        ogg_filename = Path(filename).with_suffix(".ogg").name
        ogg_target = f"audio/ogg/{ogg_filename}"
        copy_asset(destination, "audio/rendered/ogg/" + ogg_filename, ogg_target)
        sounds.append({"id": sound_id, "file": ogg_target, "midi_source": target})
    save_json(destination / "audio" / "sounds.json", {"sounds": sounds, "max_active_sounds": 1, "interruption": "pause_and_resume", "loop_count": 1})
    for name in ("GeneralUser-GS.sf2", "LICENSE.txt"):
        copy_asset(destination, "audio/synth/" + name, "audio/synth/" + name)
    display = load_output("display.json")
    display.pop("note")
    save_json(destination / "config" / "display.json", display)


def build_runtime_assets(destination):
    publish_graphics(destination)
    publish_definitions(destination)
    levels = publish_levels(destination)
    publish_text_audio_config(destination)
    definitions = {}
    for path in sorted((destination / "definitions").glob("*.json")):
        definitions[path.stem] = path.relative_to(destination).as_posix()
    save_json(destination / "catalog.json", {
        "format_version": 2, "display": "config/display.json", "font": "fonts/pixel_green.json",
        "definitions": definitions, "levels": levels, "audio": "audio/sounds.json",
        "language": {"code": "en", "interface": "localization/en/interface.json", "about": "localization/en/about.txt", "help": "localization/en/help.txt", "page_format": "rich-text-v1"},
    })


def publish_assets():
    """Write runtime assets directly into the single output folder."""
    ASSETS.mkdir(parents=True, exist_ok=True)
    build_runtime_assets(ASSETS)
    logging.info("Runtime assets ready: %s", ASSETS)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    publish_assets()
