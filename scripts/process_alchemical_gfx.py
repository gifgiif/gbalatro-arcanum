#!/usr/bin/env python3
"""Convert the ImageGen Alchemical card sheet into GBA-safe 4bpp sprites."""

from collections import deque
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art/imagegen/alchemical_cards_imagegen_v3.png"
PALETTE_SOURCE = ROOT / "graphics/alchemical_gfx.png"
OUTPUT = ROOT / "graphics/alchemical_gfx.png"
FOCUS_OUTPUT = ROOT / "graphics/alchemical_focus_gfx.png"

COLS = 6
ROWS = 4
CELL = 32
CARD_MAX_WIDTH = 28
CARD_MAX_HEIGHT = 30


def is_chroma(pixel: tuple[int, int, int]) -> bool:
    red, green, blue = pixel
    return green > 205 and red < 90 and blue < 90


def remove_connected_chroma(image: Image.Image) -> Image.Image:
    """Remove only green connected to a cell edge, preserving green symbols."""
    image = image.convert("RGB")
    width, height = image.size
    background = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def add(x: int, y: int) -> None:
        offset = y * width + x
        if not background[offset] and is_chroma(image.getpixel((x, y))):
            background[offset] = 1
            queue.append((x, y))

    for x in range(width):
        add(x, 0)
        add(x, height - 1)
    for y in range(height):
        add(0, y)
        add(width - 1, y)
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height:
                add(nx, ny)

    rgba = image.convert("RGBA")
    alpha = Image.new("L", image.size, 255)
    alpha.putdata([0 if value else 255 for value in background])
    rgba.putalpha(alpha)
    return rgba


def nearest_index(
    pixel: tuple[int, int, int], palette: list[tuple[int, int, int]]
) -> int:
    return min(
        range(1, 16),
        key=lambda index: sum(
            (pixel[channel] - palette[index][channel]) ** 2
            for channel in range(3)
        ),
    )


def main() -> None:
    source = Image.open(SOURCE).convert("RGB")
    palette_image = Image.open(PALETTE_SOURCE).convert("P")
    raw_palette = palette_image.getpalette()
    palette = [
        tuple(raw_palette[index * 3 : index * 3 + 3]) for index in range(16)
    ]

    sheet = Image.new("P", (COLS * CELL, ROWS * CELL), 0)
    sheet.putpalette(raw_palette)
    cell_width = source.width / COLS
    cell_height = source.height / ROWS

    for item_id in range(COLS * ROWS):
        column = item_id % COLS
        row = item_id // COLS
        box = (
            round(column * cell_width),
            round(row * cell_height),
            round((column + 1) * cell_width),
            round((row + 1) * cell_height),
        )
        card = remove_connected_chroma(source.crop(box))
        bounds = card.getchannel("A").getbbox()
        if bounds is None:
            continue
        card = card.crop(bounds)
        scale = min(CARD_MAX_WIDTH / card.width, CARD_MAX_HEIGHT / card.height)
        size = (
            max(1, round(card.width * scale)),
            max(1, round(card.height * scale)),
        )
        card = card.resize(size, Image.Resampling.LANCZOS).convert("RGBA")

        indexed = Image.new("P", size, 0)
        indexed.putpalette(raw_palette)
        indexed.putdata(
            [
                nearest_index((red, green, blue), palette) if alpha >= 128 else 0
                for red, green, blue, alpha in card.getdata()
            ]
        )
        destination = (
            column * CELL + (CELL - size[0]) // 2,
            row * CELL + (CELL - size[1]) // 2,
        )
        sheet.paste(indexed, destination)

    sheet.save(OUTPUT, optimize=False)
    # Focus is the existing 10px upward movement; never add a second outline.
    sheet.save(FOCUS_OUTPUT, optimize=False)


if __name__ == "__main__":
    main()
