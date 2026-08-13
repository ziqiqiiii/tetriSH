# libtetrissh Hardening Implementation Plan

> **Completed — this plan is a historical record, not work to pick up.** All
> five tasks shipped; the checkboxes below were never ticked as the work landed.
> The deliverables are in the tree: `SESSIONIO_MAX_CERT_LEN` and the
> RSA-derived bounds in `lib/libtetrissh/src/handshake.c`, `MSG_NOSIGNAL` /
> `SO_NOSIGPIPE` in `src/io.c`, `tests/test_session_security.c` and
> `tests/test_handshake_failures.c`, and `lib/libtetrissh/README.md`.
> Verified 2026-08-13.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove confirmed denial-of-service and process-termination bugs from `libtetrissh`, add adversarial regression coverage, and document library integration and security contracts.

**Architecture:** Preserve public API and successful wire bytes. Bound handshake inputs before allocation using a fixed certificate limit and RSA key-derived lengths, suppress `SIGPIPE` per send, then cover frame security and lifecycle behavior with real `socketpair()` tests. Add a library-local README as authoritative integration documentation.

**Tech Stack:** C11, POSIX sockets and pthreads, OpenSSL EVP/X509/RAND, GNU Make, ASan/UBSan, Valgrind where supported.

## Global Constraints

- Preserve signatures and layout in `lib/libtetrissh/include/tetrissh.h`.
- Preserve existing handshake and encrypted-frame wire bytes.
- Do not modify `lib/libtetrissh/src/common.c` or `lib/libtetrissh/include/libs/common.h`.
- Use no TLS or `SSL_*` API.
- Keep 65,536-byte maximum plaintext and AES-256-GCM framing.
- Build standalone with `make -C lib/libtetrissh`.
- Compile library-owned code with `-std=c11 -Wall -Wextra -Werror -pedantic`.
- Target Linux for `MSG_NOSIGNAL` behavior.
- Do not commit or push; user did not request git history changes.

---

## File Structure

- Modify `lib/libtetrissh/src/internal.h`: private certificate-size bound.
- Modify `lib/libtetrissh/src/handshake.c`: bounded peer lengths and fail-closed initialization.
- Modify `lib/libtetrissh/src/io.c`: per-send `SIGPIPE` suppression.
- Create `lib/libtetrissh/tests/test_handshake_failures.c`: malicious peer lengths and stale-state regressions.
- Modify `lib/libtetrissh/tests/test_session_frames.c`: closed-peer process-survival regression.
- Create `lib/libtetrissh/tests/test_session_security.c`: frame integrity, boundary, invalid-state, and cleanup characterization.
- Create `lib/libtetrissh/README.md`: standalone usage and security documentation.

### Task 1: Handshake Input Bounds And Failure State

**Files:**
- Modify: `lib/libtetrissh/src/internal.h`
- Modify: `lib/libtetrissh/src/handshake.c`
- Create: `lib/libtetrissh/tests/test_handshake_failures.c`

**Interfaces:**
- Consumes: existing `session_handshake_client()`, `session_handshake_server()`, exact-I/O helpers, generated 1024-bit test certificate.
- Produces: unchanged public handshake API with bounded allocations and wiped state after every failure involving a non-null session.

- [ ] **Step 1: Write stale-session regression**

Create `test_handshake_failures.c` with session seeding and reset assertions:

```c
#include "../src/internal.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void seed_session(t_session *sess)
{
	memset(sess, 0xa5, sizeof(*sess));
	sess->fd = 42;
	sess->role = TETRISSH_ROLE_CLIENT;
	sess->send_seq = 7;
	sess->recv_seq = 9;
	sess->established = 1;
}

static void assert_session_reset(const t_session *sess)
{
	size_t i;

	assert(sess->fd == -1);
	assert(sess->role == TETRISSH_ROLE_NONE);
	assert(sess->send_seq == 0);
	assert(sess->recv_seq == 0);
	assert(sess->established == 0);
	i = 0;
	while (i < TETRISSH_KEY_LEN)
	{
		assert(sess->aes_key[i] == 0);
		i++;
	}
}

static void test_invalid_arguments_reset_session(void)
{
	t_session sess;

	seed_session(&sess);
	assert(session_handshake_client(-1, &sess, NULL) == -1);
	assert_session_reset(&sess);
	seed_session(&sess);
	assert(session_handshake_server(-1, &sess, NULL, "unused") == -1);
	assert_session_reset(&sess);
	printf("PASS test_invalid_arguments_reset_session\n");
}

int main(void)
{
	test_invalid_arguments_reset_session();
	return (0);
}
```

