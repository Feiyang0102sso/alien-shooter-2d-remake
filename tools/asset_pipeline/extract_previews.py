"""Export animations, procedural samples and a browsable asset gallery."""

import html
import json

from PIL import Image, ImageDraw

from .asset_common import OUTPUT, SOURCE, PREVIEW_SCALE, compose_parts, contact_sheet, save_image, save_json

ANIMATION_RANGES = {
    "player_unarmed_walk": (3, 6), "player_machine_gun_walk": (7, 10),
    "player_weapon_walk": (11, 14), "player_unarmed_attack": (15, 18),
    "player_machine_gun_attack": (19, 22), "player_flamethrower_attack": (23, 26),
    "player_rocket_attack": (27, 30), "player_blaster_attack": (31, 34),
    "monster_1_walk": (36, 40), "monster_1_attack": (41, 44),
    "monster_2_walk": (46, 50), "monster_2_attack": (51, 54),
    "monster_3_walk": (56, 60), "monster_3_attack": (61, 64),
    "monster_4_walk": (66, 70), "monster_4_attack": (71, 74),
    "monster_5_walk": (76, 80), "monster_5_attack": (81, 84),
}
EFFECT_SPRITES = {
    "bullet": [53], "flame_projectile": [55, 54], "energy_projectile": [56],
    "explosion": [75, 76, 77], "rocket": [58, 59, 60],
    "debris_a": [65, 66, 67, 68], "debris_b": [68, 69, 70, 71],
    "debris_c": [71, 72, 73, 74], "blood_a": [82, 83, 84, 85],
    "blood_b": [85, 86, 87, 88], "impact": [78, 79, 80, 81, 82],
    "floor_flame": [89, 90], "health_pickup": [91, 92, 93, 94, 95],
}


def save_animation(entries, path):
    """Render animations around a shared anchor; 80 ms is only the nominal update period."""
    left = 0
    top = 0
    right = 1
    bottom = 1
    for entry in entries:
        image = entry["image"]
        anchor_x, anchor_y = entry["anchor"]
        left = min(left, -anchor_x)
        top = min(top, -anchor_y)
        right = max(right, image.width - anchor_x)
        bottom = max(bottom, image.height - anchor_y)
    previews = []
    for entry in entries:
        canvas = Image.new("RGBA", (right - left + 12, bottom - top + 12), "#182026")
        x = -entry["anchor"][0] - left + 6
        y = -entry["anchor"][1] - top + 6
        canvas.alpha_composite(entry["image"], (x, y))
        canvas = canvas.convert("RGB").resize((canvas.width * 3, canvas.height * 3), Image.Resampling.NEAREST)
        previews.append(canvas)
    if previews:
        path.parent.mkdir(parents=True, exist_ok=True)
        previews[0].save(path, save_all=True, append_images=previews[1:], duration=80, loop=0, disposal=2)


