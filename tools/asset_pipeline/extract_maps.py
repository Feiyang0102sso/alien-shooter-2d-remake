"""Export full maps, object positions, collision heights and static object previews."""

import logging

from PIL import Image, ImageDraw

from .asset_common import OUTPUT, SOURCE, compose_parts, contact_sheet, save_image, save_json, signed_byte

TILE_SIZE = 16
LEVEL_BYTES = 819
OBJECT_START = 12
MODULE_START = 309
MODULE_COUNT = 170
OBJECT_TILE_PARTS = {
    19: [19], 23: [18], 24: [38, 39, 40, 41, 42, 43, 44],
    25: [32, 33, 34, 35, 36, 37], 26: [84, 85, 86], 27: [87, 88],
    28: [89, 90], 29: [98, 99, 100, 101], 30: [91, 92, 93, 94, 95],
    31: [93, 94, 95], 32: [96], 33: [69, 70, 71, 72, 73],
    34: [20, 21], 35: [20, 21], 36: [63, 64, 18], 37: [74, 75, 18],
    41: [25, 26, 27, 28, 29, 30, 47],
    42: [25, 26, 27, 28, 29, 30, 48], 43: [49],
}
OBJECT_NAMES = {
    0: "player", 1: "monster_1", 2: "monster_2", 3: "monster_3", 4: "monster_4", 5: "monster_5",
    6: "armed_monster_3", 7: "armed_monster_4", 8: "armed_monster_5", 9: "armed_monster_6",
    10: "machine_gun", 11: "flamethrower", 12: "rocket_launcher", 13: "blaster",
    14: "machine_gun_ammo", 15: "flamethrower_fuel", 16: "rockets", 17: "blaster_ammo",
    18: "money", 19: "dynamite", 20: "medkit", 21: "light_armor", 22: "improved_armor",
    23: "dynamite_pickup_variant", 24: "terminal", 25: "generator",
    36: "spawner_1", 37: "spawner_2", 38: "placement_indicator", 39: "tile_137_marker",
    40: "flame", 41: "door_0", 42: "door_1", 43: "door_2",
    100: "dynamic_terrain_marker_100", 101: "dynamic_terrain_marker_101",
}