- [ ] **Step 2: Verify stale-session test fails**

Run: `make -C lib/libtetrissh test`

Expected: `test_handshake_failures` aborts because current early returns leave `fd == 42` and `established == 1`.

- [ ] **Step 3: Route invalid arguments through cleanup**

In each handshake function, keep `sess == NULL` as the only pre-initialization return. Initialize session, then reject invalid descriptors/paths through `cleanup`:

```c
if (sess == NULL)
	return (-1);
init_session(sess, fd, TETRISSH_ROLE_SERVER);
if (fd < 0 || cert_path == NULL || key_path == NULL)
	goto cleanup;
```

```c
if (sess == NULL)
	return (-1);
init_session(sess, fd, TETRISSH_ROLE_CLIENT);
if (fd < 0 || ca_path == NULL)
	goto cleanup;
```

Remove later duplicate `init_session()` calls. Existing cleanup calls `session_close(sess)` when `ok != 0`.

- [ ] **Step 4: Verify stale-session test passes**

Run: `make -C lib/libtetrissh test`

Expected: all four test binaries pass.

- [ ] **Step 5: Add malicious-length regressions**

Extend `test_handshake_failures.c` with these helpers and tests before `main`:

```c
typedef struct s_peer_args
{
	int fd;
} t_peer_args;

static void *oversized_cert_peer(void *arg)
{
	t_peer_args *peer;
	unsigned char nonce[TSH_NONCE_LEN];

	peer = arg;
	assert(tsh_read_exact(peer->fd, nonce, sizeof(nonce)) == TSH_IO_OK);
	assert(tsh_write_u32(peer->fd, TETRISSH_MAX_PLAINTEXT + 1u) == 0);
	assert(tsh_write_exact(peer->fd, "C", 1) == 0);
	assert(shutdown(peer->fd, SHUT_WR) == 0);
	return (NULL);
}

static void test_oversized_certificate_rejected_before_body(void)
{
	int fds[2];
	t_peer_args peer;
	pthread_t thread;
	t_session sess;
	char leftover;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	peer.fd = fds[1];
	assert(pthread_create(&thread, NULL, oversized_cert_peer, &peer) == 0);
	assert(session_handshake_client(fds[0], &sess, "unused-ca") == -1);
	assert(pthread_join(thread, NULL) == 0);
	assert(recv(fds[0], &leftover, 1, MSG_DONTWAIT) == 1);
	assert(leftover == 'C');
	assert_session_reset(&sess);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_oversized_certificate_rejected_before_body\n");
}

static void read_discard_blob(int fd)
{
	uint32_t len;
	unsigned char *buf;

	assert(tsh_read_u32(fd, &len) == TSH_IO_OK);
	buf = malloc(len);
	assert(buf != NULL);
	assert(tsh_read_exact(fd, buf, len) == TSH_IO_OK);
	free(buf);
}

static void *wrong_wrapped_length_peer(void *arg)
{
	t_peer_args *peer;
	unsigned char nonce[TSH_NONCE_LEN];

	peer = arg;
	memset(nonce, 0x5a, sizeof(nonce));
	assert(tsh_write_exact(peer->fd, nonce, sizeof(nonce)) == 0);
	read_discard_blob(peer->fd);
	read_discard_blob(peer->fd);
	assert(tsh_write_u32(peer->fd, 129u) == 0);
	assert(tsh_write_exact(peer->fd, "W", 1) == 0);
	assert(shutdown(peer->fd, SHUT_WR) == 0);
	return (NULL);
}

static void test_wrong_wrapped_length_rejected_before_body(void)
{
	int fds[2];
	t_peer_args peer;
	pthread_t thread;
	t_session sess;
	char leftover;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	peer.fd = fds[0];
	assert(pthread_create(&thread, NULL, wrong_wrapped_length_peer, &peer) == 0);
	assert(session_handshake_server(fds[1], &sess,
		"tests/tmp/certs/server.crt", "tests/tmp/certs/server.key") == -1);
	assert(pthread_join(thread, NULL) == 0);
	assert(recv(fds[1], &leftover, 1, MSG_DONTWAIT) == 1);
	assert(leftover == 'W');
	assert_session_reset(&sess);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_wrong_wrapped_length_rejected_before_body\n");
}
```

