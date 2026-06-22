# notcurses — reference for working on `tetrisu`

Practical notes for an agent (or teammate) touching `src/tetrisu/`. This is
not a full API dump — it's the subset actually used in this codebase, plus
the gotchas that cost real debugging time when this was integrated. For
anything beyond this, go to the primary sources:

- Upstream docs: <https://notcurses.com/>
- Real header (ground truth for exact signatures): `/opt/homebrew/Cellar/notcurses/<version>/include/notcurses/notcurses.h` (or wherever `pkg-config --cflags notcurses` points on your machine)
- `man 3 notcurses_init`, `man 3 ncvisual_blit`, `man 3 ncplane_create` once installed

## Why notcurses instead of plain ncurses

`tetrisu`'s home screen blits a real image as a background and draws a menu
+ selection indicator on top of it, in place, without disturbing the image.
Plain ncurses can't do this: it owns the whole screen through its own
internal buffer and clears it on the first `refresh()`, so anything rendered
outside curses (e.g. piping an image through an external tool like `chafa`)
gets wiped the moment curses starts. notcurses solves this natively with its
**plane** model — see below.

Tradeoff accepted for this: notcurses pulls in `ffmpeg` as a hard dependency
(for image/video decoding), versus zero extra deps for plain ncurses. Build
needs `pkg-config notcurses` for cflags/libs (see `src/tetrisu/Makefile`).

## Core concepts

- **`struct notcurses *nc`** — the whole rendering context. One per program.
  Created with `notcurses_core_init()`, torn down with `notcurses_stop()`.
- **`struct ncplane`** — a rectangular region of cells (character + color).
  Every notcurses program has a **standard plane** (`notcurses_stdplane()`),
  full-screen, created automatically. You can create more planes
  (`ncplane_create()`) bound to any existing plane; new planes start at the
  top of the z-order, so anything drawn on a later-created plane renders
  over earlier ones occupying the same cells.
- **Nothing reaches the terminal until you call `notcurses_render()`.** All
  the `ncplane_*`/`ncvisual_*` calls just mutate in-memory state.

## The pattern this codebase uses

1. `notcurses_core_init(&opts, NULL)` → get `nc`.
2. `notcurses_stdplane(nc)` → get the full-screen base plane (`std`).
3. Load + blit a background image directly onto `std`:
   ```c
   struct ncvisual *ncv = ncvisual_from_file(path);
   struct ncvisual_options vopts = {0};
   vopts.n = std;            // blit onto std itself, not a new plane
   vopts.scaling = NCSCALE_SCALE;  // fit, preserve aspect ratio
   ncvisual_blit(nc, ncv, &vopts);
   ncvisual_destroy(ncv);    // safe to destroy after blit — pixels are baked into the plane
   ```
4. Create small child planes for UI elements that sit on top of the image:
   ```c
   struct ncplane_options popts = {0};
   popts.y = row; popts.x = col; popts.rows = 1; popts.cols = 20;
   struct ncplane *overlay = ncplane_create(std, &popts);
   ncplane_set_fg_rgb8(overlay, r, g, b);   // 24-bit truecolor
   ncplane_putstr_yx(overlay, 0, 0, "text");
   ```
5. Reposition a small plane (e.g. a selection indicator) with
   `ncplane_move_yx(plane, new_y, new_x)` — cheap, no re-blit of the
   background needed, since the background plane is untouched.
6. `notcurses_render(nc)` after any batch of changes to actually draw.
7. Read input with `notcurses_get(nc, NULL /* block forever */, &ncinput)`.
   Returns a Unicode codepoint, an `NCKEY_*` constant (`NCKEY_UP`,
   `NCKEY_DOWN`, `NCKEY_ENTER`, ...), or `(uint32_t)-1` on error — always
   check for that error value before doing anything else with the key. A
   `timespec*` instead of `NULL` makes it a bounded wait instead of blocking
   forever.
8. `notcurses_stop(nc)` to tear down and restore the terminal.

`ncplane_dim_yx(plane, &rows, &cols)` gives you a plane's size in cells —
use it to position things proportionally (e.g. "72% down, 45% across")
rather than hardcoding coordinates, so layout holds at any terminal size.

## Signal handling — you probably don't need your own

By default `notcurses_core_init()` installs handlers for
`SIGINT`/`SIGILL`/`SIGSEGV`/`SIGABRT`/`SIGTERM`/`SIGQUIT` that restore the
terminal and then chain to whatever handler was previously installed. In
practice this means Ctrl+C already exits cleanly (screen restored, then
default disposition terminates the process) with no custom `signal()` call
needed. Only pass `NCOPTION_NO_QUIT_SIGHANDLERS` in `notcurses_options.flags`
if you specifically need to intercept that signal yourself instead.

## The one real gotcha: it cannot be driven through a scripted pty

At startup, notcurses queries the terminal for capabilities it can't get
from `terminfo` alone — palette colors (`OSC 4;N;?` for many indices),
foreground/background color (`OSC 10;?`/`OSC 11;?`), pixel dimensions
(`CSI 14t`/`CSI 18t`), Kitty/Sixel graphics support — and **blocks
indefinitely waiting for replies**. A real terminal emulator answers these
instantly, so interactive use is unaffected. But a test harness using
`forkpty()` to drive the binary non-interactively (no real terminal on the
other end) will hang forever at `notcurses_core_init()`, regardless of
`TERM` value — confirmed empirically (`TERM=xterm`, `TERM=vt100` both hang
indefinitely) and confirmed against the upstream docs: there is no
documented `NCOPTION_*` flag or environment variable to skip this probe.

Consequence: **`tetrisu` has no automated integration test for its
notcurses-based rendering.** Verification is manual (`make -C src/tetrisu
run`). Pure logic (state transitions, menu selection math) still gets real
unit tests — see `src/tetrisu/app_state.c` / `tests/test_app_state.c` — only
the rendering layer itself is untestable this way. If this ever needs to
change, the real fix is a full terminal-emulator stub that answers these
specific queries (not just a dumb byte-forwarding pty), which is a
significant undertaking on its own.

## Build

```bash
pkg-config --cflags notcurses   # compiler include flags
pkg-config --libs notcurses     # linker flags (pulls in glib, ffmpeg, etc.)
```

See `src/tetrisu/Makefile` for how these get folded into `FLAGS`/`LDLIBS`.
