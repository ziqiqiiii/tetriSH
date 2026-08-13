# Bug Note — A knockout rebuilt the whole screen

**Status: fixed and confirmed on screen.** The player who reported it played a
Battle Royale against the deployed server and reported the flicker gone. That is
the check [Verifying it](#verifying-it) asks for, and it is the only one that
could close this: the failure mode of a wrong damage rectangle is visual, and no
suite can see a hole in a terminal.

One thing landed after this note was written and belongs to the same defect.
`refresh_notifications` raised the repaint flag on *every* call, so the fade was
still asking for a rebuild per step — the rectangle made each one cheap rather
than stopping it. Gating the flag on a layout signature is what removed the
rest, and it is why changing the volume used to flicker too: a fade step redraws
the same rectangle at a different opacity and damages nothing under it. See
`fix(tetrisu): stop a fade step claiming the screen is stale`.

## What the bug was

Playing a Battle Royale against bots, every board on the screen vanished and
came back. The player who reported it narrowed it themselves, and exactly:

> I think every time someone gets KO'd that's when I get that flicker.

They were right, and the "every time" is the part that mattered — it is not a
race, not a load-dependent stutter, and not the terminal. A knockout raises a
notification card, and a notification card asked the screen to rebuild:

```c
/* render_multiplayer_match.c */
if (render_notification_take_repaint(ctx))
	rebuild_background = true;
```

```c
/* multiplayer_match_mode.c */
render_notification_show_effect(ctx, "K.O.", message);
```

A rebuild during a match destroys every region plane, allocates a canvas,
composes the entire screen in software, presents it, and re-emits every region.
Measured, from `TETRISU_MATCH_TRACE`: **170 to 237 ms**, several times per
knockout, because the flag is raised on every appearance *and* every fade step
of the card.

## Why the repaint flag exists

It is not gratuitous, and anybody tempted to simply delete it should read this
paragraph twice. A card is a bitmap, and so is most of what it covers.
Notcurses cannot overlap two sprixels: it annihilates the cells of the one
underneath. The screens cache their panels by signature, so once the card
expires **nothing considers those panels stale and they never come back**.
Changing the volume once left a room with a background, a title, a footer and
no panels at all. The flag is the screen's cue that its cache is lying to it.

The defect was never the flag. It was the size of the answer.

## The two false leads

Both were wrong, both were eliminated by measurement rather than by reading, and
both are recorded here because the reasoning that produced them was plausible.

**Geometry.** `render_terminal_geometry_changed` sets `rebuild = true`, and a
terminal that reflows mid-match would explain a full-screen wipe. Instrumented
it (`render_match_trace_note` in `render_background.c`), played a match: **zero
`note geometry` lines against nine forced rebuilds.** Innocent.

**A resize key.** `handle_match_key` throws the whole screen away on
`NCKEY_RESIZE || 12u`, and `NCKEY_RESIZE` is `preterunicode(1) // we received
SIGWINCH`. The working theory was that a knockout changes many arena cards at
once, producing a burst of kitty graphics-protocol replies misparsed as keys —
which would also explain the *pairs* of rebuilds in the trace. Instrumented that
branch too: **zero `note resize` lines against eight forced rebuilds.** Also
innocent, and the pairs turned out to be the card's own lifecycle.

Only after both were excluded did the third caller of `rebuild_background` —
the notification flag, in `render_multiplayer_match.c` rather than in the match
loop — become the only remaining path. It is worth noting that neither false
lead was disproved by argument. Each survived every argument available and died
to a line in a log.

## The first fix, and why it failed

Replace the rebuild with a **restage**: forget every region's cached signature
so the next incremental frame re-emits them all, without composing or presenting
the screen. The reasoning was sound as far as it went — the chrome under the
card is cells, which notcurses repaints itself, and `create_region_plane`
re-blits a plane whose geometry still fits rather than making a new one.

It worked and it bought nothing:

```text
401683558930 note restage
401683741506 incremental 182547
```

**182, 197, 209, 242 ms** — the rebuild's own price. In a Battle Royale the
regions *are* the screen, so re-emitting all of them costs what the screen
costs. The mechanism had changed and the bill had not.

This is the second time this file has been optimised on a picture of ninety-nine
players and paid for it in a room of five; see the arena-banding attempt, which
was designed for cards that mostly hold still and reverted after it measured
slower. **Measure against the room people actually play in.**

## The fix

The card damages the cells it covered and no others, and the canvas in this
process was never touched at all — only the terminal forgot. So what is owed is
the transfer of the regions overlapping the card's rectangle.

`refresh_notifications` now unions the cards' cell rectangles into
`ctx->notification_damage_*`, twice per change: once over the outgoing planes
before they are destroyed, once over the incoming ones after they are made. A
card leaving damages the cells it vacates as much as a card arriving damages the
ones it lands on. The union is reset only once a screen has taken the flag, so
two changes arriving between one repaint and the next accumulate rather than the
second losing the first's cells.

`render_multiplayer_match_pixel_restage` converts that rectangle into the
canvas' own pixels and stores it. The renderer then asks one question per
region. `pass->force` already meant *the canvas is right, only the transfer is
missing* — damage means exactly that for part of the screen, so both answer
through one predicate:

```c
static bool region_restage(const t_match_regions *pass, const t_mp_rect *rect)
{
	if (pass->force)
		return (true);
	if (!pass->damaged || rect == NULL)
		return (false);
	return (pass->damage.x < rect->x + rect->width
		&& rect->x < pass->damage.x + pass->damage.width
		&& pass->damage.y < rect->y + rect->height
		&& rect->y < pass->damage.y + pass->damage.height);
}
```

Every `pass->force` test in the region functions became `region_restage(pass,
<that region's rect>)`. Two early returns had to widen: a board's guard reaches
`MP_MATCH_CAPTION_PX` further down than its banded region, because the
name-and-score strip sits just outside it on a plane of its own, and a card
landing on the caption alone would otherwise be skipped.

Screens that are a single bitmap have nothing smaller to offer and still
rebuild, which is why the call site reads as a fallback:

```c
if (render_notification_take_repaint(ctx)
	&& !render_multiplayer_match_pixel_restage(ctx, state))
	rebuild_background = true;
```

## Verifying it

**This was the outstanding part, and it has been done — the reporter played a
Battle Royale on the deployed server and the flicker was gone.** What follows is
kept as the procedure, because it is how this would be re-checked after any
change to the notification or region paths, and because none of it is obvious
from the code.

The fix builds clean and the suites pass, but neither of those can see a hole in
a terminal, and the failure mode of a damage rectangle that is computed wrongly
is *visual*: too small or misplaced, and a card leaves a hole where it used to
be; too large, and it costs what it always did.

Run a real Battle Royale against the deployed server — the fixtures do not
exercise this, since `TETRISU_MATCH_PREVIEW=battle` never raises a K.O. card:

```bash
TETRISU_NET=1 TETRISU_HOST=159.65.11.120 TETRISU_PORT=4242 \
  TETRISU_CA_PATH=certs/demo-ca.crt \
  TETRISU_MATCH_TRACE=/tmp/match-trace.log ./src/tetrisu/bin/tetrisu
```

Create a Battle Royale room, press `B` four times to fill it with bots, start,
and **let at least one knockout happen**. Then:

```bash
grep -n "forced\|note restage" /tmp/match-trace.log
```

Three things to check, in order:

1. **A `note restage <x> <y> <w>x<h>` where a `REBUILD ... forced` used to be.**
   Any `forced` line landing on a knockout means the restage was refused, and
   the guard clause in `render_multiplayer_match_pixel_restage` says which. Two
   `forced` lines are expected and correct: one at entry, and the result screen,
   which is one full bitmap.
2. **The rectangle is plausible** — a card-sized region near a corner, in canvas
   pixels, not the whole screen and not negative. This is the number that says
   whether `ncplane_abs_yx` and `ctx->bg_row`/`bg_col` are in the frame this
   code assumes they are.
3. **The frame after the restage is cheap.** The `incremental` line following
   each `note restage` is the whole point. It was 182–242 ms; anything near that
   means the rectangle is matching everything.

And then look at the screen, which is the only check that catches a hole: after
a K.O. card fades, every board and both arena panels should still be there.

## What this does not fix

Baseline match latency is untouched — incremental p90 around 77 ms, roughly 22%
of frames over 50 ms. That is a separate problem with a separate cause, and the
arena-banding attempt was the wrong answer to it.

## The lesson

A cache invalidation is a claim about *what* is stale, and a boolean can only
claim "everything". The flag was correct and its granularity was the bug — which
does not show up as wrongness, only as cost, so it survives every test that asks
whether the picture is right. Two rounds of instrumentation were needed here
because the trigger was three call sites away from the symptom and every
plausible story about it was false.