Update `main`:

```c
int main(void)
{
	test_invalid_arguments_reset_session();
	test_oversized_certificate_rejected_before_body();
	test_wrong_wrapped_length_rejected_before_body();
	return (0);
}
```

- [ ] **Step 6: Verify malicious-length tests fail for expected reason**

Run: `make -C lib/libtetrissh test`

Expected: oversized certificate test fails because current client consumes `C`; wrapped-key test fails because current server consumes `W`.

- [ ] **Step 7: Implement certificate and RSA-derived bounds**

Add to `internal.h`:

```c
# define TSH_MAX_CERT_LEN 65536u
```

In `read_file_bytes()`, replace the size condition with:

```c
if (size <= 0 || (unsigned long)size > TSH_MAX_CERT_LEN)
	return (fclose(fp), -1);
```

Add `int key_size;` to both handshake functions and initialize it to zero. On server, after loading `priv`, require a positive key size:

```c
key_size = EVP_PKEY_get_size(priv);
if (key_size <= 0)
	goto cleanup;
```

Replace wrapped-length validation with:

```c
if (tsh_read_u32(fd, &wrapped_len) != TSH_IO_OK
	|| wrapped_len != (uint32_t)key_size)
	goto cleanup;
```

On client, reject certificate length before allocation:

```c
if (tsh_read_u32(fd, &cert_len) != TSH_IO_OK || cert_len == 0
	|| cert_len > TSH_MAX_CERT_LEN)
	goto cleanup;
```

Move certificate parsing, CA verification, public-key extraction, and key-size
calculation before reading `sig_len`:

```c
cert = load_cert_bytes(cert_bytes, (int)cert_len);
if (cert == NULL || verify_server_cert(cert, ca_path) != 1)
	goto cleanup;
pub = X509_get_pubkey(cert);
if (pub == NULL)
	goto cleanup;
key_size = EVP_PKEY_get_size(pub);
if (key_size <= 0)
	goto cleanup;
if (tsh_read_u32(fd, &sig_len) != TSH_IO_OK
	|| sig_len != (uint32_t)key_size)
	goto cleanup;
```

Keep signature allocation/read and `verify_message_pss()` after this block.
Remove old duplicate certificate verification and `X509_get_pubkey()` block.

Add one short AI-assistance comment above these checks:

```c
/* AI-assisted: reject peer lengths before allocation; RSA blobs must match
 * configured key size so a peer cannot force unbounded reads or allocations. */
```

- [ ] **Step 8: Verify handshake suite passes**

Run: `make -C lib/libtetrissh clean && make -C lib/libtetrissh test`

Expected: four test binaries pass, including all three handshake-failure cases.

### Task 2: Closed-Peer Send Must Not Terminate Process

**Files:**
- Modify: `lib/libtetrissh/tests/test_session_frames.c`
- Modify: `lib/libtetrissh/src/io.c`

**Interfaces:**
- Consumes: `session_send()` and Linux socket semantics.
- Produces: `session_send()` returns `-1`/`EPIPE` behavior without process-wide signal changes.

- [ ] **Step 1: Write failing SIGPIPE regression**

Add `<signal.h>` and `<sys/wait.h>` includes, then append:

```c
static void test_closed_peer_returns_error_without_sigpipe(void)
{
	t_session client;
	t_session server;
	int fds[2];
	pid_t pid;
	int status;

	init_pair(&client, &server, fds);
	close(fds[1]);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0)
	{
		signal(SIGPIPE, SIG_DFL);
		_exit(session_send(&client, "x", 1) == -1 ? 0 : 1);
	}
	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	close(fds[0]);
	printf("PASS test_closed_peer_returns_error_without_sigpipe\n");
}
```

Call it from `main` after existing tests.

- [ ] **Step 2: Verify SIGPIPE regression fails**

Run: `make -C lib/libtetrissh test`

Expected: assertion `WIFEXITED(status)` fails because child dies from `SIGPIPE`.

- [ ] **Step 3: Suppress SIGPIPE per send**

