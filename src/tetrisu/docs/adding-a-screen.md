# Adding a Screen

How to build a new `tetrisu` screen that stays responsive on every renderer
tier. Written from what Settings and the Leaderboard cost us to get right.

> Every rule below exists because breaking it produced a visible bug. The
> failures are recorded next to the rules so the reasoning survives.

---

## Table of Contents

- [The one rule](#the-one-rule)
- [Renderer tiers](#renderer-tiers)
- [Anatomy of a screen](#anatomy-of-a-screen)
- [Step 1 — Split the screen into layers](#step-1--split-the-screen-into-layers)
- [Step 2 — Lay the regions out](#step-2--lay-the-regions-out)
- [Step 3 — Give every layer a signature](#step-3--give-every-layer-a-signature)
- [Step 4 — Write planes in place](#step-4--write-planes-in-place)
- [Step 5 — Coalesce held keys](#step-5--coalesce-held-keys)
- [Step 6 — Skip the render when nothing moved](#step-6--skip-the-render-when-nothing-moved)
- [The five bugs](#the-five-bugs)
- [Checklist](#checklist)
- [Verifying it](#verifying-it)

---

## The one rule

**A keystroke must repaint one small region, never the screen.**

A full-screen bitmap repaint costs 30–80 ms on a stationary protocol. The input
loop is serial — one key in, one repaint out — so that repaint cost *is* the
input rate ceiling. Repaint the whole screen per key and the screen caps at
12–15 inputs per second, which reads as laggy, sticky input rather than as slow
drawing.

Measured on Sixel at 160x45 before and after applying this document:

| Screen | Whole-screen repaint | Region repaint |
|---|---|---|
| Settings | 68.8 ms/key (14.5 Hz) | 7.9 ms/key |
| Leaderboard | 82.9 ms/key (12.1 Hz) | 4.7 ms/key |

---

## Renderer tiers

Three tiers, resolved by `tetrisu_pixel_policy_for()` in `src/pixel_policy.c`.
The tier decides what a plane is allowed to do, not whether you get bitmaps:

| Tier | Terminals | Planes may |
|---|---|---|
| `TETRISU_PIXELS_MOVABLE` | Kitty protocol | move, restack, overlap freely |
| `TETRISU_PIXELS_STATIONARY` | Sixel, Linux framebuffer | be written in place only |
| `TETRISU_PIXELS_NONE` | everything else | no bitmaps; cell fallback |

Predicates: `render_pixels_available()` (any bitmap tier),
`render_pixel_planes_reliable()` (movable only), `render_compatibility_mode()`
(cell fallback).

**Write one code path for both bitmap tiers.** Settings used to branch — a
region path for movable, a flattened full-screen path for stationary — and the
stationary branch was the slow one. The constraints below satisfy both tiers at
once, so the branch is unnecessary.

Force a tier for testing with `TETRISU_RENDERER=cell|stationary|pixel`.

---

## Anatomy of a screen

A screen owns, from bottom to top:

```
ctx->bg_plane          backdrop artwork, replaced only on entry and resize
ctx->screen_plane      full-screen static layer: everything focus cannot change
region planes          one small plane per focus-sensitive area
notification planes    raised by render_notification_raise()
```

Reference implementations:

| Concern | Settings | Leaderboard |
|---|---|---|
| Layout | `src/settings_layout.c` | `src/leaderboard_presentation.c` |
| Bitmap render | `src/render_settings_font.c` | `src/render_leaderboard_font.c` |
| Cell fallback | `src/render_settings.c` | `src/render_leaderboard.c` |
| Input state | `src/settings_screen.c` | `src/leaderboard_screen.c` |
| Loop | `run_settings_screen()` in `src/main.c` | `run_leaderboard_screen()` |

`src/render_solo.c` is the oldest example of the pattern and the one the other
two were retrofitted from.

---

## Step 1 — Split the screen into layers

Sort every drawn element by *what changes it*:

- **Static layer** — changes only when the provider view model changes.
  Titles, portraits, profile text, rank rows, stat cards. Composed into one
  full-screen RGBA canvas, blitted to `ctx->screen_plane`, and **kept**: the
  cached canvas is the base every region copies from.
- **Region layers** — change when focus, selection, or a local control moves.
  Buttons, inventory panels, volume readouts, detail cards. One plane each.

Getting this wrong is not a correctness bug, it is a performance bug: anything
put in the static layer that focus can change forces a full-screen recompose per
keystroke, which is the thing this document exists to prevent.

Compose regions onto a **copy of the cached static canvas**
(`region_canvas()` in `render_settings_font.c`), not onto a transparent buffer.
Sixel cannot write transparency over existing content, but it can overwrite it,
so the cropped result must be opaque wherever the frame is.

---

## Step 2 — Lay the regions out

Region rectangles live in the layout struct (`settings_layout_t`,
`leaderboard_pixel_layout_t`), computed once from terminal geometry. Do not
hardcode them inside the compose functions — the renderer and the tests must
read the same rectangles or the tests guard a copy that can drift.

**Region planes must never overlap.** On a stationary protocol, re-emitting one
bitmap plane blanks whatever it overlaps. This is not a rule about drawing
order; two overlapping sprixels blank each other unpredictably.

Two rectangles that look disjoint in reference pixels can still collide, because
every plane is expanded out to whole cells before it is created:

```c
crop_x = region->x / cell_px_x * cell_px_x;                          /* down */
crop_right = ((region->x + region->width + cell_px_x - 1)
        / cell_px_x) * cell_px_x;                                    /* up   */
```

Two regions separated by less than one cell land on the same cell. Cell size
varies with font and terminal, so check across geometries rather than trusting
the one terminal you happen to be testing in —
`test_region_planes_never_overlap` in `tests/test_settings_screen.c` sweeps
cols 44–400, rows 20–120, cell 2–40 px and asserts pairwise disjointness.

When two regions genuinely occupy the same space, make them **mutually
exclusive** rather than stacked: keep at most one alive and destroy the other.
The powers card and the signed-in volume readout do this.

---

## Step 3 — Give every layer a signature

Each layer recomposes only when its own signature moves:

```c
signature = static_signature(view, layout);
if (ctx->screen_plane != NULL && ctx->cached_pixels != NULL
    && signature == ctx->static_signature)
    return (0);                 /* reuse */
```

Two rules that are easy to get wrong:

**Hash fields, never structs.** Hashing a struct wholesale folds in its padding
bytes, which are never written and make the signature unstable — the layer then
recomposes on keystrokes that changed nothing.

**Seed every region signature with the static signature.** Rewriting the
full-screen plane re-emits the bitmap *under* the region planes, and on a
stationary protocol that overwrites the cells they occupy. A region whose
signature did not also move is never re-emitted, so it stays blank. Seeding
makes a static rebuild implicitly dirty every region.

---

## Step 4 — Write planes in place

Create a plane once; afterwards blit into it:

```c
if (render_plane_geometry_matches(*slot, y, x, rows, cols))
    return (render_plane_blit_rgba(ctx, *slot, origin, w, h, stride));
plane = ncplane_create(ctx->std, &options);
...
if (*slot != NULL)
    ncplane_destroy(*slot);
*slot = plane;
```

Both helpers are in `src/render_background.c`.

**Destroying a sprixel plane damages every cell it covered**, forcing the bitmap
underneath to be retransmitted. Destroy-and-recreate per frame is what made the
Leaderboard cost 21 ms per keystroke on top of its compose. Only a geometry
change — a resize — should replace a plane.

Blit straight out of the composed canvas using its row stride:

```c
ncvisual_from_rgba(origin, crop_height, canvas_width * 4, crop_width);
```

The stride argument windows the canvas, so no compact intermediate buffer and no
copy is needed.

**Do not restack planes every frame.** `ncplane_move_top()` damages what it
touches. Creation order already puts regions above the static plane; only
restack when the static plane was replaced.

---

## Step 5 — Coalesce held keys

Terminal auto-repeat outruns a bitmap repaint, so held keys queue and lag
compounds. Drain the queue and fold repeats into one repaint —
`SETTINGS_INPUT_BATCH_MAX` / `LEADERBOARD_INPUT_BATCH_MAX` in `src/main.c`.

Only fold **identical movement keys**:

```c
bool	screen_navigation_keys_coalesce(uint32_t active_key, uint32_t queued_key)
{
    if (active_key != queued_key)
        return (false);
    return (active_key == NCKEY_LEFT || active_key == NCKEY_RIGHT || ...);
}
```

Folding a different key would act on it before its target state was drawn;
folding opposite arrows would swallow a real focus change. Stash the first
differing key as `pending` and handle it next iteration.

Also drop keyboard release events (`NCTYPE_RELEASE`) so one tap moves once, and
call `discard_queued_input()` on actions that leave the screen so repeats cannot
leak into the next one.

---

## Step 6 — Skip the render when nothing moved

Track whether anything was actually rewritten and skip the present if not:

```c
if (changed == 0 && ctx->notifications.count == 0)
    return (true);
return (notcurses_render(ctx->nc) == 0);
```

The notification guard matters: on tiers where
`tetrisu_pixel_policy_notification_needs_reemit()` is true,
`render_notification_raise()` re-emits notification planes and that needs
presenting even when no region changed.

---

## The five bugs

Every one of these shipped at least once. They are all invisible to unit tests
and to the terminal you developed in.

| Symptom | Cause | Fix |
|---|---|---|
| Input capped at ~12–15/s | whole screen recomposed per keystroke | split into static + region layers |
| Held key lags further and further behind | no input coalescing | drain identical movement keys |
| Buttons vanish after a data change | static rebuild re-emitted the bitmap under unchanged region planes | seed region signatures with the static one |
| Regions blank each other when a card opens | two region planes overlapping on a stationary tier | make overlapping regions mutually exclusive |
| Extra ~21 ms per keystroke | plane destroyed and recreated per frame | reuse the plane, blit in place |

---

## Checklist

Before calling a screen done:

- [ ] Static and region layers separated; nothing focus-sensitive in the static layer
- [ ] Region rectangles in the layout struct, read by both renderer and tests
- [ ] Region planes pairwise disjoint across a geometry sweep, or mutually exclusive
- [ ] Every signature hashes fields, not structs
- [ ] Every region signature seeded with the static signature
- [ ] Planes created once and written in place; replaced only on geometry change
- [ ] No per-frame `ncplane_move_top()`
- [ ] Identical movement keys coalesced; releases dropped; queue discarded on exit
- [ ] `notcurses_render()` skipped when nothing changed and no notification is up
- [ ] Cell fallback path works under `TETRISU_RENDERER=cell`
- [ ] Teardown frees planes, cached canvases, and resets signatures

---

## Verifying it

Unit tests cover layout and input policy only — the logic archive excludes
`render_*.c` because it pulls in notcurses, so **the render path has no
automated coverage.** Everything above must be checked by running the app.

Run each screen on all three tiers and confirm it draws correctly:

```bash
./bin/tetrisu                            # auto-detected tier
TETRISU_RENDERER=stationary ./bin/tetrisu
TETRISU_RENDERER=cell ./bin/tetrisu
```

Exercise the transitions that rebuild the static layer, not just focus moves —
equipping an item, refreshing data, opening and closing a detail card. Those are
where the blanking bugs live, and holding an arrow key will not find them.

`TETRISU_UI_PREVIEW=1` enables a fixture login (`p` on the login screen once
focus reaches the primary button), which reaches signed-in screens without a
server.

Two limits worth knowing on this hardware:

- **Valgrind is unusable** — it SIGILLs host-wide. Use `-fsanitize=address`.
- **ASan cannot leak-check tetrisu** — it dies in `UnmapOrDie` during notcurses
  teardown. Runtime errors are still reported, since those surface immediately.
  For leaks, watch RSS across a few hundred keystrokes instead: a leaked
  full-screen canvas is ~6 MB and shows up immediately.
