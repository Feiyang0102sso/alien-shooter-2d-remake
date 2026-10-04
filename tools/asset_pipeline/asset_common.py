"""Shared paths, serialization and preview helpers for asset extraction."""

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUTPUT = Path(__file__).resolve().parents[1] / "output"
# Internal stages consume the package snapshot and write only inside the selected output.
SOURCE = OUTPUT / "source"
ASSETS = OUTPUT / "runtime"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PREVIEW_SCALE = 3


def signed_byte(value):
    """Convert an unsigned byte to the signed Java byte range."""
    if value >= 128:
        return value - 256
    return value


def save_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def save_image(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def xor_decode(data, initial_key):
    """Increment the XOR key for each byte and wrap at 256."""
    decoded = bytearray()
    for index, value in enumerate(data):
        decoded.append(value ^ ((initial_key + index) & 255))
    return bytes(decoded)


def contact_sheet(entries, path, columns=8, scale=3):
    """Render indexed contact sheets in input order, including empty entries."""
    width = 36
    height = 36
    for label, image in entries:
        if image is not None:
            width = max(width, image.width)
            height = max(height, image.height)
    cell_width = max(108, width * scale + 12)
    cell_height = height * scale + 32
    rows = (len(entries) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_width, rows * cell_height), "#182026")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=12)
    for index, entry in enumerate(entries):
        label, image = entry
        left = (index % columns) * cell_width
        top = (index // columns) * cell_height
        draw.rectangle((left + 2, top + 2, left + cell_width - 3, top + cell_height - 3), outline="#41505b")
        draw.text((left + 7, top + 5), label, font=font, fill="#edf4e8")
        if image is not None:
            enlarged = image.convert("RGBA").resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)
            x = left + (cell_width - enlarged.width) // 2
            y = top + 25 + (height * scale - enlarged.height) // 2
            sheet.paste(enlarged, (x, y), enlarged)
    save_image(path, sheet)


def compose_parts(parts):
    """Compose parts around the original origin and return the image and anchor."""
    if not parts:
        return Image.new("RGBA", (1, 1)), [0, 0]
    left = 0
    top = 0
    right = 1
    bottom = 1
    for image, x, y in parts:
        left = min(left, x)
        top = min(top, y)
        right = max(right, x + image.width)
        bottom = max(bottom, y + image.height)
    result = Image.new("RGBA", (right - left, bottom - top))
    for image, x, y in parts:
        result.alpha_composite(image.convert("RGBA"), (x - left, y - top))
    return result, [-left, -top]