def object_visual(object_type, tiles, sprites, frames):
    """Compose the static state from e.a(Graphics,int[]) without timing or random decorations."""
    if object_type < 10:
        frame_id = 0
        direction = "right"
        if object_type > 0:
            enemy_type = min(object_type, 5)
            frame_id = 26 + enemy_type * 10
            direction = "left"
        base = frames[frame_id][direction]
        if object_type >= 5:
            variant = 1
            if object_type >= 6:
                variant = object_type - 3
            extra = sprites[148 + variant]
            extra_image = extra["images"][direction]
            if extra_image is not None:
                parts = [(base["image"], -base["anchor"][0], -base["anchor"][1])]
                parts.append((extra_image, -extra_image.width - extra["record"]["offset_x"], -extra["record"]["baseline_offset_y"]))
                result, anchor = compose_parts(parts)
                return {"image": result, "anchor": anchor}
        return base
    parts = []
    ids = OBJECT_TILE_PARTS.get(object_type, [])
    if 10 <= object_type <= 13:
        ids = [object_type]
    for tile_id in ids:
        tile = tiles[tile_id]
        if tile["image"] is not None:
            parts.append((tile["image"], tile["record"]["offset_x"], tile["record"]["offset_y"]))
    if object_type in (27, 28):
        parts.append((tiles[97]["image"], 18, -14))
    if object_type == 31:
        tile = tiles[91]
        parts.append((tile["image"], tile["record"]["offset_x"], tile["record"]["offset_y"] + 16))
    sprite_id = None
    sprite_x = 0
    sprite_y = 0
    if 14 <= object_type <= 17:
        sprite_id = 97 + object_type - 14
    elif object_type == 18:
        sprite_id = 96
    elif object_type == 20:
        sprite_id = 91
    elif object_type in (21, 22):
        sprite_id = 33 + object_type - 21
        sprite_x = 10
        sprite_y = 10
    elif object_type == 40:
        sprite_id = 89
    if sprite_id is not None:
        sprite = sprites[sprite_id]
        parts.append((sprite["images"]["right"], sprite_x + sprite["record"]["offset_x"], sprite_y - sprite["record"]["baseline_offset_y"]))
    shadow_x = 0
    shadow_width = 0
    if 10 <= object_type <= 13:
        shadow_x = 5
        shadow_width = 15
    elif 14 <= object_type <= 17:
        shadow_x = 1
        shadow_width = 10
    elif object_type == 18:
        shadow_x = 2
        shadow_width = 8
    elif object_type == 20:
        shadow_width = 8
    elif object_type in (21, 22):
        shadow_x = 2
        shadow_width = 10
    if shadow_width:
        shadow = Image.new("RGBA", (shadow_width, 4))
        ImageDraw.Draw(shadow).ellipse((0, 0, shadow_width - 1, 3), fill=(48, 48, 48, 255))
        parts.append((shadow, shadow_x, -11))
    # These procedural drawings are part of the assets; use state zero explicitly.
    if object_type == 25:
        lights = Image.new("RGBA", (6, 5))
        draw = ImageDraw.Draw(lights)
        for y in (0, 2, 4):
            draw.line((0, y, 5, y), fill=(128, 0, 0, 255))
        parts.append((lights, 12, -23))
    elif object_type == 38:
        marker = Image.new("RGBA", (36, 6))
        draw = ImageDraw.Draw(marker)
        for index in range(3):
            draw.line((index, index, index, 5 - index), fill="red")
            draw.line((35 - index, index, 35 - index, 5 - index), fill="red")
        parts.append((marker, -2, -11))
    image, anchor = compose_parts(parts)
    return {"image": image, "anchor": anchor}


def extract_objects(tiles, sprites, frames):
    visuals = {}
    records = []
    entries = []
    for object_type in range(44):
        visual = object_visual(object_type, tiles, sprites, frames)
        visuals[object_type] = visual
        relative = f"objects/{object_type:03d}.png"
        save_image(OUTPUT / relative, visual["image"])
        record = {"type": object_type, "name": OBJECT_NAMES.get(object_type, f"object_{object_type}"), "path": relative, "anchor": visual["anchor"], "state": "static_reference", "source": "e.java:3492-3766"}
        records.append(record)
        entries.append((f"{object_type:03d}", visual["image"]))
    save_json(OUTPUT / "data" / "objects.json", records)
    contact_sheet(entries, OUTPUT / "previews" / "objects.png", columns=8)
    return visuals


def blank_grid(width, height):
    rows = []
    for y in range(height):
        rows.append([0] * width)
    return rows


def render_tiles(grid, tiles):
    """Convert the bottom-up internal map to an image with a top-left origin."""
    height = len(grid)
    width = len(grid[0])
    result = Image.new("RGBA", (width * TILE_SIZE, height * TILE_SIZE), "black")
    for row in range(height):
        screen_y = (height - row - 1) * TILE_SIZE
        for column in range(width):
            tile_id = grid[row][column]
            image = tiles[tile_id]["image"]
            if image is not None:
                result.alpha_composite(image, (column * TILE_SIZE, screen_y))
    return result


def patch_wall(grid, x, y):
    """Reproduce neighboring wall patches used by exit type 43 in e.f(int,int)."""
    height = len(grid)
    width = len(grid[0])
    if not 1 <= x <= width - 2 or not 1 <= y <= height - 2:
        return
    value = grid[y][x]
    if value == 105 or 112 <= value <= 116 or 120 <= value <= 124:
        grid[y][x] = 113
    for column, row, replacement in ((x - 1, y, 116), (x + 1, y, 115), (x, y + 1, 112), (x, y - 1, 114)):
        if grid[row][column] == 105:
            grid[row][column] = replacement