In `tsh_write_exact()`, replace send flags `0` with `MSG_NOSIGNAL` and add:

```c
/* AI-assisted: convert closed-peer SIGPIPE into EPIPE so daemons can clean up
 * the affected connection instead of terminating the whole process. */
n = send(fd, in + done, len - done, MSG_NOSIGNAL);
```

- [ ] **Step 4: Verify process-survival regression passes**

Run: `make -C lib/libtetrissh test`

Expected: all tests pass and child exits status zero.

### Task 3: Frame Security And Lifecycle Coverage

**Files:**
- Create: `lib/libtetrissh/tests/test_session_security.c`

**Interfaces:**
- Consumes: current frame format, public session API, and private exact-I/O helpers for black-box wire mutation.
- Produces: characterization coverage without production behavior changes.

- [ ] **Step 1: Add session security test binary**

Create `test_session_security.c` with:

```c
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void init_session(t_session *sess, int fd, t_tetrissh_role role)
{
	size_t i;

	memset(sess, 0, sizeof(*sess));
	sess->fd = fd;
	sess->role = role;
	sess->established = 1;
	i = 0;
	while (i < TETRISSH_KEY_LEN)
	{
		sess->aes_key[i] = (unsigned char)(i + 1u);
		i++;
	}
}

static unsigned char *capture_frame(int fd, uint32_t *len)
{
	unsigned char *frame;

	assert(tsh_read_u32(fd, len) == TSH_IO_OK);
	frame = malloc(*len);
	assert(frame != NULL);
	assert(tsh_read_exact(fd, frame, *len) == TSH_IO_OK);
	return (frame);
}

static void inject_frame(int fd, const unsigned char *frame, uint32_t len)
{
	assert(tsh_write_u32(fd, len) == 0);
	assert(tsh_write_exact(fd, frame, len) == 0);
}

static void test_tampered_tag_rejected(void)
{
	int source[2];
	int target[2];
	t_session sender;
	t_session receiver;
	unsigned char *frame;
	uint32_t len;
	char out[8];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, source) == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, target) == 0);
	init_session(&sender, source[0], TETRISSH_ROLE_CLIENT);
	init_session(&receiver, target[1], TETRISSH_ROLE_SERVER);
	assert(session_send(&sender, "tag", 3) == 3);
	frame = capture_frame(source[1], &len);
	frame[TSH_GCM_NONCE_LEN] ^= 0x01u;
	inject_frame(target[0], frame, len);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	assert(receiver.recv_seq == 0);
	free(frame);
	close(source[0]);
	close(source[1]);
	close(target[0]);
	close(target[1]);
	printf("PASS test_tampered_tag_rejected\n");
}

static void test_reflected_direction_rejected(void)
{
	int source[2];
	int target[2];
	t_session sender;
	t_session receiver;
	unsigned char *frame;
	uint32_t len;
	char out[8];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, source) == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, target) == 0);
	init_session(&sender, source[0], TETRISSH_ROLE_CLIENT);
	init_session(&receiver, target[1], TETRISSH_ROLE_CLIENT);
	assert(session_send(&sender, "dir", 3) == 3);
	frame = capture_frame(source[1], &len);
	inject_frame(target[0], frame, len);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	assert(receiver.recv_seq == 0);
	free(frame);
	close(source[0]);
	close(source[1]);
	close(target[0]);
	close(target[1]);
	printf("PASS test_reflected_direction_rejected\n");
}

static void test_zero_and_maximum_frames(void)
{
	int fds[2];
	t_session client;
	t_session server;
	unsigned char *plain;
	unsigned char *out;
	size_t i;
	char empty;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	init_session(&client, fds[0], TETRISSH_ROLE_CLIENT);
	init_session(&server, fds[1], TETRISSH_ROLE_SERVER);
	assert(session_send(&client, NULL, 0) == 0);
	assert(session_recv(&server, &empty, 0) == 0);
	assert(client.send_seq == 1 && server.recv_seq == 1);
	plain = malloc(TETRISSH_MAX_PLAINTEXT);
	out = malloc(TETRISSH_MAX_PLAINTEXT);
	assert(plain != NULL && out != NULL);
	i = 0;
	while (i < TETRISSH_MAX_PLAINTEXT)
	{
		plain[i] = (unsigned char)(i & 0xffu);
		i++;
	}
	assert(session_send(&client, plain, TETRISSH_MAX_PLAINTEXT)
		== (ssize_t)TETRISSH_MAX_PLAINTEXT);
	assert(session_recv(&server, out, TETRISSH_MAX_PLAINTEXT)
		== (ssize_t)TETRISSH_MAX_PLAINTEXT);
	assert(memcmp(plain, out, TETRISSH_MAX_PLAINTEXT) == 0);
	free(plain);
	free(out);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_zero_and_maximum_frames\n");
}

static void test_malformed_lengths_rejected(void)
{
	int fds[2];
	t_session receiver;
	char out[8];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	init_session(&receiver, fds[1], TETRISSH_ROLE_SERVER);
	assert(tsh_write_u32(fds[0], TSH_FRAME_OVERHEAD - 1u) == 0);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	close(fds[0]);
	close(fds[1]);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	init_session(&receiver, fds[1], TETRISSH_ROLE_SERVER);
	assert(tsh_write_u32(fds[0],
		TSH_FRAME_OVERHEAD + TETRISSH_MAX_PLAINTEXT + 1u) == 0);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_malformed_lengths_rejected\n");
}

static void test_invalid_state_and_close(void)
{
	int fds[2];
	t_session sess;
	char byte;
	size_t i;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(&sess, 0x7f, sizeof(sess));
	sess.fd = fds[0];
	sess.role = TETRISSH_ROLE_NONE;
	sess.established = 0;
	assert(session_send(&sess, "x", 1) == -1);
	assert(session_recv(&sess, &byte, 1) == -1);
	session_close(&sess);
	assert(sess.fd == -1 && sess.role == TETRISSH_ROLE_NONE);
	assert(sess.send_seq == 0 && sess.recv_seq == 0 && sess.established == 0);
	i = 0;
	while (i < TETRISSH_KEY_LEN)
	{
		assert(sess.aes_key[i] == 0);
		i++;
	}
	assert(send(fds[0], "Z", 1, MSG_NOSIGNAL) == 1);
	assert(recv(fds[1], &byte, 1, 0) == 1 && byte == 'Z');
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_invalid_state_and_close\n");
}

int main(void)
{
	test_tampered_tag_rejected();
	test_reflected_direction_rejected();
	test_zero_and_maximum_frames();
	test_malformed_lengths_rejected();
	test_invalid_state_and_close();
	return (0);
}
```

