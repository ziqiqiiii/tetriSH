#!/usr/bin/env python3
"""Compose deterministic, text-safe Kitty auth artwork."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "default_theme"
SOURCE = ASSETS / "auth_screen.png"
WIDTH = 1448
HEIGHT = 1086
ATLAS_COLUMNS = 16
ATLAS_ROWS = 6
ATLAS_GLYPH_WIDTH = 20
ATLAS_GLYPH_HEIGHT = 40

FONT_CANDIDATES = (
    Path.home() / "Library/Fonts/MononokiNerdFont-Bold.ttf",
    Path.home() / "Library/Fonts/JetBrainsMonoNerdFont-Bold.ttf",
    Path("/System/Library/Fonts/SFNSMono.ttf"),
    Path("/System/Library/Fonts/Menlo.ttc"),
)

COLORS = {
    "gold": "#ffd06a",
    "pink": "#ff91c6",
    "lavender": "#c9a0ff",
    "cyan": "#69d4ff",
    "muted": "#8b879d",
    "footer": "#d9c6e9",
    "shadow": "#180b32",
    "well": "#070b28",
    "well_edge": "#211342",
}

FIELD_Y_LOGIN = (228, 337, 456)
FIELD_Y_SIGNUP = (228, 337, 456, 565)
VALUE_WELL = (748, 1058)
LABEL_RIGHT = 692
SEPARATOR_X = 718


def load_font(size: int) -> ImageFont.FreeTypeFont:
    for path in FONT_CANDIDATES:
        if path.exists():
            return ImageFont.truetype(path, size)
    raise FileNotFoundError("SF Mono, Menlo, or JetBrains Mono is required")


def pixel_panel(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int]) -> None:
    left, top, right, bottom = box
    cut = 6
    points = (
        (left + cut, top),
        (right - cut, top),
        (right, top + cut),
        (right, bottom - cut),
        (right - cut, bottom),
        (left + cut, bottom),
        (left, bottom - cut),
        (left, top + cut),
    )
    draw.polygon(points, fill=COLORS["well"], outline=COLORS["well_edge"])


def text(
    draw: ImageDraw.ImageDraw,
    position: tuple[int, int],
    value: str,
    font: ImageFont.FreeTypeFont,
    fill: str,
    anchor: str,
    stroke: int = 1,
) -> None:
    x, y = position
    draw.text(
        (x + 2, y + 3),
        value,
        font=font,
        fill=COLORS["shadow"],
        anchor=anchor,
    )
    draw.text(
        (x, y),
        value,
        font=font,
        fill=fill,
        anchor=anchor,
        stroke_width=stroke,
        stroke_fill=COLORS["shadow"],
    )


def draw_field_labels(
    draw: ImageDraw.ImageDraw,
    labels: tuple[str, ...],
    rows: tuple[int, ...],
) -> None:
    label_font = load_font(28)
    separator_font = load_font(28)
    for label, row in zip(labels, rows):
        pixel_panel(draw, (VALUE_WELL[0], row - 23, VALUE_WELL[1], row + 23))
        text(
            draw,
            (LABEL_RIGHT, row),
            label,
            label_font,
            COLORS["pink"],
            "rm",
        )
        text(
            draw,
            (SEPARATOR_X, row),
            ":",
            separator_font,
            COLORS["gold"],
            "mm",
            stroke=0,
        )


def compose(mode: str) -> Image.Image:
    image = Image.open(SOURCE).convert("RGBA")
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(f"{SOURCE} must be exactly {WIDTH}x{HEIGHT}")
    draw = ImageDraw.Draw(image)
    title_font = load_font(43)
    action_font = load_font(36)
    footer_font = load_font(21)
    if mode == "signup":
        title = "CREATE YOUR ACCOUNT"
        labels = ("USERNAME", "PASSWORD", "RE-ENTER PASSWORD", "SERVER ID")
        rows = FIELD_Y_SIGNUP
        primary = "SIGN UP"
        secondary = "BACK TO LOGIN"
    else:
        title = "WELCOME TO TETRISU"
        labels = ("USERNAME", "PASSWORD", "SERVER ID")
        rows = FIELD_Y_LOGIN
        primary = "LOGIN"
        secondary = "SIGN UP"
    text(draw, (WIDTH // 2, 141), title, title_font, COLORS["gold"], "mm")
    draw_field_labels(draw, labels, rows)
    if mode == "login":
        pixel_panel(draw, (500, 542, 948, 588))
    else:
        pixel_panel(draw, (500, 770, 948, 816))
    text(
        draw,
        (WIDTH // 2, 706),
        primary,
        action_font,
        COLORS["muted"],
        "mm",
    )
    text(draw, (484, 890), secondary, action_font, COLORS["lavender"], "mm")
    text(
        draw,
        (955, 890),
        "PLAY OFFLINE",
        action_font,
        COLORS["cyan"],
        "mm",
    )
    text(
        draw,
        (WIDTH // 2, 969),
        "TAB  NEXT    ENTER  SELECT    ESC  BACK",
        footer_font,
        COLORS["footer"],
        "mm",
        stroke=0,
    )
    return image


def compose_font_atlas() -> Image.Image:
    atlas = Image.new(
        "RGBA",
        (
            ATLAS_COLUMNS * ATLAS_GLYPH_WIDTH,
            ATLAS_ROWS * ATLAS_GLYPH_HEIGHT,
        ),
        (0, 0, 0, 0),
    )
    draw = ImageDraw.Draw(atlas)
    font = load_font(28)
    for glyph in range(ATLAS_COLUMNS * ATLAS_ROWS):
        character = chr(32 + glyph)
        center_x = (glyph % ATLAS_COLUMNS) * ATLAS_GLYPH_WIDTH
        center_x += ATLAS_GLYPH_WIDTH // 2
        center_y = (glyph // ATLAS_COLUMNS) * ATLAS_GLYPH_HEIGHT
        center_y += ATLAS_GLYPH_HEIGHT // 2
        draw.text(
            (center_x, center_y),
            character,
            font=font,
            fill=(255, 255, 255, 255),
            anchor="mm",
        )
    return atlas


def main() -> None:
    compose("login").save(ASSETS / "auth_login.png", optimize=True)
    compose("signup").save(ASSETS / "auth_signup.png", optimize=True)
    compose_font_atlas().save(ASSETS / "auth_font_atlas.png", optimize=True)


if __name__ == "__main__":
    main()
