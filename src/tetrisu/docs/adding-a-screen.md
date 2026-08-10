# Adding a Screen

How to build a new `tetrisu` screen that stays responsive on every renderer
tier. Written from what Settings, the Leaderboard and the Marketplace cost us
to get right.

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
- [Screens that share a plane set](#screens-that-share-a-plane-set)
- [Backdrops and screen transitions](#backdrops-and-screen-transitions)
- [Values that move without input](#values-that-move-without-input)
- [The bugs](#the-bugs)
- [Text metrics](#text-metrics)
- [Checklist](#checklist)
- [Verifying it](#verifying-it)
- [Driving it from a script](#driving-it-from-a-script)

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

There is a companion rule, learned later and the harder of the two:

**An action must not repaint the screen either.** A keystroke that repaints
everything is slow; an *action* that repaints everything is broken, because on
a stationary protocol the full-screen bitmap lands on top of every region plane
and blanks them. Buying something in the Marketplace did exactly that. Step 1
and Step 3 are where this is enforced.

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
ctx->screen_plane      full-screen static layer: what nothing on this screen changes
region planes          one small plane per focus- or action-sensitive area
notification planes    raised by render_notification_raise()
```

Reference implementations:

| Concern | Settings | Leaderboard | Marketplace | Multiplayer (x4) |
|---|---|---|---|---|
| Layout | `src/settings_layout.c` | `src/leaderboard_presentation.c` | `src/marketplace_layout.c` | `src/multiplayer_layout.c` |
| Bitmap render | `src/render_settings_font.c` | `src/render_leaderboard_font.c` | `src/render_marketplace_font.c` | `src/render_multiplayer_font.c` |
| Cell fallback | `src/render_settings.c` | `src/render_leaderboard.c` | `src/render_marketplace.c` | `src/render_multiplayer.c` |
| Input state | `src/settings_screen.c` | `src/leaderboard_screen.c` | `src/marketplace_screen.c` | `src/{multiplayer,lobby,waiting_room}_screen.c` |
| Loop | `run_settings_screen()` in `src/main.c` | `run_leaderboard_screen()` | `run_marketplace_screen()` | `run_lobby_screen()` and three siblings |

The multiplayer group is the one worth reading if you are adding **several**
related screens rather than one: four surfaces share a backdrop, a layout
function, a static canvas and one set of region planes.

The Marketplace is the one screen whose backdrop carries no authored frames:
it draws every plate and border itself against the quiet centre of a shop
scene. That makes its region rectangles free to be chosen rather than matched
to artwork, which is the easier way round if you are authoring both.

`src/render_solo.c` is the oldest example of the pattern and the one the other
two were retrofitted from.

---

## Step 1 — Split the screen into layers

Sort every drawn element by *what changes it*:

- **Static layer** — nothing the user can do on this screen changes it.
  Titles, backdrops, panel plates, panel headings, and values settled before
  the screen opened. Composed into one full-screen RGBA canvas, blitted to
  `ctx->screen_plane`, and **kept**: the cached canvas is the base every region
  copies from.
- **Region layers** — change when focus, selection, a local control, *or an
  action* moves. Buttons, inventory panels, volume readouts, detail cards,
  wallet balances. One plane each.

The dividing line is "can anything the user does move this?", not "does focus
move this". Focus is the obvious case and the cheap one to get wrong:

> Anything put in the static layer that **focus** can change forces a
> full-screen recompose per keystroke, which is the performance problem this
> document exists to prevent.

But an **action** — buying, equipping, refreshing — is the expensive one to get
wrong, because it is a correctness problem rather than a slow one:

> Anything put in the static layer that an **action** can change makes that
> action re-emit a full-screen bitmap. On a stationary protocol that lands over
> every region plane and blanks them until the next keystroke.

The Marketplace learned this the hard way. Its wallet balance started in the
static layer, which seemed safe because no keystroke moves it — but buying
does. The wallet now has a region plane of its own, while best score and rank
stay static because nothing on that screen can move them. The test is not "is
this value static-looking", it is "can the user change it without leaving".

Compose regions onto a **copy of the cached static canvas**
(`region_canvas()` in `render_settings_font.c`), not onto a transparent buffer.
Sixel cannot write transparency over existing content, but it can overwrite it,
so the cropped result must be opaque wherever the frame is.

---

## Step 2 — Lay the regions out

Region rectangles live in the layout struct (`t_settings_layout`,
`t_leaderboard_pixel_layout`), computed once from terminal geometry. Do not
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
`test_region_planes_never_overlap` in `tests/test_settings_screen.c` and
`tests/test_marketplace_screen.c` sweep cols 44–400, rows 20–120, cell 2–40 px
and assert pairwise disjointness.

**How wide does the gap have to be?** Wide enough that the rounding cannot eat
it at the coarsest supported geometry. A gap of `g` reference units becomes
`g × rows / REFERENCE_HEIGHT` cells vertically — note the cell *size* cancels
out, so only the terminal's row count matters:

```
vertical gap    ≥ REFERENCE_HEIGHT / min_rows    = 1086 / 20 ≈ 55 units
horizontal gap  ≥ REFERENCE_WIDTH  / min_cols    = 1448 / 44 ≈ 33 units
```

Round up generously — truncation in `map_rect()` costs another unit or two.
The Marketplace uses 60 vertical and 96 horizontal. Do not derive these by
eye: write the layout, then run the sweep and let it fail.

**Notification planes are planes too.** `render_notification_raise()` puts them
in the top-right corner, and they follow the same non-overlap rule as anything
else — but they are not in your layout struct, so the sweep cannot catch them.
Keep every region clear of that corner. The Marketplace's wallet region is the
leftmost stat card *only* for this reason: a region spanning the whole top
strip reached under the notification, and raising or dropping the card there
blanked the screen.

When two regions genuinely occupy the same space, make them **mutually
exclusive** rather than stacked: keep at most one alive and destroy the other.
The powers card and the signed-in volume readout do this.

**When the gap cannot be made wide enough, stop making two planes.** The mode
picker's control legend sits 28 units under its cards — half the vertical floor,
and there is nowhere to move it to, because the panel it lives in is only so
tall. Two planes that round onto the same cell blank each other, so the legend
is drawn *into* the card region instead. It costs a few extra glyphs per
keystroke and removes the failure entirely. Ask whether two rectangles need to
be separate planes before asking how to separate them.

**The layout struct holds more than planes.** It also holds plates, control
rows, and anything else the compositors need in reference space, and some of
those deliberately sit inside a region. A sweep that iterates the whole struct
will flag those as overlaps, so the sweep needs an explicit per-screen list of
the rectangles that actually become planes:

```c
static void	region_list(const t_mp_layout *layout, t_app_screen screen,
	const t_mp_rect **regions, int *count)
```

Keep that list next to the sweep, not next to the renderer, and add to it when
you add a plane. A rectangle missing from it is a plane nothing is checking.

---

## Step 3 — Give every layer a signature

Each layer recomposes only when its own signature moves:

```c
signature = static_signature(view, layout);
if (ctx->screen_plane != NULL && ctx->cached_pixels != NULL
    && signature == ctx->static_signature)
    return (0);                 /* reuse */
```

Three rules that are easy to get wrong:

**Hash fields, never structs.** Hashing a struct wholesale folds in its padding
bytes, which are never written and make the signature unstable — the layer then
recomposes on keystrokes that changed nothing.

**Seed every region signature with the static signature.** Rewriting the
full-screen plane re-emits the bitmap *under* the region planes, and on a
stationary protocol that overwrites the cells they occupy. A region whose
signature did not also move is never re-emitted, so it stays blank. Seeding
makes a static rebuild implicitly dirty every region.

**Seed them with the model too, once the static signature stops carrying it.**
This is the trap on the other side of Step 1. Moving the wallet out of the
static layer means the static signature no longer hashes ownership or prices —
so a purchase no longer dirties anything through the seed, and the shelves keep
showing the old price beside the new balance. Hash the model fields the regions
actually draw from into one value and seed from that as well:

```c
base = model_signature(&view->data.marketplace,
        ctx->marketplace_static_signature);
/* every region seeds from base, not from the static signature alone */
```

The rule generalises: **a region's seed must cover everything its content
depends on that it does not hash directly.** When you take a field out of the
static layer, put it into the region seed in the same edit.

**And nothing more than that.** The rule has a second edge, and one model hash
shared by every region on a screen walks straight off it: correct, but every
region repaints whenever any of them changes. The waiting room started that way,
so posting a chat message repainted the seat list, and readying up repainted the
transcript. Split the model hash along the same lines the regions are split:

```c
base      = room_players_signature(room, ctx->mp_static_signature);
/* slots and status seed from base; chat seeds from the transcript instead */
signature = room_chat_signature(room, ctx->mp_static_signature);
```

When a region draws a line that *quotes* a field it otherwise ignores, hash the
**rendered text** rather than the field. The lobby's status line names the room
id only in its unknown-id message; hashing the id itself repainted that plane on
every keystroke in the join field, and hashing the formatted line does not:

```c
lobby_feedback_text(state, line, sizeof(line));
signature = mp_hash_text(line, ctx->mp_static_signature);
```

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

**Typed characters never coalesce.** Two identical letters in a room id are two
edits, not a repeat of one, so a screen with a text field must keep its
coalesce predicate to movement keys only. A screen with nothing worth repeating
at all — the waiting room has no cursor to hold down — should return `false`
unconditionally and say so, rather than leaving a predicate that looks like it
does something.

---

## Screens with two input modes

A text field on a screen that also has single-letter commands is not a focus
state, it is a **mode**. Trying to make one key mean both is the trap: `c` is
"create room" in the lobby's table and the letter `c` in its join field, and
there is no clever rule that makes one keypress mean both.

Split the key handler in two and pick between them at the top:

```c
if (state->section == LOBBY_SECTION_JOIN)
    return (handle_join_key(state, key));
return (handle_rooms_key(state, key));
```

Three rules that make the mode obvious to the person typing:

- **Inside the field, every printable key is text.** The commands are
  deliberately unreachable, not overridden. A chat message containing "s" must
  not start the match.
- **Escape leaves the field, not the screen.** It is the only key whose meaning
  changes, and it changes in the direction users expect: one step out, not two.
- **Draw the mode.** A caret while focused and none while not, a coloured field
  border, a composer line that reads `[C] type a message` when closed and
  `> text_` when open. Without that the two modes look identical and the same
  key appears to do different things at random.

Accept printable ASCII only (`0x20`–`0x7e`) and drop everything else rather than
storing a placeholder. Arrow keys, function keys and multi-byte input all arrive
as codepoints, and a `?` written into a room id is a room id the server will
reject for a reason the user cannot see.

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

## Screens that share a plane set

Four surfaces that belong to one flow will want the same backdrop, the same
layout function and the same regions. Sharing is the right call - the cost is
one rule.

**Switching screens must destroy every region plane, not just the ones the next
screen will not use.** The plane slots live on the render context, so a plane
the previous screen created stays on the terminal until something destroys it.
Leaving one behind strands a region of the old screen on top of the new one, and
because its geometry still matches, the reuse path in Step 4 happily writes the
*new* screen's content into the *old* screen's rectangle.

Make the screen part of the static cache key, and tear down on the way in:

```c
screen_changed = ctx->mp_screen != view->screen;
if (rebuild_background || screen_changed)
{
    render_screen_destroy(ctx);
    forget_regions(ctx);          /* destroys all seven, clears all signatures */
    ctx->mp_screen = view->screen;
}
```

The backdrop needs the same treatment when the screens do not share one: cache
the source path alongside the geometry, or a screen that changes artwork without
changing size will reuse the previous screen's backdrop.

A full rebuild on every screen change is correct and cheap here, because a
screen change is not a keystroke. The rules in Step 1 are about what happens
*within* a screen.

---

## Backdrops and screen transitions

Everything above is about what happens *within* a screen. Entering and leaving
one has its own rule, and it is the one that cost the most:

**Never render a frame in which a live sprixel is covered.**

A covered sprixel is torn down and transmitted again when it resurfaces, and it
is re-rasterised alongside whatever covers it in the meantime. This is not a
stationary-tier quirk — it bites hardest on Kitty, where the backdrops are
full-screen `NCBLIT_PIXEL` bitmaps. At 210x71 on a Retina cell one of those is
~28 MB of RGBA, ~37 MB once base64'd, and a macOS pty drains it at around
3 MB/s. Linux hides this entirely behind bigger pty buffers and a faster
reader, so *"it is fine on Linux"* says nothing at all about whether you have
this bug.

Two ways to break the rule, both of which shipped:

- **Leaving the old backdrop stacked underneath the new one.** Every revisit to
  a screen paid for its own bitmap again. Marketplace: 15.4 s cold, 9.2 s warm.
- **Rendering before moving the old one out of the way.** Worse, because it
  costs the full bitmap on a path that looks like it is only drawing the new
  screen. `replace_visual_scaled()` stacked the incoming plane above the
  outgoing one, rendered, and parked afterwards. On the first sign-in the
  outgoing plane was the full-screen login artwork: 31 s, in one call.

The fix is one shape: an outgoing backdrop leaves the rendered area **before**
the frame is drawn — parked off-screen if something still wants it, destroyed
outright if not, because a plane freed *after* the render has already been
transmitted by it.

```c
if (old_plane != NULL && !backdrop_is_cached(ctx, old_plane))
{
    ncplane_destroy(old_plane);
    ctx->bg_plane = NULL;
}
else
    backdrop_park(ctx, old_plane);       /* ncplane_move_yx off the top */
if (notcurses_render(ctx->nc) != 0)
    ...
```

With that, a backdrop swap is 46-56 ms cold and 1-3 ms warm at 210x71.

**Parking is a movable-tier optimisation, and it is gated as one.** The whole
value of retaining a backdrop is being able to move it back later, which is
exactly what `TETRISU_PIXELS_STATIONARY` forbids. `backdrop_remember()` refuses
on any tier where `render_pixel_planes_reliable()` is false, which leaves
`backdrop_restack()` and `backdrop_park()` inert — they are both predicated on
cache membership — so sixel, the framebuffer and the cell tier keep the plain
destroy-and-rebuild behaviour that predates the cache. **If you add a plane
that survives a screen change, gate it the same way.** A cache that quietly
moves sprixels on a tier that cannot move them is the failure this rule exists
to prevent, and it will not show up on your Kitty terminal.

**Do not reach for a cell blitter to make a backdrop cheap.** It works — cell
blitting the pixel backdrops took a swap from 15,386 ms to 2 ms — and it is not
acceptable: cell blitters cap at two colours per cell, and A/B screenshots
showed Settings lose its gilded panel frames, its plate icons and its circular
buttons. Home's backdrop is `TETRISU_BLIT_DENSE` because it was authored to
survive it; the others were not. Fix the transmission, not the fidelity.

`TETRISU_BLIT_DENSE` is `NCBLIT_4x2` (octants) and `NCBLIT_3x2` (sextants) on a
notcurses older than 3.0.12, which is the release that added the octant
blitter — the enumerator does not exist before it, so naming it directly is a
compile error rather than a runtime fallback on every distribution package
still shipping 3.0.9.

**Discard queued input on the way *in*, not just on the way out.** Step 5 says
to call `discard_queued_input()` on actions that leave a screen. The transition
*into* Home after signing in needs it just as much and did not have it: keys
typed at the form arrived at the menu that replaced it, so mashing Enter while
sign-in was busy opened whichever Home item happened to be selected. Any
transition a user waits on is a transition they have typed into.

**An in-progress message must be set before the paint, not inside the call.**
`auth_form_validate()` is what sets `"SIGNING IN..."`, and it used to run inside
`auth_form_submit()` — after the only render preceding the provider call. The
status was written to a form nobody drew again until the call had already
returned, so it never appeared. Validate, paint, *then* block.

---

## Values that move without input

A countdown, a timer, a server push: anything that changes while the user is not
typing. Two consequences.

**It needs a wait with a deadline, not a blocking read.**
`render_wait_input_timeout()` returns `0` when its deadline elapses - the same
value `notcurses_get_nblock()` returns for an empty queue, so a loop that drains
input and a loop that times out test the same thing. Track the deadline against
`ui_notification_now_ms()` rather than re-arming a fixed sleep, or every
keystroke pushes the next tick back:

```c
now     = ui_notification_now_ms();
wait_ms = deadline > now ? (int)(deadline - now) : 0;
key     = render_wait_input_timeout(ctx, &input, wait_ms);
if (key == 0)
{
    deadline += WAITING_ROOM_COUNTDOWN_STEP_MS;
    ...
}
```

**It must be alone on its region.** A ticking value repaints once a second
whether or not anyone is at the keyboard, so anything sharing its plane pays
that cost forever. The waiting room's countdown sits on the status region with
nothing but the status line; the seats and the transcript are untouched between
ticks.

---

## Drawing over authored art

The backdrops are scenes, not frames. They are authored with a quiet middle and
a decorated border, and the border is where legibility goes to die.

**Panels are fine anywhere; loose text is not.** A panel draws its own opaque
plate, so it can sit over anything. A title row, a legend, or a control strip
outside every panel has nothing behind it, and lands straight on the skull
frieze or the tetromino rubble. Both the lobby and the waiting room draw a
plain opaque band behind those strips:

```c
static void	draw_band(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, int ref_y_value, int ref_height)
{
	fill_ref_rect(pixels, width, height, layout, MP_BAND_X, ref_y_value,
		MULTIPLAYER_REFERENCE_WIDTH - 2 * MP_BAND_X, ref_height, g_mp_plate,
		238u);
}
```

Bands belong in the static layer and are drawn **first**, so the panels' own
plates land on top of them. Nothing about this is visible in a plane-emission
trace — it is only found by looking at the pixels, which is the argument for
decoding a frame rather than only counting them.

**A modal is a screen, not a plane.** A panel raised over the screen it dims is
the exact shape of the blanking bug: it damages the cells it covers, and a
stationary protocol repairs that by retransmitting the full-screen bitmap
underneath, over every region above it. Draw the dim wash and the panel into
their own screen's static layer instead:

```c
fill_ref_rect(pixels, width, height, layout, 0, 0,
	MULTIPLAYER_REFERENCE_WIDTH, MULTIPLAYER_REFERENCE_HEIGHT,
	g_mp_shadow, 170u);
draw_plate(pixels, width, height, layout, &layout->panel, g_mp_green);
```

It looks identical, it costs one screen change instead of one raised plane, and
it cannot blank anything. Create Room is built this way.

**A cell plane cannot occlude a sprixel, however high it sits.**
`ncplane_move_top()` does not help; neither does creation order. This is a
notcurses rule, not a tier quirk, and it is why the rule above is not only about
blanking: a cell modal drawn over Solo, Marketplace or Settings is drawn
*through* by the bitmap planes it is supposed to cover. The confirmation dialog
hit this — the score panel and the ability rail showed through it — and the fix
was to stop making it a cell plane: it is composed in RGBA and blitted like any
other bitmap, lettered from the shared 8x16 mask through the public
`render_font_mask_load()` (`src/render_confirmation_font.c`). The cell drawing
stays as the fallback for terminals that have no bitmaps to be occluded by.

Any new overlay meant to cover a bitmap screen needs the same treatment. The
tempting shortcut — destroy the screen underneath first — is a dead end worth
recording: calling `render_solo_destroy()` before the prompt makes the dialog
legible, and Solo never comes back. It returns as *"Game paused - resize the
terminal"*, because the layout those planes were sized from goes with them, and
neither `state_changed` nor `resize_pending` rebuilds it.

**Plates round with their regions.** Draw a panel's plate from the *fitted*
rectangle in the layout struct, not from reference units recomputed at the draw
site. A plate a pixel short of the region cropped out of it leaves a seam the
stationary tier has nothing to fill with.

---

## The cell fallback

`TETRISU_RENDERER=cell` gets a self-contained panel with no artwork, and it is
not a smaller copy of the bitmap screen.

**Stack what the bitmap tier puts side by side.** The lobby draws its room table
and its join field in two columns; the cell panel puts the field under the
table. That is what keeps the minimum inside a conventional terminal — side by
side needed 80+ columns, stacked needs 58.

**Derive the minimum from the widest thing you draw, and state it in a
constant.** Then refuse smaller terminals with a notice rather than a broken
frame:

```
MULTIPLAYER NEEDS 58x20
RESIZE OR ESC
```

**Clip every value.** A username or a room id arrives from a provider and will
one day arrive from a server. One long field with no clip walks straight through
the frame border. A single `put_line()` that snprintf's with a width precision
is enough, and it is worth routing everything through it.

**Let a failed bitmap composition fall through to it.** The screen loops treat
`false` as fatal, so a renderer that gives up must degrade rather than return
it — opening a lobby must never be able to quit the client.

---

## The bugs

Every one of these shipped at least once. They are all invisible to unit tests
and to the terminal you developed in.

| Symptom | Cause | Fix |
|---|---|---|
| Input capped at ~12–15/s | whole screen recomposed per keystroke | split into static + region layers |
| Held key lags further and further behind | no input coalescing | drain identical movement keys |
| Buttons vanish after a data change | static rebuild re-emitted the bitmap under unchanged region planes | seed region signatures with the static one |
| Regions blank each other when a card opens | two region planes overlapping on a stationary tier | make overlapping regions mutually exclusive |
| Extra ~21 ms per keystroke | plane destroyed and recreated per frame | reuse the plane, blit in place |
| Rows overlap the line below at some sizes | row pitch set from the glyph size | pitch rows from the ink band, not the cap height |
| Whole screen blanks after an action | a notification plane raised over a full-screen bitmap | keep changing values in regions; report results inside one |
| A region of the previous screen sits on top of the new one | screens sharing a plane set, and only some planes destroyed on the switch | destroy every region plane and clear every signature on a screen change |
| Every region repaints when any one of them changes | one model hash seeding all of them | split the model hash along the same lines the regions are split |
| A plane repaints on every keystroke in an unrelated field | a signature hashing a field the region only sometimes draws | hash the rendered line, not the field behind it |
| Title or legend unreadable against the backdrop | loose text outside every panel, over the art's decorated border | draw an opaque band behind that strip, first, in the static layer |
| A typed letter fires a command, or a command types a letter | one key handler for a screen that has a text field | split the handler by mode; inside a field every printable key is text |
| A held key drops letters out of a typed field | a coalesce predicate that folds more than movement keys | never coalesce printable characters |
| A long username or id walks through the cell frame | a cell-mode value drawn without a width clip | route every cell write through one clipping helper |
| Entering a screen takes seconds, and leaving it takes seconds again | the outgoing backdrop left stacked under the incoming one, so its sprixel is re-sent every visit | park the retained backdrop off the top of the screen |
| One transition freezes for tens of seconds while others are instant | rendered with the outgoing backdrop still covered, and moved it away afterwards | move or destroy the old plane *before* `notcurses_render()` |
| A modal is drawn through by the screen it covers | a cell plane over a sprixel; z-order cannot fix it | compose the modal in RGBA and blit it |
| Keys pressed during a slow transition fire on the next screen | `discard_queued_input()` on exits only, not on entries | discard on any transition the user waits through |
| A "loading" message never appears | the status is set inside the blocking call, after the last paint | validate, paint, then block |
| Fine artwork detail disappears on some screens | a cell blitter used to make a backdrop cheap; they cap at two colours per cell | keep `NCBLIT_PIXEL` and fix the transmission instead |

### The blanking one, in full

This is the third bug wearing a different hat, and it is worth spelling out
because nothing in the screen's own code looks wrong.

A notification is a plane like any other. Raising or dropping one damages the
cells it covers, and a stationary protocol repairs that damage by
retransmitting the bitmap underneath — which, for a full-screen static plane,
means re-emitting it over every region above it. Seeding region signatures with
the static one does not save you: that only helps when *you* rewrite the static
plane, and here notcurses re-emits it on its own.

Settings survives this because its top strip is static and its notifications
never coincide with a static rebuild. The Marketplace did not, because buying
moved a value in the static layer *and* raised a card in the same action.

Two independent fixes, both applied:

1. **Nothing the user does rebuilds the full-screen layer.** The wallet moved
   to a region; best score and rank stayed static because nothing on that
   screen can move them.
2. **The screen never raises a plane over itself.** Purchase, equip and volume
   results are a line inside the control region instead of a notification card.
   The outcome was already legible in the wallet and the tile, so nothing was
   lost.

Either alone would probably have been enough. Both together make the failure
structurally impossible, which is worth more than knowing which one mattered —
this is a bug you cannot reproduce on a Kitty-class terminal, and diagnosing it
took a full instrumented capture (see
[Driving it from a script](#driving-it-from-a-script)).

---

## Text metrics

Two things about the shared 8x16 atlas (`SHARED_FONT_MASK_PATH`) that the
blitters get right and callers routinely get wrong.

**Row pitch is not glyph height.** The blitters draw the whole ink band, not
the cap band, so a glyph of size `g` occupies roughly `-0.25g` above the
baseline to `+1.4g` below it — the tails of `g j p q y` and the comma live down
there. `FONT_INK_TOP` and `FONT_INK_BOTTOM` bound it. A column of rows spaced
by `g` alone looks correct until a wrapped line ends in a `y` and collides with
the heading beneath it. Space rows so each clears the previous row's
descenders:

```
row pitch  ≥  1.4 × previous glyph size  +  breathing room
```

`MARKETPLACE_REF_DETAIL_STEP_Y` is derived that way, and its comment records
the arithmetic so the next person changing a glyph size sees the constraint.

**Size a row of related labels from the longest one.** `draw_text_ref()` shrinks
text to fit its box, so sizing each caption independently makes neighbouring
tiles render at visibly different sizes — it reads as a rendering glitch rather
than as typography. Fit once from the widest string the row can hold, then draw
every caption at that size: `caption_metrics()` and `draw_text_fixed()` in
`render_marketplace_font.c`. Where a canonical name is too long to survive
that, give it a short caption rather than letting it shrink into illegibility
(`slot_label()`).

---

## Checklist

Before calling a screen done:

- [ ] Static and region layers separated; nothing **focus- or action-sensitive** in the static layer
- [ ] Region rectangles in the layout struct, read by both renderer and tests
- [ ] Region planes pairwise disjoint across a geometry sweep, or mutually exclusive
- [ ] The sweep's plane list names every rectangle that becomes a plane, and only those
- [ ] No region reaches the top-right corner notifications are raised into
- [ ] Every signature hashes fields, not structs
- [ ] Every region signature seeded with the static signature **and the model**
- [ ] Planes created once and written in place; replaced only on geometry change
- [ ] No per-frame `ncplane_move_top()`
- [ ] Row pitch clears the previous row's descenders; related labels share one fitted size
- [ ] Identical movement keys coalesced; typed characters never; releases dropped; queue discarded on exit
- [ ] A screen with a text field splits its key handler by mode, and draws which mode it is in
- [ ] `notcurses_render()` skipped when nothing changed and no notification is up
- [ ] Cell fallback path works under `TETRISU_RENDERER=cell`, and every value it draws is clipped
- [ ] Loose text outside every panel has a band behind it
- [ ] Teardown frees planes, cached canvases, and resets signatures
- [ ] Every action — not just every keystroke — leaves the whole screen drawn
- [ ] Region seeds are scoped: no region repaints because an unrelated one changed
- [ ] Screens sharing a plane set destroy every plane and signature on a switch
- [ ] Anything that ticks without input sits alone on its region
- [ ] No frame is rendered with a live sprixel covered; the outgoing backdrop leaves first
- [ ] Anything retained across a screen change is gated on `render_pixel_planes_reliable()`
- [ ] An overlay meant to cover a bitmap screen is a bitmap itself
- [ ] Queued input discarded on entering a slow screen as well as leaving one
- [ ] Any status shown before a blocking call is set before the paint, not inside the call
- [ ] Entry and exit timed on a real terminal, not just assumed from the region work
- [ ] One frame of the stationary tier decoded and **looked at**, not just counted

---

## Verifying it

Unit tests cover layout and input policy only — the logic archive excludes
`render_*.c` because it pulls in notcurses, so **the render path has no
automated coverage.** Everything above must be checked by running the app, or
by driving it from a script (see [below](#driving-it-from-a-script)).

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
server. Note the second half of that sentence: `p` typed while a text field
holds focus is the letter `p`, so a script has to walk focus down to the primary
button first. Real credentials are not an alternative — `auth_form_submit()`
refuses until the server check succeeds, and there is no server.

**Time the transitions, not only the keystrokes.** The region work above is
measured per key; entering and leaving a screen is a different cost with a
different cause, and nothing in the per-key numbers predicts it. `sample(1)` on
the live process answers it directly, and the frame you are looking for is
`notcurses_render → raster_and_write → blocking_write → poll`:

```bash
sample $(pgrep -x tetrisu) 20 -mayDie -f out.txt
```

Two traps in that one line. **`pgrep -f bin/tetrisu` matches the terminal too**,
because the path is in its argv — use `pgrep -x`. And **inlining misattributes
the hot frame**: a sample once pointed at a function that a `clock_gettime`
trace proved ran in 1 ms. When the answer matters, confirm it by timing the call
explicitly rather than trusting the stack alone.

Drive the app from the real terminal rather than a synthetic one wherever you
can. kitty will do it with two flags, and it is far less work than the pty
harness below:

```bash
kitty --listen-on unix:/tmp/tetrisu-kitty -o allow_remote_control=yes -- ./bin/tetrisu
kitty @ --to unix:/tmp/tetrisu-kitty send-text --match id:1 $'\t\t\tp'
```

Two limits worth knowing on this hardware:

- **Valgrind is unusable** — it SIGILLs host-wide. Use `-fsanitize=address`.
- **ASan cannot leak-check tetrisu** — it dies in `UnmapOrDie` during notcurses
  teardown. Runtime errors are still reported, since those surface immediately.
  For leaks, watch RSS across a few hundred keystrokes instead: a leaked
  full-screen canvas is ~6 MB and shows up immediately.

---

## Driving it from a script

"Run it and look" does not survive a bug you cannot see, and the stationary
tier's failures are mostly invisible on a Kitty-class terminal. The Marketplace
blanking bug was found and fixed this way, so the technique is worth recording.

No harness is checked in — it is a throwaway you rebuild for the bug you have.
What follows is what it has to do, not a script to run.

`tetrisu` can be driven inside a pty and its output decoded offline. The one
obstacle is that **notcurses blocks on terminal capability probes** — a bare
pty never answers them, so the process hangs at `render_init()`. Answer them
and everything after works:

| Query | Reply |
|---|---|
| `\e[6n` cursor position | `\e[1;1R` |
| `\e[c` primary DA | `\e[?62;4;9;c` — the `4` claims Sixel |
| `\e[>c` secondary DA | `\e[>0;95;0c` — **not** `276`, which is Kitty's own id |
| `\e_G…\e\\` Kitty graphics | leave unanswered on the stationary tier |
| `\e[14t` / `\e[18t` | pixel size / cell size |
| `\e[?<n>$p` DECRQM | `\e[?<n>;2$y` |
| `\e[?<n>;…S` XTSMGRAPHICS | `\e[?<n>;0;256S` — status `0` means supported |
| `\eP+q…\e\\` XTGETTINCAP | `\eP0+r\e\\` (unsupported) |
| OSC `4;n;?` / `10;?` / `11;?` | any plausible `rgb:….` |

DA1 is the fence notcurses waits on, so everything else must be answered before
it.

**Claiming Sixel is not enough on its own.** notcurses prefers the Kitty
graphics protocol wherever it believes it is available, and it will conclude
that from two things a careless harness hands it: a secondary DA of `276`, which
is Kitty's own identifier, and *any* reply at all to the Kitty graphics query —
including a refusal. Report a plain xterm DA2 and leave that query unanswered,
or the run quietly exercises the movable tier and proves nothing about the one
you were testing. The symptom is a capture full of `\e_G` chunks and not one
`DCS`.

With Sixel actually chosen, `TETRISU_RENDERER=stationary` exercises the real
stationary path, and each frame arrives as `DCS … q … ST` preceded by a cursor
move. Parse `\e[<row>;<col>H` to know where each bitmap lands.

There are two different questions to ask of that capture, and they want two
different amounts of work:

- ***"Did this action re-emit the full-screen bitmap?"*** — **read the plane
  list.** Each sixel's raster header `"1;1;<w>;<h>` plus its cursor position
  tells you which plane it is and how big. No pixels needed. This is the cheap
  check and it catches the expensive bug.
- ***"Is what it drew actually readable?"*** — **decode the pixels.** Sixel is
  simple enough to decode in ~60 lines (`#n;2;r;g;b` palette, `?`–`~` as six
  vertical pixels, `!n` run-length, `$` carriage return, `-` next band).
  ImageMagick's reader drops the palette, so write your own. Composite the
  planes at their cursor positions into one image and the result is the screen
  as a real terminal would draw it.

Do not skip the second one because the first passed. They fail independently:
the multiplayer screens had a clean plane trace *and* a title row sitting
unreadably on top of a skull frieze, and no amount of counting bitmaps would
ever have said so. It is also the only way to catch a truncated caption, a
mis-sized row of labels, or a colour branch taken wrongly — sampling a few
pixels out of the composited image is a fast way to check the last of those
without trusting your own eyes:

```python
# is the STATE column green for a joinable room and red for a full one?
for label, y in (("duel-42 1/2", 216), ("duel-51 2/2", 308)):
    ...  # most common bright pixel in that cell
# duel-42 (112, 214, 173)   <- green, joinable
# duel-51 (252, 112, 142)   <- red, full
```

For the plane-list check:

```
# a purchase should touch only the regions that changed
off=… row=4  col=37 size=260x78    <- wallet
off=… row=10 col=37 size=350x198   <- characters
off=… row=10 col=79 size=360x198   <- themes
off=… row=22 col=37 size=780x258   <- detail
off=… row=37 col=37 size=780x156   <- controls
# no 1200x876 anywhere = no full-screen re-emit = nothing can blank
```

Two cautions learned the hard way:

- **Capture long enough.** notcurses settles a frame over several presents. A
  capture that stops one present early looks exactly like a blanking bug. This
  cost an afternoon: the "missing" repaint was in the bytes that arrived after
  the capture had been cut off.
- **The emulator is not the hardware.** This harness reported plane geometry a
  few pixels off from what the layout computed, and the same discrepancy showed
  up for Settings, which is known good. Use it to compare a new screen against
  an existing one, not to judge a screen on its own. Anything it flags still
  wants confirming on a real Sixel terminal such as foot.
