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


def tile_layers(first, last, anchor="top"):
    """Return one layer per tile id in an inclusive range."""
    layers = []
    for tile_id in range(first, last + 1):
        layer = {"tile": tile_id}
        if anchor != "top":
            layer["anchor"] = anchor
        layers.append(layer)
    return layers


# Sprite composition of world objects, transcribed from the original paint routine.
# Layer: tile|part id, optional x/y offset, anchor top|baseline, frame variant|visual_frame with step, states filter.
# Procedural parts (generator arcs, switch lasers, countdown text, terminal arrow) stay in the renderer.
OBJECT_VISUALS = {
    "objects": [
        {"type": 10, "layers": [{"tile": 9, "frame": "variant"}], "shadow": {"x": 5, "y": -11, "width": 15, "anchor": "baseline"}},
        {"type": 14, "layers": [{"part": 96, "frame": "variant"}], "shadow": {"x": 1, "y": -11, "width": 10, "anchor": "baseline"}},
        {"type": 18, "layers": [{"part": 96}], "shadow": {"x": 2, "y": -11, "width": 8, "anchor": "baseline"}},
        {"type": 19, "layers": [{"tile": 19}]},
        {"type": 20, "layers": [{"part": 95, "frame": "visual_frame", "step": -1}], "shadow": {"x": 0, "y": -11, "width": 8, "anchor": "top"}},
        {"type": 21, "layers": [{"part": 32, "frame": "variant", "x": 10, "y": 10}], "shadow": {"x": 2, "y": -11, "width": 10, "anchor": "baseline"}},
        {"type": 23, "layers": [{"tile": 18}]},
        {"type": 24, "layers": tile_layers(38, 44)},
        {"type": 25, "layers": tile_layers(32, 37)},
        {"type": 26, "layers": tile_layers(84, 86)},
        {"type": 27, "layers": tile_layers(87, 88) + [{"tile": 97, "x": 18, "y": -14}]},
        {"type": 28, "layers": tile_layers(89, 90) + [{"tile": 97, "x": 18, "y": -14}]},
        {"type": 29, "layers": tile_layers(98, 101)},
        {"type": 30, "layers": tile_layers(91, 95)},
        {"type": 31, "layers": [{"tile": 91, "y": 16}] + tile_layers(93, 95)},
        {"type": 32, "layers": [{"tile": 96}]},
        {"type": 33, "layers": tile_layers(69, 73, "baseline")},
        {"type": 34, "layers": tile_layers(20, 21)},
        {"type": 35, "layers": tile_layers(20, 21)},
        {"type": 36, "layers": [{"tile": 63, "anchor": "baseline"}, {"tile": 64, "anchor": "baseline", "states": [0, 1]},
                                {"tile": 18, "states": [0]}, {"tile": 19, "anchor": "baseline", "states": [1]}]},
        {"type": 37, "layers": [{"tile": 74, "anchor": "baseline"}, {"tile": 75, "anchor": "baseline"},
                                {"tile": 18, "states": [0]}, {"tile": 19, "anchor": "baseline", "states": [1]}]},
        {"type": 40, "layers": [{"part": 89, "frame": "visual_frame"}]},
        {"type": 41, "layers": [{"tile": 47, "frame": "variant", "x": -6}]},
    ],
    # Terminal doors slide inside a clip rectangle relative to the object's top-left drawing point.
    "terminal": {"door_tiles": [25, 26, 27, 28, 29, 30], "door_x": -6, "doorless_variant": 2,
                 "group_tile_base": 80, "group_x": 2, "clip": [-6, -48, 32, 48]},
    # Projectile part = part + life * life_step + variant * variant_step, indexed by projectile type.
    # facing: velocity mirrors by horizontal speed, right never mirrors, impact mirrors against the original direction.
    "projectiles": [
        {"type": 0, "part": 53, "facing": "velocity"},
        {"type": 1, "part": 53, "facing": "velocity"},
        {"type": 2, "part": 77, "life_step": -1, "facing": "right"},
        {"type": 3, "part": 54, "life_step": 1, "facing": "velocity"},
        {"type": 4, "part": 56, "facing": "velocity"},
        {"type": 5, "part": 77, "life_step": -1, "facing": "right"},
        {"type": 6, "part": 60, "life_step": -1, "facing": "impact"},
        {"type": 7, "part": 68, "life_step": -1, "variant_step": 3, "facing": "right"},
        {"type": 8, "part": 85, "life_step": -1, "variant_step": 3, "facing": "right"},
        {"type": 9, "part": 82, "life_step": -1, "facing": "right"},
        {"type": 10, "part": 53, "facing": "velocity"},
    ],
    "hud": {"health_panel_tile": 9, "weapon_icon_tile_base": 9, "explosive_tile": 18, "score_part": 96, "ammo_part_base": 96},
    # Monster type 5 carries a weapon part selected by its projectile type.
    "actors": {"weapon_actor_type": 5, "weapon_part_base": 148},
}

