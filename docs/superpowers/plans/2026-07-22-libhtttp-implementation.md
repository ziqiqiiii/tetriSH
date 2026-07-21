# libhtttp Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a self-contained `lib/libhtttp` static C library providing strict HTTTP/1.0 parsing, canonical serialisation, owned messages, required-header validation, and stateless method dispatch.

**Architecture:** One complete decrypted `libtetrissh` frame is passed to a one-shot parser using an explicit byte length. Parsed and constructed messages own all method, path, reason, header, and body memory. Callers validate context, dispatch request methods through an immutable route table, serialise exact CRLF wire bytes, and pass those bytes to `session_send()`.

**Tech Stack:** C11, POSIX `gmtime_r`, Make static archive, assertion-based C tests, Valgrind, optional AddressSanitizer and UndefinedBehaviorSanitizer.

## Global Constraints

- Build with `make -C lib/libhtttp` into `lib/libhtttp/libhtttp.a`.
- Test with `make -C lib/libhtttp test` and optional `FILTER=<suite>`.
- Public API lives at `lib/libhtttp/include/htttp.h`.
- Use `-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic`.
- Exact protocol version is `HTTTP/1.0`.
- Start lines and headers require CRLF; bare LF and stray CR fail.
- One frame is one complete message with a 65,536-byte maximum.
- At most 64 headers are accepted.
- Bodies are opaque byte arrays and may contain NUL.
- Parser accepts syntactically valid extension methods.
- No socket, crypto, logging, filesystem, room, game, or global mutable state.
- Each task starts with a failing focused test and ends with all library tests passing.
- Each successful task is committed and pushed to `origin/feat/libcorehtttp`.
- Stop for user review after every pushed commit.
- Flag `lib/libhtttp/include/htttp.h` as a shared boundary requiring both teammates' review before merge.

---

## File Structure

- Create `lib/libhtttp/Makefile`: archive, test, filter, memory-check, and cleanup targets.
- Create `lib/libhtttp/include/htttp.h`: public constants, owned types, result codes, and API contracts.
- Create `lib/libhtttp/src/message.c`: lifecycle, builders, headers, body, status phrases, and result strings.
- Create `lib/libhtttp/src/parser.c`: one-shot bounded request/response parser.
- Create `lib/libhtttp/src/serialiser.c`: two-pass canonical wire encoder.
- Create `lib/libhtttp/src/validation.c`: required headers and RFC 1123 date helper.
- Create `lib/libhtttp/src/dispatch.c`: stateless exact-method route lookup.
- Create `lib/libhtttp/tests/test_message.c`: ownership and builder tests.
- Create `lib/libhtttp/tests/test_parser.c`: grammar and body-length tests.
- Create `lib/libhtttp/tests/test_serialiser.c`: canonical wire and round-trip tests.
- Create `lib/libhtttp/tests/test_validation.c`: required-header and date tests.
- Create `lib/libhtttp/tests/test_dispatch.c`: route lookup and extension tests.
- Create `lib/libhtttp/tests/test_hardening.c`: truncation, mutation, and repetition tests.
- Create `lib/libhtttp/scripts/run_tests.sh`: formatted runner with `FILTER` support.
- Modify `lib/libhtttp/README.md`: final protocol, ownership, examples, and extension guide.

---

### Task 1: Approved Design And Execution Plan

**Files:**
- Create: `docs/superpowers/specs/2026-07-22-libhtttp-design.md`
- Create: `docs/superpowers/plans/2026-07-22-libhtttp-implementation.md`

**Interfaces:**
- Consumes: approved design discussion and course/repository research.
- Produces: reviewed source of truth for Tasks 2 through 9.

- [ ] **Step 1: Write the design specification**

Record layering, wire grammar, `STATE` representation, ownership, public API,
errors, threading, testing, and shared-boundary rules in the design path above.

- [ ] **Step 2: Write this implementation plan**

Include exact file paths, interfaces, test vectors, commands, and commit gates.

- [ ] **Step 3: Self-review both documents**

Run:

