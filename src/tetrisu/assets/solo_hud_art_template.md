# Solo HUD art contract

The runtime master canvas is **512 x 384 RGBA** (exactly 4:3). The current
`default_theme/default_board.png` is the authoritative layout; the older
large SVG/preview files in this folder are reference-only and are not loaded.

## Runtime regions

| Region | x | y | width | height |
|---|---:|---:|---:|---:|
| Next three | 80 | 4 | 160 | 36 |
| Crystal fill | 48 | 42 | 16 | 320 |
| Board interior | 80 | 48 | 160 | 320 |
| Mirurun safe box | 272 | 33 | 160 | 160 |
| Score interior | 272 | 203 | 160 | 160 |
| Controls text | terminal bottom row | terminal bottom row | 352-source-pixel span | 1 cell |

The board is exactly 10 x 20. Each logical cell is 16 x 16 master pixels.
Keep the board, next, character, score, meter, and controls interiors
transparent; the C renderer draws their changing content over the background.

## Board-frame export contract

The current board export is five pixels too high, so the renderer temporarily
moves its pixels at load time. To remove that workaround, update Layer 1 in
`default_board.aseprite`, then its PNG export, using these **zero-based,
inclusive** coordinates and operations:

1. Clear `x = 94..417`, `y = 367..383`; the renderer supplies the compact
   terminal-font controls itself.
2. Move the exact RGBA selection `x = 77..242`, `y = 43..365` down five pixels
   to `y = 48..370`, clearing its old location.
3. Copy—not move—the exact RGBA selection `x = 77..242`, `y = 40..42` to
   `y = 45..47`. The source stays in place as the Next panel's bottom edge.

Do not redraw either selection as a solid rectangle: preserve every authored
curve, corner, transparent pixel, and exact `#2E222F` frame pixel. The expected
result has a transparent gap at `x = 77..242`, `y = 43..44`, a 3px board top at
`y = 45..47`, side bounds `x = 77..79` and `x = 240..242`, and a bottom bound
at `y = 368..370`. Verify the 160 x 320 interior `x = 80..239`, `y = 48..367`
is fully transparent.

Export at exactly 512 x 384 RGBA with nearest-neighbour/pixel-perfect output,
no scaling, matte, antialiasing, padding, or palette conversion. Once this
contract is present in the PNG, `align_authored_hud()` and the `HUD_ART_*`
alignment constants can be removed from `render_solo.c`.

## Loaded exports

- `default_theme/default_board.png`: 512 x 384 RGBA HUD and solid line borders.
- `default_theme/default_tile.png`: 16 x 180 RGBA vertical atlas. Ten 16 x 16
  sprites begin at y = 0, 18, 36, ... 162 (two transparent separator rows).
  Order: I, J, L, O, S, Z, T, garbage, clear frame 1, clear frame 2.
- `default_theme/default_mirurun.png`: square RGBA character image, nearest-
  neighbour fitted into the 160 x 160 safe box without cropping.
- `shared_font_mask.png`: 128 x 96 white alpha mask. It contains ASCII
  U+0020 through U+007F in a 16 x 6 grid of 8 x 16 cells.
- `shared_numbers_mask.png`: 140 x 16 white alpha mask. Fourteen nominal
  10 x 16 slots: digits 0-9, `+`, `-`, then two blanks. The zero ink is read
  from `x = 0..7`; glyphs 1-11 are intentionally packed one pixel left and are
  read from `x = slot * 10 - 1` through the next seven pixels.

The renderer tints the two masks at draw time. It derives the ghost directly
from the active tetromino tile, so a separate ghost sprite is unnecessary.

## Terminal scaling contract

The background and entire 512 x 384 HUD use the same fitted notcurses geometry.
Terminal cell pixel dimensions are measured at startup and after every resize,
preserving the physical 4:3 aspect ratio with centered letterboxing. Static
background/HUD art is cell-rendered once with the 4x2 Unicode blitter. Moving
pieces, Mirurun, and changing HUD regions use the terminal's high-resolution
pixel protocol and are uploaded only when their content changes. This keeps
foreground sprites crisp without rebuilding a full-screen bitmap every tick. A
canvas smaller than 64 columns x 24 rows shows a resize message instead of
clipping the board.
