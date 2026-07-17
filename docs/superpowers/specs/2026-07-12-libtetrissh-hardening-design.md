# libtetrissh Hardening Design

Date: 2026-07-12

## Goal

Audit and harden `lib/libtetrissh` without changing its public API or wire
protocol, add adversarial regression coverage, and provide a standalone library
README suitable for integration and checkoff preparation.

## Current Findings

1. `session_handshake_client()` trusts peer-provided certificate and signature
   lengths before allocating memory. A malicious server can request allocations
   up to `UINT32_MAX` bytes and hold the client in a blocking read.
2. `session_handshake_server()` trusts peer-provided wrapped-key length before
   allocating memory. A malicious client can cause the same memory and blocking
   denial of service.
3. `tsh_write_exact()` calls `send()` without `MSG_NOSIGNAL`. A closed peer can
   deliver `SIGPIPE` and terminate a process instead of producing the documented
   `-1` return.
4. Handshake functions return early for invalid path arguments before resetting
   a non-null `t_session`. Reusing a session can therefore leave stale key bytes
   and `established == 1` after a failed call.
5. Existing tests cover successful handshakes, invalid CA verification, normal
   frame round trips, oversized plaintext, replay rejection, and basic exact I/O.
   They do not cover hostile handshake lengths, process survival on `EPIPE`,
   stale-state cleanup, exact size boundaries, malformed frame lengths, direct
   tag tampering, or cleanup invariants.
6. No `lib/libtetrissh/README.md` exists. Root documentation does not fully state
   descriptor ownership, blocking behavior, caller-side serialization, or
   failure handling.

## Constraints

- Preserve signatures and layout in `include/tetrissh.h`.
- Preserve existing handshake and encrypted-frame wire bytes.
- Keep `src/common.c` and `include/libs/common.h` unchanged.
- Use no TLS or `SSL_*` API.
- Keep 65,536-byte maximum plaintext and AES-256-GCM framing.
- Keep the library self-contained and buildable with
  `make -C lib/libtetrissh`.
- Use strict C11 warning flags for library-owned code.

## Production Changes

### Handshake Bounds

Add a private 65,536-byte certificate limit. `read_file_bytes()` rejects a local
server certificate above this limit, and the client rejects a peer certificate
length of zero or above the limit before allocating or reading certificate
bytes.

After receiving a bounded certificate, the client parses and verifies it before
accepting a signature body. It extracts the certified public key, obtains its
encoded RSA operation size with `EVP_PKEY_get_size()`, and requires `sig_len` to
equal that size before allocating the signature buffer. The signature remains
verified by frozen `verify_message_pss()`.

The server obtains its private-key operation size with `EVP_PKEY_get_size()` and
requires `wrapped_len` to equal that size before allocating or reading the
RSA-OAEP ciphertext. Invalid or non-positive key sizes fail the handshake.

These checks derive RSA ciphertext/signature bounds from the configured keys
instead of introducing an arbitrary large blob limit. Processing order changes,
but bytes sent on successful handshakes remain identical.

### Fail-Closed Session State

When `sess` is non-null, each handshake initializes it before validating file
paths or the descriptor. Every subsequent error reaches the existing cleanup
path, which calls `session_close()`. Failed handshakes therefore leave a wiped,
non-established session with `fd == -1` and role `NONE`.

### SIGPIPE Handling

On Linux, `tsh_write_exact()` passes `MSG_NOSIGNAL` to `send()`. Closed-peer
writes return `-1` with `errno == EPIPE` rather than terminating the process.
No process-global signal disposition is changed. The target environment is
Linux; the README states this platform dependency.

## Tests

Production fixes follow test-first order. Each confirmed bug receives a
regression that fails against current implementation before code changes.

- A fake server sends an oversized certificate length plus one body byte. Client
  must reject immediately, reset its session, and leave the body byte unread.
- A fake client completes enough of server handshake exchange to send a wrapped
  key length unequal to the private-key operation size plus one body byte.
  Server must reject immediately, reset its session, and leave that byte unread.
- Client and server handshake calls with invalid arguments must wipe a seeded
  established session.
- A child process restores default `SIGPIPE`, writes an encrypted frame to a
  closed socket, and must exit normally after `session_send()` returns `-1`.

Additional characterization and boundary coverage verifies existing intended
behavior:

- Zero-byte and exactly 65,536-byte frames round-trip successfully.
- 65,537-byte plaintext is rejected without advancing `send_seq`.
- Modified GCM tag and reflected-direction frames are rejected without
  advancing `recv_seq`.
- Frame lengths below GCM overhead and above maximum encrypted size are rejected
  before allocation.
- Invalid/unestablished sessions reject send and receive calls.
- `session_close()` wipes key bytes and resets descriptor, role, counters, and
  establishment state without closing the caller-owned descriptor.
- Exact-I/O helpers distinguish clean EOF from partial EOF and preserve u32/u64
  big-endian encoding.

Tests continue using real `socketpair()` streams and real OpenSSL operations;
no test-only production hooks or crypto mocks are added.

## Library README

Create `lib/libtetrissh/README.md` with:

- purpose, scope, dependency list, build/test/clean commands, and static-link
  example;
- minimal client and server usage examples;
- exact PEM handshake wire sequence and AES-256-GCM frame layout;
- API return values, 64 KiB boundary, and binary-buffer semantics;
- caller ownership of socket descriptors and explicit statement that
  `session_close()` wipes state but does not call `close()`;
- blocking I/O behavior, caller-configured socket timeouts, and project rule not
  to hold mutexes across session I/O;
- requirement to serialize concurrent calls on one `t_session`, especially
  concurrent sends from response and broadcast threads;
- fail-closed handling: after handshake or frame error, wipe session and close
  the descriptor rather than retrying a possibly desynchronized stream;
- security properties and limitations: server-only authentication, CA chain and
  validity checks, no hostname input, no client authentication, no TLS, and no
  explicit server key-confirmation message;
- test matrix and strict build, sanitizer, and Valgrind commands.

## Verification

1. Run clean strict build and full standalone suite.
2. Rebuild and run with AddressSanitizer and UndefinedBehaviorSanitizer.
3. Run each test under project Valgrind command where host tooling supports its
   instruction set.
4. Inspect final diff and run a focused security review.

Current host caveat: Valgrind 3.25.1 exits on an unrecognized AVX-512 instruction
inside its own `vgpreload_memcheck` `memset` before library code runs. This is a
tool/runtime incompatibility. ASan/UBSan provides memory-safety evidence on this
host; Valgrind must be rerun on a compatible checkoff environment.

## Non-Goals

- No key-confirmation round trip.
- No hostname parameter or public API redesign.
- No client certificate authentication.
- No internal mutex added to `t_session`.
- No change to PA2 course-provided crypto helpers.
- No unrelated daemon or HTTTP changes.
