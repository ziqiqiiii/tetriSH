# libtetrisbrain

The pure game-logic library for tetriSH, implemented in C. Provides the board model, tetromino spawning/movement/rotation (SRS with wall kicks), gravity, line clearing, NES-style scoring, and the Battle Royale ability transforms — with no I/O, no networking, and no side effects.

---

## Table of Contents

- [Features](#features)
- [Build](#build)
- [Using the Library](#using-the-library)
- [Board Model](#board-model)
- [API Reference](#api-reference)
- [Result Codes](#result-codes)
- [Design Constraints](#design-constraints)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)

---

## Features

- Fixed `10 × 20` board of typed cells (empty / filled / garbage) with an 8-bit colour tag
- All 7 tetrominoes with SRS rotation states and JLSTZ / I wall-kick tables
- Collision, movement, soft drop, hard drop, and gravity tick
- Line clearing with bottom-up compaction, returning `0–4` cleared
- NES-style scoring, level progression, and a gravity-interval ramp
- Six Battle Royale ability transforms (Tetris Battle Gaiden character specials)
- Pure functions only — no `malloc`, no globals, no I/O; every board is a caller-owned value

---

## Build

Build the static archive with `make`:

```bash
make
```

This compiles every `.c` under `src/` and archives them into `libtetrisbrain.a`. Compiled with `-Wall -Wextra -Werror`; no dependencies beyond a C compiler and the C standard library.

Makefile targets:

| Command       | Description                                     |
|---------------|-------------------------------------------------|
| `make`        | Build `libtetrisbrain.a`                         |
| `make test`   | Build the archive and run the unit tests         |
| `make clean`  | Remove object files and test binaries            |
| `make fclean` | Remove object files, test binaries, and the archive |
| `make re`     | Full rebuild (`fclean` + `all`)                  |

---

## Using the Library

Link the archive and add its include path when compiling your own code:

```bash
gcc my_program.c lib/libtetrisbrain/libtetrisbrain.a -I lib/libtetrisbrain/include -o my_program
```

Then include the single public header and drive the board through a game step:

```c
#include "tetrisbrain.h"

t_board board;
board_init(&board);

t_piece p = piece_spawn(PIECE_T);
piece_move(&board, &p, -1, 0);          // shift left
piece_rotate(&board, &p, 1);            // rotate clockwise (with wall kick)

if (gravity_tick(&board, &p) == BRAIN_LOCKED) {
    piece_stamp(&board, &p);            // lock the piece into the board
    int lines = board_clear_lines(&board);
    int gained = score_on_clear(lines, level_from_lines(total_lines));
}
```

---

## Board Model

- The board is `BOARD_WIDTH` (`10`) columns × `BOARD_HEIGHT` (`20`) rows, indexed `board_get(b, col, row)`.
- Coordinates are **row-down**: `row 0` is the top, `row 19` the floor. Piece shape offsets follow the same convention.
- Out-of-bounds reads via `board_get` return `CELL_FILLED` (a solid wall), so collision checks need no per-caller range guards.
- Cells carry a `t_cell_type` (`CELL_EMPTY`, `CELL_FILLED`, `CELL_GARBAGE`) and an 8-bit `color`; stamped pieces record their `t_piece_type` as the colour.

---

## API Reference

### Board (`board.c`)

| Function | Description |
|---|---|
| `board_init(b)` | Zero the board to all `CELL_EMPTY` |
| `board_get(b, col, row)` | Read a cell; out-of-bounds returns `CELL_FILLED` |
| `board_set(b, col, row, cell)` | Write a cell; out-of-bounds writes are ignored |
| `board_in_bounds(col, row)` | Test whether a coordinate lies on the board |
| `board_inject_garbage(b, lines, hole_col)` | Shift the stack up and add `lines` garbage rows with a gap at `hole_col` |
| `board_copy(dst, src)` | Copy one board over another |

### Pieces (`pieces.c`)

| Function | Description |
|---|---|
| `piece_spawn(type)` | Return a piece at its centered spawn position and rotation |
| `piece_is_valid(b, p)` | Test whether a piece's four cells are all empty and in range |
| `piece_move(b, p, dcol, drow)` | Translate the piece; `BRAIN_BLOCKED` if it would collide |
| `piece_rotate(b, p, dir)` | Rotate `dir` (`+1` CW / `-1` CCW) with SRS wall kicks; `BRAIN_BLOCKED` if no kick fits |
| `piece_stamp(b, p)` | Write the piece's four cells into the board as `CELL_FILLED` |

### Gravity (`gravity.c`)

| Function | Description |
|---|---|
| `gravity_tick(b, p)` | Fall one row; `BRAIN_LOCKED` when it lands |
| `piece_soft_drop(b, p)` | Player-triggered fall-by-one; same rule as `gravity_tick` |
| `piece_hard_drop(b, p)` | Fall until it lands |

### Line Clear (`lineclear.c`)

| Function | Description |
|---|---|
| `board_clear_lines(b)` | Remove full rows, compact downward; returns lines cleared (`0–4`) |

### Scoring (`scoring.c`)

| Function | Description |
|---|---|
| `score_on_clear(lines_cleared, level)` | NES base points (`40 / 100 / 300 / 1200`) × `(level + 1)` |
| `level_from_lines(total_lines)` | Level is `total_lines / 10` |
| `gravity_interval_ms(level)` | Tick interval, ramping `1000 ms` down `50 ms`/level, floored at `100 ms` |

### Abilities (`abilities.c`)

Pure `t_board` transforms for the Battle Royale character specials; cooldowns, charges, and targeting live server-side in `tetrisd`.

| Function | Special | Description |
|---|---|---|
| `board_cut_top(b, n)` | Wolfman | Remove the top `n` rows; the stack shifts up |
| `board_cut_bottom(b, n)` | Mirurun | Remove the bottom `n` rows; the stack shifts down |
| `board_apply_gravity(b)` | Wolfman lvl4 | Drop every suspended cell straight down, per column |
| `board_invert(b)` | Halloween "Dark" | Swap empty ↔ garbage for every cell |
| `board_fill_rows(b, n, hole_col)` | Halloween "Burn" | Overwrite the bottom `n` rows with garbage (gap at `hole_col`), no shift |
| `board_clear_cells(b, cols, rows, count)` | Halloween "Bomb" | Clear a scattered set of cells; out-of-range targets ignored |
| `board_delete_columns(b, start_col, end_col)` | Princess | Clear columns `[start_col, end_col]` across every row; bounds clamped |

---

## Result Codes

Functions that can succeed or be rejected return `t_brain_result`:

`BRAIN_OK`, `BRAIN_BLOCKED`, `BRAIN_LOCKED`, `BRAIN_GAME_OVER`, `BRAIN_CLEARED`

---

## Design Constraints

- **No I/O, no side effects.** Every function is pure logic over caller-owned boards and pieces — no `malloc`, no globals, no syscalls.
- **Out-of-bounds is solid.** `board_get` returns `CELL_FILLED` off the board, so collision logic treats walls and floor uniformly.
- **Corrupt input is safe.** A piece with an out-of-range `type`/`rotation` (e.g. from a malformed network message) fails validation instead of indexing the shape tables out of bounds.
- **Coordinate convention is fixed.** Row-down throughout; SRS guideline kick tables are pre-converted (`drow = -dy`).
- **Abilities are stateless.** They apply only the board transform; the server owns cooldowns, charges, and targeting.

---

## Architecture

A game step drives the board through these modules, each in its own `src/*.c`:

```
piece_spawn                spawn a tetromino at its SRS start
     │
     ▼
piece_move / piece_rotate  translate / SRS-rotate with wall kicks (collision-checked)
     │
     ▼
gravity_tick               fall one row; BRAIN_LOCKED on landing
     │
     ▼
piece_stamp                write the locked piece into the board
     │
     ▼
board_clear_lines          remove full rows (0–4) and compact downward
     │
     ▼
score_on_clear             award points from lines cleared and level
```

Ability transforms (`abilities.c`) are applied out of band, whenever the server resolves a Battle Royale special against a target board.

---

## Project Structure

```
libtetrisbrain/
├── include/
│   └── tetrisbrain.h       Public header — the whole API (-I include)
├── src/
│   ├── board.c             Board model, cell access, garbage injection
│   ├── pieces.c            Tetromino shapes, SRS rotation, wall kicks
│   ├── gravity.c           Gravity tick, soft drop, hard drop
│   ├── lineclear.c         Full-row detection and compaction
│   ├── scoring.c           Scoring, level, gravity-interval ramp
│   └── abilities.c         Battle Royale ability board transforms
├── tests/
│   └── test_*.c            Unit tests, one per module (each with its own main)
├── scripts/
│   └── run_tests.sh        Formatted test runner
├── obj/                    Generated objects
└── libtetrisbrain.a        Generated archive
```

---

## Testing

```bash
make test
```

Each module has a matching `tests/test_<module>.c` with its own `main()`, linked against the archive and run through the formatted runner. The tests use plain `assert` and print one `PASS` line per case.

### Filtering tests

Use `FILTER` to build and run only suites whose name contains a substring:

```bash
make test FILTER=abilities   # only tests/test_abilities.c
make test FILTER=pieces      # only tests/test_pieces.c
```
