# libstatusbody

`libstatusbody` is the shared HTTTP message-body codec library for tetriSH. It encodes the plaintext bodies `tetrisd` sends and decodes the ones `tetrisu`
receives.

---

## Table of Contents

- [Build And Test](#build-and-test)
- [Usage](#usage)
- [Codec Contract](#codec-contract)
- [The Six Codecs](#the-six-codecs)
- [Testing](#testing)
- [Project Structure](#project-structure)

---

## Build And Test

```bash
make -C lib/libstatusbody
make -C lib/libstatusbody test
make -C lib/libstatusbody test FILTER=state
make -C lib/libstatusbody clean
make -C lib/libstatusbody fclean
make -C lib/libstatusbody re
```

Build output is `lib/libstatusbody/libstatusbody.a`.

```bash
cc ... -I lib/libstatusbody/include lib/libstatusbody/libstatusbody.a
```

---

## Usage

```c
#include "statusbody.h"

t_body_state	frame;
t_body_state	decoded;
char		body[4096];
int		len;

memset(&frame, 0, sizeof(frame));
frame.seq = 42;
frame.phase = BODY_PHASE_ACTIVE;
frame.piece = (t_body_piece){ .type = 3, .rotation = 1, .col = 4, .row = 0 };
frame.charge = 7;
/* ... board cells, next queue, score/lines/level ... */

len = body_state_encode(&frame, body, sizeof(body));   /* -1: EINVAL or ERANGE */

/* tetrisu, on receipt of the HTTTP body: */
if (len > 0 && body_state_decode(body, (size_t)len, &decoded) == 0) {
    /* decoded mirrors frame field-for-field */
}
```

---

## Codec Contract

A *codec* (coder/decoder) is one `body_*_encode`/`body_*_decode` pair — the two
halves of a single body type. Every pair shares one contract:

| Call | Success | Failure |
|---|---|---|
| `body_*_encode(in, out, cap)` | bytes written | `-1`, `errno` set — see below; never writes past `cap` |
| `body_*_decode(buf, len, out)` | `0` | `-1`, `errno` set — see below; never reads past `len` |

On failure both return `-1` and set `errno`:

- `EINVAL` — a NULL argument, or a field an encoder cannot represent
- `ERANGE` — `cap` is too small to hold the encoding
- `EBADMSG` — decode only: a missing key, a malformed or out-of-range value, or trailing junk


---
## The Six Codecs

One codec per HTTTP body type, each its own `.c` file. Every subsection below
gives the pair's functions, then the wire format those functions read and write.
All bodies are plaintext `key value` lines. Single public header,
`include/statusbody.h`.

### STATE — `application/tetris-state` (`state.c`)

| Function | Description |
|---|---|
| `body_state_encode(in, out, cap)` | Serialise one gameplay snapshot; validates `phase`, `charge` 0–10, `clearing_count` 0–4, `hold` −1–15, opponent count and usernames, and cell type/color nibbles before writing |
| `body_state_decode(buf, len, out)` | Parse a snapshot back; strict on key order, requires every key, exactly 20 board rows of 20 hex chars per board, and no trailing bytes |

Fixed key order, exactly as encoded:

```
seq 42
phase active
piece 3 1 4 0
next 0 1 2
hold 5 1
score 1200
lines 14
level 2
combo 3
b2b 1
charge 7
ability 2 1
clear tetris
clearing 2 350 18 19
countdown 0
pending 0
effects 0 0 0 0 0 0 0 0
result none 0
board
00000000000000000000
...                     (exactly 20 rows)
opponents 1
opp 2 7 1 clearing 4200 9 3 4 2 6 3 8 3 rival
00000000000000000000
...                     (exactly 20 rows, per opponent)
```

| Key | Form |
|---|---|
| `piece` | `<type> <rotation> <col> <row>` |
| `next` | `<t0> <t1> <t2>` (`BODY_NEXT_COUNT` = 3) |
| `hold` | `<type\|-1> <0\|1>` — held piece and whether the hold is already spent on the falling one; `BODY_HOLD_EMPTY` (`-1`) = holding nothing |
| `ability` | `<level> <0\|1>` — last activation; level `0` = none |
| `clear` | `none\|single\|double\|triple\|tetris\|tspin\|tspin_mini\|perfect` |
| `clearing` | `<count> <ms> [<rows>...]`, `count` 0–4 |
| `countdown` | `<ms>` remaining before a dealt match begins; `0` when none is running |
| `pending` | `<rows>` of garbage queued against **this** player and not yet landed; `0` in Single |
| `effects` | `<paralysis> <inversion> <nue> <thwack> <fry> <dark> <pals> <mirror>` — the status effects riding on this player. The first four are how many of this player's pieces are left under them; the rest are `1` or `0` |
| `result` | `<none\|won\|lost> <rank>` — how the match ended for this player; `none 0` while it is still being played |
| `board` | `BODY_BOARD_ROWS` (20) lines × `BODY_BOARD_COLS` (10) hex-pair cells: nibble `type` (0–2), nibble `color` (0–15) |
| `opponents` | `<n>`, 0–`BODY_OPPONENTS_MAX`; each followed by an `opp` line and that opponent's board block |
| `opp` | `<slot> <pid> <alive> <phase> <score> <lines> <pending> <ptype> <protation> <pcol> <prow> <charge> <character> <username>` — `charge` is the same 0–10 meter the frame's own subject carries, and `character` the catalogue id of the fighter they took into this match (`0` when they named none). Both are here so a client can draw the rival's column beside their board rather than guessing at it; `username` stays last because it runs to end of line |

`phase` reads `active`, `clearing`, `paused`, `topout`, or `countdown`.
`countdown` is a dealt board being held still before a match starts — it is
every player's clock stopped, which is what makes it a different thing from
`paused`, one player's own.

`countdown`, `pending`, `effects`, `result` and `opponents` are always
written, carrying `0` or `none` when there is nothing to say, the way `clearing` has always been written
with a count of `0`. No line's presence depends on another line's value, so a
decoder never looks ahead; a Single snapshot is the frame it always was plus
those five empty lines.

`result` is not a phase, because the winner's board is doing nothing a phase
could describe — it is simply still active with a piece on it, exactly like a
board mid-game. Without a field of its own the winner would never be told they
had won. `rank` rides alongside it because a Battle Royale defeat is a placing
rather than a bare loss.

An opponent's board rides inside the recipient's snapshot rather than arriving
as a snapshot of its own, because `tetrisd` holds one `STATE` mailbox slot per
client and a second push would destroy the first — and carrying both in one
message makes them the same instant by construction.

`pending` appears twice and means the same thing about two different players:
the top-level line is garbage owed to the recipient, the `opp` field is garbage
owed to that opponent. Both land at their subject's next piece lock rather than
on arrival, so both are visible before they are real — which is the point of
sending a count at all.

`effects` exists because an effect nobody can see is indistinguishable from a
bug. Every one of them is enforced by `tetrisd` and always was — but Paralysis
looked exactly like a rotate key that had stopped responding, Inversion like
the terminal had swapped the arrows, and Nue like a stuck spacebar, because the
server refused the input and the client was never told why. Dark could not be
carried out at all: blacking out a field is the one thing in the catalogue only
a renderer can do.

The client reads these as counts to display, never as rules to apply. It
compares them against the ones it was last sent, and anything that went up is
something that just landed — a comparison rather than an event, because a
snapshot states what is true now and the `STATE` lane is a latest-wins mailbox
that may drop one.

`username` is last on its line and may not contain a space: every field before
it is positional, so one space shifts all of them. The encoder refuses such a
name rather than writing a line its own decoder would misread.

### Rooms — `LIST /rooms` rows (`rooms.c`)

| Function | Description |
|---|---|
| `body_rooms_encode(rows, count, out, cap)` | Serialise `LIST /rooms`; `count == 0` yields a valid empty body |
| `body_rooms_decode(buf, len, rows, cap, count)` | Parse room rows; an empty buffer decodes to `count == 0`, not an error; more rows than `cap` fails `ERANGE` |

One line per room:

```
<name> <mode> <players>/<slots> <status> <owner>
```

- `mode`  ∈ `SINGLE | DOUBLE | BATTLE_ROYALE` 
- `status` ∈ `WAITING | READY | SELECTING | IN_GAME | FINISHED`. `SELECTING` is the character-select window: the room is committed and nobody is playing yet
- Rooms travel by **name** (`S-01`, `BR-10`), never by id. The ids run per mode, so only the prefixed name is unique. `mode` rides as its own field so clients never parse the prefix.

### Room — `LIST /room/<name>` snapshot (`room.c`)

| Function | Description |
|---|---|
| `body_room_encode(in, out, cap)` | Serialise one authoritative waiting-room snapshot |
| `body_room_decode(buf, len, out)` | Parse the fixed room header and its ordered occupied-seat rows |

The header carries the room name, mode, status, the milliseconds left in the
character-select window, minimum players, capacity, and occupied-seat count.
Each following row carries the server slot, player id, owner/player role,
ready/waiting state, the character that seat has declared for this match, and
username. Rows must be in strictly increasing server-slot order; refreshes
therefore preserve a stable roster.

`select` is `0` when no window is running. It is the room's clock and not each
client's, so two players browsing the same roster are shown the same number and
are dealt in at the same instant. A seat is locked in exactly when its
`character` is non-zero — there is no second flag that could disagree with it,
and opening a window clears every seat's character so the fact belongs to this
match rather than the last one.

### Chat — `application/tetris-chat` (`chat.c`)

| Function | Description |
|---|---|
| `body_chat_encode(in, out, cap)` | Serialise one line of a room's feed; rejects an empty or unprintable `text`, and a sender that disagrees with the kind |
| `body_chat_decode(buf, len, out)` | Parse one feed line; requires the schema order, a non-zero `seq`, and no trailing bytes |

Player chat and system narration are **one type with two authors**, not two
mechanisms. Narration is a line the server wrote, so `kind` is `system` and
there is no `sender` line at all — an absent key rather than an empty one, so
"who wrote this" has exactly one representation per kind.

```
seq 7
at 1786294796099
kind player
sender amber          (absent when kind is system)
text good luck all
```

- `text` runs to end of line and may contain spaces, so it is last and nothing
  follows it. A newline or any other control character would end the line early
  and re-decode as a different message, so the codec **rejects** them rather
  than escaping — one message has one representation. The same rule keeps an
  escape sequence arriving as chat from being somebody else's cursor.
- `seq` is the room's own counter, starting at 1. `tetrisd`'s chat lane drops
  its oldest message rather than closing a slow client, and this is what makes
  that gap visible instead of silent.

### Profile — ProfileView (`profile.c`)

| Function | Description |
|---|---|
| `body_profile_encode(in, out, cap)` | Serialise the ProfileView body; owned lists written as `<count> <id>...` |
| `body_profile_decode(buf, len, out)` | Parse a profile body; enforces `BODY_USER_MAX` on `username` and `BODY_OWNED_MAX` on both owned lists |

Fixed key order, one key per line; owned lists are count-prefixed and capped at `BODY_OWNED_MAX` (64) in both directions:

```
username alice
wallet 1200
score 4800
rank 3
equipped_character 2
equipped_theme 1
owned_characters <count> <id>...
owned_themes <count> <id>...
```

### Leaderboard — rows (`leaderboard.c`)

| Function | Description |
|---|---|
| `body_leaderboard_encode(rows, count, out, cap)` | Serialise leaderboard rows, rank ascending; `count == 0` yields a valid empty body |
| `body_leaderboard_decode(buf, len, rows, cap, count)` | Parse leaderboard rows; an empty buffer decodes to `count == 0`, not an error |

One line per rank, ascending:

```
<rank> <username> <score>
```

### Catalogue — `LIST /store` rows (`catalogue.c`)

| Function | Description |
|---|---|
| `body_catalogue_encode(in, out, cap)` | Serialise both catalogues, each section count-prefixed |
| `body_catalogue_decode(buf, len, out)` | Parse both sections; a count past `BODY_CATALOGUE_MAX` is refused before a row is read |

Characters first, then themes:

```
characters <n>
<id> <price> <name>          (exactly n lines)
themes <n>
<id> <price> <name>          (exactly n lines)
```

The name runs to end of line and may contain spaces, which is why it is last; the id is the catalogue id, which carries gaps and is therefore not a position.

### Error Codes

| `errno` | Meaning |
|---|---|
| `EINVAL` | Caller misuse — NULL argument or an out-of-range field handed to an encoder |
| `EBADMSG` | Bad wire data — missing key, malformed or out-of-range value, trailing junk |
| `ERANGE` | Buffer too small — `cap` cannot hold the encoding, or more rows than `cap` |

---
## Project Structure

```text
libstatusbody/
├── include/statusbody.h    Public header — the whole API
├── src/
│   ├── state.c             application/tetris-state encode/decode
│   ├── rooms.c             LIST /rooms row encode/decode
│   ├── room.c              LIST /room/<name> snapshot encode/decode
│   ├── chat.c              application/tetris-chat encode/decode
│   ├── profile.c           ProfileView encode/decode
│   ├── leaderboard.c       leaderboard row encode/decode
│   ├── catalogue.c         LIST /store catalogue encode/decode
│   ├── body_util.h           Private — shared append/scan primitives
│   └── body_util.c           Private — not part of the public API
├── tests/test_*.c          Unit tests, one per module (each with its own main)
├── scripts/run_tests.sh    Formatted test runner
├── obj/                    Generated objects
└── libstatusbody.a         Generated archive
```
