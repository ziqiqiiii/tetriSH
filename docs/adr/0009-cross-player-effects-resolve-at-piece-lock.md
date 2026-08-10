# Cross-player effects resolve at the Target's piece lock

_Status: accepted, not yet implemented. `tetrisd` serves Single mode only, in
which no cross-player effect exists. This decision defines how Double and
Battle Royale work, and lands in steps 6 and 7 of
[ADR-0008](./0008-tetrisd-is-event-driven.md)._

A player's own inputs mutate their own game immediately, on arrival, exactly
as they do today. Anything that crosses players — garbage from a line clear,
and every offensive ability in `docs/themes.md` — is appended to the Target's
pending queue and applied at that Target's next piece lock, in queue order.

The line is drawn at whether an effect crosses players, because that is where
the two properties actually differ.

Own-board inputs are self-affecting, so their arrival order *is* the correct
order and there is nothing to gain by deferring them. Deferring them would
also break something real: `apply_input` returns `200` when a move applied and
`409` when it was blocked, so the HTTTP status carries the simulation outcome.
A handler that only enqueues cannot know which to return, and the alternative —
answering `202 Accepted` and letting `STATE` reveal the outcome — is a protocol
change reaching into `libhtttp`'s method table, `docs/use_cases.md` and
`test_game`, bought with up to a tick of added input latency, in exchange for
determinism on a path that has no ordering problem.

Cross-player effects are the opposite on both counts. Their outcome depends on
which of two players' packets arrived first, which makes a match unreplayable
and Battle Royale effectively untestable. And they cannot be applied on arrival
anyway, for a reason that has nothing to do with concurrency: injecting garbage
into a board while that player has an active falling piece shifts the stack up
underneath it, and the piece can end up overlapping filled cells — a state
`piece_is_valid` would have refused. Every real Tetris implementation queues
incoming garbage and applies it at a safe point. The safe point is forced by
the game rules, so the discipline exists regardless; this decision only says
where it stops.

Applying everything at the tick — one command queue, the tick as the sole
mutator of `t_game` — was considered. It is the stronger form of
[ADR-0008](./0008-tetrisd-is-event-driven.md)'s single-owner property: one
writer at one instant, a whole match replayable from a seed and a command log.
It was rejected because it pays the latency and the protocol change on every
own-board input to buy determinism only where cross-player effects already
provide it.

## Target

`docs/CONTEXT.md` had no word for the receiving end of an attack, and
`docs/themes.md` needed one badly: all twelve offensive abilities across the
four themes are written against "**the** opponent", singular and definite. That
is a two-player assumption baked into the whole catalogue. In Battle Royale
there are three to ninety-eight of them; in Single there are none, and "destroy
the opponent's blocks" has no referent at all.

**Target** is the player an offensive ability or garbage lands on, and it
resolves per mode:

- **Single** — no Target exists. Offensive abilities are unavailable. Charge
  still accrues, and self-affecting abilities such as Mirurun still work.
- **Double** — the one other player, implicitly. No protocol change.
- **Battle Royale** — drawn per resolution from the room's seeded random
  source, among players still in the game.

Battle Royale draws from the room's seed rather than a global PRNG so that a
match stays replayable from that seed, which is the property this decision buys
in the first place and should not give back. It also matches the one line of
specification that already existed: UC-12 step 3 says garbage goes to "a random
other player".

Letting the attacker name a Target was rejected for scope, not for taste — it
is the more interesting game. It needs a field on the input path, `libhtttp`
body validation, a living-opponent list in `STATE` for the client to choose
from, a target-picker UI in `tetrisu` under time pressure, and a fallback when
the named Target has already been eliminated. Tetris 99-style standing
strategies were rejected for the same reason plus a larger one: they require
badge and KO concepts that do not exist anywhere in the domain.

Three abilities were written as relationships to one specific opponent and are
reworded to be source-agnostic, which costs nothing in Double and makes them
well-defined in Battle Royale:

| Ability | Was | Now |
|---|---|---|
| Pals | lines sent by **the** opponent | lines sent by **any** opponent |
| Mirror | the next power used **by the** opponent | the next power used **against the player** |
| Copy | a copy of **their** opponent's field | a copy of **a Target's** field |

## Consequences

- `t_game` gains a pending queue for incoming effects, drained at piece lock.
  Garbage carries its line count and hole column; abilities carry whatever
  `effect_apply` needs.
- Single mode behaviour is unchanged, so step 6 cannot regress the only mode
  that currently works. The queue simply never receives anything.
- Each room needs a seeded random source, and the seed must be recorded with
  the game for a replay to mean anything.
- A Target eliminated between enqueue and resolution has its queue discarded
  rather than redirected. Redirecting would make the outcome depend on
  elimination timing, which is the non-determinism this decision removes.
- `docs/themes.md` is the source of truth for ability text and is amended as
  above; the three rewordings are the only semantic changes, and each is a
  generalisation rather than a nerf.
- UC-12 step 3 said garbage crosses into "a different room (server-managed via
  IPC)", which contradicts `docs/CONTEXT.md`'s definitions of Room ("a room
  hosts one game at a time") and Game ("one round played in a room"), UC-12's
  own single-room precondition and ownership succession, and
  `libtetrisroom`'s model. A Battle Royale game is one Room of 4–99 slots;
  routing is an in-memory call between two `t_game` structs the reactor already
  owns, and no inter-process anything is involved. The use case is corrected,
  not the glossary.
