# Double mode — implementation plan

Step 6 of the event-driven migration: make a two-player match a thing `tetrisd`
runs and `tetrisu` renders, rather than two clients each playing a private game
next to a fixture.

This document is the design and the order of work. It records what both halves
actually do today (with the file and line, because several of the findings read
as features until you follow them), the decisions the existing shapes force, the
exact wire changes, and the tests that hold each piece down. Battle Royale
(step 7) is out of scope and is called out wherever a decision here has to leave
it room.

## Table of Contents

- [Where the two halves stand](#where-the-two-halves-stand)
  - [What already works](#what-already-works)
  - [Findings in `tetrisu`'s two-player mode](#findings-in-tetrisus-two-player-mode)
  - [Findings in `tetrisd`](#findings-in-tetrisd)
- [Decisions](#decisions)
- [Wire changes](#wire-changes)
- [Server work](#server-work)
- [Client work](#client-work)
- [Tests](#tests)
- [Order of work](#order-of-work)
- [Open questions](#open-questions)

---

## Where the two halves stand

### What already works

More of Double exists than the status table suggests, and none of it is wasted.

A Double room can be created, joined, listed, chatted in, started, ticked and
recorded **today**. `read_mode` accepts `mode double`
(`handlers_lobby.c:284`), `apply_mode_shape` gives the room two slots and a
start threshold of two (`lib/libtetrisroom/src/room.c:168`), `deal_games` deals
every occupied slot rather than only the first (`room.c:645`), and
`server_rooms_tick` advances both games off the one `timerfd`. `award_game` is
already per player. The client's waiting room is real against the server:
`net_create_room`, `net_load_room`, `net_refresh_room` and `net_start_room`
(`net_provider.c:714-949`) speak `JOIN`/`LIST`/`START`, and the chat panel is
the server's feed.

So the two things missing are not the room and not the plumbing. They are:

1. a **match** — a rule that says two games in one room are playing *against*
   each other (an end condition, a winner, garbage crossing between them); and
2. a **second board on the wire** — the client can be told about its own game
   and nothing else.

Everything below is in service of those two.

### Findings in `tetrisu`'s two-player mode

**F1 — the match screen is not connected to anything.**
`multiplayer_match_mode_run` (`multiplayer_match_mode.c:38`) takes a provider
but no `t_net_client`, and `main.c:264` has none to give it. The opponent is
`state->opponent_game`, a second `t_solo_game` seeded `seed ^ 0x9e3779b9`
(`multiplayer_match.c:62`) which is advanced by `solo_game_update` every turn of
the loop with no input at all — so it stacks pieces where they fall until it
tops out. `state->opponents[]` (the Battle Royale grid) is filled once by
`seed_opponent_board` and never touched again.

**F2 — and yet a real Double game is already running behind it.**
The waiting room's auto-start reaches `net_start_room`, which sends
`START /room/D-xx`; `server_room_start` deals both boards and sets `ticking`.
The clients then navigate into a screen that discards every `STATE` the server
pushes. The server's two games play themselves out under gravity for about a
minute, `room_is_over` finally fires with **both** players topped out, and
`record_and_reset` writes two losses to the store. When the room is destroyed
the players' bindings go stale, so the waiting room they return to answers
`404` on its next `LIST`. This is the most visible symptom and it is fixed by
the same work as everything else.

**F3 — there is no win.** `mp_match_finish(&state, false, 2)`
(`multiplayer_match_mode.c:152`) is the only call site in Double: a local
top-out is a loss, and no other path ends the match. `MP_MATCH_RESULT_WON`
exists and is unreachable.

**F4 — character select cannot be honoured.** The selector runs a 15 s local
countdown (`multiplayer_match.c:167-196`) and locks a character — and then
never tells anybody. `ability_handler` reads
`player.current_equipped_character` out of the store
(`handlers_game.c:155-163`), so the character the player picked for this match
has no bearing on which four abilities their levels select from. Worse for a
match: each client starts its own 15 s from the moment it entered the screen,
and the non-owner enters up to `WAITING_ROOM_REFRESH_MS` (500 ms) plus a `LIST`
round trip late, so the two selectors are not the same clock.

**F5 — abilities are a banner.** `select_power`
(`multiplayer_match_mode.c:542`) writes `"... SELECTED - AWAITING SERVER TARGET
AUTHORITY"` into the status line and plays a sound. No `ABILITY` request is
sent, no charge is tracked, and `state->opponent_charge` is the constant `6`.

**F6 — the 3-2-1 hold cannot survive the crossing.** Online Solo freezes the
server's clock for the length of its countdown by sending `PAUSE`
(`solo_authority.c:358`). `pause_handler` answers `not-single` in any room with
more than one seat (`handlers_game.c:48`, `141`), which is correct — one player
stopping their own clock in a match is an advantage — so the countdown has to
come from the other side. See [D2](#d2--the-match-countdown-is-the-servers).

**F7 — every input is a blocking round trip inside the render loop.**
`net_request` (`net_client.c:173`) writes the request and then sits in
`receive_message` until the *response* arrives, up to `NET_REPLY_TIMEOUT_MS`
(4000 ms). `STATE` and `CHAT` frames that arrive first are filed and the wait
continues, which is right, but the loop is still stopped for a round trip per
keypress. Two consequences a match makes much worse than Solo does:

- a held key repeats at `SOLO_DEFAULT_ARR_MS` = 33 ms (`tetrisu.h:222`), so
  holding left is 30 blocking round trips a second, each one a turn of the loop
  that draws neither board;
- a reply that never comes freezes both boards for four seconds. In Solo that
  is a bad moment; in a match it is a loss.

**F8 — soft drop already outruns the server's rate limit.**
`soft_drop_interval` is `gravity_interval_ms(level) / 20`
(`solo_handling.c:245`), floored at 1 ms. Against `GRAVITY_MS`
(`lib/libtetrisbrain/src/scoring.c:15`) that is 50 ms at level 1 but 9 ms at
level 7 and 3 ms at level 10 — 111 and 333 requests a second, against
`TETRISD_DEFAULT_INPUT_RATE` of 60/s with a burst of 120. A player holding soft
drop past level 6 is rate-limited by the server, today, in online Solo. It is
listed here because Double is where it stops being survivable, and because the
fix belongs with F7's.

**F9 — the opponent board is re-encoded whole on every change.**
`regions_opponent` (`render_multiplayer_match_pixel.c:684`) compares one
signature over the whole opponent game and, on any change, re-creates the whole
opponent plane. The local board was split into `MP_MATCH_BOARD_BANDS` strips
precisely because re-encoding a whole board per frame *was* the input latency
(CLAUDE.md says so). Right now that costs nothing because the fixture opponent
only changes at gravity intervals; fed from the wire it changes at the tick
rate, and pays the exact cost the banding was introduced to avoid.

**F10 — two small ones, while the file is open.** `mp_match_layout_build`
assigns `layout->valid` twice for Double (`multiplayer_match.c:308-309`); the
first is dead and the effective gate is `rows >= 32 && cols >= 100`.
`mp_match_apply_room` runs twice on entry — once at the end of
`mp_match_state_init` (`multiplayer_match.c:64`) and once from the caller
(`multiplayer_match_mode.c:66`).

### Findings in `tetrisd`

**S1 — one `STATE` mailbox slot per client.** `t_outbox.state` is a single
message and `outbox_push_state` frees whatever was in it
(`tetrisd.h:322`, `outbox.c:65`). This is deliberate and correct — a snapshot
supersedes the one before it — but it means **a client cannot be sent two
snapshots of two different boards**: the second would destroy the first. This
is the single constraint that shapes the whole wire design below.

**S2 — a match has no end condition.** `room_is_over` is "no game is active"
(`room.c:840`). In Double that is "both players have topped out". A survivor is
never a winner, and `record_and_reset` awards `won = !topped_out`
(`room.c:890`), so the honest outcome of a match that one player wins is
currently two losses whenever the winner is eventually starved out too.

**S3 — nothing crosses between boards.** `game_ability` acts on one `t_game`
and `apply_self` returns `ABILITY_NO_TARGET` for the twelve targeted abilities
(`ability_ctrl.c:178`) regardless of mode; `ability_handler` only refuses them
early in Single (`handlers_game.c:129`). There is no pending-garbage field on
`t_game`, no Target resolution, and no queue.

**S4 — `STATE` is addressed and delivered to exactly one player.**
`push_state` builds `/room/<name>/player/<pid>` and enqueues it to `pid` alone
(`room.c:739-765`). Nothing broadcasts a board to the room.

**S5 — a forfeit does not end the match.** `server_room_forfeit`
(`room.c:407`) records the leaver, releases the seat, promotes a successor and
closes an emptied room — but the survivor's game keeps ticking in a room with
one player in it. UC-11 5a says the remaining player wins by default.

**S6 — `t_game` knows nothing about who it is playing.** No character for the
match, no pending garbage, no placement, no knockout count.

**S7 — sizing is fine for two.** `TD_MAX_GAMES` is 16 and a Double room has two
slots, so nothing here is near a limit. Battle Royale is where the same
decisions stop fitting, which is why each one below says what it costs at 99.

---

## Decisions

### D1 — the opponent's board rides in the recipient's own `STATE` frame

Given S1, a client can receive exactly one snapshot per tick. Two ways out:
widen the mailbox to one slot per subject, or put both boards in one message.

**One message.** Widening the mailbox re-opens a question the one-slot design
had closed — which of the two boards in hand is the newer one — and a client
could then draw its own board from tick *N* beside its opponent's from tick
*N−3*. Carrying the opponent inside the recipient's snapshot makes the two the
same instant by construction. The path, the method, the mailbox, the
`state_seq` numbering and `addressed_to_this_game` (`net_client.c:488`) are all
unchanged; only the body grows.

`t_body_state` gains an opponents section that Single encodes zero of, so the
Solo path is untouched by construction and its existing tests keep passing.

At 99 players this does not fit — see [Wire changes](#wire-changes) for the
arithmetic and for the field order chosen to let Battle Royale add a compact
detail level rather than a second body.

### D2 — the match countdown is the server's

F6 removes the client's only means of freezing the board, and F4 shows the two
clients' clocks are not the same clock anyway. So `START` deals the boards and
holds them: the room enters a **countdown** for `TETRISD_MATCH_COUNTDOWN_MS`
(3000) during which every game is `active`, not `paused`, and not advanced.
`STATE` carries `phase countdown` and the milliseconds left, and the client
draws its 3-2-1 from that number instead of running one.

This costs one new `t_body_phase` value and buys three things: both players
start on the same tick, the non-owner has slack to reach the match screen, and
the "pause the server for the countdown" special case disappears from
`solo_authority.c` when Solo is eventually moved onto the same mechanism.

### D3 — a Double match ends when one player is left standing

`room_is_over` becomes mode-aware. Single keeps its meaning. Double is over
when **fewer than two games are still active**, or fewer than two players are
still seated. The survivor is recorded `won = true` and the player who topped
out `won = false`, both through the existing `award_game`, both exactly once —
UC-11's DB mapping.

S5 falls out of the same rule: `server_room_forfeit` runs the check after
releasing the seat, so a quit ends the match and the remaining player wins by
default (UC-11 5a), and the quitter is still recorded as the forfeit they
already are.

### D4 — garbage is queued against a Target and lands at that player's next lock

`garbage_lines_from_clear` already exists in the brain (N−1 rows, `garbage.c`),
so the amount is not a new decision. Where it lands is.

Injecting rows under an active piece can produce a board `piece_is_valid` would
reject, so the safe point is the Target's next piece lock — a game rule, not an
optimisation, and CLAUDE.md already states it. `t_game` therefore gains
`pending_garbage`, `lock_piece`/`finish_clear` drains it, and the sender's clear
resolves at `finish_clear` (after the held clearing phase) rather than at
`begin_clear`, so a receiver never takes garbage in the middle of watching their
own rows go.

Who the Target is stays out of `game.c`: in Double it is "the other occupied
slot", and only `room.c` holds both halves of a Room. The hole column is derived
from the sender's `seq`, the same fairness trick `apply_fry` already uses
(`ability_ctrl.c:216`), so no RNG enters `libtetrisbrain`.

Abilities are the same mechanism with a different payload, which is why the
twelve targeted ones become reachable in this step: `apply_self` grows a sibling
that writes an effect or a board transform into the Target's queue instead of
its own game.

### D5 — gameplay inputs stop waiting for their reply

F7 and F8 are one problem. `MOVE`, `ROTATE`, `DROP` and `HOLD` get a send-only
path: serialise, `session_send`, return. Their responses are drained by
`net_pump` alongside `STATE` and `CHAT`.

This is honest rather than a shortcut: the client already applies nothing
optimistically, and `net_solo.c` says so in as many words — "the board changes
when the snapshot that follows says it did". The reply to a `MOVE` therefore
carries no information the next snapshot does not, except a refusal, and a
refusal is visible as the board not moving. What the drain must still act on is
`429`: a client that trips the rate limit and does not notice will keep
tripping it, so the back-off belongs in the pump.

`net_request` stays exactly as it is for everything whose answer the caller
needs: `JOIN`, `START`, `LIST`, `LEAVE`, `PROFILE`, `BUY`, `EQUIP`, `CHAT`.

This lands as step 5, ahead of the garbage work, because the server this client
talks to is a remote one — see [Order of work](#order-of-work).

F8's arithmetic is then a second, smaller decision: either clamp the client's
soft-drop repeat to something the server's budget can carry, or raise
`TETRISD_INPUT_RATE` for match play. Clamping is preferable — 111 drop requests
a second is a lot of wire for a piece that could fall those rows on one `DROP
SOFT` per gravity step — but the number has to be picked against the real rate
limit rather than guessed.

### D6 — the client binds its play path when it is seated, not when it starts

`net->play_path` is written in one place, `net_solo_start` (`net_solo.c:79`).
A player who joined a Double room and did not press Start never has one, so
`addressed_to_this_game` rejects every snapshot the server pushes them. Write
it wherever the client learns it holds a seat — `net_create_room`,
`net_load_room`, `net_refresh_room` — and clear it on `LEAVE`.

The first `STATE` to arrive then also *is* the signal that the match has begun,
which removes the up-to-500 ms polling delay the non-owner suffers today
(`WAITING_ROOM_REFRESH_MS`, `main.c:1802`).

### D7 — the character a match is played with is sent, not inferred

F4 leaves two options: have the selector `EQUIP` (an account-wide change, made
for one match, which is the wrong scope), or carry the choice in a request.

Carry it, and use the seat that already exists for it. The domain has
`SLOT_READY` and nothing on the wire ever sets it; the client has a ready toggle
that is purely local (`waiting_room_toggle_ready`). Add `READY /room/<name>`
with a body of `character <id>`:

- the server verifies ownership with `db_player_owns_character` (which answers
  `t_db_bool`, so it is tested against `DB_TRUE`, never `DB_OK`);
- the choice is stored on the room's runtime beside the slot and read by
  `deal_games` into `t_game.character_id`;
- `ability_handler` resolves `(character, level)` against *that* rather than
  against the account's equipped character;
- Double auto-starts when both slots are `READY`, which replaces the client-side
  auto-start heuristic and makes the ready button mean something.

Character select therefore moves ahead of `START`, into the waiting room. The
selector UI does not have to move — but its 15 s countdown becomes the waiting
room's, and its result becomes a `READY`.

### D8 — the opponent board is presented at its own rate

F9: fed from the wire, the opponent plane is re-encoded on every tick that
changes it. Two mitigations, both wanted:

- band the opponent board the way the local one is banded
  (`MP_MATCH_BOARD_BANDS`), so a moving opponent piece re-encodes one strip; and
- give the opponent region a presentation floor of its own. The opponent's board
  is information, not control: presenting it at ~20 Hz while the local board
  presents on every input is a trade the player cannot feel, and it keeps the
  terminal's bitmap budget where the piece they are steering is.

---

## Wire changes

Four changes, all additive. Both ends are built together, so there is no
compatibility window to keep — but the body's existing head is left byte-
identical anyway, because that is what keeps Solo's tests honest.

### 1. `t_body_state` gains an opponents section

Appended **after** the existing `board` block:

```
opponents <n>
opp <slot> <pid> <alive> <phase> <score> <lines> <pending> <username>
<20 lines of 20 hex chars>          ... repeated n times
```

`pending` is the garbage queued against that player but not yet landed — the
receiver's warning bar. `username` runs to end of line for the reason
`libstatusbody` already documents for catalogue names, and it is the reason
`db_username_valid` forbids spaces.

`n` is 0 for Single, 1 for Double. `BODY_OPPONENTS_MAX` is therefore 1 in this
step, and that is a deliberate cap rather than an oversight:
`advance_and_push` collects `t_body_state snaps[TD_MAX_GAMES]` on the stack
(`room.c:700`), so each opponent board added to the struct costs
16 × 420 bytes of stack per tick. At Battle Royale's 98 opponents the struct
alone is 41 KB and the array is unbuildable — so step 7 needs *both* a compact
opponent encoding (a 1-bit-per-cell mask: 20 rows × 10 bits = 50 hex chars,
~60 bytes an opponent, ~6 KB for 98) *and* a tick that encodes per slot instead
of collecting first. Neither is needed for two players; the field order above is
chosen so the mask can arrive as a second detail level rather than a second
body.

Size check for Double: the body is ~500 bytes today and ~960 with one opponent,
against `TETRISD_BODY_MAX_BYTES` of 8192 and a 64 KiB frame cap. Comfortable.

### 2. `t_body_phase` gains `BODY_PHASE_COUNTDOWN`

Spelled `countdown` on the wire, carried with the milliseconds remaining in the
existing `clearing`-style position (a `countdown <ms>` line). `game_snapshot`
reports it ahead of `active`; the client maps it to `countdown_active`.

### 3. `READY /room/<name>`

| Method | Path | Auth | Body | Answers |
|---|---|---|---|---|
| `READY` | `/room/<name>` | ✓ | `character <id>`, optional `ready <0\|1>` | `200` with the room snapshot, `403` when the character is not owned, `409` when not seated or the room is in game |

Client-initiated, `application/tetris-command`, one more row in `g_routes`
(`dispatch.c:4`) and one more row in `libhtttp`'s method table.

### 4. `STATE` is pushed to a player who is not its subject — no

Worth stating because it is the obvious alternative and it is refused: the path
`/room/<name>/player/<pid>` names the *subject* of the snapshot, and the client
matches it against its own play path to decide whether the frame is for it. A
frame about player B delivered to player A would either need a second path
convention or would break that check. D1 avoids the question entirely.

---

## Server work

Per file, in the order the dependencies allow.

### `lib/libstatusbody`

- `statusbody.h`: `BODY_OPPONENTS_MAX`, `t_body_opponent`, the
  `t_body_state.opponents` array and `opponent_count`, `BODY_PHASE_COUNTDOWN`,
  `t_body_state.countdown_ms`.
- `src/state.c`: encode and decode the two new sections. The round-trip law
  (`decode(encode(x)) == x`) covers them, and `EBADMSG` on a count that does not
  match the rows that follow.
- `README.md`: the body format table.

### `src/tetrisd/include/tetrisd.h`

- `TETRISD_MATCH_COUNTDOWN_MS` (3000).
- `t_game`: `character_id`, `pending_garbage`, `garbage_seq` (for the hole
  column), `placement`.
- `t_server_room`: `countdown_ms`, and the per-slot ready/character runtime
  (`ready[TD_MAX_GAMES]`, `character[TD_MAX_GAMES]`) — beside the games,
  because they share the slot's lifetime and `room_blank` must clear them for
  the same reason it clears `ticking`.
- `t_input_action`: nothing new; garbage is not an input.
- Prototypes for the new `room.c` and `game.c` entry points below.

### `src/tetrisd/src/game.c`

- `game_start` takes the character and stores it.
- `game_queue_garbage(t_game *g, int lines)` — adds to `pending_garbage`; called
  only by `room.c`.
- `lock_piece` / `finish_clear`: drain `pending_garbage` through
  `board_inject_garbage` **after** the clear resolves and **before**
  `spawn_next`, so the incoming rows are part of the board the next piece is
  validated against. A spawn that then fails is a top-out, which is the correct
  outcome of being buried.
- `game_snapshot`: report `BODY_PHASE_COUNTDOWN` when the room is holding, and
  fill nothing else differently — the opponents section is the room's to fill,
  because a game does not know it has one.

### `src/tetrisd/src/room.c`

The bulk of the work, and it stays in this file because every part of it needs
both halves of a Room.

- `server_room_ready(server_room, cli, character)` — records the choice, marks
  the slot `SLOT_READY`, and reports whether the room may now start.
- `server_room_start`: deal with characters, set `countdown_ms =
  TETRISD_MATCH_COUNTDOWN_MS`, `ticking = true`. Auto-start for Double is
  decided here (all seats ready) rather than by the client.
- `tick_room`: spend the countdown before advancing any game; while it is
  running, every game is still marked dirty each tick so the client gets its
  3-2-1 frames.
- `tick_once`: after advancing each game, resolve the clears it produced into
  garbage against the Target (`garbage_lines_from_clear` →
  `server_room_target_of` → `game_queue_garbage`), then take the snapshots.
  Resolving before snapshotting is what puts the new `pending` count in the same
  frame that shows the clear.
- `server_room_target_of(server_room, from_slot)` — Double: the other occupied
  slot with a live game. Returns `-1` when there is none, which is the same
  answer Single gives and the reason this function is the only place Battle
  Royale's four targeting modes will need to land.
- `fill_opponents(server_room, subject_slot, t_body_state *snap)` — projects
  every other live slot into the snapshot's opponents section.
- `room_is_over`: mode-aware, per [D3](#d3--a-double-match-ends-when-one-player-is-left-standing).
- `record_and_reset`: `won` is "was still active when the match ended", not
  `!topped_out`. Placement is recorded on the way out so the client can be told
  its rank.
- `server_room_forfeit`: after `room_release`, re-check `room_is_over` and end
  the match if the departure decided it.
- `room_blank`: clear the new runtime.
- Knockout narration through `room_narrate`, which already rides the chat lane
  that cannot close a client — exactly what that third lane was built for.

### `src/tetrisd/src/ability_ctrl.c`

- `apply_target(t_game *from, t_game *to, const t_ability_def *def, int arg)`:
  the twelve targeted abilities, each queued against the Target and applied at
  that player's next lock, never on arrival. Dark, Paralysis, Inversion and Nue
  are `effect_apply` on the Target's `t_effect_state`; Pentaris is five garbage
  rows; Vampire is `charge_transfer`; Sirtet is `board_invert`; Bomb is
  `board_clear_cells`; Copy is `board_copy` from the Target; Mirror arms
  `EFFECT_MIRROR` on the caster and is consumed by the *next* incoming ability.
  Every board transform keeps the "try on a copy, keep only if the piece
  survives" discipline `apply_self` already uses.
- `game_ability` grows a Target argument; `ability_handler` resolves it through
  `server_room_target_of` and answers `no-target` only when there genuinely
  isn't one.

Mirror is the one with an ordering hazard worth writing down: it steals *the
next* power used against its holder, so it has to be checked at the moment an
effect is queued against them, not at the moment it lands.

### `src/tetrisd/src/handlers_lobby.c` / `dispatch.c`

- `ready_handler`, and its row in `g_routes`.
- `start_handler` keeps its owner check for Battle Royale; Double reaches
  `server_room_start` through readiness instead.

### `src/tetrisd/README.md`

Routes, statuses, the countdown, the Target rule, and the new `STATE` section.

---

## Client work

### `src/tetrisu/tetrisu_net.h` + `net_client.c`

- `net_send(net, method, path, body)` — the send-only path of
  [D5](#d5--gameplay-inputs-stop-waiting-for-their-reply).
- `net_pump` handles responses: `429` sets a back-off deadline the caller
  respects, everything else is counted and dropped.
- `net_state_take` keeps the whole decoded body, opponents included.
- Bind and clear `play_path` per [D6](#d6--the-client-binds-its-play-path-when-it-is-seated-not-when-it-starts).

### `src/tetrisu/src/net_match.c` (new)

The Double sibling of `net_solo.c`, and it should look almost exactly like it:
`net_match_apply(net, t_mp_match_state *state)` decodes the snapshot into
`state->local_game` (reusing `net_solo_apply`'s field mapping — extract the
shared half rather than copying it) and the opponents section into
`state->opponent_game` and `state->opponents[]`.

Presentation stays local, as `docs/tetrisu-local-to-tetrisd.md` requires: the
clear animation, the danger tint, the popover, the score banner. The countdown
now comes from the wire but is still *drawn* here.

### `src/tetrisu/src/match_authority.c` (new)

`solo_authority.c`'s sibling, and for the same reason it exists: the match loop
must not ask "am I online?" per action. It answers to the server when there is
one and to the local fixture when there is not — the fixture opponent is worth
keeping exactly as `solo_authority` keeps the local rules, because it is what
makes the match screen testable and demonstrable without a server.

No `countdown_hold`: [D2](#d2--the-match-countdown-is-the-servers) removed the
need for it.

### `src/tetrisu/src/multiplayer_match_mode.c`

- Take a `t_net_client *`, and poll the session fd beside the terminal the way
  `wait_solo_input` does (`solo_mode.c:349`) — `wait_match_input` currently polls
  the terminal alone, so a pushed frame would wait out the timeout.
- Route every action through the authority instead of `solo_game_apply_action`.
- Drop the local `solo_game_update` of the opponent.
- `select_power` sends `ABILITY` and shows the server's verdict, the way
  `net_solo_ability_feedback` already does for Solo.
- Finish on the server's word: won, lost, or rank.

### `src/tetrisu/src/multiplayer_match.c`

- `mp_match_state_init` stops seeding a fixture opponent when there is a
  session.
- Fix F10's dead `layout->valid` store and the doubled `mp_match_apply_room`.

### `src/tetrisu/src/waiting_room_screen.c` + `main.c`

- Character select moves here, ahead of `START`, and its result is a `READY`
  (D7).
- The local auto-start countdown gives way to the server's room state: the
  client launches when the room says `IN_GAME` or when the first `STATE`
  arrives, whichever is first.
- `net_provider.c` gains `ready_room`.

### `src/tetrisu/src/render_multiplayer_match_pixel.c`

Band the opponent board and give it a presentation floor
([D8](#d8--the-opponent-board-is-presented-at-its-own-rate)).

---

## Tests

Every one of these is a real server driven over a real socket, because that is
what the existing suites do and it is the only way the wire is actually
exercised.

### `libstatusbody` (`lib/libstatusbody/tests/test_state.c`)

- round-trip with 0 and 1 opponents;
- a `countdown` phase round-trips with its milliseconds;
- `opponents 2` followed by one board is `EBADMSG`, not a short read;
- a Single body encoded by the new code is byte-identical to one encoded before
  it — the cheapest proof that the Solo path did not move.

### `tetrisd` (`src/tetrisd/tests/`)

New `test_double.c`, driving two harness clients through `fx_start`:

- two players join `D-01`, both `READY`, the room starts without an owner
  `START`;
- both receive `phase countdown` frames, then `active` on the same tick, and
  neither board advances during the hold;
- each player's snapshot carries exactly one opponent, and it is the other
  player's board;
- a double line clear by A raises `pending` on B's projection and injects
  exactly one row at B's next lock — not before it, which is the assertion that
  catches the whole class of bug D4 exists to avoid;
- A tops out: the match ends on that tick, A is recorded `won=false`, B `true`,
  each once (read back through `db_get_player`);
- A quits mid-match: same outcome, B wins by default (UC-11 5a);
- a targeted ability from A lands on B at B's next lock and nowhere else;
- `READY` with an unowned character is `403` and does not mark the slot;
- `RESTART` and `PAUSE` are still `not-single` in a Double room.

`test_room.c`'s existing stream-numbering case must keep passing unchanged.

### `tetrisu`

- `tests/test_multiplayer_match.c`: the opponents section maps onto
  `opponent_game`; a snapshot with `opponents 0` leaves the fixture opponent
  alone.
- `tests/integration/test_net_double.sh`, on the model of `test_net_solo.sh` and
  `tetrisd_fixture.sh`: two `net_smoke`-style clients against one server,
  through join → ready → countdown → play → garbage → win, asserting on decoded
  snapshots rather than on anything drawn.

### Manual

`make play` with two terminals, which is the only test that catches F9's frame
cost.

---

## Order of work

Each step leaves the tree building and every existing suite passing.

| # | Step | Why here |
|---|---|---|
| 1 | `libstatusbody`: opponents section + countdown phase | Everything else encodes into it, and it can land with Single still the only user |
| 2 | `tetrisd`: countdown phase, mode-aware `room_is_over`, per-player `won` | Makes a Double match *end* correctly even with no second board on the wire |
| 3 | `tetrisd`: `fill_opponents` in the tick | Two real boards on the wire; the client can now be pointed at them |
| 4 | `tetrisu`: `net_match.c`, `match_authority.c`, loop and fd wiring | The match becomes real. Stop here and it is playable without abilities or garbage |
| 5 | `tetrisu`: send-only inputs, soft-drop rate, opponent banding (D5, D8, F8, F9) | See below — this moved ahead of the garbage work on purpose |
| 6 | `tetrisd`: garbage — `pending_garbage`, Target, drain at lock | The mode becomes competitive |
| 7 | `READY` + character (D7), waiting-room select, server-side auto-start | Abilities resolve against the right character; the ready button becomes real |
| 8 | `tetrisd`: the twelve targeted abilities | Needs 6 and 7 |

Steps 1–4 are the mode; step 5 makes it playable at a distance; steps 6–8 are
the game.

**Why the latency work is step 5 and not step 8.** It was ordered last on the
reasoning that it is best measured against a real match. That reasoning holds
only if the server is on loopback. The target here is a `tetrisd` on a cloud
host, and across a real link F7 and F8 stop being polish:

- every input is a blocking round trip inside the render loop, so at 40 ms RTT
  a key held at `SOLO_DEFAULT_ARR_MS` (33 ms) issues requests faster than
  replies return, and both boards stall on each one;
- soft drop past level 6 emits more than 100 requests a second against a
  60/s budget, so the server starts answering `429` to a player who is merely
  holding a key;
- a dropped reply freezes both boards for `NET_REPLY_TIMEOUT_MS` — four
  seconds — which on a lossy link is a lost match rather than a bad moment.

None of that is visible on localhost and all of it is fatal off it. Step 4
leaves a match that works on one machine; step 5 is what makes step 4's result
true anywhere else, and it has to be in place before garbage makes the exchange
rate of a dropped input somebody else's advantage.

---

## Open questions

1. **Soft drop's request rate (F8/D5).** Clamp the client's repeat, or raise
   `TETRISD_INPUT_RATE`? The clamp is better wire behaviour but changes handling
   feel, which is a thing players notice. Needs a number picked against a real
   match over a real link, not a guess — and step 5 is where the number gets
   picked.
2. **Does the fixture opponent survive?** Keeping it (as `solo_authority` keeps
   the offline rules) costs a branch and buys a demonstrable match screen with
   no server. Recommended, but it is a call.
3. **Rematch.** A Double room is destroyed when its match ends, exactly as a
   Single one is, so "play again" is currently "go back and make another room" —
   and F2's stale-binding `404` is the current symptom of nobody having decided.
   Out of scope here, but the waiting room needs an answer before this ships.
4. **Battle Royale's compact opponent encoding.** Sketched under
   [Wire changes](#wire-changes) and deliberately not built. The thing to avoid
   is letting step 7 discover that the Double body cannot be scaled — hence the
   field order.
