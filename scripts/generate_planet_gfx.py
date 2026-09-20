#!/usr/bin/env python3
"""Generate compact 4bpp planet cards using the shared consumable palette."""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
PALETTE_SOURCE = ROOT / "graphics/alchemical_gfx.png"
OUTPUT = ROOT / "graphics/planet_gfx.png"
FOCUS_OUTPUT = ROOT / "graphics/planet_focus_gfx.png"

CELL = 32
COLS = 6
ROWS = 2


def draw_card(sheet: Image.Image, item_id: int) -> None:
    x0 = (item_id % COLS) * CELL
    y0 = (item_id // COLS) * CELL
    draw = ImageDraw.Draw(sheet)

    # Borderless-atlas card: only the card itself is opaque.
    draw.rounded_rectangle((x0 + 4, y0 + 1, x0 + 27, y0 + 30), 2, fill=4)
    draw.rectangle((x0 + 5, y0 + 2, x0 + 26, y0 + 29), fill=1)
    draw.rectangle((x0 + 7, y0 + 5, x0 + 24, y0 + 26), fill=13)

    # Stable star field; every card remains readable at 1x GBA resolution.
    star_points = ((9, 7), (22, 8), (10, 23), (21, 25), (8, 14))
    for index, (sx, sy) in enumerate(star_points):
        if (index + item_id) % 2 == 0:
            draw.point((x0 + sx, y0 + sy), fill=5)

    planet_colors = (14, 6, 12, 4, 9, 8, 12, 8, 12, 2, 6, 14)
    shadow_colors = (13, 7, 13, 15, 15, 15, 11, 15, 13, 14, 7, 13)
    color = planet_colors[item_id]
    shadow = shadow_colors[item_id]

    cx, cy = x0 + 16, y0 + 16
    if item_id == 4:  # Saturn
        draw.ellipse((x0 + 8, y0 + 14, x0 + 24, y0 + 19), outline=4, width=2)
    elif item_id == 8:  # Neptune
        draw.arc((x0 + 8, y0 + 8, x0 + 24, y0 + 24), 190, 350, fill=5, width=1)
    elif item_id == 9:  # Planet X
        draw.line((x0 + 10, y0 + 10, x0 + 22, y0 + 22), fill=8, width=2)
        draw.line((x0 + 22, y0 + 10, x0 + 10, y0 + 22), fill=8, width=2)

    draw.ellipse((cx - 6, cy - 6, cx + 6, cy + 6), fill=shadow)
    draw.ellipse((cx - 5, cy - 6, cx + 4, cy + 4), fill=color)
    draw.rectangle((cx - 3, cy - 4, cx, cy - 2), fill=5)

    if item_id == 5:  # Jupiter bands
        draw.line((cx - 5, cy - 1, cx + 5, cy - 1), fill=9)
        draw.line((cx - 4, cy + 2, cx + 4, cy + 2), fill=4)
    elif item_id == 6:  # Earth
        draw.rectangle((cx - 3, cy - 2, cx, cy + 1), fill=10)
        draw.rectangle((cx + 1, cy + 1, cx + 3, cy + 3), fill=10)
    elif item_id == 10:  # Ceres
        draw.ellipse((cx - 2, cy - 2, cx + 1, cy + 1), fill=3)
    elif item_id == 11:  # Eris
        draw.line((cx - 4, cy + 3, cx + 4, cy - 3), fill=5)

    if item_id == 4:
        draw.arc((x0 + 8, y0 + 14, x0 + 24, y0 + 19), 180, 360, fill=5, width=1)


def main() -> None:
    palette_image = Image.open(PALETTE_SOURCE).convert("P")
    palette = palette_image.getpalette()
    sheet = Image.new("P", (COLS * CELL, ROWS * CELL), 0)
    sheet.putpalette(palette)
    for item_id in range(COLS * ROWS):
        draw_card(sheet, item_id)
    sheet.save(OUTPUT, optimize=False)
    # Focus is represented by the normal selection lift, matching Alchemicals.
    sheet.save(FOCUS_OUTPUT, optimize=False)


if __name__ == "__main__":
    main()
