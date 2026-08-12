# Bots — implementation plan

A player with two laptops cannot start a Battle Royale. The mode's minimum is
four (`min_to_start = 4`, `lib/libtetrisroom/src/room.c:224`), and there is no
way to reach it without four people. This document is how one person fills a
room instead: `B` adds a bot, `K` kicks one, and the bots play well enough to
be worth beating.

It follows the shape the Double and Battle Royale plans used before they were
retired into [`../tetrisd/README.md`](../tetrisd/README.md): what exists today
with the file and line, the decisions the findings force, the work per
component, and the tests that hold each piece down.

The load-bearing finding is that **the bot already exists and already works**.
`src/tetrisu/tests/match_smoke.c` carries a full placement search that plays
over a real session against a real `tetrisd`. It is not a mock and not a stub;
it is a Tetris AI that has been in the tree for as long as the Double-mode
tests have. This plan is mostly about giving it a home, an account, and a key
to press — not about writing an opponent.

## Table of Contents

- [Where the pieces stand](#where-the-pieces-stand)
- [Decisions](#decisions)
- [Difficulty](#difficulty)
- [Server work](#server-work)
- [Client work](#client-work)
- [The bot process](#the-bot-process)
- [UX and UI](#ux-and-ui)
- [Tests](#tests)
- [Order of work](#order-of-work)
- [Open questions](#open-questions)

---

## Where the pieces stand

### Two things called a bot, and only one of them plays

**`src/tetrisu/tests/stress_client.c` — a load generator.** It picks from an
eight-entry table with no reference to the board at all:

```c
static const t_solo_action	g_actions[STRESS_ACTION_COUNT] = {
	SOLO_MOVE_LEFT, SOLO_MOVE_RIGHT, SOLO_ROTATE_CW, SOLO_ROTATE_CCW,
	SOLO_SOFT_DROP, SOLO_MOVE_LEFT, SOLO_MOVE_RIGHT, SOLO_HARD_DROP
};
...
sent = net_match_action(net, g_actions[(*seed >> 16) % STRESS_ACTION_COUNT], &result);
```

It exists to measure what N sessions cost, and it measures that honestly. It is
useless as an opponent and is not the basis for anything here.

**`src/tetrisu/tests/match_smoke.c` — a real player.** `plan_placement` tries
every rotation against every column, hard-drops each candidate, and scores the
board that results:

| Function | What it does |
|---|---|
| `plan_placement` | Picks a rotation and a column across all four rotations |
| `best_column` | Scans past both walls, because a piece's anchor is not its leftmost cell |
| `placement_score` | Prices a landed board on four features |
| `column_profile` | Heights per column and buried holes |
| `place_one_piece` | Rotate → settle → slide → hard drop → wait for the board that took it |

```c
return (lines * 760 - aggregate * 510 - holes * 357 - bumps * 184);
```

Those are the well-known tuned weights for that feature set. Survival is
strong. The comments in that file record two bugs already paid for and worth
not re-paying: the column must be re-chosen *after* rotating (a rotation near
a wall kicks the piece sideways), and the loop must wait for the board that
took the drop (or every piece is planned against the board before the last one
landed, which stacks straight to the ceiling).

### The finding that decides the difficulty tiers

The bot as written sends **no garbage at all**. Not "little" — none:

```c
//   1 line  -> 0 garbage  (single clears don't trigger Battle Royale)
static const int GARBAGE_TABLE[] = {0, 0, 1, 2, 3};
```

`placement_score` rewards `lines * 760`, flat, so a single is worth as much per
line as a Tetris and it takes every clear the instant one is available. It
clears almost only singles. Every one of them sends nothing.

This is why "today's scorer" is the *easy* tier and not the normal one, and
why the normal tier is one changed line rather than a new algorithm.

### The finding that bounds the ceiling

Garbage is flat. `room.c` sends plain `garbage_lines_from_clear(cleared)`, and
`t_score_state`'s `back_to_back` and `combo` feed the **score** only — never
the attack. Chaining Tetrises sends no more than isolated ones.

So a stronger bot cannot hit *harder* than a Tetris. It can only hit *more
often* (fewer wasted pieces) and *better aimed* (`TARGET`). That bounds
`ultra` and is why T-spins are out of scope — a T-spin double sends one row,
exactly like any other double, so the opening-book work buys nothing.

### What the client has

- `t_body_state` already carries `next[BODY_NEXT_COUNT]` and `hold` /
  `hold_used`. Lookahead and hold need no wire change.
- `TARGET` is served and the client already sends it (`net_match_set_target`).
- The waiting room is a real screen with a key map and a chat composer, so `B`
  and `K` have somewhere to live.

### What does not exist

- Any bot outside `tests/`.
- Any notion on the server of an account that is not a person.
- Any way for a client to seat somebody other than itself.

---

## Decisions

### D1 — bots are client-spawned real clients, not server-side seats

A bot is a child process of the `tetrisu` that added it. It signs in over the
same TCP port, completes the same handshake, and sends the same `JOIN`,
`READY`, `MOVE` and `DROP` as a person. `tetrisd` never learns that it is not
one.

The alternative — a seat with no connection, ticked by the room — is cleaner on
the wire and much worse everywhere else. A seat is tied to a `t_client` at
roughly ten sites in `room.c` alone, plus the registry and the outbox, and
every one would grow a "unless it is a bot" branch. The reactor's lifetime rule
(`client_kill` parks, `client_reap` frees) would need a second shape of thing
to park.

Against that, the client-spawned route reuses `net_client.c` whole, and it
reuses the AI *exactly as it is already proven* — the same code path
`match_smoke.c` has been exercising against a real server for as long as Double
has existed.

### D2 — kicking needs no protocol

The bot is your client's own child. `K` sends it `SIGTERM`; its socket closes;
the server drops it from the room through the disconnect path every dropped
connection already takes — including `room_release`'s owner succession, which
is exactly right, because a bot must never inherit a room.

There is no `KICK` method, no route, no authorisation question, and no way to
kick a person: a client can only signal processes it started.

Leaving the room, quitting the client, and closing the terminal are the same
mechanism — `tetrisu` reaps its children on the way out of the waiting room and
again in its exit path.

### D3 — an orphaned bot must die on its own

D2 has one hole: `SIGKILL` on the client, or a crash, leaves bots seated
forever with nobody able to remove them.

The bot holds the read end of a pipe whose write end is the parent's. If the
parent dies by any means, the read returns EOF and the bot exits. `libcoreipc`
already owns the self-pipe primitive. This is checked on the same poll the bot
already runs for its session, so it costs one `fd`.

### D4 — a pool of accounts, claimed and released

Fixed names (`bot1`…`bot4`) do not work, and the reason is a rule that is right
and should not be bent: **a player holds at most one connection — a second
`LOGIN` displaces the first.** Four fixed names means four bots for the whole
server, and the fifth silently evicts the first from a match in progress.

So: `TETRISD_BOT_ACCOUNTS` (default 32) accounts created at boot if absent. A
bot logging in takes the first free one; disconnecting releases it. A room that
wants a bot and finds the pool empty is told so and adds nobody.

Bot multi-login is explicitly **not** the answer. The one-connection rule is an
invariant other things lean on, and bots are the worst possible place to put a
hole in it.

### D5 — bots are not people, and the leaderboard has to know

Bots play real games, so `award_game` runs on them: they would earn wallet
points and rank. Within an evening the board is bots.

A reserved username prefix, refused by `SIGNUP` and filtered by `db_leaderboard`
and `db_rank`. The prefix is a `libmacminidb` rule and not a `tetrisd` one,
because the store is what ranks.

Note honestly: the pool's password has to be known to the client binary, so
anyone holding it can log in as a pool bot and occupy seats. On a LAN, for a
course project, that is acceptable. It is not a property to rely on if this
were ever exposed.

### D6 — four bots per room, and only the owner adds them

Enough to reach a Battle Royale's minimum from one person, and one for a
Double. Adding is an owner action for the same reason starting is.

---

## Difficulty

Three tiers, and the split is decided by the two findings above rather than by
taste.

| Tier | Scorer | Search | Sends |
|---|---|---|---|
| `easy` | today's weights | 1 ply, ~25% of pieces placed at random instead | nothing — it clears singles and stacks badly enough to top out |
| `normal` | garbage-weighted | 1 ply + `next[0]` | Tetrises, because singles now score zero |
| `ultra` | garbage-weighted | 2 ply + hold as a branch + live `TARGET` | the same per clear, but far more often and aimed |

**`easy` needs the noise, not just the weak attack.** Today's weights are a
near-perfect *survival* set: left alone, the bot does not die, it merely never
attacks. That is not an easy opponent, it is a stalemate — in Battle Royale it
would survive to the final few every time and do nothing. The random-placement
injection is what makes it lose.

**`normal` is one line.** Replace `lines * 760` with
`garbage_lines_from_clear(lines) * W`. Singles drop to zero, so the bot starts
holding rows back and building for a Tetris on its own — no new machinery.

**`ultra` cannot hit harder, so it hits oftener.** Two plies with hold as a
search branch is about 6k boards per piece at roughly one piece a second: free.
It buys consistency, which is Tetrises per minute. `TARGET` is the other half —
switch to Attackers while being hit, KOs to finish somebody. With the fan-out
landed, Attackers on an ultra bot answers every attacker at once.

Speed is not a lever. Bots are bound by `TETRISD_INPUT_RATE`, the same limiter
a person hits.

### Out of scope

T-spins. `piece_rotate_with_kick` exposes `kick_index`, which is exactly the
detection signal, so recognising one is easy — but *building* the slots is an
opening-book problem, and since garbage is flat a T-spin double sends one row
like any double. All of the work, none of the damage.

---

## Server work

### `lib/libmacminidb`

- Reserved prefix in `db_username_valid`'s neighbourhood — a separate
  `db_username_is_reserved`, because the charset rule and the policy rule are
  different questions and `db_username_valid` is documented as the format's.
- `db_signup` refuses a reserved name.
- `db_leaderboard` and `db_rank` skip reserved accounts. This is the one that
  matters: a filter in the encoder would leave `db_rank` telling a person they
  are 14th out of a board that shows 9 rows.

### `src/tetrisd`

- `TETRISD_BOT_ACCOUNTS` in `config.c`, documented in `.tetrishrc`.
- Pool creation at boot: sign up any missing pool account, ignore `DB_EXISTS`.
- Claim and release on the login and disconnect paths.
- Nothing else. No new route, no new method, no seat that is not a client.

---

## Client work

### `src/tetrisu/src/bot_brain.c` (new)

The AI, lifted out of `tests/match_smoke.c` and given the tiers. Pure: takes a
`t_body_state`, returns a plan. No I/O, no session — the same discipline
`libtetrisbrain` is held to, and what makes it unit-testable without a server.

`match_smoke.c` then calls this module instead of carrying its own copy, so the
two cannot drift and the existing Double-mode integration test becomes coverage
for the shipped bot.

### `src/tetrisu/src/bot_proc.c` (new)

Spawn, track and reap the children. Owns the deadman pipe (D3) and the child
table. Kills every child on leaving a room and on exit.

### `src/tetrisu/src/bot_main.c` (new)

The child's own entry: claim an account, log in, `JOIN` the room named on the
command line, `READY`, then loop the brain against the session until the
parent's pipe closes.

The `--room NAME` behaviour lives here and nowhere else, which is what makes a
bot join *your* room rather than opening one of its own the way `stress_client`
does.

### Waiting room

`B` and `K`, owner-only, with the seat list showing which seats are bots.

---

## The bot process

```
tetrisu (owner)                      tetrisd
  │  B pressed
  ├─ pipe(deadman)
  ├─ fork/exec bot_main --room R --level normal
  │                                      │
  │            bot_main ── LOGIN ────────▶  claims a pool account
  │                     ── JOIN /room/R ─▶  ordinary seat, ordinary narration
  │                     ── READY ────────▶
  │                     ◀─ STATE ────────   plays via bot_brain
  │
  │  K pressed / left room / exited
  ├─ SIGTERM ──────────▶ bot exits ──────▶  ordinary disconnect, seat released
  │
  └─ (killed) ─ deadman EOF ─▶ bot exits ─▶  same path
```

Everything below the first arrow is a path `tetrisd` already has. That is the
point of D1.

---

## UX and UI

- The waiting room's seat list marks bot seats and their level.
- `B` adds one at the room's current default level; the level is chosen in
  Settings, not per bot, because four separate prompts to add four bots is
  worse than one setting.
- `B` at four bots, or with the pool empty, says why and adds nobody.
- `K` kicks the highlighted seat only when it is a bot, and is silent
  otherwise — a key that sometimes kicks people is a key nobody presses.
- Bots appear in chat narration exactly like players (`joined the room`),
  because they did.

---

## Tests

| Test | Where | Asserts |
|---|---|---|
| `test_bot_brain.c` | `tetrisu` unit | A board with a row one cell short is completed, not buried |
| ″ | ″ | `normal` prefers a Tetris over an available single; `easy` does not |
| ″ | ″ | `easy` tops out inside N pieces on a clean board; `normal` does not |
| ″ | ″ | The plan is re-derived after a rotation, not carried across it |
| `test_bot_proc.c` | `tetrisu` unit | Children are reaped on room exit; the table never leaks a pid |
| ″ | ″ | Closing the parent's pipe end exits the child |
| `test_bot_pool.c` | `tetrisd` | A claimed account is not claimed twice; disconnect releases it |
| ″ | ″ | An empty pool refuses rather than displacing a logged-in bot |
| `test_leaderboard.c` | `libmacminidb` | A reserved account never appears in a page or a rank |
| ″ | ″ | `db_signup` refuses a reserved name |
| `test_bots.sh` | `tetrisu` integration | One human client + 3 bots reach a dealt Battle Royale and a bot survives 30 s |

The integration test is the one that would have caught every bug in this plan.

---

## Order of work

| # | Step | Depends on |
|---|---|---|
| 0 | `bot_brain.c` — lift, tier, unit-test | — |
| 1 | `match_smoke.c` calls it instead of its own copy | 0 |
| 2 | Reserved prefix + leaderboard exclusion | — |
| 3 | Account pool: config, boot creation, claim/release | 2 |
| 4 | `bot_main.c` — a bot that joins a named room from the command line | 0, 3 |
| 5 | `bot_proc.c` — spawn, deadman pipe, reap | 4 |
| 6 | `B` / `K` in the waiting room, seat list marks | 5 |
| 7 | `test_bots.sh` end to end | 6 |
| 8 | `ultra`: 2-ply, hold, live `TARGET` | 0, 7 |

Steps 0–2 are independent and could land in any order. Step 8 is deliberately
last: it is the only step whose value is judged by playing rather than by a
test, and everything before it is worth having without it.

---

## Open questions

- **Does a bot count toward the leaderboard of the *room*?** Placings are a
  match fact, not an account fact, so a bot placing 2nd is fine and reads
  correctly. Only the persistent board excludes them. Flagging it because it
  is the sort of thing that looks like a bug when first seen.
- **Should `easy` be beatable by someone who has never played Tetris?** The
  25% figure is a guess. It wants one evening of play to settle, and it is one
  constant.
- **Does the pool need to survive a restart?** As written the accounts persist
  in the store and only the claim state is in memory, so a restart frees every
  claim — which is correct, since the sessions died with it.
