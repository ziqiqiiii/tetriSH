# tetrisu

The terminal game client for tetriSH, implemented in C on notcurses. It renders
the image-based home screen and now includes a local playable Endless Solo
Battle while the authoritative `tetrisd` game loop is being built.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Controls](#controls)
- [Menu Items](#menu-items)
- [Assets](#assets)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Authors](#authors)

---

## Features

- Image background rendered as a letterboxed 4 x 2 cell backdrop, with the
  selector bunny kept on its own high-resolution pixel plane
- Splash intro video streamed over the background, skippable with any key
- Bunny-sprite menu selector positioned from the rendered background geometry;
  scales with terminal size
- Kernel-sleeping keyboard input through notcurses' pollable input descriptor;
  arrow-key selection wraps around
- Optional background music and menu SFX via SDL2_mixer, with runtime volume control
- Best-effort audio — missing device, assets, or SDL libraries degrade to silent, never fatal
- Audio compiled out entirely (`-DTETRISU_ENABLE_AUDIO=0`) when SDL2/SDL2_mixer are absent
- Endless 10 x 20 Solo play with SRS, seven-bag generation, next-three preview,
  ghost piece, move-reset lock delay, modern scoring, and a guaranteed 350 ms
  view of the final board before any top-out panel appears
- Responsive 4:3 Solo layout built from one 512 x 384 master canvas, fitted to
  the terminal without changing the HUD aspect ratio
- Transparent image HUD with exact `#2E222F` authored borders, 16 x 16
  tetromino sprites, custom text/number masks, and centered Mirurun art
- Low-resolution scenery plus independently refreshed high-resolution HUD
  planes; bitmap-safe backends use atomic pixel-piece planes with the exact
  authored tile sprites, while unsafe or unsupported backends use a
  true-colour quadrant-cell fallback
- Attractive compatibility mode for terminals without safe bitmap rendering:
  a visible mode badge, terminal-font menu labels, native terminal selector,
  4 x 2
  HOLD/NEXT/HUD art, a true-colour cell board, and native terminal text for
  score statistics and pause/top-out instructions
- Bounded input batches and a 30 FPS presentation ceiling coalesce rapid
  movement and rotation without delaying gameplay state or flooding the PTY
- Terminal-aware press/release handling with immediate taps, 167 ms DAS,
  33 ms ARR, 20x soft drop, last-pressed direction priority, and safe
  terminal-repeat fallback when release events are unavailable
- Dirty row/HUD signatures rebuild only changed content, while the compact
  control legend uses one crisp terminal-font row
- Responsive PTY geometry checks reflow both home and Solo between compact and
  full layouts without busy-waiting when a terminal does not report resize as
  input
- Interactive ten-segment Mirurun crystal meter that gains one charge per two
  cleared lines, exposes evenly spaced `2 / 4 / 6 / 8` ability thresholds, and
  supports hover descriptions, mouse clicks, and `1`-`4` hotkeys
- Local Mirurun level-one activation removes the bottom four settled rows;
  opponent-targeted levels two-four currently spend charge and show a clearly
  labelled Solo test effect without mutating the board
- Two-frame progressive clear animation: 200 ms through level 6, then
  175/150/125 ms at levels 7/8/9 and 100 ms from level 10 onward

HOLD is implemented under temporary local Solo authority. The
[migration guide](../../docs/tetrisu-local-to-tetrisd.md) describes how it
becomes server-authoritative without rewriting the renderer.

---

## Prerequisites

notcurses (render) is **required**; SDL2 and SDL2_mixer (audio) are
**optional** and compiled out when absent. This Makefile owns those
component-only dependencies. The repository Makefile owns GCC, make,
pkg-config, OpenSSL, Readline, and ncurses.

```bash
make deps                          # check/install tetrisu render + audio deps
make check-tetrisu-deps            # check only; compiles a tiny notcurses probe
```

Package installation may request sudo access. Use the following when system changes are not allowed.

```bash
make deps AUTO_INSTALL_DEPS=0      # check-only; fail instead of installing
```

If APT has no `libnotcurses-dev`, the default
`INSTALL_NOTCURSES_FROM_SOURCE=1` builds notcurses from source. Ubuntu may need
its `universe` repository; RHEL-compatible systems need EPEL/CRB; Fedora and
openSUSE Tumbleweed ship `notcurses-devel`. Build it explicitly with:

```bash
make install-notcurses-from-source NOTCURSES_VERSION=v3.0.17
```

---

## Build

Build the `bin/tetrisu` binary with `make`:

```bash
make
```

Plain `make` first runs repository-level `make deps` unless invoked with
`DEPS_READY=1`, then checks Tetrisu dependencies and compiles with
`-Wall -Wextra -Werror`.

Makefile targets:

| Command      | Description                                        |
|--------------|----------------------------------------------------|
| `make`       | Check dependencies, then build `bin/tetrisu`       |
| `make run`   | Build and launch `tetrisu` immediately             |
| `make test`  | Build and run the unit tests                       |
| `make deps`  | Check/install render + audio dependencies          |
| `make clean` | Remove object files and test binaries              |
| `make fclean`| Remove object files, test binaries, and `bin/`     |
| `make re`    | Full rebuild (`fclean` + `all`)                    |

---

## Run

```bash
./bin/tetrisu
```

Or build and run in one step:

```bash
make run
```

Renderer selection defaults to `auto`: terminals with safe bitmap support keep
the authored high-resolution presentation, while unsupported or unsafe
terminals enter the cell-based compatibility presentation automatically. Force
that presentation anywhere for testing or preference with:

```bash
TETRISU_RENDERER=cell make run
```

The accepted values are `auto` and `cell`. Missing, empty, or unrecognised
values behave like `auto`.

`tetrisu` requires a real terminal: notcurses queries palette, pixel geometry,
and graphics-protocol support at startup. It exits with
`notcurses_core_init failed` when `$TERM` has no usable terminfo entry.

---

## Controls

| Key | Action |
|---|---|
| Any key | Dismiss the splash and enter the main menu |
| `↑` / `↓` | Move the menu selection (wraps at the ends) |
| `Enter` | Select the highlighted item |
| Mouse hover | Highlight the menu item under the pointer |
| Left click | Select the menu item under the pointer |
| `+` / `=` | Raise music volume one step |
| `-` / `_` | Lower music volume one step |
| `q` | Quit |
| `Enter` on Single Player | Start local Endless Solo |
| `←` / `→` | Move the active piece |
| `↑` or `X` | Rotate clockwise |
| `Z` | Rotate counter-clockwise |
| `↓` | Soft drop; 1 point per descended cell |
| `Space` | Hard drop and lock; 2 points per descended cell |
| `C` | Hold or swap the active piece once before it locks |
| `1` | Mirurun (2 charge): remove the bottom four settled rows |
| `2` | Inversion (4 charge): visual-only Solo test activation |
| `3` | Pentaris (6 charge): visual-only Solo test activation |
| `4` | Sirtet (8 charge): visual-only Solo test activation |
| Mouse hover/click | Show an ability description / activate its meter circle |
| `P` | Pause/resume Solo |
| `R` | Restart after top-out |
| `Esc` or `Q` | Return from Solo to the home screen |

Single Player opens the playable local mode. The other four menu items still
print a `[<item>] not wired up yet` message.

---

## Menu Items

| Item | Status |
|---|---|
| `Single Player` | Playable local Endless mode; can remain as offline play |
| `Multiplayer` | Stub — prints "not wired up yet" |
| `Marketplace` | Stub — prints "not wired up yet" |
| `Leaderboard` | Stub — prints "not wired up yet" |
| `Settings` | Stub — prints "not wired up yet" |

---

## Assets

Asset paths are compile-time macros resolved against `ASSET_DIR`, which the
Makefile sets to `src/tetrisu/assets`. The client loads:

| Macro | Role |
|---|---|
| `SPLASH_ASSET_PATH` | Clean home-screen artwork; five exact labels are rasterized from the shared pixel font at runtime |
| `BUNNY_ASSET_PATH` | Bunny selector sprite (PNG with alpha) |
| `INTRO_VIDEO_PATH` | MP4 splash intro streamed over the background |
| `INTRO_AUDIO_PATH` | MP3 played once alongside the intro |
| `HOME_BGM_PATH` | Looping home-screen background music |
| `SOLO_BACKGROUND_PATH` | Full-screen Solo Battle background |
| `DEFAULT_HUD_PATH` | Transparent 512 x 384 Solo HUD/frame |
| `DEFAULT_TILE_PATH` | Guideline-color tiles, garbage, and two clear frames |
| `DEFAULT_MIRURUN_PATH` | Solo character portrait, centered in its panel |
| `SHARED_FONT_MASK_PATH` | White alpha mask for all HUD text |
| `SHARED_NUMBERS_MASK_PATH` | White alpha mask for digits and `+`/`-` |
| `MENU_MOVE_SFX_PATH` | Sound on up/down selection movement |
| `MENU_SELECT_SFX_PATH` | Sound on selection confirmation |

Missing assets are non-fatal: a missing bunny sprite falls back to a text marker,
and missing audio files are silently skipped. The exact authored-HUD geometry,
including the board-border export contract, is documented in
[`assets/solo_hud_art_template.md`](assets/solo_hud_art_template.md).

---

## Architecture

`main()` builds the render context, plays the intro, then dispatches input to
the render and audio modules:

```
render_init (background)
     │
     ▼
render_intro_play          stream INTRO_VIDEO_PATH; skippable, best-effort audio
     │
     ▼
render_menu_create         draw the bunny selector over the background
     │
     ▼
  input loop               render_wait_key → dispatch:
     ├── ↑/↓   menu_move_selection + render_menu_move_bunny + move SFX
     ├── Enter Single Player -> solo_mode_run -> return to menu
     ├── Enter other item -> menu_stub_text + render_menu_show_message
     ├── +/-   audio_volume_up / audio_volume_down
     └── q     APP_QUIT
```

Modules (each a `.c` under `src/`):

| File | Responsibility |
|---|---|
| `main.c` | Entry point; wires render + audio and runs the input loop |
| `app_state.c` | Handle pure menu state, selection, and labels |
| `render_background.c` | notcurses init, background blit, `render_wait_key`, teardown |
| `renderer_policy.c` | renderer environment parsing and forced compatibility policy |
| `render_intro.c` | Splash video streaming with skip-on-input |
| `render_menu.c` | Bunny selector plane and on-screen messages |
| `audio.c` | Optional SDL2_mixer music and SFX; no-ops when audio is compiled out |
| `solo_game.c` | Pure local session state/timing; temporary authority boundary |
| `solo_abilities.c` | Mirurun metadata, charge spending, board transform, and mouse geometry |
| `render_solo.c` | Own terminal planes, dirty signatures, and responsive layout |
| `render_solo_canvas.c` | Load assets and compose pixel-perfect HUD/board canvases |
| `solo_mode.c` | Run the poll-driven local Solo input and render loop |

`app_state.c` is pure logic with no rendering or audio dependencies. It joins
`solo_game.c` in `obj/logic.a`, which unit tests link without notcurses or SDL.

---

## Project Structure

```
tetrisu/
├── src/
│   ├── main.c                 Entry point + input loop → bin/tetrisu
│   ├── app_state.c            Pure menu/state logic → logic.a (unit-tested)
│   ├── render_background.c    notcurses init, background, input, teardown
│   ├── renderer_policy.c      Renderer environment and compatibility policy
│   ├── render_intro.c         Splash video streamer
│   ├── render_menu.c          Bunny selector + messages
│   ├── render_solo.c          Solo planes, layout, and dirty-region updates
│   ├── render_solo_canvas.c   Asset loading + pixel-canvas composition
│   ├── solo_mode.c            Poll-driven local Solo loop
│   ├── solo_game.c            Pure local gameplay session
│   └── audio.c                Optional SDL2_mixer audio
├── scripts/
│   ├── install_deps.sh        Install render/audio packages per OS
│   ├── install_notcurses.sh   Build + install notcurses from source
│   ├── check_deps.sh          Compile/link probe for render/audio deps
│   ├── deps.sh                Check/install/re-check dependency orchestration
│   └── run_tests.sh           Formatted unit-test runner
├── assets/                    Images / video / audio (ASSET_DIR)
├── tests/test_*.c             Unit tests (each with its own main)
├── obj/                       Generated objects + logic.a
├── bin/tetrisu                Generated binary
├── tetrisu.h                  Public header (reached via -I.)
└── Makefile
```

---

## Testing

```bash
make test
```

The unit suite covers the notcurses/SDL-free app and Solo game state. Run the
strict component build without allowing dependency installation with:

```bash
make clean DEPS_READY=1 AUTO_INSTALL_DEPS=0
make all test DEPS_READY=1 AUTO_INSTALL_DEPS=0
```

For a native macOS ownership check, launch the full client through Apple
`leaks`, exercise Solo repeatedly, then quit normally:

```bash
MallocStackLogging=1 leaks -atExit -- ./bin/tetrisu
```

On Linux, run the mandatory PR ownership check with Valgrind and quit the
interactive client normally:

```bash
valgrind --leak-check=full --show-leak-kinds=all \
  --error-exitcode=1 ./bin/tetrisu
```

The Solo suite includes a 100,000-rotation state stress case and passes under
AddressSanitizer and UndefinedBehaviorSanitizer. The live WezTerm regression
also drives a 10,000-rotation burst followed by sustained paced movement and
rotation. Process memory remains bounded, the terminal queue drains normally,
and Apple `leaks` reports `0 leaks for 0 total leaked bytes`. The renderer
requires a real terminal for ownership checks because notcurses queries pixel
geometry and graphics-protocol capabilities during startup.

---

## Authors

| Role | Member | Owns |
|---|---|---|
| Tetrisu / Tetrisd | Sanjan Krishna Sarat | Client rendering, shell, game server |
| Tetrisu / Core libraries | Thong Zi Qi | Client integration, protocols, shared libraries |

---

*50.003 × 50.005 CoreStack Challenge — SUTD, Summer 2026*
