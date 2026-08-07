# Migrating local Solo Battle to `tetrisd`

## Purpose

Solo Battle is currently playable without a server so the terminal layout,
controls, timing, scoring, and animation can be tested before `tetrisd` exists.
This is a temporary authority boundary. The final system must keep every board
mutation in `tetrisd` and render only server-provided `STATE` snapshots.

No HTTTP or shared event schema is changed by the local implementation.
`libhtttp` is still planning-only, so both team members must approve the exact
action and `STATE` wire fields before either endpoint is implemented.

## What can be reused unchanged

| Current code | Final owner | Migration treatment |
|---|---|---|
| `libtetrisbrain` board, SRS, 7-bag, scoring, gravity helpers | `tetrisd` | Link the same pure archive into the server |
| `render_solo.c` 512 x 384 board, previews, ghost, score, meter, and Mirurun composition | `tetrisu` | Keep; change its input from `t_solo_game` to a decoded snapshot/view model |
| Key bindings in `render_solo.c` | `tetrisu` | Keep; serialise actions instead of mutating local state |
| Two-frame clear animation | `tetrisu` | Keep as presentation driven by a server clear sequence/row mask |
| `solo_game.c` tests | server game-session tests | Move the cases to `tetrisd`'s room/ticker test harness |

`libtetrisbrain` remains pure throughout the migration: no sockets, clocks,
mutexes, global RNG, rendering, or filesystem access belong in the library.

## Target action flow

```text
keyboard
  -> tetrisu serialises one action with libhtttp
  -> libtetrissh encrypts and frames it
  -> TCP
  -> tetrisd reactor validates session/player/room
  -> apply libtetrisbrain operation to server-owned state
  -> mark the game dirty
  -> the next tick encodes a STATE snapshot and queues it in the outbox
  -> the reactor seals and writes it; a slow socket never blocks the tick
  -> tetrisu replaces its view model and redraws
```

Actions required for the current Solo feature are left, right, clockwise
rotation, counter-clockwise rotation, soft drop, hard drop, pause/resume, and
restart. These are conceptual actions, not frozen method names. Define their
final HTTTP spelling in the joint protocol review rather than copying local C
enum values onto the wire.

## Server-owned state

Create one server-side session value per Solo player containing the fields now
held by `t_solo_game`:

- board and active piece;
- next-three queue and caller-owned 7-bag/RNG state;
- score, combo, back-to-back state, total lines, and level;
- gravity, lock-delay, lock-reset, and clear-animation timing;
- clear-row mask/sequence number;
- Mirurun crystal charge, partial two-line progress, ability costs, and
  activation feedback;
- paused, clearing, and top-out state.

Do not send the bag contents or RNG state to the client. The next-three queue is
enough to render the UI, while keeping future pieces authoritative.

## `STATE` snapshot requirements

The gameplay snapshot needs only renderable values:

- monotonically increasing state sequence number;
- 10 x 20 settled board cells;
- active piece type, rotation, column, and row;
- next three piece types;
- score, lines, level, combo, and back-to-back flag;
- crystal charge from 0 to 10;
- last accepted/rejected ability activation for short client feedback;
- phase (`active`, `clearing`, `paused`, `top-out`);
- clearing row indices and remaining animation time;
- last scoring result for Single/Double/Tetris/T-Spin/Perfect Clear text.

The ghost does not need a wire field. `tetrisu` can project a copy of the
server-provided active piece against the server-provided settled board because
that calculation is visual-only and cannot alter the game.

## Thread and blocking rules

`tetrisd` has exactly one owner of all mutable game state
([ADR-0008](adr/0008-tetrisd-is-event-driven.md)). Gravity, lock delay,
line-clear completion, level changes, spawning and action dispatch all happen
on the reactor thread, so a move and a gravity tick cannot interleave and there
is no lock to take. Do not add one: if a change here seems to need a mutex, the
work has been put on the wrong thread.

The pattern is therefore:

1. validate and mutate with `libtetrisbrain`;
2. mark the game dirty;
3. let the tick encode the snapshot and queue it in the player's outbox.

Nothing on this path calls `send()`. The reactor writes when the socket is
ready, and a peer that stops reading loses intermediate snapshots to the
one-slot `STATE` mailbox rather than delaying anyone's gravity. Social/log
datagrams triggered by scoring must use `MSG_DONTWAIT` and drop on a full or
missing destination.

## Migration sequence

1. Finalise `libhtttp` action and `STATE` schemas with both members reviewing.
2. Add server game-session storage and initialise it with the same pure helpers
   used by `solo_game_init()`.
3. Move elapsed-time calls from `solo_game_update()` into the server's tick.
4. Map validated HTTTP actions to the same pure movement/rotation/drop calls.
5. Mark the game dirty and let the server's tick snapshot and queue it.
6. Add a `tetrisu` network/view-model module; make `render_solo.c` consume it.
7. Route `1`-`4` and meter clicks through an HTTTP `ABILITY` request. Validate
   charge, equipped character, target, and phase inside `tetrisd/ability.c`;
   then apply Mirurun through the pure `board_cut_bottom()` transform on the
   reactor. Never trust or deduct the client-side charge value.
8. Remove all calls from `tetrisu` input to `solo_game_apply_action()` and
   `solo_game_activate_ability()`, plus all local gravity updates. Keep the
   local implementation only as an explicitly selected offline test mode, or
   delete it.
9. Publish final score/points from `tetrisd` to `marketd` using the required
   non-blocking event channel.

## Acceptance checks

- Two identical seeds produce identical server queues in unit tests.
- A client cannot change its board by withholding or fabricating a `STATE`.
- A move cannot race a gravity tick: both run on the reactor.
- A slow client cannot delay another player's gravity.
- Replayed/out-of-order snapshots are ignored by sequence number.
- Disconnect/reconnect never reconstructs authority from client state.
- The existing Solo renderer still shows next-three, ghost, score, level,
  interactive crystal meter, ability feedback, two-frame clear animation,
  pause, restart, and top-out.