```bash
git diff --check
git diff -- docs/superpowers/specs/2026-07-22-libhtttp-design.md docs/superpowers/plans/2026-07-22-libhtttp-implementation.md
```

Expected: no whitespace errors, placeholders, `/arena` wire decisions, or
conflicting function signatures.

- [ ] **Step 4: Commit and push**

```bash
git status --short
git diff --stat
git log --oneline -10
git add docs/superpowers/specs/2026-07-22-libhtttp-design.md
git add docs/superpowers/plans/2026-07-22-libhtttp-implementation.md
git commit -m "[libhtttp] document protocol design and implementation plan"
git push origin feat/libcorehtttp
```

Expected: one documentation-only commit pushed to the tracked feature branch.
Stop for user review.

---

### Task 2: Standalone Scaffold And Message Lifecycle

**Files:**
- Create: `lib/libhtttp/Makefile`
- Create: `lib/libhtttp/include/htttp.h`
- Create: `lib/libhtttp/src/message.c`
- Create: `lib/libhtttp/tests/test_message.c`
- Create: `lib/libhtttp/scripts/run_tests.sh`

**Interfaces:**
- Consumes: no other project library.
- Produces: `libhtttp.a`, final public types/signatures, message init/free, status phrase helper, and result-string helper.

- [ ] **Step 1: Add the complete public header**

Define these constants and types exactly:

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
```

Declare the `t_htttp_header`, `t_htttp_message`, and `t_htttp_route` structures
and every function shown in the design spec. Document ownership and failure
output for every public function.

- [ ] **Step 2: Write failing lifecycle tests**

Create `tests/test_message.c` with:

```c
static void	test_init_and_free_are_idempotent(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(message.method == NULL);
	assert(message.path == NULL);
	assert(message.reason == NULL);
	assert(message.headers == NULL);
	assert(message.header_count == 0);
	assert(message.body == NULL);
	assert(message.body_len == 0);
	htttp_message_free(&message);
	htttp_message_free(&message);
	printf("PASS test_init_and_free_are_idempotent\n");
}

