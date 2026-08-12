# Battle Royale — implementation plan

Step 7 of the event-driven migration, and the last mode: make a 4–99 player
match a thing `tetrisd` runs, `tetrisu` renders at a rate a terminal can carry,
and a person can read at a glance.

This document is the design and the order of work. It follows the shape of
[`double-mode-plan.md`](double-mode-plan.md) deliberately: what both halves
actually do today (with the file and line, because several of the findings read
as features until you follow them), the arithmetic that forces most of the
decisions, the exact wire changes, the UX the mode needs to feel finished, and
the tests that hold each piece down.

Double is the floor this stands on. Everything it built — the server-held
countdown, `READY` with a character, the Target rule, garbage at the receiver's
lock, the eleven targeted abilities, `room_rematch` — is reused unchanged.
Battle Royale is not a second match implementation; it is the same one with a
Target that has to be *chosen*, an outcome that is a *placing* rather than a
verdict, and ninety-eight boards that cannot be sent the way one board is.

## Table of Contents

- [Where the two halves stand](#where-the-two-halves-stand)
  - [What already works](#what-already-works)
  - [The arithmetic that decides the design](#the-arithmetic-that-decides-the-design)
  - [Findings in `tetrisd`](#findings-in-tetrisd)
  - [Findings in `tetrisu`'s Battle Royale screen](#findings-in-tetrisus-battle-royale-screen)
- [Decisions](#decisions)
- [Wire changes](#wire-changes)
- [Server work](#server-work)
- [Client work](#client-work)
- [UX and UI](#ux-and-ui)
- [Tests](#tests)
- [Order of work](#order-of-work)
- [Open questions](#open-questions)

---

## Where the two halves stand

### What already works

More than the status table suggests, and none of it needs replacing.

A Battle Royale room can already be **created, listed, joined, chatted in and
left**. `read_mode` accepts `mode br` and `mode battle-royale`
(`handlers_lobby.c:306`), `apply_mode_shape` gives the room `br_slots` seats
with a floor of four and a `min_to_start` of four
(`lib/libtetrisroom/src/room.c`, `apply_mode_shape`), `lobby_create_room` names
it `BR-nn`, and `room_can_accept` deliberately does **not** refuse a `READY`
room — which is exactly the "a `READY` room keeps accepting joiners up to
`slot_count`" rule `docs/use_cases.md:263` asks for. `t_room` already carries
`ROOM_MAX_SLOTS` (99) slots and `BODY_ROOM_MEMBERS_MAX` is already 99
(`statusbody.h:45`), so the waiting room's roster is 99-ready on both ends.

The match machinery is mode-agnostic where it counts. `deal_games` deals every
occupied slot, `server_rooms_tick` advances every room off the one `timerfd`,
`room_is_over` is already "fewer than two games still active"
(`room.c:1583`) — which is the correct Battle Royale end condition, not just
Double's — `award_game` is already per player, and `room_rematch` already keeps
the room alive so the survivors come back to the seats they never left. The
chat lane exists *because* of this mode: the third outbox lane was built so a
room narrating a Battle Royale's knockouts could not fill the response FIFO and
kill a slow connection (`tetrisd.h:338-362`).

The client has a whole Battle Royale presentation already authored: the arena
grid with its own layout, the targeting diamond, the KO/alive HUD, the compact
card renderer, and a compatibility renderer beside the pixel one.

So the missing pieces are narrower than "the mode":

1. **ninety-eight boards on the wire**, which the current body cannot carry at
   any price (see the arithmetic below);
2. **a Target that is chosen**, rather than "the other seat";
3. **a placing**, rather than won/lost;
4. **a client fed by the server**, rather than by the fixture it still runs; and
5. **the mode's own rules** — owner-started, late joiners, knockouts, spectating
   after elimination.

### The arithmetic that decides the design

Measured on this tree (`sizeof` under the project's own headers):

| Thing | Bytes | At 99 |
|---|---:|---:|
| `t_game` | 1 920 | 190 KB per room |
| `t_server_room` (`TD_MAX_GAMES` 16) | 30 984 | ~191 KB |
| `t_server` | 2 403 896 | ~12.6 MB |
| `t_body_state` (1 opponent) | 1 064 | — |
| `t_body_opponent` | 496 | 48.6 KB in one snapshot |
| `snaps[TD_MAX_GAMES]` on the tick's stack | 17 024 | **103 KB** |

> **Corrected.** This row read **4.8 MB** in the first draft, and that figure is
> reachable only in a design [D1](#d1--the-arena-is-a-second-detail-level-not-a-second-body)
> rejects — it is 99 × `sizeof(t_body_state)` *with* `BODY_OPPONENTS_MAX` also
> raised to 99 (99 × 496 = 49 104 per state). With `BODY_OPPONENTS_MAX` held at
> 1, `sizeof(t_body_state)` stays 1 064 and the array is 105 336 bytes. Measured,
> not inferred. The ordering argument below survives the correction and is
> restated on the real number.

Three of those numbers are the whole design:

- **`t_body_opponent` × 98 = 48.6 KB inside one `t_body_state`.** The struct
  would be bigger than the frame cap on its own. The full-fidelity opponent
  projection does not scale, exactly as `statusbody.h:46-57` already says in as
  many words.
- **Encoded, one full opponent board is ~470 bytes** (a header line plus 20
  lines of 20 hex chars). Ninety-eight of them is **46 KB per snapshot**, against
  `TETRISD_BODY_MAX_BYTES` of 8192 (`tetrisd.h:127`) and a 64 KiB frame
  (`HTTTP_MAX_MESSAGE_SIZE`). Even if it fit, 99 clients × 46 KB at the tick
  rate is **380 MB/s**. It is not a tuning problem.
- **A 1-bit-per-cell mask is ~80 bytes an opponent** — 20 rows × 10 bits = 200
  bits = 50 hex chars, plus a short header. Ninety-eight of them is 7.8 KB,
  which fits a frame but not the current body cap, and still costs 65 MB/s if
  pushed every tick to everybody.

So the mask is necessary and not sufficient: the arena also needs its **own
cadence**. At 5 Hz a full 99-card arena is ~10 KB a push, ~50 KB/s per client
and ~5 MB/s for a full 99-player room — and ~36 KB/s at the eight seats this is
expected to default to.

The obvious further saving — send only the cards that changed — is refused, and
not on cost grounds: the STATE lane cannot tell the server which snapshot a
client received (S16), so a delta scheme has nothing to advance its bookkeeping
against. See [D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock).

### Findings in `tetrisd`

**S1 — a Battle Royale room cannot hold more than 16 players.** `TD_MAX_GAMES`
is 16 (`tetrisd.h:177`) and `config_set` clamps `br_slots` to it
(`config.c:126`), so `TETRISD_BATTLE_ROYALE_SLOTS=99` is silently a
configuration error. The domain is already 99-wide; only the runtime is not.

**S2 — `TETRISD_DEFAULT_MAX_CLIENTS` is 64** (`tetrisd.h:97`). A 99-player room
cannot be filled by a server that will not accept 99 connections.

**S3 — the room deals itself without the owner.** `ready_handler` opens the
select window as soon as every occupied seat is ready and the room is not Single
(`handlers_ready.c:66-68`). For Double that *is* the rule. For Battle Royale
`docs/use_cases.md:560` says the Owner presses Start — so today a Battle Royale
room with 4/20 seats filled and all four ready would start itself, and the
sixteen players still joining would find the room `IN_GAME`.

**S4 — the Target is "the first other live slot".** `server_room_target_of`
(`room.c:1342`) walks the slots and returns the first one that is alive. In
Double that is the only answer. In Battle Royale it means **every player attacks
the lowest-numbered survivor**, which is not a targeting rule, it is a pile-on.
`docs/CONTEXT.md:44-48` requires a draw from the room's seeded random source.
The function was deliberately left as a function for exactly this
(`room.c:1334-1341`).

**S5 — no randomness source exists in a room.** `libtetrisbrain` takes none by
design, `deal_games` derives a per-game seed from the clock and the player id
(`room.c:965`), and there is nothing per room. Target selection needs one that
is the room's, so a match is reproducible from its seed for a test.

**S6 — rank is decided at the end, for everybody at once.** `settle_results`
(`room.c:1631`) writes `rank = count_live_games(...) + 1` to every inactive
slot at the moment the match ends — so in a 20-player match, all nineteen losers
are rank 2. A placing has to be taken **when the player is eliminated**, because
that is the only moment it is a fact.

**S7 — there are no knockouts.** Nothing records who buried whom, so `ko_count`
has no source, the "KOs" targeting mode has nothing to sort on, and the KO feed
the chat lane was built for has nothing to narrate.

**S8 — nothing tracks who is attacking whom.** `settle_garbage` (`room.c:1286`)
queues rows against a Target and forgets the sender immediately, so the
"Attackers" targeting mode and the client's `targeting_local` flag have no
source either.

**S9 — one player's move makes ninety-nine snapshots.** `spread_dirty`
(`room.c:1248`) marks every game in a non-Single room dirty as soon as any of
them changes, because in Double every frame carries both boards. At 99 players
that is 99 encodes and 99 seals per tick, ~83 times a second, for a body that
mostly repeats itself.

**S10 — `fill_opponents` is capped at one.** `room.c:1433` stops at
`BODY_OPPONENTS_MAX` (`statusbody.h:57`), so a Battle Royale client is currently
told about exactly one rival — whichever slot comes first.

**S11 — a forfeit mid-match does not place the leaver.** `server_room_forfeit`
(`room.c:691`) records the game and releases the seat, which is right, but it
assigns no rank; and `room_is_over` is only evaluated on the next tick, which is
correct but means the ordering of "leaver placed" and "match ended" is
accidental rather than decided.

**S12 — the select window refuses late joiners, which is right, and the
`WAITING → READY → SELECTING` path has no Battle Royale shape.** A 40-seat room
that reaches 4 ready is `READY`; the owner's `START` opens the window
(`server_room_start`, `room.c:418`) and `room_can_accept` then refuses
newcomers. That is the behaviour we want — but nothing tells the twelve people
still in the lobby that the room they were about to join has closed, and nothing
decides what happens to seats that are occupied but *not* ready when the owner
starts.

**S13 — `TETRISD_BODY_MAX_BYTES` is 8192** (`tetrisd.h:127`), and every handler
builds its body into `t_request_context.body` of that size. The arena section
needs more, and the constant is on the request path as well as the push path.

**S14 — one player leaving cancels the whole select window.**
`server_room_forfeit` (`room.c:713-721`) calls `room_abort_selection` and stops
ticking whenever the room is `SELECTING`, unconditionally. In Double that is
correct — one of two players leaving means there is no match. In Battle Royale
it hands any single player a cancel button: thirty people join, the owner
starts, and one disconnect two seconds into character select ends it for
everybody. See [D14](#d14--a-battle-royale-select-window-survives-a-departure).

**S15 — a slot is not a stable identity, and three per-slot arrays already
disagree because of it.** `room_release` → `promote_into`
(`lib/libtetrisroom/src/release.c`) *moves* a promoted successor into the
vacated owner's slot and clears the one they were in.
`rehome_successor` (`room.c:1852`) follows that move for `games[]` and
`dirty[]` — and for nothing else. `character[]`, `result[]` and `rank[]` are
indexed by the same slot and are not moved.

The consequence is live today, in Double: if the owner leaves during the select
window, the successor's declared fighter stays attached to their old slot.
`server_room_all_locked` then reads `character[0] == 0` for an occupied seat and
never returns true, so the room waits out the full `TETRISD_MATCH_SELECT_MS`
and deals the successor whatever their account has equipped rather than what
they picked. It is invisible because the fallback is silent.

This is the finding behind [D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot):
anything that must survive a match cannot be keyed on a slot index.

**S16 — a pushed `STATE` is not delivered, it is *mailed*.**
`outbox_push_state` (`outbox.c:64-75`) frees whatever snapshot was pending and
replaces it, and `load_frame` (`clientio.c`) seals exactly one message into a
single send buffer at a time. So between a snapshot being built and it reaching
the socket there is a window in which the next snapshot destroys it. This is
deliberate and correct — a snapshot supersedes the one before it — but it means
**the server can never know which snapshot a client actually received**, which
rules out any acknowledged-delta scheme on this lane.
See [D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock).

### Findings in `tetrisu`'s Battle Royale screen

**U1 — the arena is a fixture and always has been.** `mp_match_apply_room`
(`multiplayer_match.c:75-136`) fills every opponent card from
`seed_opponent_board`, a deterministic pattern keyed off the card's index
(`multiplayer_match.c:641`), and sets `garbage_pending` to
`(index * 3 + 1) % 5`. Those boards are written once and never advance. Online,
`net_match_apply` overwrites the first `snap->opponent_count` cards — that is
**at most one** — so a live Battle Royale would show one real board and
ninety-seven decorative ones.

**U2 — `players_alive` is computed from the opponents the frame carried.**
`apply_opponents` (`net_match.c:259-268`) counts `snap->opponents[i].alive` and
adds the local player, so with `BODY_OPPONENTS_MAX` of 1 the HUD would read
`ALIVE 2/40`. The alive count is a room fact and has to come from the room.

**U3 — `ko_count` is never written.** It is read by the HUD
(`render_multiplayer_match.c:321`) and by the pixel HUD, and no line in the tree
assigns it. Same for `incoming_attackers`, set once in
`mp_match_state_init` (`multiplayer_match.c:53`) to the constant 2, and
`targeting_local`, set only by the fixture (`multiplayer_match.c:115-116`).

**U4 — the four targeting modes are local decoration.** `mp_match_target_handle_key`
(`multiplayer_match.c:255`) moves `state->target_mode` and writes a status
banner; nothing is sent, and the server has no such concept. W/A/S/D are
currently a mode switch the game does not have.

**U5 — the opponent array is dense, and deltas need it sparse.**
`opponents[APP_ROOM_MAX_PLAYERS - 1]` (`tetrisu.h:2595-2603`) is indexed by
position in the frame, not by slot. A frame that carries only the cards that
changed cannot be applied to it without reshuffling every card, and a
disconnected player cannot be distinguished from a card that simply was not sent
this time.

**U6 — the arena is re-encoded whole whenever any card changes.**
`regions_battle` (`render_multiplayer_match_pixel.c:1019`) hashes each half of
the opponent array and, on any difference, redraws the entire side region and
recreates its plane (`regions_side`). This is finding F9 of the Double plan at
forty-nine times the area: at 99 players some card changes on essentially every
frame, so both halves of the arena are re-encoded continuously — precisely the
cost `MP_MATCH_BOARD_BANDS` was introduced to remove for the local board.

**U7 — the arena signature hashes raw structs.** `opponents_signature`
hashes `count * sizeof(state->opponents[0])` bytes — ~1.7 KB per card, ~170 KB
for both halves — per frame. It is also hashing padding, so it is a correctness
hazard as well as a cost: two identical arenas can hash differently.

**U8 — the mode's grid is uniform, so 98 cards get 98 equal slices.**
`opponent_grid` (`render_multiplayer_match_pixel.c`) picks the column count that
maximises tile size, and `draw_opponent_region` lays every card out on that
grid. At 98 cards in two side columns the tile floor is 1 px, which is a
smudge — there is no notion of *which* rivals deserve the room (the ones
attacking you, the ones about to die, the leaders).

**U9 — the layout gate is severe and silent.** `mp_match_layout_build`
(`multiplayer_match.c:349`) requires `rows >= 36 && cols >= 132` for Battle
Royale, and `mp_match_pixel_layout_build` returns an invalid layout below
640×480 px. A terminal one row short gets no screen and no explanation.

**U10 — `MP_BR_OPPONENT_COUNT` (98) is defined and used nowhere**
(`tetrisu.h:726`). Every loop bounds itself on `APP_ROOM_MAX_PLAYERS - 1`
instead, so the constant is a comment.

**U11 — the local top-out ends the match against the fixture only, and the
fixture's rank is `players_alive`** (`multiplayer_match_mode.c:225-232`). That
is right online — the server's verdict decides — but it means the whole
"eliminated, now spectating" state has never been drawn: online, a player who
tops out sees their dead board and nothing else until the match ends.

**U12 — `mp_match_finish` floors a loss at rank 2** (`multiplayer_match.c:285`),
which is correct for Double and wrong for a 40th place. The server's rank is
passed in and then partly discarded.

**U13 — the waiting room already has the right Battle Royale rules.**
`waiting_room_can_start` requires the owner, ≥ 4 players and every seat ready
(`waiting_room_screen.c:203-227`), and the roster is 99-capable. This is the
one part of the client that needs almost nothing.

---

## Decisions

### D1 — the arena is a second detail level, not a second body

`t_body_state` keeps its `opponents` section exactly as Double built it —
`BODY_OPPONENTS_MAX` stays **1**, and Double's frame does not change by a byte.
Battle Royale adds an **arena section** below it: one line per opponent slot,
each carrying a 1-bit-per-cell occupancy mask instead of a full cell grid.

Why a second level rather than widening the existing one: the full projection
carries a colour nibble and a type nibble per cell because Double draws the
rival's board at nearly the size of your own. A Battle Royale card is a
thumbnail — the shape of the stack, whether it is dying, whether it is aiming at
you. Colour per cell is information the screen cannot show and the wire cannot
afford. The mask is 1/16th the bytes and loses nothing the player can see.

`t_body_state` therefore gains a fixed-size arena array sized by a new
`BODY_ARENA_MAX` (99), of a compact `t_body_arena_slot` (~40 bytes) rather than
a `t_body_opponent` (496) — ~4 KB in the struct rather than 48 KB.

### D2 — the tick encodes per slot; it stops collecting first

`advance_and_push` collecting `t_body_state snaps[TD_MAX_GAMES]` on the stack
was fine for 16 and is 103 KB at 99 — and grows to over half a megabyte once
the arena section is in the body. It becomes a loop that builds one snapshot,
encodes it, pushes it, and reuses the same buffer — one `t_body_state` on the
tick's stack instead of ninety-nine. Nothing else about the pass changes; the
ordering guarantee it provides (advance everything, settle garbage, *then*
snapshot) is preserved because those are still three passes over the room.

This is the change `statusbody.h:46-57` predicted, and it is worth doing in the
same step as D1 even though it is invisible.

### D3 — the arena is a full compact snapshot on its own clock

The player's own board is pushed as it is today — the moment it changes. The
arena section rides a **cadence** (`TETRISD_BR_ARENA_MS`, default 200 ms = 5 Hz)
and, when it rides, carries **every occupied slot**. No deltas, no
acknowledgement, no per-client bookkeeping.

The obvious optimisation — send only the cards that changed since this client
last saw one — is **wrong on this lane**, and S16 is why. `outbox_push_state`
frees the pending snapshot and replaces it, so a snapshot carrying "slots 3, 17
and 44 changed" can be destroyed by the next tick's snapshot before it ever
reaches the socket. Advancing a per-client `arena_seen` at encode time would
then record as delivered a card the client never received, and that card would
never be sent again: the arena would silently and permanently diverge, worst on
exactly the slow connections the mailbox exists to protect.

There are two ways out and only one of them is worth its weight. The careful
one is to advance `arena_seen` at the moment a snapshot leaves the mailbox and
is sealed into the send buffer (`load_frame`), which is the first point it is
no longer replaceable. That works — but it puts a delivery-tracking invariant
inside the write path, for a lane whose entire design premise is that it does
not track delivery.

The other is to make loss not matter. A full arena is idempotent: a dropped one
costs nothing because a complete replacement arrives 200 ms later. That is the
same philosophy the STATE mailbox already runs on — "a snapshot supersedes the
one before it" — applied one level down, and it deletes `arena_gen`,
`arena_seen`, the generation comparison and the reasoning about what a client
has seen. **Take the full snapshot.**

The card list is then also the roster: a client replaces its whole arena from
each push, so a seat nobody is in is gone by not appearing, and no separate
`roster` line is needed. An *eliminated* player's card is still sent, with its
alive bit clear, because their final board is what the arena shows for the rest
of the match — absence means "not in this room", never "knocked out".

What it costs, honestly: a full arena at 99 players is ~10 KB, so 99 clients at
5 Hz is **~5 MB/s** against the ~1.8 MB/s a delta scheme would use. At the size
this is expected to ship (see [Open questions](#open-questions) 2 — a default of
8, a tested ceiling of 24) it is 36 KB/s and 280 KB/s respectively. The delta
scheme buys nothing at those sizes and costs a correctness invariant on the
write path at every size.

A frame that carries no arena says so (`arena absent 0`) and the client keeps
what it has. Single and Double always send `arena absent 0`.

### D4 — Battle Royale does not spread dirty

`spread_dirty` (`room.c:1248`) exists because a Double frame carries both
boards, so anyone's move stales everyone's frame. In Battle Royale the rival
boards ride the arena section, which has its own clock — so a player's own frame
is dirty only when their own board changes, and the arena is dirty on its timer.
`spread_dirty` becomes Double-only, which is where its comment already says its
reasoning comes from.

Without this, S9 stands: 99 encodes and seals per tick for frames whose only
change is somebody else's piece.

### D5 — the Target is drawn by the sender's declared mode, from the room's own RNG

`server_room_target_of` grows a mode and a candidate set. Four modes, matching
the four the client already draws (`multiplayer_match.c:263-270`) and the
Tetris 99 idiom they came from:

| Key | Mode | Candidate set |
|---|---|---|
| `A` | Randoms *(default)* | every live opponent |
| `W` | KOs | live opponents with the highest stack — closest to topping out |
| `S` | Attackers | live opponents whose garbage has landed on you within `TETRISD_BR_ATTACKER_MS` |
| `D` | Badges | live opponents with the most knockouts |

Every mode narrows a candidate set and then **draws from it at random**, so
`docs/CONTEXT.md`'s "drawn per resolution from the room's seeded random source"
stays literally true — the mode chooses the urn, not the ball. An empty
candidate set (no attackers yet, everyone tied) falls back to Randoms rather
than dropping the attack, so a targeting mode can never cost a player the
garbage they earned.

The RNG is the room's: one `uint32_t rng` on `t_server_room`, seeded at deal
time, advanced by an xorshift in `room.c`. No randomness enters
`libtetrisbrain`, and a match replays exactly from its seed — which is what
makes the targeting tests deterministic.

`CONTEXT.md` and `use_cases.md` both need a sentence for this: the Target
definition currently says "drawn per resolution from the room's seeded random
source, among players still in the game", which is now the *default* mode and
the fallback for all of them. Docs are updated in the same step, not after.

### D6 — the mode travels on its own tiny request

`TARGET /room/<name>` with a body of `mode <random|ko|attackers|badges>`,
answered `200` with no body. It is not carried on `MOVE` (which is send-only and
must stay a fixed shape), and it is not inferred from anything — the player
pressed a key and the server has to be told.

It is rate-limited on the **chat** bucket rather than the input one: it is a
person pressing a mode key a few times a match, not a held key, and it must not
cost a piece movement (the same reasoning `tetrisd.h:766-775` gives for chat).

Declaring a mode is remembered per slot for the match and cleared by
`room_blank` with everything else that shares a slot's lifetime.

### D7 — a placing is taken when the player is eliminated, once per tick

The moment a game goes inactive in a Battle Royale room, the room writes that
player's placing — the number of players who were still in the match when the
tick began, including them.

Written at elimination, that number is a fact; written at the end (S6), it is
the same number for everybody.

**Simultaneous eliminations share a placing.** A tick advances every game before
anything is settled, so two players can top out inside the same authoritative
tick — and there is nothing to separate them except the order `tick_once` walks
the slots in. Letting that decide would mean slot 31 places 7th and slot 72
places 8th for identical deaths, which is an array index deciding a result.

So elimination is a **two-pass step per tick**, not a per-slot side effect:

1. after every game has been advanced, collect the slots that went inactive
   this tick;
2. give all of them the same placing — `live_at_tick_start - eliminated_this_tick + 1`
   — and take `live` down by the whole group.

Five players tied on one tick from a field of twenty all place 16th, and the
next elimination places 15th. Placings therefore skip rather than repeat-and-
overlap, which is how every sport handles a tie and is the only rule that does
not need a tiebreak nobody can observe.

The winner is rank 1 and `BODY_RESULT_WON`; everybody else is
`BODY_RESULT_LOST` with their placing. `t_body_state.rank` already exists and is
already documented as being for exactly this
(`statusbody.h`, "`rank` rides with the result because Battle Royale's loss is a
placing rather than a bare defeat").

A player who leaves or disconnects mid-match is placed by the same rule at the
moment they are forfeited (S11), so quitting at 40th place records 40th and not
"lost". A forfeit is its own event and not part of a tick's group, because it
arrives on a request rather than on the clock — it takes the placing alone.

### D8 — a knockout is credited to the last player whose garbage landed

The participant record ([D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot))
gains `last_attacker_id` — the **player id** whose garbage or ability most
recently *landed* on this board. It is stamped at the lock that drains the
queue, not when the attack is queued, so a player buried by rows that arrived
after the sender left still attributes correctly. On a top-out, that player's
`ko` goes up.

It is a player id and not a slot for the reason S15 gives: `promote_into` moves
players between slots mid-match, so a slot recorded now can name somebody else
later. Resolving an id to a slot is a lookup the room already does
(`slot_of_player`, `room.c:352`); resolving a stale slot to a player is not
possible at all.

A top-out with no `last_attacker_id` — a player who buried themselves — is
credited to nobody, which is honest and is also what keeps "KOs" a measure of
aggression rather than of luck. So is a top-out whose attacker has since left:
the record survives, the credit stands.

KO counts are on the wire per card, drive the `Badges` targeting mode, and are
narrated: `room_narrate` on the chat lane, which is the third outbox lane's
entire reason for existing (`tetrisd.h:346-353`).

Deliberately **not** built: a KO-derived attack multiplier. Tetris 99's badge
system is a whole economy, `docs/game-economics.md` does not have one, and the
mode is complete without it. It is named in [Open questions](#open-questions).

### D9 — the owner starts a Battle Royale; readiness only makes it possible

`ready_handler`'s auto-selection (S3) becomes Double-only. In Battle Royale,
readiness moves the room `WAITING → READY` and stops there; the owner's `START`
is what opens the select window, exactly as `docs/use_cases.md:560` says and
exactly as `waiting_room_can_start` already assumes (U13).

Seats that are occupied but not ready when the owner starts are **seated in the
match anyway**. The alternative — evicting them — turns a start into a kick, and
`room_can_start` already refuses the start if the room is below `min_to_start`.
Readiness in Battle Royale is a signal to the owner, not a contract.

### D10 — elimination is not the end of the screen

A player who tops out stays in the match, watching. UC-12 5a says so, the arena
is the reason to, and the client already keeps rendering because the verdict
only arrives with the result field. What is missing is the *state*: the server
already sends `phase topout` with `result none` for an eliminated player, and
that pair — dead board, no verdict yet — is the spectating state. It needs a
rank in it, so the client can say "#37" the moment it is decided rather than at
the end, so `rank` is sent as soon as it is taken (D7) with `result` still
`none`.

### D11 — sizing: raise `TD_MAX_GAMES` to 99 and keep the games inline

`t_server_room.games` at 99 is 190 KB; sixty-four rooms is **12.2 MB**, and
`t_server` grows from 2.4 MB to ~12.6 MB. It is `calloc`'d once
(`server.c:30`), never on a stack, so the cost is one allocation at boot.

The alternative is allocating `games` per room from `server_room_open`, which
saves ~10 MB and adds a failure path to room creation plus a free path to
`room_close` — in the one file whose entire discipline is that a room's two
halves share a lifetime. Twelve megabytes is not worth reopening that. If the
footprint ever matters, the lever is `LOBBY_MAX_ROOMS`, not the games array.

`TETRISD_DEFAULT_MAX_CLIENTS` goes to 128, and `config_validate`'s ceiling
(`TETRISD_MAX_CLIENTS_LIMIT`, 4096) already accommodates it.

### D12 — the client's arena becomes slot-indexed

`t_mp_match_state.opponents[]` is indexed by slot, not by position in the frame
(U5). [D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock) removed
the delta that first required this, but it is still the right shape and now for
plainer reasons: a card keeps its position on screen across pushes, so a rival
does not slide across the arena when somebody above them is knocked out; the
per-card render signature ([D13](#d13--the-arena-is-drawn-per-card-not-per-side))
compares like with like across frames; and a slot is what `TARGET`, the
attacking-you flag and the KO feed all name.

Applying a push is therefore: clear every card, write the ones the push carries.
`present` stops meaning "the fixture filled this in" and starts meaning "this
slot was in the arena the server last sent". The fixture keeps working — it
simply fills slots 1..N-1 instead of positions 0..N-2.

### D13 — the arena is drawn per card, not per side

`regions_battle` splits the arena into two planes and rebuilds a whole plane
whenever any card in it changes (U6). It becomes per-card: each card owns its
own signature (a cheap field hash, not a struct hash — U7), and a card that has
not changed is not redrawn. Cards are grouped into a small fixed number of
**bands** per side, exactly as the local board is banded, so the plane count
stays bounded and a single card's change re-encodes one band rather than half
the screen.

The arena also gets a presentation floor of its own, as the Double opponent has
(`D8` of the Double plan) — it is fed at 5 Hz, so presenting it faster than that
is work with nothing behind it.

And the grid stops being uniform (U8): the cards are **sorted into tiers** by
what the player needs to see — the ones attacking you first, then the ones you
are attacking, then the rest by placing — so the near columns hold the rivals
that matter and the far ones hold the crowd. At 98 players the far cards are
drawn as a compact strip (a height bar plus a danger tint) rather than a 1-pixel
board, which is the honest thing to draw at that size.

Tier membership decides *which column* a card is drawn in, never which array
slot it lives in ([D12](#d12--the-clients-arena-becomes-slot-indexed)) — the
tiering is a sort of an index list, so a card that changes tier moves on screen
without its history being lost.

### D14 — a Battle Royale select window survives a departure

S14: `server_room_forfeit` aborts the window unconditionally, which hands one
player a cancel button for a thirty-person match. The rule becomes the same one
the room already uses everywhere else — **is the room still startable?**

- **Battle Royale:** a departure during `SELECTING` closes the window only if
  the room has fallen below `min_to_start`. Above it, the window keeps running
  on its own clock and the room deals itself as usual. The departing player's
  seat is released and their declared fighter goes with it, so
  `server_room_all_locked` reads a smaller room and can still end the window
  early.
- **Double:** unchanged. Losing one of two players *is* falling below
  `min_to_start`, so the existing behaviour falls out of the same rule rather
  than being a special case beside it.

If the owner is the one who left, ownership transfers exactly as it does now —
and the window does not care, because nothing about `SELECTING` is the owner's
once it is open.

That last point is only true after the parallel-array bug S15 found is fixed,
which is why [D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot)
is not optional for this decision.

### D15 — attribution and match state key on player id, not on slot

S15 is the finding: `promote_into` moves a promoted successor into the vacated
owner's slot, `rehome_successor` follows that move for `games[]` and `dirty[]`,
and `character[]`, `result[]` and `rank[]` are left behind. The bug is live in
Double today (an owner leaving during select strands the successor's fighter),
and every field this plan adds — `ko`, `rank`, `last_attacker_id`,
`target_mode` — is another parallel array with the same hazard, on a mode where
mid-match departures are the normal case rather than the end of the match.

Adding four more arrays to `rehome_successor` is the wrong fix: it makes the
correctness of the mode depend on a list somebody has to remember to extend.

So the per-slot arrays become **one record, keyed on player id**:

```c
typedef struct s_br_participant
{
    t_player_id     player_id;      /* the key; 0 = free */
    t_item_id       character;      /* declared for this match */
    bool            alive;
    int             rank;           /* 0 until eliminated */
    int             ko;
    t_body_result   result;
    t_target_mode   target_mode;
    t_player_id     last_attacker_id;
    t_attacker_ring attackers;      /* (player_id, when_ms), last 8 */
}   t_br_participant;
```

One array of these on `t_server_room`, found by player id. A player moving
between slots then changes nothing about their match state, because their match
state was never filed under where they were sitting — and `rehome_successor`
shrinks to what it was always meant to be: moving a *board* to follow a seat.

It also gives the record a lifetime the board does not have, which is the second
half of why this is worth doing. `forfeit_slot` (`room.c:1743`) copies the game
out and `game_reset`s the slot, so anything living on `t_game` is gone the
moment a player disconnects — including the KO count of a player who quits in
3rd place, and the `last_attacker_id` of a board somebody was about to be
credited for burying. The record survives the board, which is exactly what a
*participant* is.

`character[]` moves into it in the same step, which is what fixes the live
Double bug; `result[]` and `rank[]` follow so there is one place a match's
outcome is written down.

> **Step 0 is landed** (`t_participant` in `tetrisd.h`, `participant_of` /
> `participant_open` / `participant_character` / `participant_close` in
> `room.c`, `test_a_fighter_does_not_move_seats_with_its_owner` in
> `test_double.c`). Three deviations from what is written above, all
> deliberate:
>
> - **It is called `t_participant`, not `t_br_participant`.** `docs/naming.md`
>   §1 is "a reader who has never opened the header should be able to say what
>   a name means", and it fails `rb_drops` for exactly the reason `br_` fails
>   here. The record also holds Single's and Double's `character` and `result`,
>   so naming it after the mode that motivated it would be wrong as well as
>   abbreviated.
> - **It carries four fields, not nine.** `player_id`, `character`, `result`,
>   `rank` — the three arrays it replaces, plus the key. `ko`, `alive`,
>   `target_mode`, `last_attacker_id` and the attacker ring land with the steps
>   that fill them (7 and 8), because a field nothing writes is a field nothing
>   can be wrong about. What step 0 buys those steps is the *home*, which is
>   what makes them additions to a record rather than four more parallel
>   arrays.
> - **A record is released when its player leaves, unless a match is running.**
>   Not specified above, and needed: the array is bounded at `TD_MAX_GAMES`, so
>   a record left behind by someone who left the room is one the next player to
>   sit down cannot have, and a room whose seats turn over enough times would
>   quietly stop recording what anybody declared. The `ROOM_IN_GAME` exception
>   is what keeps the paragraph above true — a player who quits in 3rd place
>   still finished 3rd, and once `forfeit_slot` has reset their board the
>   record is the only thing left holding it.
>
> The bug is verified rather than argued: the new test asserts a successor
> promoted into the departing owner's seat is not wearing the fighter that
> owner declared, and it fails on the pre-fix tree at exactly that assertion.
> 22 of 22 `tetrisd` suites pass.

The attacker ring is eight `(player_id, when_ms)` pairs — 128 bytes — rather
than a `TD_MAX_GAMES`-wide array of timestamps (792 bytes at 99). Eight because
nobody has been usefully attacked by more than a handful of people inside
`TETRISD_BR_ATTACKER_MS`, and the ring answers the `Attackers` targeting mode's
only question: who has landed rows on me recently.

---

## Wire changes

Five changes. The first four are additive and Double's frame is byte-identical
after all of them; the fifth is a constant.

### 1. `t_body_state` gains an arena section

Appended **after** the existing `opponents` block, so every existing line keeps
its position:

```
arena <full|absent> <count>
a <slot> <pid> <flags> <score> <lines> <pending> <ko> <rank> <mask>
                                   ... repeated <count> times
```

- `arena absent 0` means "nothing about the arena this frame; keep what you
  have", and no `a` lines follow. Single and Double always send `arena absent 0`,
  which is how their bodies stay identical in every field that matters and gain
  one fixed line — the codec's existing idiom (`clearing` has always been
  present with a count of 0).
- `arena full <n>` is the **complete** occupied roster, so the card list is the
  roster and there is no separate roster line: a client replaces its whole arena
  from the push, and a seat that does not appear is a seat nobody is in.
  See [D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock).
- `pid` is the player id, because attribution and the KO feed name a player and
  a slot is not one ([D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot)).
  `slot` stays because it is where the card is drawn.
- `flags` is a bitfield: alive, attacking-you, targeted-by-you, in-clear.
- `mask` is 50 hex chars — 20 rows of 10 bits, MSB-first per row, 1 = occupied.
  Cell type and colour are deliberately not carried; see [D1](#d1--the-arena-is-a-second-detail-level-not-a-second-body).
- `rank` is 0 while the player is alive and their placing once they are out.

An eliminated player's card is still sent, alive bit clear — absence means "not
in this room", never "knocked out".

### 2. `t_body_state.alive` and `.players` — the room's own counts

Two integers on every frame, because the HUD's `ALIVE 34/40` must be readable
from a frame that carries no arena at all (U2). They are always written; Single
and Double send `1 1` and `2 2`.

### 3. `TARGET /room/<name>`

| Method | Path | Auth | Body | Answers |
|---|---|---|---|---|
| `TARGET` | `/room/<name>` | ✓ | `mode <random\|ko\|attackers\|badges>` | `200`, `400` on an unknown mode, `404` when not seated, `409` outside a Battle Royale match |

Client-initiated, `application/tetris-command`, one row in `g_routes`
(`dispatch.c:4`) and one in `libhtttp`'s method table.

### 4. `t_body_room` — nothing

The waiting room already carries `slot_count`, `min_to_start`, per-member
`ready` and `character`, and `BODY_ROOM_MEMBERS_MAX` is already 99. Battle
Royale needs no room-body change at all, which is why U13 is the shortest
finding in this document.

### 5. The body cap is derived, not chosen

`TETRISD_BODY_MAX_BYTES` is 8192 (S13), and a full arena does not fit it. The
replacement is not a rounder guess — it is the encoded maximum, computed from
the constants that produce it, so a field added to a card cannot silently
overrun the buffer that carries it.

One `a` line, worst case, field by field:

| Field | Bytes |
|---|---:|
| `a ` | 2 |
| `slot` + space | 3 |
| `pid` (uint64) + space | 21 |
| `flags` + space | 3 |
| `score` (uint64) + space | 21 |
| `lines` + space | 5 |
| `pending` + space | 4 |
| `ko` + space | 3 |
| `rank` + space | 3 |
| `mask` (20 × 10 bits) | 50 |
| newline | 1 |
| **total** | **116** |

So `BODY_ARENA_LINE_MAX` = 116, and the arena section's ceiling is
`14 + BODY_ARENA_MAX × BODY_ARENA_LINE_MAX` = 11 498 bytes at 99. The rest of a
Battle Royale body (no opponents section — it sends `opponents 0`) is ~520
bytes, giving `BODY_STATE_MAX_BYTES` ≈ **12 KB**.

`TETRISD_BODY_MAX_BYTES` therefore becomes `16384`: the computed ceiling rounded
up to the next power of two, with a `_Static_assert` in `statusbody.h` tying it
to `BODY_STATE_MAX_BYTES` so the relationship is checked by the compiler rather
than by this table. It is comfortable against the 64 KiB frame cap
(`HTTTP_MAX_MESSAGE_SIZE`), and it is a buffer that lives once per request
context, not once per client.

Two of those fields are worth questioning rather than paying for: `pid` and
`score` are 21 bytes each and together are 36% of the line. `pid` earns it
([D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot)).
`score` does not obviously — no card at thumbnail size draws it, and the
tiering sorts on placing and attacks. Dropping it would take the line to 95 and
the section to 9.4 KB. Left in for now because the compatibility renderer's HUD
reads it; revisit when the tiering is built.

---

## Server work

Per file, in the order the dependencies allow.

### `lib/libstatusbody`

- `statusbody.h`: `BODY_ARENA_MAX` (99), `BODY_ARENA_LINE_MAX` (116),
  `BODY_STATE_MAX_BYTES` (derived), `t_body_arena_slot`, `t_body_state.arena`,
  `.arena_count`, `.arena_present`, `.alive`, `.players`.
  `BODY_OPPONENTS_MAX` stays 1, with a comment saying why it now stays 1
  *permanently* rather than pending the migration's step 7 — the arena is what
  its existing comment was waiting for, and it arrived beside it rather than
  replacing it.
- `src/state.c`: encode and decode the arena section and the mask codec.
  `EBADMSG` for a count that does not match the `a` lines that follow, a mask
  that is not 50 hex chars, a slot outside `BODY_ARENA_MAX`, a duplicate slot,
  and an `a` line under `arena absent`.
- `README.md`: the body format table.

The round-trip law (`decode(encode(x)) == x`) covers all of it, and a Single
body encoded by the new code must be byte-identical to one encoded before it
except for the three new fixed lines — the cheapest proof the Solo and Double
paths did not move.

### `src/tetrisd/include/tetrisd.h`

- `TD_MAX_GAMES` 16 → 99; `TETRISD_DEFAULT_MAX_CLIENTS` 64 → 128;
  `TETRISD_BODY_MAX_BYTES` 8192 → 16384, with the `_Static_assert` against
  `BODY_STATE_MAX_BYTES` that makes it derived rather than chosen.
- `TETRISD_BR_ARENA_MS` (200), `TETRISD_BR_ATTACKER_MS` (8000),
  `TETRISD_BR_ATTACKER_RING` (8).
- `t_target_mode` (`TARGET_RANDOM`, `TARGET_KO`, `TARGET_ATTACKERS`,
  `TARGET_BADGES`).
- `t_br_participant` and the attacker ring
  ([D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot)).
- `t_server_room`: `rng`, `arena_ms`, `alive`,
  `participants[TD_MAX_GAMES]` — **replacing** `character[]`, `result[]` and
  `rank[]`, which are folded into it rather than joined by four more siblings.
- `t_game`: nothing new. Every field this mode adds outlives the board it would
  have lived on (S15), so all of it is on the participant record.
- `t_room_binding`: nothing new. The arena is a full snapshot, so there is no
  per-client bookkeeping ([D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock)).

### `src/tetrisd/src/room.c`

The bulk of it, again, and for the same reason: everything here needs both
halves of a Room.

- `participant_of(server_room, pid)` — the one lookup everything else goes
  through, and the reason nothing below indexes a match fact by slot.
- `deal_games`: seed `server_room->rng` from the room, open a participant record
  per seated player with `target_mode = TARGET_RANDOM` and `alive = true`.
- `advance_and_push` → a per-slot encode-and-push loop ([D2](#d2--the-tick-encodes-per-slot-it-stops-collecting-first)).
  One `t_body_state` on the stack.
- `spread_dirty`: Double only ([D4](#d4--battle-royale-does-not-spread-dirty)).
- `arena_tick(server_room, elapsed_ms)`: spend `arena_ms`, and when it expires
  mark every player as owing an arena. No hashing, no generations — the push is
  the whole roster.
- `fill_arena(server_room, subject, t_body_state *snap)`: writes every occupied
  slot's card, with the attacking-you and targeted-by-you flags resolved for
  this particular subject. It is the only per-recipient part of the arena, which
  is why the section is built per snapshot rather than once per tick and shared.
- `server_room_target_of(server_room, from_slot)`: grows the mode and the
  candidate sets ([D5](#d5--the-target-is-drawn-by-the-senders-declared-mode-from-the-rooms-own-rng)).
  Double's answer is unchanged and is reached without touching the RNG.
- `server_room_set_target(server_room, cli, mode)`: records a player's mode on
  their participant record.
- `settle_garbage`: carries the **sender's player id** with the queued rows, so
  the receiver can stamp it at the lock that lands them.
- `sweep_eliminations(server_room)`: the two-pass placing step
  ([D7](#d7--a-placing-is-taken-when-the-player-is-eliminated-once-per-tick)) —
  collect every game that went inactive this tick, give the group one placing,
  credit each knockout to `last_attacker_id`
  ([D8](#d8--a-knockout-is-credited-to-the-last-player-whose-garbage-landed)),
  narrate, and take `alive` down by the size of the group.
- `settle_results`: writes the winner's `WON`/rank 1 and leaves the placings the
  eliminations already took.
- `server_room_forfeit`: place the leaver before releasing the seat; keep the
  select window open unless the room falls below `min_to_start`
  ([D14](#d14--a-battle-royale-select-window-survives-a-departure)).
- `rehome_successor`: shrinks to moving `games[]` and `dirty[]` only, which is
  all it was ever meant to do — the match state it used to leave behind no
  longer lives at a slot index (S15).
- `room_blank`: clear the new runtime — `rng`, `arena_ms`, `alive`,
  `participants`. This is the same rule that made `ticking` a bug when it was
  left behind (`docs/bugs/room_runtime_outlived_its_room.md`).

### `src/tetrisd/src/game.c`

- The pending queue carries a sender id, and the drain at lock reports it back
  to `room.c` so the participant record is stamped there — a `t_game` still
  knows nothing about who it is playing.
- `game_snapshot` unchanged; the arena, the counts and the rank are the room's
  to write, exactly as the countdown and the result already are
  (`decorate_snapshot`, `room.c:1404`).

### `src/tetrisd/src/handlers_ready.c` / `handlers_lobby.c` / `dispatch.c`

- `ready_handler`: auto-selection becomes Double-only ([D9](#d9--the-owner-starts-a-battle-royale-readiness-only-makes-it-possible)),
  and the declared character is written to the participant record rather than to
  `character[slot]` — which is the fix for the live Double bug S15 found.
- `start_handler` is unchanged — it already refuses a non-owner and a room below
  `min_to_start`, which is the whole of the Battle Royale start rule.
- `target_handler` in a new `handlers_target.c`, and its row in `g_routes`.

### `src/tetrisd/src/config.c`

- `br_slots` range becomes `4..ROOM_MAX_SLOTS` (the domain's floor is already 4;
  the current lower bound of 2 is a Double number in a Battle Royale key).

### `src/tetrisd/README.md`, `docs/CONTEXT.md`, `docs/use_cases.md`

- The mode's routes, the targeting table, the placing rule, the KO rule, the
  arena cadence and its arithmetic.
- `CONTEXT.md`'s **Target** entry: the seeded draw is now the default mode and
  the fallback for the other three. `CONTEXT.md` is the glossary and is checked
  before a term is invented, so "targeting mode", "arena", "knockout" and
  "placing" belong in it.
- `use_cases.md` UC-12 step 3: the Target is drawn from the sender's declared
  mode's candidate set.

---

## Client work

### `src/tetrisu/tetrisu.h`

- `opponents[]` becomes slot-indexed and `present` becomes "in the roster"
  ([D12](#d12--the-clients-arena-becomes-slot-indexed)).
- The card gains `ko`, `rank`, `score`, `height`, `targeted_by_you`, and keeps
  `targeting_local` — now written from the wire.
- `MP_BR_OPPONENT_COUNT` is either used as the bound everywhere or deleted (U10).

### `src/tetrisu/src/net_match.c`

- Decode the arena into the slot-indexed cards: clear every card and write the
  ones the push carries, because a push is the whole roster
  ([D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock)). An
  `arena absent` frame leaves the cards untouched.
- `players_alive` / `players_total` from the frame's own counts, not from the
  card count (U2).
- `ko_count`, `incoming_attackers` (the number of cards flagged attacking-you)
  and `rank` from the frame (U3).

### `src/tetrisu/src/multiplayer_match.c`

- `mp_match_apply_room` stops seeding fixture boards when there is a session
  (U1) — the same branch `mp_match_state_init` already needs for the fixture
  opponent.
- `mp_match_target_handle_key` sends `TARGET` through the provider and shows the
  server's answer, rather than writing a banner (U4). It is send-and-report, not
  send-and-wait: a refused mode leaves the previous one standing and says so,
  and the next arena push is what confirms a mode took.
- `mp_match_finish` stops flooring a Battle Royale rank at 2 (U12).
- The layout gate degrades instead of refusing (U9) — see below.

### `src/tetrisu/src/net_provider.c`

- `set_target(userdata, room_id, mode)`, on the model of `ready_room`.

### `src/tetrisu/src/render_multiplayer_match_pixel.c`

- Per-card signatures over the card's fields, not over the struct's bytes
  (U6/U7).
- Banded arena planes ([D13](#d13--the-arena-is-drawn-per-card-not-per-side)),
  a presentation floor matched to `TETRISD_BR_ARENA_MS`, and the tiering that
  decides which cards get the near columns.
- The far-crowd strip for cards below the size at which a board is legible.

### `src/tetrisu/src/render_multiplayer_match.c`

The compatibility renderer gets the same information (KO, alive, rank,
attackers) — it is the fallback for a terminal without pixel support and it is
currently the more honest of the two, because it draws from the same state.

---

## UX and UI

The mode is only finished if it reads. Concretely, and in the order a player
meets them:

**Arrival.** The lobby's Battle Royale filter shows `4/20`-style occupancy and
the room's status, and a room that fills or starts while you are looking at it
updates without a keypress. The waiting room already narrates joins and
ownership on the chat feed; a Battle Royale room narrates its fill level at the
thresholds that matter (`min_to_start` reached, one seat left).

**The select window at scale.** Fifteen seconds and a 40-player roster is a lot
of screen. The roster scrolls, the local seat is pinned, and the window's clock
is the room's — all of which already exists from Double. What is new is showing
*how many* have locked in (`23/40 locked`), because at 40 players "everyone else
is waiting for you" is not visible from a list.

**The countdown.** The room's 3-2-1, already server-held. Nothing to build.

**The arena, tiered.** Near columns: the players attacking you, then the player
you are aiming at, then the rest by placing. Attackers are outlined in red — the
renderer already has the branch (`render_multiplayer_match.c:475-490`), it has
never had the data. A card that is about to top out gets the danger tint the
local board already uses.

**The targeting diamond.** W/A/S/D, already drawn, now backed. The selected mode
is highlighted and the count of candidates is shown beside it
(`S ATTACKERS (3)`), because a mode with an empty set silently falls back to
Randoms and the player should be able to see why.

**Incoming garbage.** The `pending` count is already on the wire for the local
player and already drawn as a warning bar. At 99 players it is the single most
important number on the screen and should be the loudest.

**Knockouts.** Every KO is a line on the room feed (`PLAYER x knocked out
PLAYER y`) and a brief card, on the notification plane the ability announcements
already use (`announce_effects`, `multiplayer_match_mode.c:810`) — not a screen
shake, for the reason that function's comment already gives.

**Elimination.** The board dims, the placing appears (`#37`), the HUD switches to
`SPECTATING`, and the arena keeps running ([D10](#d10--elimination-is-not-the-end-of-the-screen)).
The controls line changes to say what still works. This is the single largest
piece of missing UX, because today the screen simply stops meaning anything.

**The result.** Rank first and large, then the KO count, then the score and what
it earned. The results screen must not close the client — that is a known defect
with a post-mortem (`docs/bugs/the_results_screen_closed_the_application.md`) —
and it returns to the room, which `room_rematch` has kept alive.

**Degradation.** `rows >= 36 && cols >= 132` is the arena's *comfortable* size,
not its minimum (U9). Below it: drop to one side column, then to the crowd strip
alone, then to a text arena in the compatibility renderer. A terminal too small
for even that gets a message naming the size it needs — never a blank screen.

**Audio.** The existing SFX set covers it; the mode adds a knockout sting and an
"attacker acquired" cue, and the countdown ticks are already there.

---

## Tests

Every server test is a real server over a real socket, because that is what the
existing suites do and it is the only way the wire is exercised. The harness
(`src/tetrisd/tests/harness.h`) already drives multiple clients — `test_double.c`
drives two and `hc_lock_in` exists for the select window.

### `libstatusbody` (`tests/test_state.c`)

- an arena round-trips at 0, 1, 50 and 99 cards;
- `arena absent 0` round-trips and carries no cards;
- a Single body is byte-identical to one encoded before the change, save the
  three new fixed lines;
- `arena full 3` with two `a` lines is `EBADMSG`, not a short read;
- a mask of 49 or 51 hex chars, a slot ≥ `BODY_ARENA_MAX`, a duplicate slot, and
  an `a` line under `arena absent` are each `EBADMSG`;
- a 99-card arena with every field at its maximum encodes within
  `BODY_STATE_MAX_BYTES` — the assertion that keeps the derived cap honest as
  fields are added;
- the mask codec is exercised against a board with a filled row, an empty row
  and a single cell at each edge.

### `tetrisd` (new `tests/test_battle_royale.c`)

Driving `TETRISD_BATTLE_ROYALE_SLOTS=6` and six harness clients unless noted:

- four players join `BR-01` and all ready: the room is `READY` and **does not**
  start ([D9](#d9--the-owner-starts-a-battle-royale-readiness-only-makes-it-possible));
  a fifth can still join; the owner's `START` opens the window;
- a non-owner's `START` is `403 not-owner`; a `START` at 3 players is
  `409 too-few-players`;
- every player receives `phase countdown`, then `active` on the same tick;
- each arena push carries **every** occupied slot, including the players who
  have been eliminated, and a slot released mid-match stops appearing;
- an arena arrives no faster than `TETRISD_BR_ARENA_MS`, and a player who does
  nothing at all still has their card in every push
  ([D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock)) — the
  inverse of the delta assertion, and the one that catches a delta creeping back
  in as an optimisation;
- a player's own board still updates at the tick rate while the arena does not
  ([D4](#d4--battle-royale-does-not-spread-dirty));
- **targeting**: with a fixed room seed, `TARGET mode ko` sends A's garbage to
  the tallest stack and `mode attackers` sends it to the player who last buried
  A; an empty candidate set falls back to a random live opponent and never drops
  the garbage; `TARGET` in a Double room is `409`;
- **placing**: six players eliminated in a known order are recorded 6, 5, 4, 3,
  2 and the survivor 1, each rank arriving in the eliminated player's own
  snapshot *before* the match ends ([D10](#d10--elimination-is-not-the-end-of-the-screen));
- **simultaneous elimination**: two players driven to top out on the same tick
  receive the **same** placing, and the next elimination skips it — asserted
  with the pair in ascending and in descending slot order, because the bug this
  catches is iteration order deciding a result
  ([D7](#d7--a-placing-is-taken-when-the-player-is-eliminated-once-per-tick));
- **knockouts**: the player whose garbage landed last gets the KO; a self-inflicted
  top-out credits nobody; a KO whose attacker has since disconnected is still
  credited to them; the KO count is on the arena card and on the feed;
- a quit at 4/6 places the quitter 4th and the match plays on; the last two
  players decide it normally;
- **the select window survives a departure** ([D14](#d14--a-battle-royale-select-window-survives-a-departure)):
  with 6 seated and the window open, one player leaving leaves it open and the
  match is dealt to 5; leaving until 3 remain closes it and returns the room to
  `WAITING`; the same test in a Double room still closes the window on the first
  departure;
- **slot moves do not move match state** ([D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot)):
  the owner leaves mid-match, the successor is promoted into their slot, and the
  successor's KO count, placing and declared fighter are all still theirs. The
  same case in a Double room during the select window — which is the live bug
  S15 found — is asserted directly: the successor's declared character survives
  the promotion and the window still ends early once every seat has locked in;
- `db_record_game` is called once per participant, `won = true` for the survivor
  alone, read back through `db_get_player`;
- `PAUSE` and `RESTART` are still `not-single`;
- a room at `slot_count` refuses a join with `JOIN_FULL`; a room in the select
  window refuses with `JOIN_IN_GAME`.

### Scale (`make stress`)

`tests/stress_client.c` already puts a fleet of players on one server and reports
the cost. Extend it to seat a full room:

- 99 clients in one Battle Royale room, played for 60 s;
- assert the reactor's tick does not slip (the elapsed the timer reports stays
  within tolerance of the configured tick), and report bytes/s per client and in
  aggregate against the ~50 KB/s and ~5 MB/s a full-snapshot arena budgets;
- run the same at 8 and 24 seats, which are the sizes this is expected to ship
  at ([Open questions](#open-questions) 2), and report all three — the 99 number
  is the ceiling, the other two are the promise;
- assert no client is closed for outbox overflow — the response FIFO must never
  fill, which is the property the third lane exists to protect.

This suite is the one that can falsify
[D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock)'s cadence, and
it is where the full-snapshot decision is paid for or vindicated: if 5 MB/s at
99 is intolerable and the 24-seat number is not, the answer is a lower ceiling
rather than a delta scheme the mailbox cannot acknowledge.

### `tetrisu`

- `tests/test_multiplayer_match.c`: an arena maps onto slot-indexed cards; an
  `arena absent` frame leaves them alone; a push that omits a slot clears that
  card; `players_alive` comes from the frame's count and not from the cards.
- `tests/test_net_match_opponents.c`: extended for the arena decode.
- a new layout test: the Battle Royale layout degrades through its tiers rather
  than returning invalid, at 132×36, 100×30 and 80×24.
- `tests/integration/test_net_battle_royale.sh`, on the model of
  `test_net_double.sh`: four headless clients against one server through
  join → ready → owner start → select → play → knockout → placing → result,
  asserting on decoded snapshots rather than on anything drawn.

### Manual

`make play` with four terminals is the only test that catches the arena's frame
cost, exactly as two terminals was for Double.

---

## Order of work

Each step leaves the tree building and every existing suite passing.

| # | Step | Why here |
|---|---|---|
| 0 | ✅ **Landed.** The participant record: fold `character[]`, `result[]` and `rank[]` into one player-id-keyed struct ([D15](#d15--attribution-and-match-state-key-on-player-id-not-on-slot)) | It is a **bug fix in Double**, not Battle Royale work (S15) — an owner leaving during select strands the successor's fighter today. Landing it first means it ships with Double's tests as its evidence, and every field the later steps add has somewhere correct to live |
| 1 | `advance_and_push` encodes per slot ([D2](#d2--the-tick-encodes-per-slot-it-stops-collecting-first)) | **Before** the constant is raised, never after — see below. Correct at 16 games, invisible from outside, every suite green |
| 2 | Sizing: `TD_MAX_GAMES` 99, `MAX_CLIENTS` 128, `br_slots` range, derived body cap ([D11](#d11--sizing-raise-tdmaxgames-to-99-and-keep-the-games-inline)) | Everything else needs a room that can hold the players, and step 1 has already removed the one thing that scaled badly with it |
| 3 | `libstatusbody`: arena section, counts, mask codec ([D1](#d1--the-arena-is-a-second-detail-level-not-a-second-body)) | Everything encodes into it, and it lands with Single and Double still the only users |
| 4 | Owner-started Battle Royale ([D9](#d9--the-owner-starts-a-battle-royale-readiness-only-makes-it-possible)) and a select window that survives a departure ([D14](#d14--a-battle-royale-select-window-survives-a-departure)) | Two branches, both fixing real defects (S3, S14), and together they are what makes every later test able to control when a match begins and trust that it stays begun |
| 5 | `tetrisd`: arena cadence, `fill_arena`, no spread ([D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock), [D4](#d4--battle-royale-does-not-spread-dirty)), **and the `make stress` extension** | Ninety-eight boards on the wire, at a rate that survives. The stress suite lands *with* it because it is the only thing that can falsify the cadence, and a cadence proved two steps later is a cadence rewritten two steps later |
| 6 | `tetrisu`: slot-indexed cards, arena decode, real counts ([D12](#d12--the-clients-arena-becomes-slot-indexed)) | The mode becomes real. Playable, unranked, untargeted |
| 7 | `tetrisd`: placing at elimination, knockouts, narration ([D7](#d7--a-placing-is-taken-when-the-player-is-eliminated-once-per-tick), [D8](#d8--a-knockout-is-credited-to-the-last-player-whose-garbage-landed)) | The mode becomes a competition rather than a survival timer. Needs 0 — both facts live on the participant record. Lights up the arena's `alive` and `rank` fields and the attacking-you flag |
| 8 | `tetrisd`: targeting modes, the room's RNG, the `TARGET` route ([D5](#d5--the-target-is-drawn-by-the-senders-declared-mode-from-the-rooms-own-rng), [D6](#d6--the-mode-travels-on-its-own-tiny-request)) | Needs 7 — two of the four modes sort on knockouts and on who attacked you |
| 9 | `tetrisu`: `net_provider.set_target`, `mp_match_target_handle_key` sends and reports ([D6](#d6--the-mode-travels-on-its-own-tiny-request)) | The client half of step 8, and its own step because until it lands W/A/S/D is still the decoration U4 found. Small, and it is what makes the mode *played* rather than watched |
| 10 | `tetrisu`: banded per-card arena, tiering, crowd strip ([D13](#d13--the-arena-is-drawn-per-card-not-per-side)) | The cost only exists once the arena is live, and the tiering sorts on attackers and placings — which is step 7's data reaching the screen |
| 11 | UX: elimination and spectating, KO feed, targeting affordance, degradation | The mode is playable at step 10 and *finished* here |
| 12 | Docs: `CONTEXT.md`, `use_cases.md`, both READMEs | Written against what was built, not against what was planned |

Step 0 is a Double bug fix; 1–5 are the wire; 6 is the mode; 7–9 are the game;
10–11 are why anybody would play it twice.

**Why the scale work is early and not last.** Steps 1 and 2 change no behaviour
and pass every existing test, which makes them cheap to land and cheap to
revert — and doing them last means doing them underneath step 5, which is the
only genuinely new code in the plan and the worst place to be changing the
shape of a struct.

**Why the encode fix comes before the constant, and not after.** The obvious
order is "make the room big, then fix what the size broke", and the first draft
justified reversing it with a 4.8 MB stack frame that does not exist — see the
correction under [the arithmetic](#the-arithmetic-that-decides-the-design). The
real figure is 103 KB, which would not have crashed anything.

The order still stands, on a smaller and more honest claim. `advance_and_push`
declares `t_body_state snaps[TD_MAX_GAMES]`, so with the constant raised first
every tick of every room pays 103 KB of the reactor's stack to hold snapshots
each read exactly once — and that number is not stable, because step 3 puts the
arena section inside `t_body_state` and takes the array past half a megabyte
without touching this function. Landing the encode fix first means the array is
gone before either number can be true.

The incremental encode is correct at sixteen games as well as ninety-nine, so
landing it first costs nothing and means the oversized frame never exists in any
committed state of the tree.

**Why the participant record is step 0 and not part of step 7.** It was going to
be an implementation detail of the knockout work. It is not: S15 is a defect in
Double that exists in the tree right now, its blast radius grows with every
per-slot array this plan adds, and it is the precondition for
[D14](#d14--a-battle-royale-select-window-survives-a-departure) being safe.
Landing it alone, against Double's existing suite plus one new case, is the only
version of this change that can be verified rather than argued about.

**What is deliberately dark between steps.** Step 5 puts the arena on the wire
with three fields it cannot yet fill: `alive` is true for everybody, `rank` is
0, and the attacking-you flag is clear, because elimination and attacker
tracking are step 7. That is intended and is not a stub to come back to — the
codec carries them from step 3, the room writes them from step 7, and the arena
is legible in between. A reader implementing step 5 should not go looking for
the data.

---

## Open questions

1. **Does a knockout buy anything?** Tetris 99's badges multiply attack, and
   [D8](#d8--a-knockout-is-credited-to-the-last-player-whose-garbage-landed)
   deliberately does not build that — `docs/game-economics.md` has no such
   mechanic and the mode is complete without one. If it is wanted, the cheapest
   honest version is a garbage multiplier of `1 + ko/4` capped at 2, resolved in
   `settle_garbage` where the row count is already computed. It is a game-design
   call, not an engineering one.
2. **What is the *supported* room size?** The design carries 99, and with the
   full-snapshot arena ([D3](#d3--the-arena-is-a-full-compact-snapshot-on-its-own-clock))
   a full room costs ~5 MB/s rather than the ~1.8 MB/s a delta scheme would.
   That trade was taken deliberately and it makes this question load-bearing
   rather than academic: the number that ships as
   `TETRISD_BATTLE_ROYALE_SLOTS` — and the number the stress suite gates on —
   decides whether the simplification was free. Recommend a default of 8
   (~36 KB/s), a tested ceiling of 24 (~280 KB/s), and 99 supported but
   documented as needing the bandwidth. If 99 has to be cheap, the lever is the
   cadence (200 ms → 300 ms) and then the `score` field
   ([Wire changes](#5-the-body-cap-is-derived-not-chosen)), in that order —
   never a delta the mailbox cannot acknowledge.
3. **Should an eliminated player be able to leave?** UC-12 5a says they wait out
   the remainder, and the placing is already taken, so leaving costs them
   nothing and frees a connection. Leaving mid-match currently means forfeiting,
   and forfeiting a game that is already over is a code path with two meanings.
   Recommend: `LEAVE` after elimination releases the seat without re-recording
   the game, guarded by `game->recorded` — which already exists for exactly this
   class of double-count.
4. **Late joiners into a running match.** `room_can_accept` refuses, which is
   right, and a 40-seat room that starts at 4 wastes 36 seats. A lobby that
   showed "starting in 30 s" would fill rooms better than any server change.
   Out of scope; it is a lobby feature, not a match one.
5. **Does the fixture arena survive?** Double kept its fixture opponent so the
   match screen is demonstrable without a server, and the same argument applies
   here — a 98-card arena is the hardest thing in the client to eyeball, and the
   fixture is the only way to do it deterministically. Recommend keeping it,
   behind the same online branch.
