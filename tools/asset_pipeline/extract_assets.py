"""Preprocess the selected Alien Shooter snapshot into a self-contained output."""

import logging
import struct

from .asset_common import OUTPUT, SOURCE, save_json
from .extract_data import extract_audio, extract_numeric_tables, extract_text
from .extract_images import extract_font, extract_frames, extract_packages, extract_sprites, extract_tiles
from .extract_maps import extract_maps
from .extract_previews import build_gallery, export_help_icons, export_procedural_effects, extract_animations, map_overview, render_english_menu

from .publish_assets import publish_assets
from .render_audio import render


def export_index_table():
    data = (SOURCE / "hd.png").read_bytes()
    rows = []
    for offset in range(0, len(data), 4):
        image_offset, length = struct.unpack_from(">HH", data, offset)
        rows.append({"entry": offset // 4, "table_byte_offset": offset, "image_offset": image_offset, "image_length": length})
    save_json(OUTPUT / "data" / "image_index.json", {"rows": rows, "tiles_table_start": 0, "sprites_table_start": 724, "byte_order": "big"})


def write_readme():
    """Describe the generated folders without audit manifests."""
    text = """# 素材输出

重新生成：tools/run_pipeline.bat "游戏包路径"。输出固定在 tools/output/，再次运行会覆盖旧结果。

- runtime/：供游戏使用的图集、关卡、文本、MIDI、音色及许可。
- source/：原包资源快照。
- images/、packages/、sprites/、tiles/、frames/、maps/：拆图、组合帧和关卡数据。
- audio/：原 MIDI、事件、裁剪音色、OGG/WAV 和试听页。
- text/、data/、font/、effects/：文字、参数、字体及效果信息。
- index.html：素材浏览页；pipeline.log：本次处理日志。
- .venv/、.cache/：自动准备的 Python 环境与依赖缓存。

只处理本游戏。预览是静态素材展示，动态效果由游戏运行时实现。
"""
    (OUTPUT / "README.md").write_text(text, encoding="utf-8")


def main():
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    logging.info("Extracting assets from %s", SOURCE)
    catalog, inventory = extract_packages()
    tiles = extract_tiles(catalog)
    sprites = extract_sprites(catalog)
    frames = extract_frames(sprites)
    extract_font(catalog)
    definitions = extract_numeric_tables()
    extract_text()
    audio = extract_audio()
    export_index_table()
    maps = extract_maps(tiles, sprites, frames, definitions["d0"])
    extract_animations(frames, sprites)
    export_procedural_effects()
    export_help_icons(tiles[0], sprites, catalog)
    render_english_menu(catalog)
    map_overview()
    render()
    build_gallery(maps, audio)
    write_readme()
    publish_assets()
    logging.info("Done. Open %s", OUTPUT / "index.html")


if __name__ == "__main__":
    main()