static void	test_status_and_error_text(void)
{
	assert(strcmp(htttp_reason_phrase(200), "OK") == 0);
	assert(strcmp(htttp_reason_phrase(413), "Payload Too Large") == 0);
	assert(htttp_reason_phrase(418) == NULL);
	assert(strcmp(htttp_result_string(HTTTP_ERR_TOO_LARGE),
		"message too large") == 0);
	printf("PASS test_status_and_error_text\n");
}
```

- [ ] **Step 3: Run the test to verify failure**

```bash
make -C lib/libhtttp test FILTER=message
```

Expected: compilation/link failure because lifecycle helpers do not exist yet.

- [ ] **Step 4: Implement lifecycle and immutable text tables**

`htttp_message_init()` zeroes the complete structure after a NULL guard.
`htttp_message_free()` frees each header name/value, the header array, method,
path, reason, and body before returning to empty state. Reason and result helpers
return pointers to immutable literals and `NULL` for unknown numeric values.

Add one AI-assistance comment above the cleanup loop explaining why partial
message cleanup must be idempotent.

- [ ] **Step 5: Add standalone Makefile and test runner**

Mirror `libtetrissh` component conventions without OpenSSL/pthread linkage:

```make
NAME      := libhtttp.a
CC        := gcc
FLAGS     := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic
AR        := ar
ARFLAGS   := rcs
RM        := rm -rf
INC       := -Iinclude
SRC       := $(wildcard src/*.c)
OBJ       := $(SRC:src/%.c=obj/%.o)
TEST_SRC  := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRC:tests/%.c=tests/bin/%)
```

Targets must include `all`, `test`, `memcheck`, `clean`, `fclean`, and `re`.
`memcheck` runs every test binary with:

```text
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1
```

Copy the established formatted `FILTER` behavior from
`lib/libtetrissh/scripts/run_tests.sh`, changing only component labels.

- [ ] **Step 6: Verify focused and full tests**

```bash
make -C lib/libhtttp clean
make -C lib/libhtttp
make -C lib/libhtttp test FILTER=message
make -C lib/libhtttp test
```

Expected: warning-free archive build; one suite passes.

- [ ] **Step 7: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/Makefile lib/libhtttp/include/htttp.h
git add lib/libhtttp/src/message.c lib/libhtttp/tests/test_message.c
git add lib/libhtttp/scripts/run_tests.sh
git commit -m "[libhtttp] add standalone library scaffold"
git push origin feat/libcorehtttp
```

Stop for user review.

---

### Task 3: Owned Message Builders

**Files:**
- Modify: `lib/libhtttp/src/message.c`
- Modify: `lib/libhtttp/tests/test_message.c`

**Interfaces:**
- Consumes: initialized `t_htttp_message` from Task 2.
- Produces: request/response constructors, case-insensitive headers, binary-safe body copies, and complete cleanup.

- [ ] **Step 1: Write failing deep-copy tests**

Add tests proving:

```c
char			method[] = "MOVE";
unsigned char	body[] = {'A', '\0', 'B'};

assert(htttp_message_make_request(&message, method,
	"/room/r1/player/p17") == HTTTP_OK);
method[0] = 'X';
assert(strcmp(message.method, "MOVE") == 0);
assert(htttp_message_set_header(&message, "Player-Id", "p17") == HTTTP_OK);
assert(strcmp(htttp_message_get_header(&message, "player-id"), "p17") == 0);
assert(htttp_message_set_body(&message, body, sizeof(body)) == HTTTP_OK);
body[0] = 'Z';
assert(message.body[0] == 'A');
assert(message.body_len == 3);
```

Also test response construction, case-insensitive replacement, empty-body reset,
64 accepted headers, rejected 65th header, and cleanup after partial ownership.

- [ ] **Step 2: Run focused test and observe failure**

```bash
make -C lib/libhtttp test FILTER=message
```

Expected: unresolved builder functions or failed assertions.

- [ ] **Step 3: Implement minimal owned builders**

Use checked `malloc()`/`realloc()` and private string-copy helpers. Constructors
require an initialized, empty output and deep-copy into temporary pointers
before changing it. Header setting replaces an existing value case-insensitively
or grows the array by one. Body length zero frees the previous body and stores
NULL.

- [ ] **Step 4: Run focused, full, and memory tests**

```bash
make -C lib/libhtttp test FILTER=message
make -C lib/libhtttp test
make -C lib/libhtttp memcheck FILTER=message
```

Expected: all pass, zero Valgrind errors/leaks.

- [ ] **Step 5: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/src/message.c lib/libhtttp/tests/test_message.c
git commit -m "[libhtttp] add owned message builders"
git push origin feat/libcorehtttp
```

Stop for user review.

---

### Task 4: Strict Start-Line And Header Parser

**Files:**
- Create: `lib/libhtttp/src/parser.c`
- Create: `lib/libhtttp/tests/test_parser.c`

**Interfaces:**
- Consumes: message builders from Task 3.
- Produces: `htttp_parse()` for no-body requests/responses and bounded headers.

- [ ] **Step 1: Write valid parser tests**

Use exact byte arrays:

```c
"JOIN /room/a HTTTP/1.0\r\n\r\n"
"PAUSE /room/a HTTTP/1.0\r\nX-Extension: enabled\r\n\r\n"
"HTTTP/1.0 409 INVALID_MOVE\r\nDate: Thu, 01 Jan 1970 00:00:00 GMT\r\n\r\n"
"JOIN /room/a HTTTP/1.0\r\nTrace: room:a:join\r\n\r\n"
```

Assert request/response type, copied fields, custom reason, unknown method,
trimmed header value, and first-colon preservation.

- [ ] **Step 2: Write malformed parser tests**

Cover wrong/missing version, lowercase/invalid method, path without `/`, path
space/control/NUL, malformed status, missing reason, bare LF, stray CR, missing
blank line, whitespace before colon, empty name, folded header, controls,
DEL/non-ASCII bytes, case-insensitive duplicate, and 65 headers.

- [ ] **Step 3: Run parser test and observe failure**

```bash
make -C lib/libhtttp test FILTER=parser
```

Expected: unresolved `htttp_parse()`.

- [ ] **Step 4: Implement bounded line parsing**

Scan only within `data_len`. Locate CRLF without `strstr()` or unbounded string
functions. Classify a line beginning with `HTTTP/` as response; otherwise parse
request. Allocate each token only after validating its byte range. Split every
header at its first colon and reject duplicate names before calling the public
header setter.

Add one AI-assistance comment near bounded line scanning explaining that
`session_recv()` does not NUL-terminate network data.

- [ ] **Step 5: Verify tests and memory**

```bash
make -C lib/libhtttp test FILTER=parser
make -C lib/libhtttp test
make -C lib/libhtttp memcheck FILTER=parser
```

- [ ] **Step 6: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/src/parser.c lib/libhtttp/tests/test_parser.c
git commit -m "[libhtttp] parse start lines and headers"
git push origin feat/libcorehtttp
```

Stop for user review.

---

### Task 5: Body And Content-Length Parsing

**Files:**
- Modify: `lib/libhtttp/src/parser.c`
- Modify: `lib/libhtttp/tests/test_parser.c`

**Interfaces:**
- Consumes: bounded header parser and binary body setter.
- Produces: exact body-length parsing through 65,536-byte complete messages.

- [ ] **Step 1: Write body success tests**

Cover `LEFT`, empty body with omitted length, empty body with zero length, and:

```c
const unsigned char body[] = {'A', '\0', 'B'};
```

Build complete input with explicit lengths so `memcmp()` proves all three body
bytes survive.

- [ ] **Step 2: Write length failure tests**

Cover missing length with trailing data, signs, non-digits, internal spaces,
`size_t` overflow, duplicate length, short body, long body, and trailing bytes.

For exact maximum, use this 50-byte prefix plus 65,486 body bytes:

```text
STATE /room/a HTTTP/1.0\r\nContent-Length: 65486\r\n\r\n
```

Assert total 65,536 succeeds and total 65,537 returns `HTTTP_ERR_TOO_LARGE`.

- [ ] **Step 3: Run parser suite and observe failures**

```bash
make -C lib/libhtttp test FILTER=parser
```

- [ ] **Step 4: Implement checked decimal parsing and exact body copy**

Before multiplying by ten, check `value > (SIZE_MAX - digit) / 10`. Require
`Content-Length` when bytes remain after the blank line. Compare parsed length
against exact remaining bytes before allocating/copying the body.

- [ ] **Step 5: Verify tests and memory**

```bash
make -C lib/libhtttp test FILTER=parser
make -C lib/libhtttp test
make -C lib/libhtttp memcheck FILTER=parser
```

- [ ] **Step 6: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/src/parser.c lib/libhtttp/tests/test_parser.c
git commit -m "[libhtttp] enforce body length boundaries"
git push origin feat/libcorehtttp
```

Stop for user review.

---

### Task 6: Canonical Serialiser

**Files:**
- Create: `lib/libhtttp/src/serialiser.c`
- Create: `lib/libhtttp/tests/test_serialiser.c`

**Interfaces:**
- Consumes: complete owned `t_htttp_message`.
- Produces: allocated exact-length canonical HTTTP bytes.

- [ ] **Step 1: Write exact request-output test**

Construct a MOVE message and compare exact bytes against:

```text
MOVE /room/r1/player/p17 HTTTP/1.0\r\n
Content-Type: application/tetris-command\r\n
Player-Id: p17\r\n
Content-Length: 4\r\n
\r\n
LEFT
```

The actual fixture is one adjacent C string without source-code newlines.

- [ ] **Step 2: Write response, failure, and round-trip tests**

Cover response status/custom reason, empty body, stale stored `Content-Length`
replacement, embedded-NUL body, text-field CR/LF/NUL injection, exact 65,536
output, oversized output, NULL outputs on failure, and parse-serialise-parse
semantic equality.

- [ ] **Step 3: Run serialiser suite and observe failure**

```bash
make -C lib/libhtttp test FILTER=serialiser
```

- [ ] **Step 4: Implement two-pass serialisation**

First pass validates public text fields and checked-adds every encoded byte.
Second pass writes start line, ordinary headers in insertion order, generated
length when body is non-empty, blank CRLF, and exact body bytes. Do not append a
wire NUL. Set output pointer/length to empty before any failure can occur.

Add one AI-assistance comment near checked size accumulation explaining why
attacker-controlled lengths must not wrap before allocation.

- [ ] **Step 5: Verify tests and memory**

```bash
make -C lib/libhtttp test FILTER=serialiser
make -C lib/libhtttp test
make -C lib/libhtttp memcheck FILTER=serialiser
```

- [ ] **Step 6: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/src/serialiser.c lib/libhtttp/tests/test_serialiser.c
git commit -m "[libhtttp] serialize canonical messages"
git push origin feat/libcorehtttp
```

Stop for user review.

---

### Task 7: Required Header Validation And RFC 1123 Date

**Files:**
- Create: `lib/libhtttp/src/validation.c`
- Create: `lib/libhtttp/tests/test_validation.c`

**Interfaces:**
- Consumes: parsed or constructed owned messages.
- Produces: contextual required-header validation and deterministic UTC date formatting.

- [ ] **Step 1: Write deterministic date test**

```c
char	date[HTTTP_DATE_BUFSIZE];

assert(htttp_format_date((time_t)0, date) == HTTTP_OK);
assert(strcmp(date, "Thu, 01 Jan 1970 00:00:00 GMT") == 0);
```

- [ ] **Step 2: Write required-header matrix**

Assert response without `Date` fails; valid response date passes; request body
requires command content type; `STATE` body requires state content type;
authenticated request requires `Player-Id`; unauthenticated `JOIN` passes
without it; extension method body uses command content type.

- [ ] **Step 3: Run validation suite and observe failure**

```bash
make -C lib/libhtttp test FILTER=validation
```

- [ ] **Step 4: Implement validation and date formatting**

Use case-insensitive existing header lookup. Compare media types exactly. Use
`gmtime_r()` followed by bounded `snprintf()` using fixed English weekday and
month tables; do not use locale-sensitive `%a` or `%b`. Require the resulting
wire string to occupy 29 bytes plus NUL.

- [ ] **Step 5: Verify tests and memory**

```bash
make -C lib/libhtttp test FILTER=validation
make -C lib/libhtttp test
make -C lib/libhtttp memcheck FILTER=validation
```

- [ ] **Step 6: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/src/validation.c lib/libhtttp/tests/test_validation.c
git commit -m "[libhtttp] validate required protocol headers"
git push origin feat/libcorehtttp
```

Stop for user review.

---

### Task 8: Stateless Method Dispatch

**Files:**
- Create: `lib/libhtttp/src/dispatch.c`
- Create: `lib/libhtttp/tests/test_dispatch.c`

**Interfaces:**
- Consumes: request-form `t_htttp_message` and immutable `t_htttp_route` array.
- Produces: exact method lookup, context forwarding, and handler-result propagation.

- [ ] **Step 1: Write handler selection test**

```c
static int	handle_pause(const t_htttp_message *message, void *context)
{
	int	*count;

	count = context;
	assert(strcmp(message->method, "PAUSE") == 0);
	(*count)++;
	return (202);
}
```

Dispatch one `PAUSE` request and assert count becomes one and handler result is
202. This proves extension requires no parser modification.

- [ ] **Step 2: Write dispatch failure tests**

Cover unknown method, response message, NULL message, NULL routes with nonzero
count, NULL handler entry, and NULL handler-result pointer.

- [ ] **Step 3: Run dispatch suite and observe failure**

```bash
make -C lib/libhtttp test FILTER=dispatch
```

- [ ] **Step 4: Implement immutable linear route lookup**

Validate all arguments and request type. Compare method with `strcmp()`. Invoke
only first exact match, store its integer result, and return `HTTTP_OK`. Return
`HTTTP_ERR_NO_HANDLER` when no route matches. Retain no table/context pointers.

- [ ] **Step 5: Verify tests and memory**

```bash
make -C lib/libhtttp test FILTER=dispatch
make -C lib/libhtttp test
make -C lib/libhtttp memcheck FILTER=dispatch
```

- [ ] **Step 6: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/src/dispatch.c lib/libhtttp/tests/test_dispatch.c
git commit -m "[libhtttp] add stateless method dispatch"
git push origin feat/libcorehtttp
```

Stop for user review.

---

### Task 9: Malformed-Input Hardening And Final Documentation

**Files:**
- Create: `lib/libhtttp/tests/test_hardening.c`
- Modify: `lib/libhtttp/README.md`
- Modify: public comments only if tests/review found incomplete contracts.

**Interfaces:**
- Consumes: complete library API from Tasks 2 through 8.
- Produces: adversarial coverage, final user documentation, and verified release-ready archive.

- [ ] **Step 1: Write truncation and mutation tests**

For valid request and response fixtures, call `htttp_parse()` on every shorter
prefix and assert non-success plus safe cleanup. Mutate each CR and LF delimiter,
repeat parse/free and serialise/free loops, preserve values containing later
colons, and verify every declared result has a diagnostic string.

- [ ] **Step 2: Run hardening test and observe failures**

```bash
make -C lib/libhtttp test FILTER=hardening
```

Expected: any unhandled truncation/result-string case fails before hardening.

- [ ] **Step 3: Make minimal hardening corrections**

Fix only demonstrated parser, cleanup, or diagnostic gaps. Do not add streaming,
JSON, path routing, authentication state, or daemon integration.

- [ ] **Step 4: Replace planning README**

Document layering, exact grammar, `/room` paths, request-form `STATE`, ownership,
required validation, status mapping, thread safety, build/test/memcheck commands,
and a `PAUSE` route extension example. Use byte-correct examples only.

- [ ] **Step 5: Run clean strict verification**

```bash
make -C lib/libhtttp fclean
make -C lib/libhtttp
make -C lib/libhtttp test
make -C lib/libhtttp test FILTER=parser
make -C lib/libhtttp test FILTER=serialiser
make -C lib/libhtttp test FILTER=validation
make -C lib/libhtttp test FILTER=dispatch
make -C lib/libhtttp test FILTER=hardening
make -C lib/libhtttp memcheck
```

Expected: all builds warning-free, every suite passes, Valgrind reports zero
errors and zero definitely/indirectly/possibly lost bytes.

- [ ] **Step 6: Run sanitizer verification**

```bash
make -C lib/libhtttp fclean
make -C lib/libhtttp test FLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer"
```

Expected: all suites pass without sanitizer report. If sanitizer runtime is
unavailable, report exact toolchain error; do not claim sanitizer success.

- [ ] **Step 7: Restore ordinary build and verify root discovery**

```bash
make -C lib/libhtttp fclean
make -C lib/libhtttp test
make libs AUTO_INSTALL_DEPS=0
git status --short --ignored
```

Expected: root build includes `lib/libhtttp`; only ignored generated objects,
archives, and test binaries remain outside source diff.

- [ ] **Step 8: Commit and push**

```bash
git diff --check
git status --short
git diff
git log --oneline -10
git add lib/libhtttp/README.md lib/libhtttp/tests/test_hardening.c
git add lib/libhtttp/include/htttp.h lib/libhtttp/src
git commit -m "[libhtttp] harden protocol and document API"
git push origin feat/libcorehtttp
```

Stop for final user review before branch completion choices.

---

## Per-Commit Review Template

```text
### Change: <title>

Files changed: every file and function
Why: protocol or systems requirement
Line-by-line: key lines and why they exist
Mutex/IPC: held locks, channel, and blocking behavior
Failure mode: returned result and cleanup behavior
Tests: exact commands and observed results
Commit: SHA and pushed branch
```

For any commit touching `lib/libhtttp/include/htttp.h`, append:

```text
SHARED BOUNDARY: modifies lib/libhtttp/include/htttp.h.
Both Sanjan and Zi Qi must review before merge.
```
