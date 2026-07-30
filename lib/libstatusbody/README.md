# libstatusbody

`libstatusbody` is the shared HTTTP message-body codec library for tetriSH. It encodes the plaintext bodies `tetrisd` sends and decodes the ones `tetrisu`
receives.

---

## Table of Contents

- [Build And Test](#build-and-test)
- [Usage](#usage)
- [Codec Contract](#codec-contract)
- [The Four Codecs](#the-four-codecs)
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

t_sb_state	frame;
t_sb_state	decoded;
char		body[4096];
int		len;

memset(&frame, 0, sizeof(frame));
frame.seq = 42;
frame.phase = SB_PHASE_ACTIVE;
frame.piece = (t_sb_piece){ .type = 3, .rotation = 1, .col = 4, .row = 0 };
frame.charge = 7;
/* ... board cells, next queue, score/lines/level ... */

len = sb_state_encode(&frame, body, sizeof(body));   /* -1: EINVAL or ERANGE */

/* tetrisu, on receipt of the HTTTP body: */
if (len > 0 && sb_state_decode(body, (size_t)len, &decoded) == 0) {
    /* decoded mirrors frame field-for-field */
}
```

---

## Codec Contract

A *codec* (coder/decoder) is one `sb_*_encode`/`sb_*_decode` pair — the two
halves of a single body type. Every pair shares one contract:

| Call | Success | Failure |
|---|---|---|
| `sb_*_encode(in, out, cap)` | bytes written | `-1`, `errno` set — see below; never writes past `cap` |
| `sb_*_decode(buf, len, out)` | `0` | `-1`, `errno` set — see below; never reads past `len` |

On failure both return `-1` and set `errno`:

- `EINVAL` — a NULL argument, or a field an encoder cannot represent
- `ERANGE` — `cap` is too small to hold the encoding
- `EBADMSG` — decode only: a missing key, a malformed or out-of-range value, or trailing junk


---
## The Four Codecs

One codec per HTTTP body type, each its own `.c` file. Every subsection below
gives the pair's functions, then the wire format those functions read and write.
All bodies are plaintext `key value` lines. Single public header,
`include/statusbody.h`.

### STATE — `application/tetris-state` (`state.c`)

| Function | Description |
|---|---|
| `sb_state_encode(in, out, cap)` | Serialise one gameplay snapshot; validates `phase`, `charge` 0–10, `clearing_count` 0–4 and cell type/color nibbles before writing |
| `sb_state_decode(buf, len, out)` | Parse a snapshot back; strict on key order, requires every key, exactly 20 board rows of 20 hex chars, and no trailing bytes |

Fixed key order, exactly as encoded:

```
seq 42
phase active
piece 3 1 4 0
next 0 1 2
score 1200
lines 14
level 2
combo 3
b2b 1
charge 7
ability 2 1
clear tetris
clearing 2 350 18 19
board
00000000000000000000
...                     (exactly 20 rows)
```

| Key | Form |
|---|---|
| `piece` | `<type> <rotation> <col> <row>` |
| `next` | `<t0> <t1> <t2>` (`SB_NEXT_COUNT` = 3) |
| `ability` | `<level> <0\|1>` — last activation; level `0` = none |
| `clear` | `none\|single\|double\|triple\|tetris\|tspin\|tspin_mini\|perfect` |
| `clearing` | `<count> <ms> [<rows>...]`, `count` 0–4 |
| `board` | `SB_BOARD_ROWS` (20) lines × `SB_BOARD_COLS` (10) hex-pair cells: nibble `type` (0–2), nibble `color` (0–15) |

### Rooms — `LIST /rooms` rows (`rooms.c`)

| Function | Description |
|---|---|
| `sb_rooms_encode(rows, count, out, cap)` | Serialise `LIST /rooms`; `count == 0` yields a valid empty body |
| `sb_rooms_decode(buf, len, rows, cap, count)` | Parse room rows; an empty buffer decodes to `count == 0`, not an error; more rows than `cap` fails `ERANGE` |

One line per room:

```
<name> <mode> <players>/<slots> <status> <owner>
```

- `mode`  ∈ `SINGLE | DOUBLE | BATTLE_ROYALE` 
- `status` ∈ `WAITING | READY | IN_GAME | FINISHED`
- Rooms travel by **name** (`S-01`, `BR-10`), never by id. The ids run per mode, so only the prefixed name is unique. `mode` rides as its own field so clients never parse the prefix.

### Profile — ProfileView (`profile.c`)

| Function | Description |
|---|---|
| `sb_profile_encode(in, out, cap)` | Serialise the ProfileView body; owned lists written as `<count> <id>...` |
| `sb_profile_decode(buf, len, out)` | Parse a profile body; enforces `SB_USER_MAX` on `username` and `SB_OWNED_MAX` on both owned lists |

Fixed key order, one key per line; owned lists are count-prefixed and capped at `SB_OWNED_MAX` (64) in both directions:

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
| `sb_leaderboard_encode(rows, count, out, cap)` | Serialise leaderboard rows, rank ascending; `count == 0` yields a valid empty body |
| `sb_leaderboard_decode(buf, len, rows, cap, count)` | Parse leaderboard rows; an empty buffer decodes to `count == 0`, not an error |

One line per rank, ascending:

```
<rank> <username> <score>
```

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
│   ├── profile.c           ProfileView encode/decode
│   ├── leaderboard.c       leaderboard row encode/decode
│   ├── sb_util.h           Private — shared append/scan primitives
│   └── sb_util.c           Private — not part of the public API
├── tests/test_*.c          Unit tests, one per module (each with its own main)
├── scripts/run_tests.sh    Formatted test runner
├── obj/                    Generated objects
└── libstatusbody.a         Generated archive
```
