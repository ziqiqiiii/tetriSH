# Solo HUD art contract

The runtime master canvas is **512 x 384 RGBA** (exactly 4:3). The current
`default_theme/default_board.png` is the authoritative layout; the older
large SVG/preview files in this folder are reference-only and are not loaded.

## Runtime regions

| Region | x | y | width | height |
|---|---:|---:|---:|---:|
| Next three | 80 | 4 | 160 | 36 |
| Crystal fill | 48 | 42 | 16 | 320 |
| Board interior | 80 | 43 | 160 | 320 |
| Mirurun safe box | 272 | 33 | 160 | 160 |
| Score interior | 272 | 203 | 160 | 160 |
| Controls text | 96 | 369 | 320 | 13 |

The board is exactly 10 x 20. Each logical cell is 16 x 16 master pixels.
Keep the board, next, character, score, meter, and controls interiors
transparent; the C renderer draws their changing content over the background.

## Loaded exports

- `default_theme/default_board.png`: 512 x 384 RGBA HUD and solid line borders.
- `default_theme/default_tile.png`: 16 x 180 RGBA vertical atlas. Ten 16 x 16
  sprites begin at y = 0, 18, 36, ... 162 (two transparent separator rows).
  Order: I, J, L, O, S, Z, T, garbage, clear frame 1, clear frame 2.
- `default_theme/default_mirurun.png`: square RGBA character image, nearest-
  neighbour fitted into the 160 x 160 safe box without cropping.
- `shared_font_mask.png`: 128 x 96 white alpha mask. It contains ASCII
  U+0020 through U+007F in a 16 x 6 grid of 8 x 16 cells.
- `shared_numbers_mask.png`: 140 x 16 white alpha mask. Fourteen 10 x 16 cells:
  digits 0-9, `+`, `-`, then two blanks.

The renderer tints the two masks at draw time. It derives the ghost directly
from the active tetromino tile, so a separate ghost sprite is unnecessary.

## Terminal scaling contract

The background and entire 512 x 384 HUD use the same fitted notcurses plane
geometry. Terminal cell pixel dimensions are measured at startup and after
every resize, preserving the physical 4:3 aspect ratio with centered
letterboxing. The static background may use Kitty/Sixel, but the animated HUD
always uses the 4x2 Unicode image blitter and is uploaded only when its pixels
change. This avoids retaining a new full-screen bitmap for every game tick. A
canvas smaller than 64 columns x 24 rows shows a resize message instead of
clipping the board.
