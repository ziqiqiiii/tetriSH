# libtetrissh Design

Date: 2026-07-04

## Goal

Create `lib/libtetrissh` as a self-contained static C library for tetriSH secure sessions. It must provide server and client handshake APIs plus encrypted frame I/O used by `tetrisd`, `tetrisu`, `chatd`, and `marketd` once those binaries exist.

The library implements the CoreStack secure layer between TCP and HTTTP:

```text
HTTTP message
  -> libtetrissh encrypted frame
  -> TCP socket
```

## Requirements

- Build standalone with `make -C lib/libtetrissh` into `lib/libtetrissh/libtetrissh.a`.
- Test standalone with `make -C lib/libtetrissh test`.
- Public API lives in `lib/libtetrissh/include/tetrissh.h`.
- Use the PA2 `common.c/common.h` crypto helper files from `/mnt/windows_d/coding/50.005_Computer_Systems_Engineering/2026-pa2-50005-mac-mini` without modifying their contents.
- Do not touch the existing shell `src/tetrish/includes/common.h`; that file is unrelated to PA2 crypto helpers.
- No TLS and no `SSL_*` API.
- Every post-handshake frame carries exactly one HTTTP message and has a 64 KiB plaintext limit.
- Follow `-std=c11 -Wall -Wextra -Werror -pedantic` where practical. If PA2 staff code emits warnings under `-pedantic`, keep warning suppression local to vendored staff objects only.

## Chosen Approach

Use a self-contained `lib/libtetrissh` directory with read-only PA2 helper files copied into normal library paths: `lib/libtetrissh/src/common.c` and `lib/libtetrissh/include/libs/common.h`.

PA2 `common.c` provides certificate loading, certificate verification, RSA-PSS signing/verification, RSA-OAEP encryption/decryption, and exact socket send/read helpers. It only exposes AES-128-CBC + HMAC for symmetric encryption. CoreStack requires AES-256 frames, so `libtetrissh` will implement its own AES-256-GCM frame functions using OpenSSL EVP. This keeps the handshake aligned with PA2 helpers while matching CoreStack wording for frame encryption.

## Non-Goals

- No HTTTP parser in this library.
- No game logic, rooms, logging, or daemon thread code.
- No client-side prediction or server-authoritative state logic.
- No certificate generation automation in the first pass; tests may generate temporary certificates via OpenSSL commands.
- No backwards compatibility with earlier wire formats because `libtetrissh` does not exist in this repo yet.

## Public API

```c
typedef enum e_tetrissh_role
{
    TETRISSH_ROLE_NONE = 0,
    TETRISSH_ROLE_CLIENT,
    TETRISSH_ROLE_SERVER
}   t_tetrissh_role;

typedef struct s_session
{
    int              fd;
    t_tetrissh_role  role;
    unsigned char    aes_key[32];
    uint64_t         send_seq;
    uint64_t         recv_seq;
    int              established;
}   t_session;

int     session_handshake_server(int fd, t_session *sess,
            const char *cert_path, const char *key_path);
int     session_handshake_client(int fd, t_session *sess,
            const char *ca_path);
ssize_t session_send(t_session *sess, const void *buf, size_t len);
ssize_t session_recv(t_session *sess, void *buf, size_t max_len);
void    session_close(t_session *sess);
```

Return conventions:

- Handshake functions return `0` on success and `-1` on failure.
- `session_send` returns plaintext bytes accepted for encryption, or `-1` on failure.
- `session_recv` returns plaintext bytes copied into caller buffer, `0` on clean peer close, or `-1` on malformed/tampered frame or syscall failure.
- `session_close` wipes key material with `OPENSSL_cleanse()` and resets fields.

## Handshake Wire Protocol

All handshake length fields are 4-byte big-endian unsigned integers unless noted. Fixed-width integers avoid depending on PA2 FTP mode framing and make the protocol local to `libtetrissh`.

Server side:

```text
1. client -> server: client_nonce[32]
2. server -> client: cert_len[4] || cert_pem[cert_len]
3. server -> client: sig_len[4] || rsa_pss_sha256(client_nonce)[sig_len]
4. client -> server: wrapped_len[4] || rsa_oaep_sha256(aes_key[32])[wrapped_len]
5. both sides mark session established
```

Client side checks:

- Generate nonce with `RAND_bytes()`.
- Load server certificate from bytes using PA2 `load_cert_bytes()`.
- Verify certificate against `ca_path` using PA2 `verify_server_cert()`.
- Verify signature over the exact nonce using PA2 `verify_message_pss()`.
- Generate fresh AES-256 key with `RAND_bytes(aes_key, 32)`.
- Extract public key from verified certificate and wrap AES key using PA2 `rsa_encrypt_block(..., use_oaep = 1)`.

Server side checks:

- Load certificate with PA2 `load_cert_file()`.
- Load private key with PA2 `load_private_key()`.
- Sign exact client nonce using PA2 `sign_message_pss()`.
- Decrypt wrapped AES key using PA2 `rsa_decrypt_block(..., use_oaep = 1)`.
- Reject decrypted key unless decrypted length is exactly 32 bytes.

## Encrypted Frame Wire Protocol

Each frame carries one plaintext HTTTP message.

```text
frame_len[4] || nonce[12] || tag[16] || ciphertext[frame_len - 28]
```