# Runtime names for cells of the original string table (row, column); duplicates keep the first cell.
INTERFACE_LABELS = {
    "sound_prompt_enable": (0, 0), "sound_prompt_question": (0, 1), "yes": (0, 2), "no": (0, 3),
    "level_prefix": (0, 4), "restarting": (0, 5), "please_wait": (0, 6), "terminal_exit": (0, 7),
    "select": (1, 0), "back": (1, 1), "new_game": (1, 2), "continue": (1, 3), "select_level": (1, 4),
    "settings": (1, 5), "help": (1, 6), "about": (1, 7), "exit": (1, 8), "menu": (1, 9), "difficulty": (1, 11),
    "easy": (1, 12), "normal": (1, 13), "hard": (1, 14), "sound_on": (1, 15), "sound_off": (1, 16),
    "vibration_on": (1, 17), "vibration_off": (1, 18), "restart": (2, 3),
    "terminal_prefix": (3, 0), "terminal_activated": (3, 1), "generator": (3, 2), "generator_running": (3, 3),
    "sector_exit": (3, 4), "sector_exit_open": (3, 5),
}
# Whole rows of the original string table shown as message boxes.
INTERFACE_MESSAGES = {
    "find_dynamite_place": 4, "terminal_travel": 5, "switch_weapon": 6, "ammo_picked_up": 7,
    "plant_dynamite": 8, "light_armor": 9, "heavy_armor": 10, "area_cleared": 11,
    "level_failed": 12, "level_complete": 13, "exit_unavailable": 14, "game_complete": 15,
}
# Desktop edition: keyboard and mouse wording replaces phone keypad hints.
DESKTOP_MESSAGES = {
    "terminal_travel": ["PRESS S", "TO TRAVEL", "THROUGH THE TERMINAL"],
    "switch_weapon": ["Q / E OR 1-5", "TO SWITCH", "YOUR CURRENT WEAPON"],
}
DESKTOP_HELP_CONTROLS = [
    ("LEFT (4) - Move left", "A - Move left"),
    ("RIGHT (6) - Move right", "D - Move right"),
    ("UP (2) - Jump", "W - Jump / Stand up"),
    ("DOWN (8) - Look down", "S - Crouch / Look down / Terminal"),
    ("(0) - Switch weapon", "Q / E - Switch weapon\n1-4 - Select gun\n5 - Pistol"),
    ("SOFT1 (5) - Fire", "LEFT MOUSE - Fire"),
    ("SOFT2 (#) - Open menu", "ESC - Open menu"),
]


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
        # The range keeps original stepping; frames lists only the non-empty frames inside it.
        record = {"name": animation["name"], "range": animation["source_range"], "frames": animation["frame_ids"]}
        if "fire_frames" in animation:
            record["fire_frames"] = animation["fire_frames"]
        animations.append(record)
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
    save_json(destination / "definitions" / "object_visuals.json", OBJECT_VISUALS)


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


def publish_interface_text(destination):
    """Name the original string table cells and apply desktop wording."""
    table = load_output("text/en/strings.json")
    rows = table["rows"]
    labels = {}
    for key, (row, column) in INTERFACE_LABELS.items():
        labels[key] = rows[row][column]
    messages = {}
    for key, row in INTERFACE_MESSAGES.items():
        messages[key] = rows[row]
    for key, lines in DESKTOP_MESSAGES.items():
        messages[key] = lines
    save_json(destination / "localization" / "en" / "interface.json", {"language": table["language"], "labels": labels, "messages": messages})
    copy_asset(destination, "text/en/t0.txt", "localization/en/about.txt")
    help_text = (OUTPUT / "text" / "en" / "t1.txt").read_text(encoding="utf-8")
    # Replace loaded control labels while preserving the original body text and icons.
    for original, desktop in DESKTOP_HELP_CONTROLS:
        if original not in help_text:
            raise ValueError(f"Help control line not found: {original}")
        help_text = help_text.replace(original, desktop, 1)
    (destination / "localization" / "en" / "help.txt").write_text(help_text, encoding="utf-8", newline="\n")


def publish_text_audio_config(destination):
    publish_interface_text(destination)
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
    save_json(destination / "audio" / "sounds.json", {"sounds": sounds, "soundfont": "audio/synth/GeneralUser-GS.sf2", "max_active_sounds": 1, "interruption": "pause_and_resume", "loop_count": 1})
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
    # Every texture referenced by sprite regions; the renderer loads exactly this list.
    textures = []
    for path in sorted((destination / "textures").glob("*.png")):
        textures.append(path.relative_to(destination).as_posix())
    textures.append("fonts/pixel_green.png")
    save_json(destination / "catalog.json", {
        "format_version": 3, "display": "config/display.json", "font": "fonts/pixel_green.json",
        "definitions": definitions, "levels": levels, "textures": textures, "audio": "audio/sounds.json",
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
