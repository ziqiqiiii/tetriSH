# tetrisu Homepage (Splash + Main Menu) — Design

## Scope

First testable slice of `tetrisu`, the terminal game client. No networking, no
`libtetrissh`/`libhtttp`, no game board. Just:

1. Splash screen — renders `assets/splash.png` via `chafa`, waits for a keypress.
2. Main menu — 4 items, arrow-key navigation, Enter prints a stub line (nothing
   wired up to the network yet).
3. Quit cleanly on `q` or `SIGINT`.

Everything else (lobby, options screen, marketplace, in-game board, animations,
chat) is out of scope for this slice and gets its own design/plan later.

## State machine

```c
typedef enum {
    APP_SPLASH,
    APP_MAIN_MENU,
    APP_QUIT,
} app_state_t;
```

`APP_QUIT` exists so the main loop has an explicit terminal condition and can
run `endwin()` before `exit()`, rather than calling `exit()` mid-render and
leaving the terminal in a broken (no-echo, raw-mode) state.

Transitions:
- `APP_SPLASH` + any keypress → `APP_MAIN_MENU`
- `APP_MAIN_MENU` + `q` → `APP_QUIT`
- `APP_MAIN_MENU` + Enter on an item → stays `APP_MAIN_MENU`, prints a stub
  message (`"[<item>] not wired up yet"`)
- `SIGINT` from any state → `APP_QUIT` (handled via flag, see below)

Main menu items, fixed order: Solo Battle, Multiplayer Battle, Marketplace,
Options.

## Asset

`src/tetrisu/assets/splash.png` — a screenshot of the original Tetris Battle
Gaiden title plate (gold plate + 4 kanji medallions), with the bottom menu-text
row already cropped out by hand. Real game screenshot, used here for an
internal/coursework project, not for redistribution.

## Render + transition pipeline

Sequencing matters because chafa and ncurses can't both own the terminal at
once:

1. Before `initscr()`: shell out to `chafa src/tetrisu/assets/splash.png`. No
   `--size` flag — chafa auto-detects terminal size from the tty when stdout
   isn't redirected.
2. Wait for a keypress using raw termios (`tcgetattr`/`tcsetattr` to flip to
   cbreak + no-echo, read one byte, restore). Not ncurses yet — starting curses
   here would clobber chafa's raw output. Any key advances; no timeout
   fallback (dropped for v1 — `select()`-on-stdin timeout adds complexity with
   no real testing value here).
3. `initscr()`, `cbreak()`, `noecho()`, `keypad(stdscr, TRUE)` — ncurses now
   owns the screen for the menu and everything after it.

## Input handling

Main menu, once in ncurses mode:
- `KEY_UP` / `KEY_DOWN` — move selection, wraps at item 0 and item 3
- `KEY_ENTER` / `\n` — print stub line for the selected item, stay on menu
- `q` — transition to `APP_QUIT`

`SIGINT`: handler only sets `volatile sig_atomic_t quit_requested = 1`
(signal handlers must not call ncurses — not signal-safe). Main loop checks
the flag every iteration; on seeing it, breaks out, calls `endwin()`, then
exits. Same teardown path as pressing `q`.

## Code layout

Pure logic separated from I/O, same principle as `libtetrisbrain` (no I/O, no
side effects in the logic layer):

```
src/tetrisu/
    Makefile                 # make -C src/tetrisu [run|test|clean|fclean|re]
    assets/splash.png
    src/
        main.c                # event loop: poll stdin, dispatch on app_state_t
        app_state.c/.h         # pure: app_handle_key(), menu_move_selection(),
                                # menu_stub_text() — no ncurses calls, fully
                                # unit-testable
        render_splash.c        # I/O: chafa subprocess call, raw-termios keywait
        render_menu.c           # I/O: ncurses draw + keypad input read
    tests/
        test_menu_nav.c         # unit: selection wrap at 0/3
        test_app_transitions.c  # unit: state transitions, stub text per item
        test_integration.c      # integration: forkpty + scripted keystrokes
                                  # against the real built binary
    scripts/run_tests.sh
    obj/
    bin/tetrisu
```

## Build

Self-contained Makefile, same pattern as `lib/libtetrisbrain`:

```bash
make -C src/tetrisu          # builds src/tetrisu/bin/tetrisu
make -C src/tetrisu run      # build (if needed) + exec
make -C src/tetrisu test     # run unit + integration tests
make -C src/tetrisu clean    # rm objects
make -C src/tetrisu fclean   # clean + rm binary
make -C src/tetrisu re       # fclean + rebuild
```

Links: `-lncurses` only. No `-lssl`/`-lcrypto`/`libtetrissh`/`libhtttp` — this
slice never touches the network.

## Testing

**Unit tests** (no terminal needed, pure functions):
- `test_menu_nav.c` — feed `KEY_UP`/`KEY_DOWN` sequences, assert index wraps
  correctly at item 0 and item 3.
- `test_app_transitions.c` — assert `APP_SPLASH` + any key → `APP_MAIN_MENU`;
  assert `APP_MAIN_MENU` + `'q'` → `APP_QUIT`; assert Enter on each menu item
  returns the correct stub string.

**Integration test** — ncurses requires a real tty (`isatty()` check), so it
can't run headless. Use `forkpty()` (POSIX; header is `<pty.h>` on Linux,
`<util.h>` on macOS/BSD — guarded with `#ifdef __APPLE__`) to spawn the real
built binary attached to a pseudo-terminal, write a scripted keystroke
sequence to the pty master (advance splash, arrow down twice, Enter, `q`),
read back captured output, and assert: exit code 0, and captured bytes
contain the expected stub-text substring (e.g. `"not wired up yet"`). Catches
real wiring bugs (chafa missing, ncurses init failure, module miswiring) that
pure unit tests can't.

No automated test attempts to assert on rendered pixel/character output of
the splash image or menu box-drawing — not worth the effort for this slice;
that's covered by manually running `make -C src/tetrisu run`.

## Out of scope (explicitly deferred)

- Options screen, Marketplace screen, Lobby screen — menu items are stubs only
- Any networking (`libtetrissh`, `libhtttp`, connecting to `tetrisd`)
- Game board rendering, ability animations, chat panel
- State-stack navigation (push/pop for ESC-pause-menu reuse) — not needed until
  the in-game pause menu exists