`frame_len` is the byte count after the length field. Maximum plaintext length is 65536 bytes. The encrypted token can be slightly larger because it includes nonce and tag.

AES-256-GCM inputs:

- Key: `sess->aes_key[32]`.
- Nonce: 12 random bytes generated per frame using `RAND_bytes()`.
- AAD: 8-byte big-endian sequence number plus 1-byte role direction marker.
- Tag: 16 bytes from `EVP_CIPHER_CTX_ctrl(... EVP_CTRL_GCM_GET_TAG ...)`.

Replay/order behavior:

- `send_seq` increments after each successful send.
- `recv_seq` increments after each successful receive.
- Receiver authenticates expected `recv_seq` as AAD. Replayed or reordered frames fail GCM tag verification.

Direction marker:

- Client-to-server frames use `0x43` (`C`).
- Server-to-client frames use `0x53` (`S`).
- The sender marker is included in AAD so a ciphertext from one direction cannot be reflected into the other direction under the same key and sequence.

## File Layout

```text
lib/libtetrissh/
    Makefile
    include/
        tetrissh.h
    src/
        handshake.c
        session.c
        io.c
        internal.h
    tests/
        test_io.c
        test_session_crypto.c
        test_handshake_socketpair.c
    scripts/
        run_tests.sh
    include/
        libs/common.h
    src/
        common.c
```

## Component Roles

`io.c`:

- `tsh_read_exact()` loops on `recv()` until requested bytes are read or the peer closes.
- `tsh_write_exact()` loops on `send()` until requested bytes are written.
- `tsh_read_u32()` and `tsh_write_u32()` handle 4-byte big-endian length fields.

`handshake.c`:

- Implements `session_handshake_server()` and `session_handshake_client()`.
- Owns all `X509 *`, `EVP_PKEY *`, and malloc buffers allocated during handshake.
- Uses a single `cleanup:` path so partial failures free everything.

`session.c`:

- Implements `session_send()`, `session_recv()`, and `session_close()`.
- Owns AES-256-GCM encryption/decryption and replay/order counters.
- Wipes decrypted temporary plaintext and session key material on close/failure where appropriate.

`internal.h`:

- Private constants and helper declarations only.
- Not installed for consumers.

## Error Handling

- Any malformed length, oversized frame, certificate failure, signature failure, RSA failure, RNG failure, or GCM tag failure returns failure to caller.
- Handshake failure calls `session_close()` before returning `-1`.
- `session_recv()` returns `0` only when peer closes before any frame bytes are read. Partial frame close returns `-1` because the stream is corrupted.
- Functions never `exit()`; callers decide whether to close sockets or log.

## Memory Ownership

- Caller owns `t_session` storage.
- Library owns only temporary allocations inside calls.
- PA2 helper allocations are freed by the `libtetrissh` caller site that requested them.
- `session_recv()` copies plaintext into caller buffer and wipes its temporary plaintext buffer before freeing.
- `session_close()` wipes `sess->aes_key` even if handshake was only partially established.

## Blocking Behavior

`session_send()` and `session_recv()` may block on the TCP socket. This library does not hold mutexes and does not own daemon locks, so callers must follow the project rule: copy data out of protected structures, unlock, then call `session_send()`.

## Testing Strategy

1. IO tests with `socketpair()`:
   - exact write/read succeeds across short buffers
   - EOF before full frame is detected
   - u32 big-endian encode/decode works

2. AES-256-GCM unit tests:
   - encrypt/decrypt round trip
   - tampered ciphertext fails
   - tampered tag fails
   - replayed frame fails because expected sequence number changed
   - payload over 64 KiB is rejected

3. Handshake integration tests with `socketpair()` and threads:
   - server and client complete handshake using temporary test cert/key/CA
   - post-handshake `session_send/session_recv` round trip succeeds both directions
   - invalid CA path fails client handshake
   - corrupted signature path fails when a test hook or fixture is available

4. Build verification:
   - `make -C lib/libtetrissh clean`
   - `make -C lib/libtetrissh`
   - `make -C lib/libtetrissh test`
   - `make libs`

## Incremental Commits And Pushes

Each increment must build or fail only on the intentionally missing next layer. Each successful increment gets its own commit and push to `origin/MacMiniSSH`.

Planned increments:

1. Design spec only.
2. Add PA2 common files and library skeleton.
3. Add exact I/O helpers and tests.
4. Add AES-256-GCM frame send/recv and tests.
5. Add server handshake.
6. Add client handshake.
7. Add socketpair end-to-end handshake and frame tests.
8. Update README/security docs with final wire format and demo explanation.

## Explanation Contract

After each code increment, produce the project-required checkoff summary:

```text
### Change: <title>

**Files changed:** list each file and function

**Why:** systems-level reason

**Line-by-line:** key lines and why they exist

**Mutex/IPC:** lock/channel/blocking behavior

**Failure mode:** what returns or gets dropped on failure
```

## Open Risks

- The PA2 helper code may not compile cleanly under `-pedantic`; if so, compile vendored staff code with the least strict compatible flags and keep strict flags for `libtetrissh` source.
- Test certificates may require local OpenSSL CLI availability. If unavailable, tests should skip certificate generation with a clear message instead of silently passing.
- CoreStack docs mention PA2 `common.c` as the crypto source, while AES-256-GCM requires direct OpenSSL EVP code. This design documents that exception explicitly because PA2 `common.c` does not expose AES-256.
