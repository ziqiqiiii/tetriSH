# libhtttp Review Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix confirmed protocol, validation, dispatch, build, test-harness, and documentation defects found by full-branch review.

**Architecture:** Preserve one-shot codec design and public function signatures while intentionally extending `t_htttp_route` with per-route validation policy. Enforce wire invariants before serialization, normalize builder header values at ownership boundary, validate calendar dates without locale/timezone state, require structurally valid requests before dispatch, and make build/test targets fail deterministically.

**Tech Stack:** C11, POSIX `gmtime_r`, GNU Make, Bash test runner, GCC/Clang sanitizers, Valgrind.

## Global Constraints

- Keep `libhtttp` standalone with no socket, crypto, filesystem, mutex, or mutable global state.
- Keep `HTTTP/1.0`, CRLF grammar, 65,536-byte message limit, 64-header wire limit, and binary-safe bodies unchanged.
- Keep all existing enum values and public function signatures unchanged.
- `t_htttp_route` layout intentionally changes: update every positional
  initializer and clean-rebuild all consumers to prevent mixed-layout objects.
- Both Sanjan and Zi Qi must review `lib/libhtttp/include/htttp.h` before merge.
- Normalize only leading/trailing SP/HTAB in builder-supplied header values.
- Validate response dates as real Gregorian dates with matching weekday.
- Use tests before production changes and preserve strict warning-free flags.
- Do not commit or push without explicit user request.

---

### Task 1: Header Ownership And Wire Limits

**Files:**
- Modify: `lib/libhtttp/tests/test_message.c`
- Modify: `lib/libhtttp/tests/test_serialiser.c`
- Modify: `lib/libhtttp/src/message.c`
- Modify: `lib/libhtttp/src/serialiser.c`
- Modify: `lib/libhtttp/include/htttp.h`

**Interfaces:**
- Consumes: existing message builders and serializer.
- Produces: normalized owned header values and serializer output with at most 64 wire headers.

- [ ] **Step 1: Add failing header normalization and malformed-count tests**

```c
assert(htttp_message_set_header(&message, "Player-Id", " \t p17 \t")
	== HTTTP_OK);
assert(strcmp(htttp_message_get_header(&message, "player-id"), "p17") == 0);
message.header_count = HTTTP_MAX_HEADERS + 1u;
assert(htttp_message_get_header(&message, "Player-Id") == NULL);
```

- [ ] **Step 2: Add failing 64-header body serialization test**

Build 64 ordinary headers plus one-byte body and assert
`htttp_serialize()` returns `HTTTP_ERR_TOO_MANY_HEADERS`. Build 63 ordinary
headers plus body, serialize, parse, and assert exactly 64 parsed headers.

- [ ] **Step 3: Run RED tests**

Run: `make -C lib/libhtttp test FILTER=message`
Expected: normalization/count assertion failure.

Run: `make -C lib/libhtttp test FILTER=serialiser`
Expected: 64-header body serialization unexpectedly succeeds.

- [ ] **Step 4: Normalize values and bound lookup**

Add a private copy helper that checks original length, trims leading/trailing
SP/HTAB, allocates the trimmed byte count plus NUL, and is used for both header
insert and replacement. Reject `header_count > HTTTP_MAX_HEADERS` in
`htttp_message_get_header()`.

- [ ] **Step 5: Count emitted headers before serialization**

Count non-`Content-Length` headers during structural validation, add one when
body is non-empty, and return `HTTTP_ERR_TOO_MANY_HEADERS` when count exceeds
`HTTTP_MAX_HEADERS`.

- [ ] **Step 6: Correct public contracts and run GREEN tests**

Document value normalization, `HTTTP_ERR_TOO_LARGE`, preservation of non-empty
constructor outputs, and valid-message preconditions.

Run: `make -C lib/libhtttp test FILTER=message`
Expected: PASS.

Run: `make -C lib/libhtttp test FILTER=serialiser`
Expected: PASS.

---

### Task 2: Date, Parser, And Dispatch Validation

**Files:**
- Modify: `lib/libhtttp/tests/test_validation.c`
- Modify: `lib/libhtttp/tests/test_parser.c`
- Modify: `lib/libhtttp/tests/test_dispatch.c`
- Modify: `lib/libhtttp/src/validation.c`
- Modify: `lib/libhtttp/src/parser.c`
- Modify: `lib/libhtttp/src/dispatch.c`
- Modify: `lib/libhtttp/include/htttp.h`

