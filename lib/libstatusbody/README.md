# libstatusbody

> The HTTTP message-body codec shared by both ends of tetriSH — ten body types,
> each an encode/decode pair over plaintext `key value` lines.

`tetrisd` encodes, `tetrisu` and `tetrisctl` decode. This README carries the wire
formats; `include/statusbody.h` documents every struct and the reasoning behind
each one, `lib/libhtttp/README.md` the messages that carry them.

---

## Table of Contents

- [Build And Test](#build-and-test)
- [Codec Contract](#codec-contract)
- [Body Formats](#body-formats)
- [Project Structure](#project-structure)

---

## Build And Test

```bash
make -C lib/libstatusbody              # -> lib/libstatusbody/libstatusbody.a
make -C lib/libstatusbody test         # add FILTER=state for one suite
make -C lib/libstatusbody clean|fclean|re
```

Link it with `cc ... -I lib/libstatusbody/include lib/libstatusbody/libstatusbody.a`.
C11 under `-Wall -Wextra -Werror -pedantic`; one suite per codec plus
`test_hardening.c`, which asserts every worst-case encode fits the frame cap.

---

## Codec Contract

A codec is one `body_*_encode` / `body_*_decode` pair — the two halves of one
body type, over `#include "statusbody.h"`. `decode(encode(x)) == x`, and `encode`
is deterministic.

| Call | Success | Failure |
|---|---|---|
| `body_*_encode(in, out, cap)` | bytes written; never writes past `cap` | `-1`, `errno` set |
| `body_*_decode(buf, len, out)` | `0`; never reads past `len` | `-1`, `errno` set |

| `errno` | Meaning |
|---|---|
| `EINVAL` | Caller misuse — NULL argument, or a field an encoder cannot represent |
| `ERANGE` | Buffer too small — `cap` cannot hold the encoding, or more rows than `cap` |
| `EBADMSG` | Bad wire data — missing key, misordered line, out-of-range value, trailing junk |

Rules every body shares:

- Lines are `key value`, LF-terminated, in a fixed order; decoders require every
  key in that order and refuse trailing bytes.
- A field that may contain spaces (`username`, chat `text`, item `name`) is last
  on its line, and an encoder refuses a value that would shift the positional
  fields before it.
- Row bodies (`rooms`, `leaderboard`, `players`) take `rows`, `cap`, `count`; an
  empty buffer decodes to `count == 0` rather than an error.
- The library allocates nothing — the caller passes a buffer and its capacity.
  Size a `STATE` buffer against `BODY_STATE_MAX_BYTES`, never against a number
  that happened to be big enough when it was written.

---

## Body Formats

Every body is `application/tetris-status`, except pushed `STATE`
(`application/tetris-state`) and `CHAT` (`application/tetris-chat`).

### State — pushed `STATE` (`state.c`, `arena.c`)

```
seq <u64>
phase active|clearing|paused|topout|countdown
piece <type> <rotation> <col> <row>
next <t0> <t1> <t2>                    BODY_NEXT_COUNT
hold <type|-1> <spent>                 -1 is BODY_HOLD_EMPTY; spent blocks a
                                       second swap on the falling piece
score <u64>
lines <n>   level <n>   combo <n>   b2b <0|1>   charge <0-BODY_CHARGE_MAX>
ability <level> <accepted>             last activation; level 0 = none
clear none|single|double|triple|tetris|tspin|tspin_mini|perfect
clearing <count 0-4> <elapsed_ms> [<rows>...]   named rows still filled below
countdown <ms>                         0 when no match is being dealt
pending <rows>                         garbage owed here, landing at the next lock
effects <paralysis> <inversion> <nue> <thwack> <fry> <dark> <pals> <mirror>
                                       first four count pieces left under them
result <none|won|lost> <rank>
board                                  then BODY_BOARD_ROWS (20) lines of
                                       BODY_BOARD_COLS (10) hex-pair cells:
                                       type nibble (0-2), colour nibble (0-15)
opponents <n>                          0-BODY_OPPONENTS_MAX (1)
opp <slot> <pid> <alive> <phase> <score> <lines> <pending> <ptype> <protation>
    <pcol> <prow> <charge> <character> <username>    then that opponent's board
counts <players> <alive>               the room's head count, on every frame
arena <full|absent> <n>
a <slot> <pid-hex> <flags> <lines> <pending> <ko> <rank> [cells]    per card
```

`countdown`, `pending`, `effects`, `result` and `opponents` are always written,
carrying `0` or `none` when there is nothing to say, so no line's presence
depends on another's value and a decoder never looks ahead. `phase countdown` is
every player's clock stopped, `paused` one player's own; `result` is not a phase,
a winner's board being simply still active.

`arena full <n>` is the **complete** occupied roster — a seat that does not
appear is a seat nobody is in, never a knocked-out player — and is not the same
answer as `arena absent 0`, which is no news about the arena at all. A card's
`slot` runs **1–`BODY_ARENA_MAX`** (99), `flags` is a bitfield (alive,
attacking-you, targeted-by-you, in-clear, cells-present), `rank` is `0` until the
player is out, and `cells` is 200 hex characters — one nibble per cell, `0`
empty, `1` garbage, `2 +` type a piece — carried only when the cells-present
flag says so, a dead board never changing again. Colour is not carried: a card is
a thumbnail a few terminal cells wide.

### Rooms — `LIST /rooms` (`rooms.c`)

```
<name> <mode> <players>/<slots> <status> <owner>
```

`mode` ∈ `SINGLE | DOUBLE | BATTLE_ROYALE`; `status` ∈ `WAITING | READY |
SELECTING | IN_GAME | FINISHED`, `SELECTING` being the character-select window —
committed, nobody playing yet. Rooms travel by **name** (`S-01`, `BR-10`), never
by id: ids run per mode, so only the prefixed name is unique, and `mode` rides as
its own field so a client never parses the prefix.

### Room — `LIST /room/<name>` (`room.c`)

```
room BR-10             then mode, status, select, required, capacity, members
members 2              <n>, then that many slot lines
slot 1 7 owner ready 3 amber
                       <slot> <pid> <owner|player> <ready|waiting> <character>
                       <username>, in strictly increasing slot order
```

Rows being ordered, a refresh preserves a stable roster. `select` is the
milliseconds left in the character-select window and `0` when none is running —
the room's clock, not each client's. A seat is locked in exactly when its
`character` is non-zero; there is no second flag that could disagree with it.

### Chat — `CHAT /room/<name>`, both directions (`chat.c`)

```
seq 7
at 1786294796099
kind player
sender amber          (absent when kind is system)
text good luck all
```

Player chat and system narration are one type with two authors: narration is a
line the server wrote, so `kind` is `system` and `sender` is **absent** rather
than empty. A control character in `text` would re-decode as a different message,
so the codec rejects one rather than escaping it. `seq` is the room's own counter
from 1 — the chat lane drops its oldest message rather than closing a slow
client, and this is what makes that gap visible.

### Profile — `PROFILE`, `BUY`, `EQUIP` (`profile.c`)

```
username alice
wallet 1200
score 4800
rank 3
equipped_character 2
equipped_theme 1
owned_characters <count> <id>...     count-prefixed, capped at
owned_themes <count> <id>...         BODY_OWNED_MAX (64) both ways
```

### Leaderboard — `LEADERBOARD` (`leaderboard.c`)

```
<rank> <username> <score>            one line per rank, ascending
```

### Catalogue — `LIST /store` (`catalogue.c`)

```
characters <n>
<id> <price> <name>          (exactly n lines)
themes <n>
<id> <price> <name>          (exactly n lines)
```

A count past `BODY_CATALOGUE_MAX` (16) is refused before a row is read. The id is
the catalogue id, which carries gaps and is therefore never a position.

### Health — `STATUS /admin` (`health.c`)

```
pid <n>
uptime_ms <n>
connections <n>
rooms <n>
tick_ms <n> configured
sink reaching|unreachable
```

`tick_ms` carries the literal word `configured` because that is what the number
is — the interval from `.tetrishrc`, not an observed rate — and the decoder
requires the label, a body that dropped it parsing to the same number while
claiming a measurement nobody made. There is no `health` field: the fields are
the Health.

### Players — `PLAYERS /admin` (`players.c`)

```
<connection> <username> <room>       one line per connection
```

A connection that has not logged in is a Client but not yet a Player, and is
still listed — its username reads `(anonymous)` (`BODY_ANONYMOUS`), one in no
room reads `-`, both placeholders rather than empty fields because the row is
positional. The wire cannot tell an anonymous row from a player registered under
that exact name, so `t_body_player_row` carries `authenticated` and callers read
that instead of the name.

`BODY_PLAYERS_MAX` (256) is the listing cap, as a **row count** rather than a
byte count so a fixture can prove truncation and a reader can predict it; the
overflow travels as an `Omitted: <n>` header and the status stays `200`.

### Dropped — `DROPPED /admin` (`dropped.c`)

```
dropped <n>
```

Log records `tetrisd` never sent because its ring buffer was full — never summed
with `tetrislogd`'s Rejected or Degraded, which are different quantities (see
`docs/CONTEXT.md`). A server with nothing dropped still sends `dropped 0`, so an
empty body means the answer never arrived and decodes as `EBADMSG`.

---

## Project Structure

```text
libstatusbody/
├── include/statusbody.h    Public header — the whole API
├── src/                    One .c per body type, named in its section above
│   └── body_util.{c,h}     Private — shared append/scan primitives
├── tests/test_*.c          One suite per codec, plus hardening
├── scripts/run_tests.sh    Formatted test runner
└── obj/, libstatusbody.a   Generated objects and archive
```
