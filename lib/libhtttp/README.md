# libhtttp

> The plaintext HTTTP/1.0 codec for tetriSH, sitting above `libtetrissh` —
> structured messages, a one-shot parser, a canonical serialiser, header
> validation, and method dispatch.

This README carries the grammar and the method table; `libstatusbody` documents
the body formats, `src/tetrisd/README.md` the statuses each route answers with.

---

## Table of Contents

- [Build And Test](#build-and-test)
- [Usage](#usage)
- [Wire Grammar](#wire-grammar)
- [Method Table](#method-table)
- [Headers And Bodies](#headers-and-bodies)
- [Dispatch](#dispatch)
- [API Reference](#api-reference)
- [Architecture](#architecture)
- [Project Structure](#project-structure)

---

## Build And Test

```bash
make -C lib/libhtttp                     # -> lib/libhtttp/libhtttp.a
make -C lib/libhtttp test                # add FILTER=parser for one suite
make -C lib/libhtttp memcheck
make -C lib/libhtttp clean|fclean|re
```

Link it with `cc ... -I lib/libhtttp/include lib/libhtttp/libhtttp.a`. C11 under
`-Wall -Wextra -Werror -pedantic`.

---

## Usage

```c
/* inbound: one complete decrypted frame from session_recv() */
htttp_message_init(&message);
if (htttp_parse(frame, frame_len, &message) == HTTTP_OK)
    htttp_dispatch(&message, routes, route_count, ctx, &handler_result);
htttp_message_free(&message);

/* outbound: build, serialise, hand the bytes to session_send() */
htttp_message_init(&message);
htttp_message_make_response(&message, 200, htttp_reason_phrase(200));
htttp_message_set_header(&message, "Date", date_buf);
htttp_message_set_body(&message, body, body_len);
if (htttp_serialize(&message, &wire, &wire_len) == HTTTP_OK)
{
    session_send(&sess, wire, wire_len);
    free(wire);                     /* the caller owns the serialised buffer */
}
htttp_message_free(&message);
```

---

## Wire Grammar

```text
REQUEST      ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
REQUEST-LINE ::= METHOD SP PATH SP "HTTTP/1.0" CRLF

RESPONSE     ::= STATUS-LINE *(HEADER CRLF) CRLF [BODY]
STATUS-LINE  ::= "HTTTP/1.0" SP STATUS-CODE SP REASON-PHRASE CRLF

HEADER       ::= FIELD-NAME ":" OWS FIELD-VALUE OWS
FIELD-NAME   ::= 1*(ALPHA / DIGIT / "-")
FIELD-VALUE  ::= *(HTAB / SP / VCHAR)
OWS          ::= *(SP / HTAB)
BODY         ::= exactly Content-Length opaque bytes
```

| Rule | Constraint |
|---|---|
| `METHOD` | Starts `A-Z`; later bytes `A-Z`, `0-9`, `_`, or `-` |
| `PATH` | Starts `/`, visible ASCII except spaces; meaning belongs to handlers |
| `STATUS-CODE` | Exactly three digits, `100`–`599` |
| `REASON-PHRASE` | Non-empty visible ASCII plus spaces |
| Line endings | CRLF everywhere — bare LF and stray CR are rejected |
| Header folding | Rejected, as are case-insensitive duplicate names |
| `Content-Length` | One or more decimal digits, no sign or spaces |
| Size limits | `HTTTP_MAX_MESSAGE_SIZE` (65,536 bytes), `HTTTP_MAX_HEADERS` (64) |

Input is an explicit pointer and length — no NUL terminator required.

---

## Method Table

Auth means a non-empty `Player-Id` header.

| Method | Path | Auth | Purpose |
|---|---|:---:|---|
| `SIGNUP` | `/account` | — | Register an account |
| `LOGIN` | `/session` | — | Authenticate and bind the session |
| `LIST` | `/rooms` | ✓ | Browse open rooms |
| `JOIN` | `/room/<id>` | ✓ | Create or join a room |
| `LEAVE` | `/room/<id>` | ✓ | Leave a room |
| `START` | `/room/<id>` | ✓ | Owner starts the game |
| `READY` | `/room/<id>` | ✓ | Declare or withdraw readiness, and name the fighter to play it with |
| `CHAT` | `/room/<id>` | ✓ | Post a room message; **server-pushed** when delivering the feed |
| `TARGET` | `/room/<id>` | ✓ | Battle Royale — which kind of rival this player's garbage goes to (`mode random\|ko\|attackers\|badges`) |
| `MOVE` | `/room/<id>/player/<pid>` | ✓ | Translate the falling piece |
| `ROTATE` | `/room/<id>/player/<pid>` | ✓ | Rotate the falling piece |
| `DROP` | `/room/<id>/player/<pid>` | ✓ | Soft or hard drop |
| `ABILITY` | `/room/<id>/player/<pid>` | ✓ | Activate a Gaiden ability |
| `STATE` | `/room/<id>/player/<pid>` | ✓ | **Server-pushed** authoritative game state; the path names its subject |
| `BUY` | `/store/character/<cid>`, `/store/theme/<tid>` | ✓ | Purchase a catalogue item |
| `EQUIP` | `/player/<pid>/character/<cid>`, `/player/<pid>/theme/<tid>` | ✓ | Set a default character or theme |
| `PROFILE` | `/player/<pid>` | ✓ | Fetch the ProfileView body |
| `LEADERBOARD` | `/leaderboard` | ✓ | Fetch ranked rows |
| `STATUS` | `/admin` | — | `tetrisctl` — server Health |
| `ROOMS` | `/admin` | — | `tetrisctl` — list rooms |
| `PLAYERS` | `/admin` | — | `tetrisctl` — list connected sessions |
| `DROPPED` | `/admin` | — | `tetrisctl` — the producer-side Dropped counter |
| `KICK` | `/admin/player/<username>` | — | `tetrisctl` — remove a player |
| `SHUTDOWN` | `/admin` | — | `tetrisctl` — graceful shutdown |

`STATE` and `CHAT` are pushed in *request* form and arrive between ordinary
replies, so a client read loop cannot assume one request means one reply. The
`/admin` methods carry no Auth mark because they travel only on the Control
channel, where authorisation is filesystem permission on a `0600` socket — the
reachability *is* the credential, and nothing arriving there names a Player.

---

## Headers And Bodies

- Names are ASCII letters, digits, and hyphens; lookup and duplicate detection
  are case-insensitive, whitespace before `:` is rejected, unknown names are
  retained, and a value is trimmed of surrounding SP/HTAB.
- Parsing splits at the **first** colon, so `Trace: room:a:join` keeps
  `room:a:join`.
- Bodies are opaque bytes and may contain NUL, CR, or LF. A non-empty body needs
  a decimal `Content-Length` equal to all remaining frame bytes; an empty body
  may omit it or declare `0`.
- Serialisation ignores any stored `Content-Length` and emits its own from
  `body_len`. That generated header counts against the 64-header limit, leaving
  a body at most 63 others.

`htttp_validate()` then applies contextual requirements, without mutating the
message.

| Message | Requirement |
|---|---|
| Response | RFC 1123 UTC `Date` naming a real Gregorian date whose weekday matches |
| Request with body | `Content-Type: application/tetris-command` |
| `STATE` request with body | `Content-Type: application/tetris-state` |
| `CHAT` request with body | Either `application/tetris-command` or `application/tetris-chat` |
| Response with body | `Content-Type: application/tetris-status` |
| Authenticated request | Non-empty `Player-Id`, when `HTTTP_VALIDATE_AUTHENTICATED_REQUEST` is set |

A validator cannot see which direction a message travels, so `CHAT` — the one
method that goes both ways — accepts either type and every other method exactly
one. `SIGNUP`, `LOGIN`, the initial `JOIN`, and pushed `STATE` omit the
authenticated-request flag.

---

## Dispatch

Routing is exact and case-sensitive; the first match runs, an unknown method
returns `HTTTP_ERR_NO_HANDLER`. `htttp_dispatch()` matches the table, then
applies base validation, then the route's `validation_flags` — so a malformed
request, a body missing its `Content-Type`, or an authenticated route with an
empty `Player-Id` never reaches application code.

```c
static const t_htttp_route routes[] = {
    {"PAUSE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, handle_pause}
};

htttp_dispatch(&message, routes, sizeof(routes) / sizeof(routes[0]),
    context, &handler_result);
```

Any valid extension method routes without a library change — zero flags for a
public one, `HTTTP_VALIDATE_AUTHENTICATED_REQUEST` for a player one. That flag
only checks the header is *present*: the daemon must still compare `Player-Id`
against the player bound to the connection, since `libtetrissh` authenticates
the server and the frame bytes, not the client's claimed identity.

---

## API Reference

Single public header, `include/htttp.h`, which documents every contract:
`htttp_parse`, `htttp_serialize`, `htttp_validate`, `htttp_format_date`,
`htttp_dispatch`, and the message surface — `htttp_message_init` / `_free` /
`_make_request` / `_make_response` / `_set_header` / `_get_header` / `_set_body`,
plus `htttp_reason_phrase` and `htttp_result_string`. A handler is
`int (*)(const t_htttp_message *message, void *context)`.

Types are `t_htttp_message`, `t_htttp_header`, `t_htttp_route`,
`t_htttp_message_type`, and `t_htttp_result` — the last reporting *local* codec,
validation, and dispatch outcomes, never a wire status. Default reason phrases
ship for `200`, `201`, `400`, `401`, `403`, `404`, `409`, `413`, `429`, `500`.

---

## Architecture

```text
session_recv()  ->  htttp_parse()  ->  htttp_validate()  ->  htttp_dispatch()  ->  handler
handler  ->  message builders  ->  htttp_validate()  ->  htttp_serialize()  ->  session_send()
```

One `libtetrissh` frame carries exactly one complete HTTTP message. No sockets,
no encryption, no stream reassembly, no authentication, no body schemas, no game
rules. `assets/libhtttp-sequence.md` draws the full client–server exchange.

The caller owns every message and every buffer: builders deep-copy their fields,
constructors and `htttp_parse()` need initialised empty output, and
`htttp_serialize()` returns an exact-length buffer released with `free()`.

The library keeps no parser, dispatch, transport, or global state, so
independent messages can be handled from different threads; the caller must not
concurrently mutate one message, route table, or context. `htttp_dispatch()`
invokes its handler synchronously and inherits that handler's blocking.

---

## Project Structure

```text
libhtttp/
├── include/htttp.h         Public header — the whole API
├── src/
│   ├── message.c           Message lifecycle, headers, body, reason/result text
│   ├── parser.c            One-shot frame parser
│   ├── serialiser.c        Canonical wire-byte emitter
│   ├── validation.c        Contextual header rules and RFC 1123 dates
│   └── dispatch.c          Route matching and handler invocation
├── assets/                 Sequence-diagram sources and renders
├── tests/test_*.c          Unit tests, one per module, plus fuzz and hardening
├── scripts/run_tests.sh    Formatted test runner
└── obj/, libhtttp.a        Generated objects and archive
```
