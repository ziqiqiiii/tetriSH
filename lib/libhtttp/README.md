# libhtttp

`libhtttp` is the standalone plaintext HTTTP/1.0 codec used above
`libtetrissh`. It builds and owns structured messages, parses one complete
decrypted frame, serialises canonical wire bytes, validates required protocol
headers, and dispatches request methods through a caller-supplied table.

## Layering

```text
session_recv()
  -> complete decrypted plaintext frame
  -> htttp_parse()
  -> htttp_validate()
  -> htttp_dispatch()
  -> application handler

application handler
  -> message builder helpers
  -> htttp_validate()
  -> htttp_serialize()
  -> session_send()
```

One `libtetrissh` frame contains one complete HTTTP message. `libhtttp` does
not open sockets, encrypt data, reassemble streams, authenticate players, parse
body schemas, or apply game rules.

## Build And Test

```bash
make -C lib/libhtttp
make -C lib/libhtttp test
make -C lib/libhtttp test FILTER=parser
make -C lib/libhtttp memcheck
make -C lib/libhtttp clean
make -C lib/libhtttp fclean
make -C lib/libhtttp re
```

Build output is `lib/libhtttp/libhtttp.a`. Consumers link the archive and add
the public include directory:

```bash
cc ... -I lib/libhtttp/include lib/libhtttp/libhtttp.a
```

- **Plaintext only:** `libhtttp` parses and serialises bytes above the secure
  session. It has no socket or cryptographic code and no inter-library link
  dependency.
- **One-shot parser:** the normal caller supplies one complete decrypted frame.
  A proposed `HTTTP_INCOMPLETE` result remains useful as a defensive response
  when the caller passes a truncated buffer.
- **Caller-owned messages:** parsed structures and body storage belong to the
  caller; the library retains no state between calls.
- **Strict grammar:** the version literal is `HTTTP/1.0`, line endings are CRLF,
  headers split on their first colon, and body length must match
  `Content-Length`.
- **Extensible dispatch:** methods map to registered handler functions, keeping
  application rules in `tetrisd` or `tetrisu`.

The library and tests compile as C11 with `-Wall -Wextra -Werror -pedantic`.
The test runner treats both nonzero exits and emitted `FAIL` records as suite
failures. `memcheck` treats every Valgrind leak kind as an error.

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

- `METHOD` starts with `A-Z`; later bytes may be `A-Z`, `0-9`, `_`, or `-`.
- `PATH` starts with `/` and contains visible ASCII except spaces.
- Baseline game paths use `/room/...`; path meaning belongs to handlers.
- `STATUS-CODE` is exactly three digits from 100 through 599.
- `REASON-PHRASE` is non-empty visible ASCII plus spaces.
- Every protocol line ends with CRLF. Bare LF and stray CR are rejected.
- Header folding and case-insensitive duplicate names are rejected.
- `Content-Length` contains one or more decimal digits with no sign or spaces.
- Input is an explicit byte pointer and length; it need not end with NUL.
- Complete messages are limited to 65,536 bytes and 64 emitted headers.

Required game methods are `JOIN`, `LEAVE`, `START`, `MOVE`, `ROTATE`, `DROP`,
and `STATE`. Syntactically valid extension methods are accepted without parser
changes. Pushed game state is request-form, not a status response:

Escaped wire bytes:

```text
STATE /room/main HTTTP/1.0\r\nContent-Type: application/tetris-state\r\nContent-Length: 3\r\n\r\nabc
```

Body is exactly three bytes: `abc`.

## Headers And Bodies

Header names contain ASCII letters, digits, and hyphens. Lookup and duplicate
detection are ASCII case-insensitive. Whitespace before `:` is rejected;
parsing and builder insertion normalize values by trimming leading and trailing
space or horizontal tab. Embedded whitespace is preserved. Parsing splits at
first colon, so `Trace: room:a:join` preserves `room:a:join` as value. Unknown
headers remain in message.

Bodies are opaque bytes and may contain NUL, CR, or LF. Every non-empty body
requires decimal `Content-Length`, and declared length must equal all remaining
frame bytes. Empty bodies may omit it or declare zero. Serialisation ignores a
stored `Content-Length` value and emits one canonical value from `body_len`;
empty bodies omit header. This generated header counts toward the 64-header wire
limit: a body may accompany at most 63 non-`Content-Length` headers.

