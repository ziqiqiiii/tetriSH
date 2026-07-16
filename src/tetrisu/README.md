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

---

## Features

- Image background rendered as a letterboxed 4 x 2 cell backdrop, with the
  selector bunny kept on its own high-resolution pixel plane
- Splash intro video streamed over the background, skippable with any key
- Bunny-sprite menu selector positioned from the rendered background geometry (scales with terminal size)
- Kernel-sleeping keyboard input through notcurses' pollable input descriptor;
  arrow-key selection wraps around
- Optional background music and menu SFX via SDL2_mixer, with runtime volume control
- Best-effort audio — missing device, assets, or SDL libraries degrade to silent, never fatal
- Audio compiled out entirely (`-DTETRISU_ENABLE_AUDIO=0`) when SDL2/SDL2_mixer are absent
- Endless 10 x 20 Solo play with SRS, seven-bag generation, next-three preview,
  ghost piece, move-reset lock delay, modern scoring, and immediate top-row
  lock-out
- Responsive 4:3 Solo layout built from one 512 x 384 master canvas, fitted to
  the terminal without changing the HUD aspect ratio
- Transparent image HUD with exact `#2E222F` authored borders, 16 x 16
  tetromino sprites, custom text/number masks, and centered Mirurun art
- Cell-rendered scenery plus disjoint high-resolution HUD planes; an active
  piece and its ghost each use one atomic pixel plane, so horizontal movement
  cannot shear the four blocks apart
- Dirty row/HUD signatures rebuild only changed content, while the compact
  control legend uses one crisp terminal-font row
- Responsive PTY geometry checks reflow Solo between compact and full layouts
  without busy-waiting when a terminal does not report resize as input
- Display-only ten-segment Mirurun crystal meter, charged by cleared lines
- Two-frame, 200 ms sprite animation before cleared rows compact

Hold is intentionally omitted from this project mode. Solo state is temporarily
local; [the migration guide](../../docs/tetrisu-local-to-tetrisd.md) describes
how it becomes server-authoritative without rewriting the renderer.

---

## Prerequisites

notcurses (render) is **required**; SDL2 and SDL2_mixer (audio) are **optional** and compiled out when absent. This Makefile owns those component-only dependencies; shared native deps (GCC, make, pkg-config, OpenSSL, Readline, ncurses) come from the repository-level Makefile.

```bash
make deps                          # check/install tetrisu render + audio deps
make check-tetrisu-deps            # check only; compiles a tiny notcurses probe
```

Package installation may request sudo access. Use the following when system changes are not allowed.

```bash
make deps AUTO_INSTALL_DEPS=0      # check-only; fail instead of installing
```

If no APT `libnotcurses-dev` is available, notcurses is built from source (`INSTALL_NOTCURSES_FROM_SOURCE=1`, the default). Ubuntu may need its `universe` repository; RHEL-compatible systems need EPEL/CRB; Fedora and openSUSE Tumbleweed ship `notcurses-devel`. Build notcurses from source explicitly with:

```bash
make install-notcurses-from-source NOTCURSES_VERSION=v3.0.17
```

---

## Build

Build the `bin/tetrisu` binary with `make`:

```bash
make
```

Plain `make` first runs the repository-level `make deps` (unless invoked with `DEPS_READY=1`), then this Makefile's own `deps`, then compiles. Compiled with `-Wall -Wextra -Werror`.

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

`tetrisu` requires a real terminal: notcurses queries it for palette, pixel geometry, and graphics-protocol support at startup. It exits with a `notcurses_core_init failed` message if `$TERM` has no usable terminfo entry.

---

## Controls

| Key | Action |
|---|---|
| Any key | Dismiss the splash and enter the main menu |
| `↑` / `↓` | Move the menu selection (wraps at the ends) |
| `Enter` | Select the highlighted item |
| `+` / `=` | Raise music volume one step |
| `-` / `_` | Lower music volume one step |
| `q` | Quit |
| `Enter` on Solo Battle | Start local Endless Solo |
| `←` / `→` | Move the active piece |
| `↑` or `X` | Rotate clockwise |
| `Z` | Rotate counter-clockwise |
| `↓` | Soft drop; 1 point per descended cell |
| `Space` | Hard drop and lock; 2 points per descended cell |
| `P` | Pause/resume Solo |
| `R` | Restart after top-out |
| `Esc` or `Q` | Return from Solo to the home screen |

Solo Battle opens the playable local mode. The other three menu items still
print a `[<item>] not wired up yet` message.

---

## Menu Items

| Item | Status |
|---|---|
| `Solo Battle` | Playable local Endless mode; later migrated to `tetrisd` |
| `Multiplayer Battle` | Stub — prints "not wired up yet" |
| `Marketplace` | Stub — prints "not wired up yet" |
| `Options` | Stub — prints "not wired up yet" |

---

## Assets

Asset paths are compile-time macros resolved against `ASSET_DIR` (the Makefile sets it to `src/tetrisu/assets`). The client loads:

| Macro | Role |
|---|---|
| `SPLASH_ASSET_PATH` | Home-screen background image (labels are baked in) |
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

Missing assets are non-fatal: a missing bunny sprite falls back to a text marker, and missing audio files are silently skipped.

---

## Architecture

`main()` builds the render context, plays the intro, then loops on input dispatched to the render and audio modules:

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
     ├── Enter Solo Battle -> solo_mode_run -> return to menu
     ├── Enter other item -> menu_stub_text + render_menu_show_message
     ├── +/-   audio_volume_up / audio_volume_down
     └── q     APP_QUIT
```

Modules (each a `.c` under `src/`):

| File | Responsibility |
|---|---|
| `main.c` | Entry point; wires render + audio and runs the input loop |
| `app_state.c` | Pure state/menu logic — key→state transitions, selection, item labels (no notcurses/SDL) |
| `render_background.c` | notcurses init, background blit, `render_wait_key`, teardown |
| `render_intro.c` | Splash video streaming with skip-on-input |
| `render_menu.c` | Bunny selector plane and on-screen messages |
| `audio.c` | Optional SDL2_mixer music and SFX; no-ops when audio is compiled out |
| `solo_game.c` | Pure local session state/timing; temporary authority boundary |
| `render_solo.c` | Cell backdrop, high-resolution HUD/sprite planes, responsive layout, poll-driven input, and animation |

`app_state.c` is pure logic with no rendering or audio dependencies, so it is archived into `obj/logic.a` and linked into the unit tests without pulling in notcurses or SDL.

---

## Project Structure

```
tetrisu/
├── src/
│   ├── main.c                 Entry point + input loop → bin/tetrisu
│   ├── app_state.c            Pure menu/state logic → logic.a (unit-tested)
│   ├── render_background.c    notcurses init, background, input, teardown
│   ├── render_intro.c         Splash video streamer
│   ├── render_menu.c          Bunny selector + messages
│   ├── render_solo.c          Solo composition, input loop, clear animation
│   ├── solo_game.c            Pure local gameplay session
│   └── audio.c                Optional SDL2_mixer audio
├── scripts/
│   ├── install_deps.sh        Install render/audio packages per OS
│   ├── install_notcurses.sh   Build + install notcurses from source
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

The current renderer was stress-tested through 25 hard drops, five Solo
exit/re-entry cycles, and live resize between 46 x 60 and 196 x 60 terminal
cells. Apple `leaks` reported `0 leaks for 0 total leaked bytes`; all 13 Solo
tests and all seven `libtetrisbrain` suites also pass under AddressSanitizer and
UndefinedBehaviorSanitizer. A real terminal remains necessary for the final
graphics check because notcurses performs terminal capability and pixel-
geometry queries during startup.