def extract_animations(frames, sprites):
    records = []
    table = (SOURCE / "ca.png").read_bytes()
    for name, bounds in ANIMATION_RANGES.items():
        frame_ids = []
        for frame_id in range(bounds[0], bounds[1] + 1):
            if table[frame_id * 8] != 0:
                frame_ids.append(frame_id)
        for direction in ("right", "left"):
            entries = []
            for frame_id in frame_ids:
                entries.append(frames[frame_id][direction])
            save_animation(entries, OUTPUT / "animations" / f"{name}_{direction}.gif")
        records.append({"name": name, "frame_ids": frame_ids, "source_range": list(bounds), "source": "e.java:4425-4483", "preview_frame_ms": 80})
    # Export variants of standing and walking frames modified by the original m(int).
    weapon_components = {2: [29, 30], 3: [25, 0], 4: [26, 27]}
    variants = []
    for weapon, replacement in weapon_components.items():
        for frame_id in (2, 11, 12, 13, 14):
            components = list(table[frame_id * 8:frame_id * 8 + 8])
            slot = 5
            if frame_id == 2:
                slot = 4
            components[slot] = replacement[0]
            components[slot + 1] = replacement[1]
            for direction in ("right", "left"):
                parts = []
                for sprite_id in components:
                    if sprite_id == 0:
                        continue
                    sprite = sprites[sprite_id]
                    image = sprite["images"][direction]
                    x = sprite["record"]["offset_x"]
                    if direction == "left":
                        x = -image.width - x
                    parts.append((image, x, -sprite["record"]["baseline_offset_y"]))
                result, anchor = compose_parts(parts)
                path = f"frames/weapons/{weapon}/{direction}/{frame_id:03d}.png"
                save_image(OUTPUT / path, result)
                variants.append({"weapon": weapon, "base_frame": frame_id, "direction": direction, "components": components, "anchor": anchor, "path": path})
    save_json(OUTPUT / "data" / "weapon_frame_variants.json", variants)
    save_json(OUTPUT / "data" / "animations.json", records)
    for name, sprite_ids in EFFECT_SPRITES.items():
        entries = []
        for sprite_id in sprite_ids:
            sprite = sprites[sprite_id]
            image = sprite["images"]["right"]
            if image is None:
                continue
            result, anchor = compose_parts([(image, sprite["record"]["offset_x"], -sprite["record"]["baseline_offset_y"])])
            entries.append({"image": result, "anchor": anchor})
        save_animation(entries, OUTPUT / "animations" / f"effect_{name}.gif")
    save_json(OUTPUT / "data" / "effect_sprite_sequences.json", {"sequences": EFFECT_SPRITES, "note": "Preview sequences cover source sprite ranges. Runtime direction, lifetime and frame counters remain defined by e.java:3772-3808."})


def export_procedural_effects():
    """Export finite-state geometry samples and document time-dependent behavior separately."""
    entries = []
    for width in (8, 10, 15):
        shadow = Image.new("RGBA", (width, 4))
        ImageDraw.Draw(shadow).ellipse((0, 0, width - 1, 3), fill=(48, 48, 48, 255))
        path = OUTPUT / "effects" / f"shadow_{width}.png"
        save_image(path, shadow)
        entries.append((f"shadow {width}", shadow))
    for frame in range(3):
        lights = Image.new("RGBA", (6, 5))
        draw = ImageDraw.Draw(lights)
        colors = [(128, 0, 0, 255), (176, 0, 0, 255), (255, 0, 0, 255)]
        for y in (0, 2, 4):
            draw.line((0, y, 5, y), fill=colors[frame])
        save_image(OUTPUT / "effects" / f"generator_lights_{frame}.png", lights)
        entries.append((f"light {frame}", lights))
    arrow = Image.new("RGBA", (7, 4))
    draw = ImageDraw.Draw(arrow)
    for row in range(4):
        width = 7 - row * 2
        draw.line((row, row, row + width - 1, row), fill=(0, 255, 0, 255))
    save_image(OUTPUT / "effects" / "exit_arrow.png", arrow)
    entries.append(("exit arrow", arrow))
    contact_sheet(entries, OUTPUT / "previews" / "procedural_effects.png", columns=4, scale=6)
    source_lines = []
    if (SOURCE / "e.java").is_file():
        source_lines = (SOURCE / "e.java").read_text(encoding="utf-8").splitlines()
    excerpts = []
    ranges = [(3290, 3312, "screen_lines_and_shake"), (3492, 3766, "objects_and_procedural_effects"), (3772, 3808, "projectile_visual_selection"), (4425, 4483, "actor_animation_ranges")]
    for start, end, name in ranges:
        if not source_lines:
            continue
        text = "\n".join(source_lines[start - 1:end])
        path = OUTPUT / "effects" / "source_reference" / f"{name}.txt"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text + "\n", encoding="utf-8")
        excerpts.append({"name": name, "source": "e.java", "start_line": start, "end_line": end, "path": path.relative_to(OUTPUT).as_posix()})
    save_json(OUTPUT / "effects" / "recipes.json", {
        "raster_sample_accuracy": "Pillow previews; Java ME ellipse/round-rect rasterization has not been pixel-compared.",
        "shadows": {"color_rgb": [48, 48, 48], "sizes": [[8, 4], [10, 4], [15, 4]]},
        "generator": {"light_colors_rgb_hex": ["800000", "B00000", "FF0000"], "active_light_colors_rgb_hex": ["008000", "00B000", "00FF00"], "spark_colors_rgb_hex": ["00CCFF", "0080FF"], "spark_random_bits": 3, "source": "e.java:3564-3602"},
        "screen_shake": {"translation_axis": "y", "update_positive": "h = -h + 1", "update_negative": "h = -h - 1"},
        "remaining_runtime_behaviors": ["Random electrical lines", "Animated red placement markers", "Teleporter fallback rounded rectangles", "Door clipping, countdown glyphs and translated exit label", "Screen transition, shake and HUD counters"],
        "source_excerpts": excerpts,
    })


