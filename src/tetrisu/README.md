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
- Cutesy Mirurun-and-speaker pixel-art volume feedback on Home and Solo:
  `MUSIC`, a rounded percentage, and a 16-step crystal bar; compatibility mode
  recreates the same artwork with dense terminal cells, and silent-audio
  builds retain the visual feedback
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
- Dedicated Settings/Profile screen with live fixture profile, portrait,
  equipped character/theme, full catalogues, wallet, score, rank, and
  Settings-to-Marketplace routing
- Signed-in Settings navigates as a grid: the two inventory panels sit above
  the control row, arrows step through them, and `Enter` equips the focused
  character or theme. `[` / `]` remain a shortcut for cycling characters.
- Inventory slots render catalogue thumbnails from their asset paths. The
  character inventory is a four-column, one-row grid; the theme inventory is a
  four-column, two-row grid, with the seventh theme occupying the partial
  final row.
- Inventory focus uses a centred gold-outlined plate and label; equipped state
  is shown independently by a star. The focused item's full name appears in a
  compact panel header, while narrow theme slots use short fitted captions.
- The preview fixture intentionally locks Princess, Wolf-man, Haaland, and
  Clauding. Locked thumbnails are desaturated but remain focusable for visual
  inspection; `Enter` refuses to equip them and raises an `ITEM NOT OWNED`
  notification directing the user to the Marketplace. The same notification
  has authored pixel art and a compatibility-mode presentation.
