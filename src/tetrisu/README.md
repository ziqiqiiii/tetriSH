# tetrisu

The terminal game client for tetriSH, implemented in C on notcurses. Renders an image-based home screen, plays a splash intro video, drives a bunny-selector main menu, and handles keyboard input — with optional SDL2_mixer music and sound effects.

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

- Image background blitted onto the notcurses standard plane, letterboxed to preserve the source aspect ratio
- Splash intro video streamed over the background, skippable with any key
- Bunny-sprite menu selector positioned from the rendered background geometry (scales with terminal size)
- Non-blocking keyboard input; arrow-key selection with wrap-around
- Optional background music and menu SFX via SDL2_mixer, with runtime volume control
- Best-effort audio — missing device, assets, or SDL libraries degrade to silent, never fatal
- Audio compiled out entirely (`-DTETRISU_ENABLE_AUDIO=0`) when SDL2/SDL2_mixer are absent

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

Menu items are not wired to gameplay yet — selecting one prints a `[<item>] not wired up yet` message.

---

## Menu Items

| Item | Status |
|---|---|
| `Solo Battle` | Stub — prints "not wired up yet" |
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
     ├── Enter menu_stub_text + render_menu_show_message + select SFX
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

Only `app_state.c` logic is unit-tested — it is the notcurses/SDL-free code, linked from `obj/logic.a`. There is no automated integration test: notcurses blocks on terminal-capability probes that a real terminal answers instantly but a scripted pty does not, so end-to-end verification is a manual `make run`.
