# libhtttp Design

Date: 2026-07-22

## Goal

Create `lib/libhtttp` as a self-contained static C library for tetriSH
plaintext protocol messages. It must parse and serialise HTTTP/1.0 requests and
responses, own parsed message memory, validate required protocol headers, and
dispatch request methods through a caller-supplied table.

The library forms the application layer above `libtetrissh`:

```text
structured HTTTP message
  -> libhtttp serialised plaintext
  -> libtetrissh encrypted frame
  -> TCP socket
```

One complete decrypted `libtetrissh` frame is one complete HTTTP message.

## Sources And Decisions

This design was derived from:

- the course tetriSH page at <https://natalieagus.github.io/50005/pa/tetrish>
- `README.md`
- `docs/corestack.md`
- `docs/architecture.md`
- `docs/requirements.md`
- `docs/checkoff-prep.md`
- `docs/code_style.md`
- the existing `libtetrissh` frame API and tests

The course page deliberately leaves parser representation, ownership, error
codes, and dispatch details open. Its normative method table and examples also
conflict: the table uses `/room/<id>`, while examples use `/arena/<name>` and an
undefined `GET` method. This library adopts one coherent baseline:

- baseline game paths use `/room/...`
- required methods are `JOIN`, `LEAVE`, `START`, `MOVE`, `ROTATE`, `DROP`, and
  `STATE`
- pushed state is a server-originated request-form `STATE` message
- parser accepts other syntactically valid methods so extensions do not require
  parser changes
- published example `Content-Length` values are not copied because several are
  byte-incorrect

## Requirements

- Build standalone with `make -C lib/libhtttp` into
  `lib/libhtttp/libhtttp.a`.
- Test standalone with `make -C lib/libhtttp test`.
- Public API lives in `lib/libhtttp/include/htttp.h`.
- Compile with `-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror
  -pedantic`.
- Accept an explicit byte pointer and byte length; input is not required to be
  NUL-terminated.
- Limit each complete message to 65,536 bytes, matching
  `TETRISSH_MAX_PLAINTEXT`.
- Use exact `HTTTP/1.0` version text and CRLF line endings.
- Keep message bodies binary-safe.
- Retain no parser or dispatch state between calls.
- Perform no socket, crypto, logging, filesystem, room, or game-state work.
- Allocate no mutable global state.

## Non-Goals

- No TCP or Unix socket operations.
- No calls to `session_send()` or `session_recv()`.
- No encryption, certificate, or authentication implementation.
- No JSON parser or game-state body schema.
- No path-template matching or extraction of room/player IDs.
- No room permissions, move legality, rate limiting, or status selection from
  application state.
- No streaming parser or reassembly of a message across frames.
- No request pipelining or transaction-correlation protocol in the first
  integration.
- No daemon/client integration until those network paths exist.
- No backward compatibility because no executable `libhtttp` API exists yet.

## Layering And Message Flow

The caller owns transport and application policy:

```text
session_recv()
  -> complete plaintext bytes
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

`session_recv()` returns raw bytes and does not append NUL. Therefore every
parser operation uses explicit bounds and never calls an unbounded string
function on network input.

`STATE` can arrive while the client waits for an ordinary response. Its wire
form is deliberately distinguishable from a status response:

```text
STATE /room/main HTTTP/1.0\r\n
Content-Type: application/tetris-state\r\n
Content-Length: 3\r\n
\r\n
...binary or textual state bytes...
```

Initial client integration allows one outstanding client request. The receive
loop distinguishes request-form `STATE` from status-line responses. Generic
headers allow a later request-ID extension without changing parser structure.

## Wire Grammar

```text
REQUEST       ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
REQUEST-LINE  ::= METHOD SP PATH SP "HTTTP/1.0" CRLF