- [ ] **Step 2: Run characterization suite**

Run: `make -C lib/libtetrissh test`

Expected: all tests pass. Any failure indicates existing behavior differs from
the approved design and must be investigated before changing production code.

### Task 4: Standalone Library README

**Files:**
- Create: `lib/libtetrissh/README.md`

**Interfaces:**
- Consumes: final source behavior and `include/tetrissh.h` signatures.
- Produces: integration, protocol, security, and testing reference for users and checkoff.

- [ ] **Step 1: Write README**

Create sections in this exact order:

```markdown
# libtetrissh

`libtetrissh` provides tetriSH's authenticated secure-session layer between a
connected stream socket and HTTTP. It implements server authentication,
RSA-wrapped session-key exchange, and one-message-per-frame AES-256-GCM I/O.

## Security At A Glance
## Requirements
## Build And Link
## Quick Start
### Server
### Client
## Public API
## Handshake Wire Protocol
## Encrypted Frame Format
## Ownership, Blocking, And Concurrency
## Error Handling
## Tests
## File Layout
## Security Scope And Limitations
```

Populate these sections with exact final facts:

- Dependencies: C11 compiler, POSIX/Linux sockets, OpenSSL libssl/libcrypto,
  pthreads for tests, OpenSSL CLI for temporary test certificates.
- Commands: `make -C lib/libtetrissh`, `make -C lib/libtetrissh test`,
  `make -C lib/libtetrissh clean`, and static link command using
  `lib/libtetrissh/libtetrissh.a -Ilib/libtetrissh/include -lssl -lcrypto`.
- Both quick-start examples create zeroed `t_session`, call the corresponding
  handshake, use `session_send/session_recv`, then call `session_close` and
  `close(fd)` in that order.