- Moving the focus onto a character opens its powers card automatically, the
  way the Solo ability meters describe themselves. `I` still pins the card for
  the equipped character from anywhere. The four canonical crystal powers are
  sourced from
  [Tetris.wiki's Tetris Battle Gaiden reference](https://tetris.wiki/Tetris_Battle_Gaiden).
- Settings is keyboard-only: it disables pointer reporting on entry, so no
  hover or click reaches it, and the screens that use the pointer turn it back
  on when they are entered
- Settings caches its composed static layer — the v2 backdrop, profile,
  portrait, and stats — so a focus change never re-reads the portrait or
  redraws the text above it. The movable tier cuts region planes from that
  layer; the stationary tier stamps a full frame from it, because a Sixel
  plane laid over another sprixel is re-emitted whenever the plane beneath it
  is damaged and the two then blank each other out. Queued key repeats are
  drained into the state before the repaint, so a held arrow cannot outrun the
  screen.
- Settings keeps the large authored frame cached and repaints only compact
  controls, inventory focus/equipped overlays, ability card, or volume value
  when they change. Character and theme thumbnails are cached independently
  by their catalogue asset path.
- Offline Settings keeps only current-run local controls/status and never
  invents account identity, inventory, wallet, score, or rank
- `+` / `-` drive one volume. Menu and gameplay effects are scaled by it
  alongside the music, so the per-effect constants stay purely as the mix
  balance; at full volume the balance is exactly what it was before

HOLD is implemented under temporary local Solo authority. The
[migration guide](../../docs/tetrisu-local-to-tetrisd.md) describes how it
becomes server-authoritative without rewriting the renderer.

---

## Prerequisites

notcurses 3.0.5 or newer (render) is **required**; SDL2 and SDL2_mixer (audio) are
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

If APT has no compatible `libnotcurses-dev`—including Debian releases whose
package is older than 3.0.5—the default `INSTALL_NOTCURSES_FROM_SOURCE=1` builds
notcurses from source. Ubuntu may need its `universe` repository;
RHEL-compatible systems need EPEL/CRB; Fedora and openSUSE Tumbleweed ship
`notcurses-devel`. Build it explicitly with:

```bash
make install-notcurses-from-source NOTCURSES_VERSION=v3.0.12
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

For deterministic UI testing of the signed-in fixture path, opt in explicitly:

```bash
TETRISU_UI_PREVIEW=1 TETRISU_RENDERER=cell ./bin/tetrisu
```

On Login, move focus to the visible `PREVIEW` button and press `Enter`, click
it, or press `P` while an action button is focused. This creates the marked
`LOCAL UI PREVIEW` session with `navigation.offline=false`, so Home,
Leaderboard, Settings, and Settings → Marketplace are reachable. Without
`TETRISU_UI_PREVIEW=1`, the preview action is not rendered or accepted; real
server sign-in and Play Offline retain their existing behavior.

Or build and run in one step:

```bash
make run
```

Renderer selection defaults to `auto`, which probes the terminal once and
picks one of three tiers:

| Tier | Chosen for | Presentation |
|---|---|---|
| movable | kitty and Ghostty — Kitty-protocol terminals measured to free a replaced image | Authored bitmaps everywhere; the selector and the falling piece are their own planes and slide |
| stationary | Sixel and the Linux framebuffer (foot, XTerm, mlterm, VTE ≥ 0.78, `/dev/fb0`), plus other image-registry terminals such as WezTerm, iTerm2, Konsole, and contour | The same authored bitmaps, but no bitmap is ever moved: the board is flattened into one image redrawn in place, and the selector is destroyed and blitted again at its new row |
| cell | Terminals reporting no bitmap support | True-colour terminal cells throughout, with the `:: COMPATIBILITY MODE ::` badge |

Two independent properties decide this, and conflating them is what previously
sent perfectly capable terminals to the cell renderer:

**Can a bitmap plane move?** Only on the Kitty protocols. notcurses implements
sprixel movement in `kitty_move` and leaves `ti->pixel_move` NULL for both
`setup_sixel_bitmaps()` and `setup_fbcon_bitmaps()`, and redisplaying a Sixel
cannot write transparency over what is already on screen. So Sixel terminals
draw bitmaps, just never moving ones — no version of foot changes that, since
foot implements the Kitty *keyboard* protocol but not the graphics one. Note
that this bans `ncplane_move_yx()` on a bitmap plane, not bitmaps that change
position: destroying a plane and blitting a new one damages the cells the old
one held and repaints them from the layer below, which is an ordinary render
rather than the sprixel wipe Sixel cannot honour. Both the board and the menu
selector rely on that distinction to keep their artwork here.

**Does memory stay bounded?** Only the Kitty and iTerm2 protocols hand the
terminal an image registry that a buggy terminal can grow without bound. Sixel
and the framebuffer paint straight into the grid and keep nothing. The backend
enum cannot grade the registry terminals, because notcurses reserves
`NCPIXEL_KITTY_ANIMATED` and `NCPIXEL_KITTY_SELFREF` for kitty itself and drops
every other Kitty-graphics terminal onto `NCPIXEL_KITTY_STATIC`. Measured-good
terminals move bitmaps; every other detected registry backend takes the
stationary tier, which retransmits on board change rather than once per frame.

Any tier can be forced, which is how an unmeasured terminal is tried or a
suspected rendering bug is bisected:

```bash
TETRISU_RENDERER=cell make run        # terminal cells only
TETRISU_RENDERER=stationary make run  # bitmaps, never moved
TETRISU_RENDERER=pixel make run       # bitmaps, freely moved
```

The accepted values are `auto`, `cell`, `stationary`, and `pixel`. Missing,
empty, or unrecognised values behave like `auto`. Forcing `pixel` on a terminal
whose bitmap registry is not measured is exactly the case the automatic gate
avoids: watch the process's memory while a game runs before trusting it.

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
| `Esc` | Return to the parent screen |
| `Esc` in sign-in modal | Dismiss the modal |
| `Tab` / `←` / `→` in modal | Toggle focus between Back and Sign In |
| `Enter` in modal | Confirm the focused button |
| Mouse click in modal | Confirm the clicked button |
| `L` / `S` / `O` on Entry | Open Login / Sign Up / play offline |
| `Enter` in Lobby/Create Room | Open the next room-flow scaffold |
| `D` / `B` in Waiting Room | Open Double / Battle Royale scaffold |
| `+` / `=` | Raise music volume one step |
| `-` / `_` | Lower music volume one step |
| `q` | Quit |
| `←` / `→` in the Settings control row | Cycle Back, Marketplace, Volume -, Volume + (Settings takes no mouse input) |
| `↑` from the Settings control row | Focus the characters catalogue |
| `←` / `→` at an inventory panel edge | Cross between the characters and themes panels |
| `↓` past the last inventory row | Drop back to the control row |
| `Enter` on an inventory slot | Equip an owned item; locked items show Marketplace guidance |
| Focus on a character slot | Opens that character's powers card automatically |
| `Tab` in Settings | Step through every slot and control in one order |
| `M` in signed-in Settings | Open Marketplace; `Enter` on the visible button does the same |
| `[` / `]` (or `,` / `.`) in signed-in Settings | Select the previous / next owned character |
| `I` in signed-in Settings | Toggle the selected character's four-power info card |
| `P` on Login with `TETRISU_UI_PREVIEW=1` | Sign into the clearly marked local UI fixture |
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

Single Player opens the playable local mode. In offline mode, Multiplayer,
Marketplace, and Leaderboard open a sign-in-required modal with Escape to
dismiss and Sign In to navigate to the Login screen. When authenticated or in
fixture mode, those items open their dedicated or scaffold screens marked
`LOCAL UI PREVIEW`. Leaderboard has its complete top-ten presentation:
`R` refreshes, `Esc` returns Home, Left/Right or Tab changes the focused button,
and both Back and Refresh support mouse hover/click.

---

## Menu Items

| Item | Status |
|---|---|
| `Single Player` | Playable local Endless mode; can remain as offline play |
| `Multiplayer` | Navigable lobby/create/waiting/match scaffolds |
| `Marketplace` | Typed fixture-backed scaffold |
| `Leaderboard` | Complete top-three podium and positions 4–10, with refresh/error states |
| `Settings` | Dedicated live Profile/Settings screen; offline mode shows local controls only |

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
| `SETTINGS_BACKGROUND_PATH` | Cached 1448 x 1086 Settings/Profile backdrop (`settings_profile_background_v2.png`) |
| `HALLOWEEN_PORTRAIT_PATH`, `PRINCESS_PORTRAIT_PATH`, `WOLFMAN_PORTRAIT_PATH` | Character inventory thumbnails; Mirurun uses `DEFAULT_MIRURUN_PATH` |
| `SETTINGS_THEME_CLASSIC_PREVIEW_PATH` | `settings_previews/theme_classic.png` (192 x 192 Classic thumbnail) |
| `SETTINGS_THEME_DESIGN_AI_UNIVERSITY_PREVIEW_PATH` | `settings_previews/theme_design_ai_university.png` (192 x 192 Design AI University thumbnail) |
| `SETTINGS_THEME_SNOWMAN_PREVIEW_PATH` | `settings_previews/theme_snowman.png` (192 x 192 Do You Wanna Build a Snowman thumbnail) |
| `SETTINGS_THEME_HAALAND_PREVIEW_PATH` | `settings_previews/theme_haaland.png` (192 x 192 Haaland thumbnail) |
| `SETTINGS_THEME_AL_MERQAEDES_PREVIEW_PATH` | `settings_previews/theme_al_merqaedes.png` (192 x 192 Al Merqaedes F1 Team thumbnail) |
| `SETTINGS_THEME_NUCLEAR_GHANDI_PREVIEW_PATH` | `settings_previews/theme_nuclear_ghandi.png` (192 x 192 Nuclear Ghandi thumbnail) |
| `SETTINGS_THEME_CLAUDING_PREVIEW_PATH` | `settings_previews/theme_clauding.png` (192 x 192 Clauding thumbnail) |
| `VOLUME_NOTIFICATION_PATH` | Mirurun-and-speaker pixel-art volume card |
| `OWNERSHIP_NOTIFICATION_PATH` | Mirurun, lock, and shop-bag ownership-error card |
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
render_auth_show           Login / Sign Up / Play Offline; native-text fields
     │
     ▼
render_menu_create         after authentication/offline entry, draw home menu
     │
     ▼
  screen loop              render_wait_input → validated navigation:
     ├── ↑/↓   menu_move_selection + render_menu_move_bunny + move SFX
     ├── Enter Single Player -> solo_mode_run -> return to menu
     ├── Enter Leaderboard -> dedicated top-ten screen + refresh/back
     ├── Enter Settings -> live Profile/Settings screen + local controls
     ├── Enter other item -> typed LOCAL UI PREVIEW scaffold
     ├── Esc    explicit parent screen
     ├── +/-   audio_volume_up / audio_volume_down
     └── q     APP_SCREEN_QUIT
```

Modules (each a `.c` under `src/`):

| File | Responsibility |
|---|---|
| `main.c` | Entry point; wires render + audio and runs the input loop |
| `app_state.c` | Validate the complete screen graph, Back routes, menu state, and labels |
| `app_provider.c` | Typed screen models and marked local fixture provider |
| `settings_screen.c` | Pure Settings focus traversal and semantic actions |
| `render_settings.c` | Live profile/inventory/settings panel; chooses the pixel renderer or self-contained cell fallback |
| `render_settings_font.c` | Composes the cached v2 Settings frame, catalogue thumbnails, and focus/equipped overlays |
| `leaderboard_screen.c` | Pure leaderboard focus and action handling |
| `auth_form.c` | UTF-8 auth input, masking, focus, validation, and provider submission |
| `render_background.c` | notcurses init, background blit, `render_wait_key`, teardown |
| `renderer_policy.c` | renderer environment parsing and forced compatibility policy |
| `render_intro.c` | Splash video streaming with skip-on-input |
| `render_menu.c` | Bunny selector plane and on-screen messages |
| `render_auth.c` | Pixel-art login/sign-up frame and terminal-only fallback |
| `render_screen.c` | Shared native-terminal shell and plane cleanup |
| `render_leaderboard.c` | Homepage-backed podium/table and cell compatibility renderer |
| `audio.c` | Optional SDL2_mixer music and SFX; no-ops when audio is compiled out |
| `solo_game.c` | Pure local session state/timing; temporary authority boundary |
| `solo_abilities.c` | Mirurun metadata, charge spending, board transform, and mouse geometry |
| `render_solo.c` | Own terminal planes, dirty signatures, and responsive layout |
| `render_solo_canvas.c` | Load assets and compose pixel-perfect HUD/board canvases |
| `solo_mode.c` | Run the poll-driven local Solo input and render loop |

`app_state.c` and `app_provider.c` are pure logic with no rendering or audio
dependencies. They join `solo_game.c` in `obj/logic.a`, which unit tests link
without notcurses or SDL.

---

## Project Structure

```
tetrisu/
├── src/
│   ├── main.c                 Entry point + input loop → bin/tetrisu
│   ├── app_state.c            Pure validated screen graph → logic.a
│   ├── app_provider.c         Typed models + local fixture provider
│   ├── leaderboard_screen.c   Pure leaderboard focus/actions → logic.a
│   ├── auth_form.c            Pure authentication form state → logic.a
│   ├── render_background.c    notcurses init, background, input, teardown
│   ├── renderer_policy.c      Renderer environment and compatibility policy
│   ├── render_intro.c         Splash video streamer
│   ├── render_menu.c          Bunny selector + messages
│   ├── render_auth.c          Auth artwork overlay + cell fallback
│   ├── render_screen.c        Native-terminal screen scaffold
│   ├── render_settings.c      Profile/settings screen + controls
│   ├── render_leaderboard.c   Dedicated podium/table + compatibility view
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

The Settings and preview-auth tests are included in that command. For a
manual signed-in Settings check, run the preview command above, choose
`PREVIEW` on Login, and select Settings on Home. Confirm the v2 profile
backdrop and portrait appear, each inventory slot shows its thumbnail, and the
character panel is one row of four while the theme panel is two rows of four
with a three-slot final row. Move focus between slots and verify the focused
selection plate/gold label is distinct from the equipped star, equip a
character and a theme with `Enter`, then press `Enter` on Princess, Wolf-man,
Haaland, or Clauding and confirm the item stays unequipped while the top-right
Marketplace notice appears. Repeat with `TETRISU_RENDERER=cell` to confirm its
compatibility presentation. Hold an arrow to confirm focus tracks the last
key, press `I`, confirm the mouse does nothing there, resize the terminal, and
open Marketplace before pressing Back. For offline separation, omit the
environment variable, choose Play Offline, open Settings, and confirm that
only local status, renderer mode, and music volume appear.

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