def export_help_icons(tiles, sprites, catalog):
    references = [("tile", 10), ("tile", 11), ("tile", 12), ("tile", 13), ("tile", 18), ("tile", 19), ("tile", 81), ("tile", 82), ("tile", 83), ("tile", 136), ("tile", 137), ("sprite", 33), ("sprite", 34), ("sprite", 91), ("sprite", 96), ("sprite", 97), ("sprite", 30), ("sprite", 99), ("sprite", 100), ("image", "or_f.png"), ("image", "la.png")]
    records = []
    entries = []
    for icon_id, reference in enumerate(references):
        kind, source_id = reference
        if kind == "tile":
            image = tiles[source_id]["image"]
        elif kind == "sprite":
            image = sprites[source_id]["images"]["right"]
        else:
            image = catalog[source_id][0]["image"]
        path = f"text/icons/{icon_id:02d}.png"
        save_image(OUTPUT / path, image)
        records.append({"id": icon_id, "source_kind": kind, "source_id": source_id, "path": path})
        if icon_id < 19:
            entries.append((f"{icon_id:02d}", image))
    contact_sheet(entries, OUTPUT / "previews" / "help_icons.png", columns=7)
    save_json(OUTPUT / "text" / "icons.json", records)


def render_english_menu(catalog):
    """Preview English menus with original bitmap glyphs at three times logical size."""
    font = json.loads((OUTPUT / "font" / "font.json").read_text(encoding="utf-8"))
    image = Image.new("RGBA", (240, 320), "black")
    background = catalog["or.png"][0]["image"]
    image.alpha_composite(background, ((240 - background.width) // 2, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((22, 140, 217, 292), fill=(0, 0, 0, 235))
    labels = ["ALIEN SHOOTER", "NEW GAME", "CONTINUE", "SELECT LEVEL", "SETTINGS", "HELP", "ABOUT", "EXIT"]
    y = 154
    for label in labels:
        width = 0
        for character in label:
            width += font["glyphs"][ord(character) - 32]["advance"]
        x = (240 - width) // 2
        for character in label:
            record = font["glyphs"][ord(character) - 32]
            glyph = Image.open(OUTPUT / record["path"]).convert("RGBA")
            draw_y = y - record["baseline_offset_y"] - record["rectangle"][3]
            image.alpha_composite(glyph, (x + record["offset_x"], draw_y))
            x += record["advance"]
        y += 18
    save_image(OUTPUT / "previews" / "english_menu_720x960.png", image.resize((720, 960), Image.Resampling.NEAREST))
    save_json(OUTPUT / "display.json", {"logical_width": 240, "logical_height": 320, "default_scale": PREVIEW_SCALE, "window_width": 720, "window_height": 960, "texture_filter": "nearest", "aspect_ratio": "3:4", "fit_small_display": "choose largest fitting integer scale", "note": "Menu image is a layout preview, not a running game screenshot."})


def build_gallery(map_summaries, audio_summaries):
    """Build a static local inspection page without external scripts or network dependencies."""
    sections = []
    sections.append('<h1>Alien Shooter · 素材校验</h1><p>原始编号、锚点和图层均保留。像素预览使用最近邻缩放。</p>')
    sections.append('<p><a href="audio/rendered/index.html">全部 17 个 OGG 音频试听</a></p>')
    sections.append('<p><a href="README.md">导出说明</a> · <a href="audio/index.json">MIDI 信息</a></p>')
    sections.append('<h2>720×960 显示预览</h2><p>3 倍等比例；此图用于排版验收，并非游戏运行截图。</p><img class="screen" src="previews/english_menu_720x960.png"/>')
    sections.append('<h2>8 关完整场景</h2><p>固定对象的静态参考；运行时随机装饰、计时状态和初始重力落点另见 level.json。</p>')
    for level in map_summaries:
        number = level["level"]
        directory = f"maps/level_{number:02d}"
        sections.append(f'<h3>Level {number} · {level["size"][0]}×{level["size"][1]}</h3><p><a href="{directory}/level.json">关卡数据</a> · <a href="{directory}/background.png">背景</a> · <a href="{directory}/objects.png">对象层</a> · <a href="{directory}/collision.png">碰撞高度</a> · <a href="{directory}/object_markers.png">对象编号</a></p><a href="{level["path"]}"><img src="{level["path"]}" loading="lazy"/></a>')
    sections.append('<h2>逐项校验表</h2>')
    for path in sorted((OUTPUT / "previews").glob("*.png")):
        if path.stem == "english_menu_720x960":
            continue
        relative = path.relative_to(OUTPUT).as_posix()
        sections.append(f'<details><summary>{html.escape(path.stem)}</summary><a href="{relative}"><img src="{relative}" loading="lazy"/></a></details>')
    sections.append('<h2>动画预览</h2><p>使用原版名义更新周期 80ms 展示；真实持续时间由运行时状态控制。</p><div class="animations">')
    for path in sorted((OUTPUT / "animations").glob("*.gif")):
        relative = path.relative_to(OUTPUT).as_posix()
        sections.append(f'<figure><img src="{relative}" loading="lazy"/><figcaption>{html.escape(path.stem)}</figcaption></figure>')
    sections.append('</div><h2>英文文本</h2><p><a href="text/en/strings.json">菜单与提示</a> · <a href="text/en/t0.txt">关于</a> · <a href="text/en/t1.txt">帮助</a></p>')
    sections.append('<h2>原始 MIDI</h2><p>保留原字节，尚未选定 1:1 合成器；浏览器不一定支持直接播放 MIDI。</p><ul>')
    for item in audio_summaries:
        sections.append(f'<li><a href="audio/midi/{item["file"]}">{item["file"]}</a> · {item["duration_seconds"]:.3f}s · Format {item["format"]}</li>')
    sections.append('</ul>')
    style = 'body{background:#101719;color:#dce9de;font:16px system-ui;max-width:1200px;margin:36px auto;padding:0 24px}a{color:#9bdbb1}h1,h2{color:#b6ed99}h2{margin-top:56px}img{max-width:100%;image-rendering:pixelated;background:#182026}img.screen{width:360px}details{border:1px solid #3d5045;padding:14px;margin:10px 0}summary{cursor:pointer}.animations{display:flex;gap:16px;flex-wrap:wrap}figure{margin:0;padding:14px;background:#182026;max-width:250px}figcaption{font-size:12px;margin-top:8px;overflow-wrap:anywhere}'
    page = '<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>Alien Shooter 素材校验</title><style>' + style + '</style><body>' + '\n'.join(sections) + '</body></html>'
    (OUTPUT / "index.html").write_text(page, encoding="utf-8")


def map_overview():
    """Create a compact overview of all eight maps without modifying the originals."""
    canvas = Image.new("RGB", (1200, 1100), "#182026")
    draw = ImageDraw.Draw(canvas)
    for index in range(8):
        path = OUTPUT / "maps" / f"level_{index + 1:02d}" / "scene.png"
        with Image.open(path) as source:
            image = source.convert("RGB")
        image.thumbnail((580, 235), Image.Resampling.NEAREST)
        x = (index % 2) * 600 + 10
        y = (index // 2) * 275 + 30
        canvas.paste(image, (x, y))
        draw.text((x, y - 20), f"Level {index + 1}", fill="white")
    save_image(OUTPUT / "previews" / "all_maps.png", canvas)