**Interfaces:**
- Consumes: normalized messages from Task 1.
- Produces: calendar-correct response dates, precise missing-version errors, and structurally validated dispatch input.

- [ ] **Step 1: Add failing Gregorian date tests**

Reject `Mon, 31 Feb 2025 00:00:00 GMT`, non-leap `29 Feb`, and mismatched
weekday. Accept `Thu, 29 Feb 2024 00:00:00 GMT`.

- [ ] **Step 2: Add failing parser and dispatch tests**

Assert `JOIN /room/a \r\n\r\n` returns
`HTTTP_ERR_MALFORMED_START_LINE`. Assert dispatch rejects request messages with
NULL path or body without required `Content-Type`. Correct PAUSE first-match
handler naming and assertions.

- [ ] **Step 3: Run RED suites**

Run: `make -C lib/libhtttp test FILTER=validation`
Expected: impossible date accepted.

Run: `make -C lib/libhtttp test FILTER=parser`
Expected: missing version returns unsupported version.

Run: `make -C lib/libhtttp test FILTER=dispatch`
Expected: malformed request reaches handler or returns wrong result.

- [ ] **Step 4: Implement locale-independent Gregorian checks**

Parse weekday and month indices, enforce month-specific day count with Gregorian
leap-year rules, and calculate weekday using integer Gregorian arithmetic.

- [ ] **Step 5: Correct parser and dispatch guards**

Reject empty version token before exact version comparison. After confirming
request type, call `htttp_validate(message, 0u)` before route lookup so malformed
shape and required body headers cannot reach callbacks. Route entries carry
`validation_flags`; dispatch applies matching flags before callback so
authenticated routes require `Player-Id` without a separate caller step.

- [ ] **Step 6: Run GREEN suites**

Run all three focused suites; expected: PASS.

---

### Task 3: Deterministic Build And Test Failures

**Files:**
- Modify: `lib/libhtttp/Makefile`
- Modify: `lib/libhtttp/scripts/run_tests.sh`

**Interfaces:**
- Consumes: current wildcard source/object lists and PASS/FAIL output protocol.
- Produces: archives containing only current objects and test targets that fail on every displayed failure/leak kind.

- [ ] **Step 1: Confirm stale archive behavior**

Inject a generated `stale.o` archive member, run ordinary `make`, and confirm
current rule leaves it present.

- [ ] **Step 2: Recreate archive from current object list**

Use a phony force prerequisite; remove `libhtttp.a` immediately before `ar rcs`
so deleted/renamed sources cannot survive as archive members.

- [ ] **Step 3: Strengthen test failure detection**

Set binary status nonzero when runner reads a line beginning `FAIL `. Add
`--errors-for-leak-kinds=all` to memcheck command.

- [ ] **Step 4: Verify build and runner behavior**

Reinject `stale.o`, run `make`, and assert `ar t` no longer lists it. Run full
tests and full generic-baseline Valgrind; expected: all pass with zero errors.

---

### Task 4: Hardening Coverage And Final Documentation

**Files:**
- Modify: `lib/libhtttp/tests/test_hardening.c`
- Modify: `lib/libhtttp/README.md`

**Interfaces:**
- Consumes: corrected codec and harness.
- Produces: sanitizer-visible truncation boundaries and final accurate usage contract.

- [ ] **Step 1: Make every truncation allocation exact**

For each nonzero prefix, allocate exactly `prefix_len`, copy only those bytes,
parse, assert failure/empty output, then free. Keep zero-length case on a valid
one-byte pointer.

- [ ] **Step 2: Update documentation**

Document header normalization, semantic Gregorian date validation, dispatch
validation behavior, and 64-header generated-`Content-Length` rule.

- [ ] **Step 3: Run release verification**

```bash
make -C lib/libhtttp fclean
make -C lib/libhtttp test
make -C lib/libhtttp test FLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer"
make -C lib/libhtttp fclean
make -C lib/libhtttp test
VALGRIND_LIB=/tmp/opencode/valgrind-generic/usr/lib/valgrind make -C lib/libhtttp memcheck VALGRIND=/tmp/opencode/valgrind-generic/usr/bin/valgrind
make libs AUTO_INSTALL_DEPS=0
```

Expected: six suites pass under ordinary and sanitizer builds; Valgrind reports
zero errors/leaks; root library discovery succeeds; `git diff --check` is clean.