RESPONSE      ::= STATUS-LINE *(HEADER CRLF) CRLF [BODY]
STATUS-LINE   ::= "HTTTP/1.0" SP STATUS-CODE SP REASON-PHRASE CRLF
```

### Request Line

- `METHOD` begins with `A` through `Z`.
- Remaining method bytes may be `A` through `Z`, `0` through `9`, `_`, or `-`.
- `PATH` begins with `/`.
- Path bytes are visible ASCII except space.
- Tokens are separated by exactly one ASCII space.
- Version is exactly `HTTTP/1.0`.
- Lowercase methods are malformed rather than silently normalised.

The parser validates path syntax only. Meaning of `/room/<id>` and
`/room/<id>/player/<pid>` belongs to application handlers.

### Status Line

- Version is exactly `HTTTP/1.0`.
- Status code is exactly three decimal digits in the range 100 through 599.
- Reason phrase is non-empty visible ASCII and spaces.
- Custom phrases such as `INVALID_MOVE` remain valid.

The default reason helper covers:

| Code | Phrase |
|---|---|
| 200 | OK |
| 201 | Created |
| 400 | Bad Request |
| 401 | Unauthorized |
| 403 | Forbidden |
| 404 | Not Found |
| 409 | Conflict |
| 413 | Payload Too Large |
| 429 | Too Many Requests |
| 500 | Internal Server Error |

### Headers

- Header syntax is `name: value` followed by CRLF.
- Name uses ASCII letters, digits, and hyphen, with at least one byte.
- Whitespace before the colon is rejected.
- Parser splits on the first colon, so later colons remain value data.
- Leading and trailing space or horizontal tab around values is removed.
- Header values may be empty; otherwise they contain visible ASCII plus space
  or horizontal tab. DEL and bytes above ASCII are rejected.
- Header lookup and duplicate detection are ASCII case-insensitive.
- Duplicate header names are rejected, including differently cased names.
- Header folding and control characters are rejected.
- Unknown headers are retained.
- At most 64 headers are accepted.

Rejecting all duplicates avoids ambiguous `Content-Length` interpretation and
keeps header lookup deterministic. No required tetriSH header needs duplicate
wire fields.

### Bodies And Content-Length

- Body begins immediately after the first `\r\n\r\n` delimiter.
- Body is opaque bytes and may contain NUL, CR, or LF.
- Any non-empty body requires one decimal `Content-Length` header.
- Empty body may omit `Content-Length` or use `Content-Length: 0`.
- Signs, non-digits, internal whitespace, integer overflow, or duplicate length
  fields are rejected.
- Parsed length must equal every remaining byte exactly.
- Short body, long body, and trailing bytes return a length-mismatch error.
- Serialiser ignores any stored `Content-Length` value and emits one value
  computed from the owned body.
- Serialiser omits `Content-Length` for an empty body.

`Content-Length` always counts body bytes, not characters or code points.

### Line Endings

Start lines and headers require CRLF. Bare LF and a CR not followed by LF are
malformed. Body bytes are excluded from this line-ending scan.

## Required Header Validation

Syntactic parsing is separate from contextual protocol validation. This lets a
daemon parse malformed or unauthenticated traffic far enough to select an
appropriate status without embedding authentication state in the codec.

`htttp_validate()` enforces:

- every response has a non-empty `Date` header
- request bodies use `Content-Type: application/tetris-command`
- `STATE` bodies use `Content-Type: application/tetris-state`
- an authenticated-request flag requires `Player-Id`

Initial `JOIN` can be validated without the authenticated flag. Server-originated
`STATE` also omits that flag. Applications remain responsible for deciding when
a session becomes authenticated and whether a supplied player ID belongs to it.

`htttp_format_date()` accepts an explicit `time_t` and writes RFC 1123 UTC text.
Using an injected timestamp keeps tests deterministic. Implementation uses
`gmtime_r()` rather than shared static calendar storage, then formats from fixed
English weekday/month tables rather than locale-sensitive `%a` or `%b` output.

## Owned Message Model

Parsed and constructed messages use one explicit ownership model:

- caller allocates the `t_htttp_message` structure
- `htttp_message_init()` creates the empty state
- constructors and `htttp_parse()` require an initialized, empty output message
- builders deep-copy methods, paths, reasons, headers, and bodies
- parser builds the same owned representation
- `htttp_message_free()` releases every field and restores empty state
- cleanup is idempotent
- body length is stored separately, so embedded NUL is preserved
- library retains no pointer into caller input after `htttp_parse()` returns

Functions that allocate build temporary state first. Failure releases partial
state and leaves the required empty output message empty. A caller must free an
existing message before reusing it as constructor or parser output; this avoids
silently discarding caller-owned allocations.

The message structure is public for simple C integration and checkoff
explainability. Builders and validation remain the supported mutation path;
serialisation validates public fields before using them.

## Public API

```c
# define HTTTP_VERSION                         "HTTTP/1.0"
# define HTTTP_MAX_MESSAGE_SIZE                65536u
# define HTTTP_MAX_HEADERS                     64u
# define HTTTP_DATE_BUFSIZE                    30u
# define HTTTP_CONTENT_TYPE_COMMAND            "application/tetris-command"
# define HTTTP_CONTENT_TYPE_STATE              "application/tetris-state"
# define HTTTP_VALIDATE_AUTHENTICATED_REQUEST  0x01u