def parse_objects(data, width, height, definitions):
    records = []
    cursor = OBJECT_START
    while cursor < MODULE_START:
        x, y, object_type = data[cursor:cursor + 3]
        record = {"record_id": (cursor - OBJECT_START) // 3 + 1, "offset": cursor, "raw": [x, y, object_type], "type": object_type}
        cursor += 3
        if object_type == 0:
            record["kind"] = "empty"
        else:
            record["kind"] = "object"
            record["name"] = OBJECT_NAMES.get(object_type, f"object_{object_type}")
            record["tile_position_from_top"] = [x, y]
            record["world_position_y_up"] = [x * 16, height * 16 - y * 16 - 15]
            screen_y = y * 16 + 15
            screen_x = x * 16
            if object_type < 10:
                screen_y -= 8
            elif object_type < len(definitions) and definitions[object_type]:
                screen_x -= definitions[object_type][4]
            record["preview_anchor"] = [screen_x, screen_y]
            if object_type in (36, 37):
                parameters = list(data[cursor:cursor + 3])
                record["spawn_parameters_raw"] = parameters
                record["spawn_count"] = parameters[0]
                record["spawn_types"] = [parameters[1] >> 4, parameters[1] & 15, parameters[2] >> 4, parameters[2] & 15]
                cursor += 3
        records.append(record)
    if cursor != MODULE_START:
        raise ValueError("Object records overlap module records")
    return records


def export_modules(all_tiles):
    data = (SOURCE / "obg.png").read_bytes()
    records = []
    for module_id in range(84):
        values = list(data[module_id * 16:module_id * 16 + 16])
        records.append({"id_one_based": module_id + 1, "tiles_top_to_bottom": values})
    save_json(OUTPUT / "maps" / "modules.json", records)
    for theme, tiles in enumerate(all_tiles):
        entries = []
        for record in records:
            result = Image.new("RGBA", (64, 64), "black")
            for index, tile_id in enumerate(record["tiles_top_to_bottom"]):
                image = tiles[tile_id]["image"]
                if image is not None:
                    result.alpha_composite(image, ((index % 4) * 16, (index // 4) * 16))
            save_image(OUTPUT / "maps" / "modules" / f"theme_{theme}" / f"{record['id_one_based']:03d}.png", result)
            entries.append((f"{record['id_one_based']:03d}", result))
        contact_sheet(entries, OUTPUT / "previews" / f"map_modules_theme_{theme}.png", columns=7, scale=2)
    return records


def extract_maps(all_tiles, sprites, frames, definitions):
    export_modules(all_tiles)
    visuals = extract_objects(all_tiles[0], sprites, frames)
    map_data = (SOURCE / "ll.png").read_bytes()
    module_data = (SOURCE / "obg.png").read_bytes()
    summaries = []
    for level in range(len(map_data) // LEVEL_BYTES):
        data = map_data[level * LEVEL_BYTES:(level + 1) * LEVEL_BYTES]
        width = data[0] + 1
        height = data[1] + 1
        theme = data[2]
        grid = blank_grid(width, height)
        collision = blank_grid(width, height)
        placements = []
        for index in range(MODULE_COUNT):
            offset = MODULE_START + index * 3
            x, raw_y, module_id = data[offset:offset + 3]
            y = signed_byte(raw_y)
            placements.append({"id": index, "x": x, "y": y, "module_id": module_id})
            if module_id == 0:
                continue
            if not 1 <= module_id <= 84:
                raise ValueError(f"Invalid module ID: {module_id}")
            for module_row in range(4):
                row = height - y - 1 - module_row
                for module_column in range(4):
                    column = x + module_column
                    if not 0 <= row < height or not 0 <= column < width:
                        continue
                    tile_id = module_data[(module_id - 1) * 16 + module_row * 4 + module_column]
                    if grid[row][column] == 0 and tile_id != 0:
                        grid[row][column] = tile_id
                        if 145 <= tile_id < 157 and tile_id != 152:
                            collision[row][column] = 8
                        elif 152 <= tile_id < 170:
                            collision[row][column] = 16
        tiles = all_tiles[theme]
        background = render_tiles(grid, tiles)
        level_dir = OUTPUT / "maps" / f"level_{level + 1:02d}"
        save_image(level_dir / "background.png", background)
        objects = parse_objects(data, width, height, definitions)
        patched_grid = []
        object_collision = []
        for row in grid:
            patched_grid.append(row.copy())
        for row in collision:
            object_collision.append(row.copy())
        for record in objects:
            if record["kind"] == "empty":
                continue
            x, source_y = record["tile_position_from_top"]
            y = height - source_y - 1
            object_type = record["type"]
            if object_type in (26, 27, 28, 30):
                object_collision[y + 1][x] = 16
                object_collision[y + 2][x] = 4
            elif object_type in (29, 31):
                object_collision[y + 1][x] = 12
                if object_type == 29:
                    object_collision[y + 1][x + 1] = 12
            elif object_type == 39:
                patched_grid[y][x] = 137
            elif object_type == 42:
                patched_grid[y + 2][x + 2] = 121
            elif object_type == 43:
                patch_wall(patched_grid, x + 2, y + 1)
        scene = render_tiles(patched_grid, tiles)
        object_layer = Image.new("RGBA", scene.size)
        markers = scene.copy()
        draw = ImageDraw.Draw(markers)
        for record in objects:
            object_type = record["type"]
            if record["kind"] == "empty":
                continue
            x, y = record["preview_anchor"]
            if object_type in visuals:
                visual = visuals[object_type]
                anchor_x, anchor_y = visual["anchor"]
                object_layer.alpha_composite(visual["image"], (x - anchor_x, y - anchor_y))
            draw.rectangle((x - 2, y - 2, x + 2, y + 2), fill="#ffcf40")
            draw.text((x + 4, y - 10), f"{record['record_id']}:{object_type}", fill="#ffcf40", stroke_width=1, stroke_fill="black")
        player_x = data[9] * 16 + definitions[0][4]
        player_y = data[10] * 16
        player = visuals[0]
        object_layer.alpha_composite(player["image"], (player_x - player["anchor"][0], player_y - player["anchor"][1]))
        scene.alpha_composite(object_layer)
        save_image(level_dir / "objects.png", object_layer)
        save_image(level_dir / "scene.png", scene)
        save_image(level_dir / "object_markers.png", markers)
        collision_image = Image.new("RGBA", scene.size)
        collision_draw = ImageDraw.Draw(collision_image)
        for row in range(height):
            for column in range(width):
                surface = object_collision[row][column]
                if surface > 0:
                    bottom = (height - row) * 16
                    collision_draw.rectangle((column * 16, bottom - surface, column * 16 + 15, bottom - 1), fill=(0, 220, 160, 180))
        save_image(level_dir / "collision.png", collision_image)
        metadata = {
            "level": level + 1, "theme": theme, "tile_size": 16,
            "width_tiles": width, "height_tiles": height, "raw_header": list(data[:12]),
            "player_spawn": {"screen_anchor": [player_x, player_y], "raw": list(data[9:12])},
            "grid_origin": "bottom_left", "preview_origin": "top_left", "tiles": grid,
            "tiles_after_static_object_patches": patched_grid, "base_collision_heights": collision,
            "collision_after_static_object_patches": object_collision, "placements": placements, "objects": objects,
            "scene_scope": "full_extent_static_reference",
            "runtime_differences": ["Random clutter, random crate-stack offsets and pickup bobbing are not frozen into this static preview.", "Player spawn is shown before initial gravity settling; enemy states use first animation frame.", "Door/spawner timers and dynamic terrain markers remain in the object records for the game runtime."],
        }
        save_json(level_dir / "level.json", metadata)
        summaries.append({"level": level + 1, "theme": theme, "size": list(scene.size), "path": f"maps/level_{level + 1:02d}/scene.png"})
        logging.info("Exported level %d: %dx%d px", level + 1, width * 16, height * 16)
    save_json(OUTPUT / "maps" / "index.json", summaries)
    return summaries