## Ownership

Caller owns `t_htttp_message` storage and every successful parse or builder
result:

```c
t_htttp_message message;
t_htttp_result result;

htttp_message_init(&message);
result = htttp_parse(frame, frame_len, &message);
if (result == HTTTP_OK) {
    /* use message; all fields remain owned by message */
}
htttp_message_free(&message);
```

Constructors and `htttp_parse()` require initialized empty output. Builders
deep-copy method, path, reason, headers, and binary body. Parse failure leaves
output empty. `htttp_message_free()` releases all owned fields, restores empty
state, and is safe to repeat. `htttp_serialize()` returns an exact-length byte
buffer released with `free()`.

## Required Validation

Parsing checks wire syntax. `htttp_validate()` separately applies contextual
requirements:

| Message | Requirement |
|---|---|
| Response | RFC 1123 UTC `Date` naming a real Gregorian date with matching weekday |
| Request with body | `Content-Type: application/tetris-command` |
| `STATE` request with body | `Content-Type: application/tetris-state` |
| Authenticated request | Non-empty `Player-Id` when `HTTTP_VALIDATE_AUTHENTICATED_REQUEST` is set |

Initial `JOIN` and server-originated `STATE` calls omit authenticated-request
flag. `htttp_format_date()` writes deterministic 29-byte RFC 1123 UTC text plus
NUL into `HTTTP_DATE_BUFSIZE` bytes.

## Method Dispatch

Dispatch is exact and case-sensitive. First matching route runs; handler return
value is written separately from `t_htttp_result`. Unknown methods return
`HTTTP_ERR_NO_HANDLER`. Complete route table is validated before any handler is
called. Dispatch also applies structural and base protocol validation before
route lookup, so malformed requests or bodies missing required `Content-Type`
cannot reach a handler. After matching, route `validation_flags` are applied;
authenticated routes therefore reject a missing or empty `Player-Id` before
application code runs.

```c
static int handle_pause(const t_htttp_message *message, void *context)
{
    (void)message;
    (void)context;
    return (200);
}

static const t_htttp_route routes[] = {
    {"PAUSE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, handle_pause}
};

int handler_result;

if (htttp_dispatch(&message, routes,
        sizeof(routes) / sizeof(routes[0]), context,
        &handler_result) != HTTTP_OK) {
    /* INVALID_ARGUMENT, INVALID_MESSAGE, MISSING_REQUIRED_HEADER,
     * or NO_HANDLER; malformed requests never reach handlers. */
}
```

Adding `PAUSE` needs no parser change: register route with zero flags for a
public method or `HTTTP_VALIDATE_AUTHENTICATED_REQUEST` for a player method,
then implement handler in calling application. This flag checks required
identity metadata only. Daemon must still compare `Player-Id` with player bound
to secure connection; `libtetrissh` authenticates server and frame bytes, not
client identity.

## Status And Error Ownership

`t_htttp_result` reports local codec/validation/dispatch outcomes. Use
`htttp_result_string()` for diagnostics. Caller maps protocol failures to wire
responses:

| Condition | Typical response |
|---|---|
| Malformed grammar, headers, or body length | `400 Bad Request` |
| Unknown method | `400 Bad Request` |
| Authentication failure | `401 Unauthorized` |
| Authenticated but forbidden operation | `403 Forbidden` |
| Missing room or player | `404 Not Found` |
| Invalid game transition | `409 Conflict` |
| Message larger than one secure frame | `413 Payload Too Large` |
| Application rate limit | `429 Too Many Requests` |
| Internal application failure | `500 Internal Server Error` |

Library supplies default phrases for `200`, `201`, `400`, `401`, `403`, `404`,
`409`, `413`, `429`, and `500`. Calling daemon owns status selection and all
application policy.

## Thread Safety

Library retains no parser, dispatch, transport, or mutable global state.
Independent messages can be parsed, validated, serialised, and dispatched from
different threads. Caller must not concurrently mutate same message, route
table, context, or handler-owned state. Codec functions perform no blocking IPC.
`htttp_dispatch()` invokes selected handler synchronously and therefore inherits
handler blocking and locking behavior. Daemon handlers must copy state under
lock, unlock, then perform any blocking syscall; no mutex may remain held across
network or IPC backpressure.