- Handshake bytes:
  `nonce[32]`, `cert_len[4] || cert_pem`,
  `sig_len[4] || RSA-PSS-SHA256(nonce)`, and
  `wrapped_len[4] || RSA-OAEP-SHA256(aes_key[32])`.
- Frame bytes: `frame_len[4] || nonce[12] || tag[16] || ciphertext`.
- `frame_len` excludes its own prefix and maximum encrypted body is 65,564
  bytes for 65,536 bytes of plaintext.
- AAD is `sequence_be[8] || direction[1]`, with `0x43` client-to-server and
  `0x53` server-to-client.
- `session_close()` wipes state but never closes caller-owned descriptor.
- I/O is blocking; caller configures socket timeouts and never holds a mutex
  across calls.
- One `t_session` is not internally synchronized. Serialize sends and receives;
  concurrent response/broadcast sends need a per-connection write mutex.
- Any handshake or frame `-1` is connection-fatal: call `session_close`, close
  descriptor, and do not retry on potentially desynchronized stream.
- Authentication is server-only. Certificate chain and validity are checked
  against dedicated CA, but API has no hostname input, client auth, TLS, or
  explicit key-confirmation message.
- Test table maps test files to exact I/O, frame round trips, adversarial
  handshake lengths, SIGPIPE, tamper/reflection, boundaries, and cleanup.
- Include strict, sanitizer, and Valgrind commands; state current host Valgrind
  AVX-512 incompatibility separately from library results.
```

- [ ] **Step 2: Validate README against code**

Run searches:

```bash
rg "session_(handshake_server|handshake_client|send|recv|close)" lib/libtetrissh/include/tetrissh.h lib/libtetrissh/README.md
rg "TSH_(GCM_NONCE_LEN|GCM_TAG_LEN|CLIENT_MARKER|SERVER_MARKER)" lib/libtetrissh/src/internal.h lib/libtetrissh/README.md
```

Expected: README names all five public functions and values match source constants.

### Task 5: Full Verification And Review

**Files:**
- Verify all files above; no new production files.

**Interfaces:**
- Consumes: completed hardening, tests, README.
- Produces: strict build and memory-safety evidence plus final review findings.

- [ ] **Step 1: Run clean strict suite**

Run: `make -C lib/libtetrissh fclean && make -C lib/libtetrissh test`

Expected: zero warnings; all test binaries pass.

- [ ] **Step 2: Run ASan/UBSan suite**

Run:

```bash
make -C lib/libtetrissh fclean
make -C lib/libtetrissh test \
  CFLAGS='-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' \
  COMMON_CFLAGS='-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer'
```

Expected: all tests pass with no sanitizer diagnostics.

- [ ] **Step 3: Restore normal artifacts**

Run: `make -C lib/libtetrissh fclean && make -C lib/libtetrissh test`

Expected: normal strict build and suite pass.

- [ ] **Step 4: Attempt Valgrind and record host result**

Run each `tests/bin/test_*` with:

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
  --error-exitcode=1 ./tests/bin/<test-name>
```

Expected on compatible host: no leaks or memory errors. Current host may stop
inside `vgpreload_memcheck` on unsupported AVX-512 `memset`; remove only core
files generated by this run and report tool incompatibility.

- [ ] **Step 5: Inspect final diff**

Run: `git diff --check && git diff -- lib/libtetrissh docs/superpowers/specs/2026-07-12-libtetrissh-hardening-design.md docs/superpowers/plans/2026-07-12-libtetrissh-hardening.md`

Expected: no whitespace errors; no changes to frozen common files or public API.

- [ ] **Step 6: Focused security review**

Review changed lines for unchecked peer lengths, cleanup bypasses, descriptor
ownership ambiguity, blocking under locks, test deadlocks, and accidental wire
changes. Findings must include exact file/line references; resolve confirmed
bugs and rerun Steps 1-3.

## Self-Review

- Spec coverage: Tasks 1-2 fix all confirmed bugs; Task 3 adds approved frame
  coverage; Task 4 covers every README requirement; Task 5 covers strict,
  sanitizer, Valgrind, diff, and review verification.
- Public types remain `t_session` and `t_tetrissh_role`; no public signature or
  struct field changes appear in any task.
- Successful wire order and lengths remain unchanged; only rejection timing for
  invalid peer input changes.
- No course-provided `common.c/common.h` edit appears in any task.
- No git commit or push step appears because user did not request either action.