typedef enum
{
	HTTTP_MESSAGE_REQUEST = 0,
	HTTTP_MESSAGE_RESPONSE
}	t_htttp_message_type;

typedef enum
{
	HTTTP_OK = 0,
	HTTTP_ERR_INVALID_ARGUMENT,
	HTTTP_ERR_TOO_LARGE,
	HTTTP_ERR_NO_MEMORY,
	HTTTP_ERR_MALFORMED_START_LINE,
	HTTTP_ERR_UNSUPPORTED_VERSION,
	HTTTP_ERR_MALFORMED_HEADER,
	HTTTP_ERR_TOO_MANY_HEADERS,
	HTTTP_ERR_DUPLICATE_HEADER,
	HTTTP_ERR_INVALID_CONTENT_LENGTH,
	HTTTP_ERR_LENGTH_MISMATCH,
	HTTTP_ERR_MISSING_REQUIRED_HEADER,
	HTTTP_ERR_INVALID_MESSAGE,
	HTTTP_ERR_NO_HANDLER
}	t_htttp_result;

typedef struct
{
	char	*name;
	char	*value;
}	t_htttp_header;

typedef struct
{
	t_htttp_message_type	type;
	char					*method;
	char					*path;
	unsigned int			status_code;
	char					*reason;
	t_htttp_header			*headers;
	size_t					header_count;
	unsigned char			*body;
	size_t					body_len;
}	t_htttp_message;

typedef struct
{
	const char	*method;
	int			(*handler)(const t_htttp_message *message, void *context);
}	t_htttp_route;

void			htttp_message_init(t_htttp_message *message);
void			htttp_message_free(t_htttp_message *message);
t_htttp_result	htttp_message_make_request(t_htttp_message *message,
					const char *method, const char *path);
t_htttp_result	htttp_message_make_response(t_htttp_message *message,
					unsigned int status_code, const char *reason);
t_htttp_result	htttp_message_set_header(t_htttp_message *message,
					const char *name, const char *value);
const char		*htttp_message_get_header(const t_htttp_message *message,
					const char *name);
t_htttp_result	htttp_message_set_body(t_htttp_message *message,
					const void *body, size_t body_len);
const char		*htttp_reason_phrase(unsigned int status_code);
const char		*htttp_result_string(t_htttp_result result);

t_htttp_result	htttp_parse(const unsigned char *data, size_t data_len,
					t_htttp_message *out);
t_htttp_result	htttp_serialize(const t_htttp_message *message,
					unsigned char **out, size_t *out_len);
t_htttp_result	htttp_validate(const t_htttp_message *message,
					unsigned int flags);
t_htttp_result	htttp_format_date(time_t timestamp,
					char out[HTTTP_DATE_BUFSIZE]);
t_htttp_result	htttp_dispatch(const t_htttp_message *message,
					const t_htttp_route *routes, size_t route_count,
					void *context, int *handler_result);
```

Serialised output is allocated with `malloc()`. Caller passes it to
`session_send()` and then releases it with `free()`.

## Parser Failure Policy

| Result | Meaning |
|---|---|
| `HTTTP_ERR_INVALID_ARGUMENT` | NULL or invalid API argument |
| `HTTTP_ERR_TOO_LARGE` | input or output exceeds 65,536 bytes |
| `HTTTP_ERR_NO_MEMORY` | allocation failed |
| `HTTTP_ERR_MALFORMED_START_LINE` | invalid request/status structure |
| `HTTTP_ERR_UNSUPPORTED_VERSION` | version is not exact `HTTTP/1.0` |
| `HTTTP_ERR_MALFORMED_HEADER` | CRLF/header/token rule failed |
| `HTTTP_ERR_TOO_MANY_HEADERS` | more than 64 headers |
| `HTTTP_ERR_DUPLICATE_HEADER` | repeated case-insensitive name |
| `HTTTP_ERR_INVALID_CONTENT_LENGTH` | missing, malformed, or overflowing length |
| `HTTTP_ERR_LENGTH_MISMATCH` | declared body length differs from remaining bytes |
| `HTTTP_ERR_MISSING_REQUIRED_HEADER` | contextual protocol header absent or wrong |
| `HTTTP_ERR_INVALID_MESSAGE` | inconsistent in-memory message or response dispatch |
| `HTTTP_ERR_NO_HANDLER` | no route matches request method |

Suggested application mapping:

- grammar, version, header, length, and unknown method errors -> `400`
- oversized message known before secure send -> `413`
- authentication failure -> `401`
- authenticated but forbidden operation -> `403`
- missing room/player -> `404`
- invalid move or room-state conflict -> `409`
- application rate limit -> `429`
- allocation/internal application failure -> `500`

A malformed but authenticated secure frame can receive `400` and leave the
connection open. A `libtetrissh` authentication or frame failure is
connection-fatal because plaintext cannot be trusted.

## Serialiser Design

The serialiser performs two bounded passes:

1. Validate the in-memory start line and header text, count exact encoded bytes,
   and reject size overflow.
2. Allocate the exact byte count and write canonical CRLF output.

Ordinary headers retain insertion order. Any stored `Content-Length` is skipped
and replaced with the body byte count. No terminating NUL is part of returned
wire length.

On failure, serialiser sets `*out` to `NULL` and `*out_len` to zero.

## Dispatch Design

Dispatch is stateless. Caller supplies an immutable route array:

```c
static const t_htttp_route routes[] = {
	{"JOIN", handle_join},
	{"MOVE", handle_move},
	{"STATE", handle_state}
};
```

`htttp_dispatch()` accepts request-form messages only, compares method text
exactly, invokes one handler with caller context, and copies the handler's
integer result to `handler_result`. It does not parse paths, construct statuses,
or retain routes.

Adding `PAUSE` requires a handler and route entry only. Parser and serialiser do
not change.

## Threading, Mutexes, And IPC

All mutable state belongs to the caller or current function invocation. Static
tables contain immutable reason/error strings only. Separate messages can be
parsed, validated, serialised, and dispatched concurrently.

`libhtttp` performs no blocking syscall and holds no mutex. Daemon callers must
follow the project rule:

```text
lock game state -> copy/build message -> unlock -> serialise/send
```

TCP backpressure must never occur while a room mutex is held.

## File Layout

```text
lib/libhtttp/
    Makefile
    README.md
    include/
        htttp.h
    src/
        message.c
        parser.c
        serialiser.c
        validation.c
        dispatch.c
    tests/
        test_message.c
        test_parser.c
        test_serialiser.c
        test_validation.c
        test_dispatch.c
        test_hardening.c
    scripts/
        run_tests.sh
```

Generated `obj/`, `tests/bin/`, and `libhtttp.a` remain ignored build outputs.

## Testing Strategy

1. Message ownership:
   - init/free is idempotent
   - every builder deep-copies input
   - header lookup/replacement is case-insensitive
   - body preserves embedded NUL
   - 65th header is rejected

2. Parser grammar:
   - valid request, response, custom reason, and extension method
   - strict version and token rules
   - bare LF, stray CR, NUL, controls, and folding rejected
   - first-colon split
   - case-insensitive duplicate rejection

3. Body boundaries:
   - exact, zero, missing, malformed, overflowing, short, and long lengths
   - binary body
   - exact 65,536-byte complete message accepted
   - 65,537-byte input rejected

4. Serialiser:
   - canonical CRLF bytes
   - computed `Content-Length`
   - text-field injection rejected
   - exact output size and overflow
   - parse/serialise round trip

5. Validation:
   - deterministic RFC 1123 date
   - response `Date`
   - command/state content type
   - authenticated `Player-Id`

6. Dispatch:
   - exact handler and context
   - handler result propagation
   - unknown method and response rejection
   - `PAUSE` extension without parser changes

7. Hardening:
   - every truncated prefix
   - delimiter mutation
   - repeated parse/free and serialise/free
   - Valgrind with all leak kinds and nonzero error exit
   - ASan/UBSan where supported

## Incremental Commits

Each passing increment is committed and pushed to `feat/libcorehtttp`, followed
by a teaching summary and explicit user approval before the next increment.

1. Design and implementation plan documents.
2. Build scaffold, public API, lifecycle, and first tests.
3. Owned request/response/header/body builders.
4. Strict start-line and header parser.
5. Body and `Content-Length` parsing.
6. Canonical serialiser and round-trip tests.
7. RFC 1123 date and required-header validation.
8. Stateless method dispatch.
9. Malformed-input hardening and final README.

## Explanation Contract

After every code increment, provide:

```text
### Change: <title>

Files changed: every file and function
Why: protocol or systems requirement
Line-by-line: key lines and why they exist
Mutex/IPC: held locks, channel, and blocking behavior
Failure mode: returned result and cleanup behavior
Tests: commands and observed results
Commit: SHA and pushed branch
```

## Shared Boundary

`lib/libhtttp/include/htttp.h` defines public protocol types and dispatch
interfaces used by multiple binaries. Both Sanjan and Zi Qi must review it
before merge. No repository-level duplicate `include/htttp.h` will be created.
