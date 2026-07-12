# libtetrissh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build self-contained `lib/libtetrissh` with authenticated handshake and AES-256 encrypted frame I/O.

**Architecture:** `libtetrissh` is one static library with a public header, private I/O helpers, handshake code, session frame code, and PA2 `common.c/common.h` copied unchanged into normal library paths. PA2 common is used for X.509, RSA-PSS, and RSA-OAEP helpers; AES-256-GCM frames are implemented in `session.c` because PA2 common only exposes AES-128-CBC + HMAC.

**Tech Stack:** C11, POSIX sockets, OpenSSL EVP/X509/RAND, pthreads for socketpair handshake tests, Make static archive.

## Global Constraints

- Build standalone with `make -C lib/libtetrissh` into `lib/libtetrissh/libtetrissh.a`.
- Test standalone with `make -C lib/libtetrissh test`.
- Public API lives in `lib/libtetrissh/include/tetrissh.h`.
- Use PA2 `common.c/common.h` from `/mnt/windows_d/coding/50.005_Computer_Systems_Engineering/2026-pa2-50005-mac-mini` without modifying their contents.
- Do not touch existing shell `src/tetrish/includes/common.h`.
- No TLS and no `SSL_*` API.
- Every post-handshake frame carries exactly one HTTTP message and has a 64 KiB plaintext limit.
- Use AES-256-GCM for encrypted frames.
- Every successful increment gets a commit and `git push origin MacMiniSSH`.
- After every code increment, write checkoff explanation with files, why, key lines, mutex/IPC, and failure mode.

---

## File Structure

- Create `lib/libtetrissh/Makefile`: builds `libtetrissh.a`, test binaries, and PA2 common object.
- Create `lib/libtetrissh/include/tetrissh.h`: public API and `t_session` state.
- Create `lib/libtetrissh/src/internal.h`: private constants and helper declarations.
- Create `lib/libtetrissh/src/io.c`: exact read/write and u32 big-endian helpers.
- Create `lib/libtetrissh/src/session.c`: AES-256-GCM frame send/receive and secure close.
- Create `lib/libtetrissh/src/handshake.c`: client/server nonce, cert, RSA-PSS, RSA-OAEP handshake.
- Create `lib/libtetrissh/scripts/run_tests.sh`: consistent test runner.
- Create `lib/libtetrissh/scripts/generate_test_certs.sh`: temporary CA/server certificates for handshake tests.
- Create `lib/libtetrissh/tests/test_io.c`: exact I/O tests.
- Create `lib/libtetrissh/tests/test_session_frames.c`: AES frame tests.
- Create `lib/libtetrissh/tests/test_handshake_socketpair.c`: client/server end-to-end handshake test.
- Create `lib/libtetrissh/include/libs/common.h`: exact copy of PA2 staff header.
- Create `lib/libtetrissh/src/common.c`: exact copy of PA2 staff implementation.
- Modify `README.md`: document final AES-256-GCM frame choice and build command.

---

### Task 1: Library Skeleton And PA2 Common

**Files:**
- Create: `lib/libtetrissh/Makefile`
- Create: `lib/libtetrissh/include/tetrissh.h`
- Create: `lib/libtetrissh/src/internal.h`
- Create: `lib/libtetrissh/src/io.c`
- Create: `lib/libtetrissh/src/session.c`
- Create: `lib/libtetrissh/src/handshake.c`
- Create: `lib/libtetrissh/scripts/run_tests.sh`
- Create: `lib/libtetrissh/include/libs/common.h`
- Create: `lib/libtetrissh/src/common.c`

**Interfaces:**
- Consumes: PA2 common API from `include/libs/common.h`.
- Produces: `libtetrissh.a`, public `tetrissh.h`, stubbed public functions with final signatures.

- [ ] **Step 1: Add PA2 common files unchanged**

Copy exact file contents from:

```text
/mnt/windows_d/coding/50.005_Computer_Systems_Engineering/2026-pa2-50005-mac-mini/includes/libs/common.h
/mnt/windows_d/coding/50.005_Computer_Systems_Engineering/2026-pa2-50005-mac-mini/source/libs/common.c
```

to:

```text
lib/libtetrissh/include/libs/common.h
lib/libtetrissh/src/common.c
```

Do not edit the copied content. Verify exact copy with:

```bash
cmp "/mnt/windows_d/coding/50.005_Computer_Systems_Engineering/2026-pa2-50005-mac-mini/includes/libs/common.h" "lib/libtetrissh/include/libs/common.h"
cmp "/mnt/windows_d/coding/50.005_Computer_Systems_Engineering/2026-pa2-50005-mac-mini/source/libs/common.c" "lib/libtetrissh/src/common.c"
```

Expected: both commands produce no output and exit `0`.

- [ ] **Step 2: Add public header**

Create `lib/libtetrissh/include/tetrissh.h` with:

```c
#ifndef TETRISSH_H
# define TETRISSH_H

# include <stddef.h>
# include <stdint.h>
# include <sys/types.h>

# define TETRISSH_KEY_LEN 32
# define TETRISSH_MAX_PLAINTEXT 65536u

typedef enum e_tetrissh_role
{
	TETRISSH_ROLE_NONE = 0,
	TETRISSH_ROLE_CLIENT,
	TETRISSH_ROLE_SERVER
} t_tetrissh_role;

typedef struct s_session
{
	int             fd;
	t_tetrissh_role role;
	unsigned char   aes_key[TETRISSH_KEY_LEN];
	uint64_t        send_seq;
	uint64_t        recv_seq;
	int             established;
} t_session;

int		session_handshake_server(int fd, t_session *sess,
			const char *cert_path, const char *key_path);
int		session_handshake_client(int fd, t_session *sess,
			const char *ca_path);
ssize_t	session_send(t_session *sess, const void *buf, size_t len);
ssize_t	session_recv(t_session *sess, void *buf, size_t max_len);
void	session_close(t_session *sess);

#endif
```

- [ ] **Step 3: Add private header**

Create `lib/libtetrissh/src/internal.h` with:

```c
#ifndef TETRISSH_INTERNAL_H
# define TETRISSH_INTERNAL_H

# include "tetrissh.h"
# include <stddef.h>
# include <stdint.h>

# define TSH_NONCE_LEN 32u
# define TSH_GCM_NONCE_LEN 12u
# define TSH_GCM_TAG_LEN 16u
# define TSH_FRAME_OVERHEAD (TSH_GCM_NONCE_LEN + TSH_GCM_TAG_LEN)
# define TSH_CLIENT_MARKER 0x43u
# define TSH_SERVER_MARKER 0x53u

typedef enum e_tsh_io_result
{
	TSH_IO_OK = 1,
	TSH_IO_EOF = 0,
	TSH_IO_ERR = -1
} t_tsh_io_result;

t_tsh_io_result	tsh_read_exact(int fd, void *buf, size_t len);
int				tsh_write_exact(int fd, const void *buf, size_t len);
t_tsh_io_result	tsh_read_u32(int fd, uint32_t *value);
int				tsh_write_u32(int fd, uint32_t value);
void			tsh_u64_be(uint64_t value, unsigned char out[8]);
unsigned char	tsh_send_marker(t_tetrissh_role role);
unsigned char	tsh_recv_marker(t_tetrissh_role role);

#endif
```

- [ ] **Step 4: Add stub source files**

Create `lib/libtetrissh/src/io.c` with:

```c
#include "internal.h"

t_tsh_io_result	tsh_read_exact(int fd, void *buf, size_t len)
{
	(void)fd;
	(void)buf;
	(void)len;
	return (TSH_IO_ERR);
}

int	tsh_write_exact(int fd, const void *buf, size_t len)
{
	(void)fd;
	(void)buf;
	(void)len;
	return (-1);
}

t_tsh_io_result	tsh_read_u32(int fd, uint32_t *value)
{
	(void)fd;
	(void)value;
	return (TSH_IO_ERR);
}

int	tsh_write_u32(int fd, uint32_t value)
{
	(void)fd;
	(void)value;
	return (-1);
}

void	tsh_u64_be(uint64_t value, unsigned char out[8])
{
	for (int i = 7; i >= 0; --i)
	{
		out[i] = (unsigned char)(value & 0xffu);
		value >>= 8;
	}
}

unsigned char	tsh_send_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_CLIENT_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_SERVER_MARKER);
	return (0u);
}

unsigned char	tsh_recv_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_SERVER_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_CLIENT_MARKER);
	return (0u);
}
```

Create `lib/libtetrissh/src/session.c` with:

```c
#include "internal.h"
#include <openssl/crypto.h>
#include <string.h>

ssize_t	session_send(t_session *sess, const void *buf, size_t len)
{
	(void)sess;
	(void)buf;
	(void)len;
	return (-1);
}

ssize_t	session_recv(t_session *sess, void *buf, size_t max_len)
{
	(void)sess;
	(void)buf;
	(void)max_len;
	return (-1);
}

void	session_close(t_session *sess)
{
	if (sess == NULL)
		return;
	OPENSSL_cleanse(sess->aes_key, sizeof(sess->aes_key));
	sess->fd = -1;
	sess->role = TETRISSH_ROLE_NONE;
	sess->send_seq = 0;
	sess->recv_seq = 0;
	sess->established = 0;
}
```

Create `lib/libtetrissh/src/handshake.c` with:

```c
#include "internal.h"

int	session_handshake_server(int fd, t_session *sess,
		const char *cert_path, const char *key_path)
{
	(void)fd;
	(void)sess;
	(void)cert_path;
	(void)key_path;
	return (-1);
}

int	session_handshake_client(int fd, t_session *sess, const char *ca_path)
{
	(void)fd;
	(void)sess;
	(void)ca_path;
	return (-1);
}
```

- [ ] **Step 5: Add Makefile**

Create `lib/libtetrissh/Makefile` with:

```make
NAME := libtetrissh.a
CC := gcc
AR := ar
ARFLAGS := rcs
RM := rm -rf

CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic
COMMON_CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2
INC := -Iinclude
LDLIBS := -lssl -lcrypto

SRC_DIR := src
OBJ_DIR := obj
TESTS_DIR := tests
TEST_BIN_DIR := $(TESTS_DIR)/bin
COMMON_SRC := $(SRC_DIR)/common.c
COMMON_OBJ := $(OBJ_DIR)/common.o

SRC := $(filter-out $(COMMON_SRC),$(wildcard $(SRC_DIR)/*.c))
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(COMMON_OBJ)

TEST_SRC := $(wildcard $(TESTS_DIR)/test_*.c)
TEST_BINS := $(TEST_SRC:$(TESTS_DIR)/%.c=$(TEST_BIN_DIR)/%)

.PHONY: all test clean fclean re

all: $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c include/tetrissh.h $(SRC_DIR)/internal.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(COMMON_OBJ): $(COMMON_SRC) include/libs/common.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(COMMON_CFLAGS) $(INC) -c $< -o $@

$(NAME): $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

$(TEST_BIN_DIR)/test_%: $(TESTS_DIR)/test_%.c $(NAME)
	@mkdir -p $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(NAME) $(LDLIBS) -pthread -o $@

test: $(NAME) $(TEST_BINS)
	@./scripts/run_tests.sh $(TEST_BINS)

clean:
	$(RM) $(OBJ_DIR) $(TEST_BIN_DIR) tests/tmp

fclean: clean
	$(RM) $(NAME)

re: fclean all
```

- [ ] **Step 6: Add test runner**

Create `lib/libtetrissh/scripts/run_tests.sh` with:

```bash
#!/bin/sh
set -eu

pass=0
fail=0

for test_bin in "$@"; do
    printf '%s\n' "==> $test_bin"
    if "$test_bin"; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
done

printf '\nlibtetrissh tests: %d passed, %d failed\n' "$pass" "$fail"
test "$fail" -eq 0
```

- [ ] **Step 7: Build skeleton**

Run:

```bash
make -C lib/libtetrissh clean
make -C lib/libtetrissh
```

Expected: `lib/libtetrissh/libtetrissh.a` exists. No tests exist yet, so no test command in this task.

- [ ] **Step 8: Commit and push**

Run:

```bash
git status --short
git diff --stat
git add lib/libtetrissh
git commit -m "[libtetrissh] add library skeleton"
git push origin MacMiniSSH
```

Expected: branch pushes one commit.

---

### Task 2: Exact I/O Helpers

**Files:**
- Modify: `lib/libtetrissh/src/io.c`
- Create: `lib/libtetrissh/tests/test_io.c`

**Interfaces:**
- Consumes: `tsh_read_exact`, `tsh_write_exact`, `tsh_read_u32`, `tsh_write_u32`, `tsh_u64_be` declarations from `internal.h`.
- Produces: reliable blocking I/O helpers used by handshake and encrypted frames.

- [ ] **Step 1: Write failing IO tests**

Create `lib/libtetrissh/tests/test_io.c` with:

```c
#include "../src/internal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void	test_u32_round_trip(void)
{
	int		fds[2];
	uint32_t	value;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	assert(tsh_write_u32(fds[0], 0x01020304u) == 0);
	assert(tsh_read_u32(fds[1], &value) == TSH_IO_OK);
	assert(value == 0x01020304u);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_u32_round_trip\n");
}

static void	test_exact_buffer_round_trip(void)
{
	int			fds[2];
	const char	*msg = "split-safe socket payload";
	char		buf[64];

	memset(buf, 0, sizeof(buf));
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	assert(tsh_write_exact(fds[0], msg, strlen(msg)) == 0);
	assert(tsh_read_exact(fds[1], buf, strlen(msg)) == TSH_IO_OK);
	assert(strcmp(buf, msg) == 0);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_exact_buffer_round_trip\n");
}

static void	test_eof_before_payload_is_error(void)
{
	int		fds[2];
	char	buf[8];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	assert(tsh_write_exact(fds[0], "abc", 3) == 0);
	close(fds[0]);
	assert(tsh_read_exact(fds[1], buf, sizeof(buf)) == TSH_IO_ERR);
	close(fds[1]);
	printf("PASS test_eof_before_payload_is_error\n");
}

static void	test_u64_big_endian(void)
{
	unsigned char out[8];

	tsh_u64_be(0x0102030405060708ull, out);
	assert(out[0] == 0x01);
	assert(out[1] == 0x02);
	assert(out[2] == 0x03);
	assert(out[3] == 0x04);
	assert(out[4] == 0x05);
	assert(out[5] == 0x06);
	assert(out[6] == 0x07);
	assert(out[7] == 0x08);
	printf("PASS test_u64_big_endian\n");
}

int	main(void)
{
	test_u32_round_trip();
	test_exact_buffer_round_trip();
	test_eof_before_payload_is_error();
	test_u64_big_endian();
	return (0);
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
make -C lib/libtetrissh test
```

Expected: `test_u32_round_trip` fails because `tsh_write_u32` stub returns `-1`.

- [ ] **Step 3: Implement exact I/O**

Replace `lib/libtetrissh/src/io.c` with:

```c
#include "internal.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

t_tsh_io_result	tsh_read_exact(int fd, void *buf, size_t len)
{
	unsigned char	*out;
	size_t			done;
	ssize_t			n;

	out = buf;
	done = 0;
	while (done < len)
	{
		n = recv(fd, out + done, len - done, 0);
		if (n == 0)
			return (done == 0 ? TSH_IO_EOF : TSH_IO_ERR);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			return (TSH_IO_ERR);
		}
		done += (size_t)n;
	}
	return (TSH_IO_OK);
}

int	tsh_write_exact(int fd, const void *buf, size_t len)
{
	const unsigned char	*in;
	size_t				done;
	ssize_t				n;

	in = buf;
	done = 0;
	while (done < len)
	{
		n = send(fd, in + done, len - done, 0);
		if (n <= 0)
		{
			if (n < 0 && errno == EINTR)
				continue;
			return (-1);
		}
		done += (size_t)n;
	}
	return (0);
}

t_tsh_io_result	tsh_read_u32(int fd, uint32_t *value)
{
	unsigned char	buf[4];
	t_tsh_io_result res;

	if (value == NULL)
		return (TSH_IO_ERR);
	res = tsh_read_exact(fd, buf, sizeof(buf));
	if (res != TSH_IO_OK)
		return (res);
	*value = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
		| ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
	return (TSH_IO_OK);
}

int	tsh_write_u32(int fd, uint32_t value)
{
	unsigned char	buf[4];

	buf[0] = (unsigned char)((value >> 24) & 0xffu);
	buf[1] = (unsigned char)((value >> 16) & 0xffu);
	buf[2] = (unsigned char)((value >> 8) & 0xffu);
	buf[3] = (unsigned char)(value & 0xffu);
	return (tsh_write_exact(fd, buf, sizeof(buf)));
}

void	tsh_u64_be(uint64_t value, unsigned char out[8])
{
	for (int i = 7; i >= 0; --i)
	{
		out[i] = (unsigned char)(value & 0xffu);
		value >>= 8;
	}
}

unsigned char	tsh_send_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_CLIENT_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_SERVER_MARKER);
	return (0u);
}

unsigned char	tsh_recv_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_SERVER_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_CLIENT_MARKER);
	return (0u);
}
```

- [ ] **Step 4: Run tests to verify pass**

Run:

```bash
make -C lib/libtetrissh test
```

Expected: `test_io` prints four `PASS` lines and test summary shows `1 passed, 0 failed`.

- [ ] **Step 5: Commit and push**

Run:

```bash
git status --short
git diff --stat
git add lib/libtetrissh/src/io.c lib/libtetrissh/tests/test_io.c
git commit -m "[libtetrissh] add exact socket io"
git push origin MacMiniSSH
```

Expected: branch pushes one commit.

---

### Task 3: AES-256-GCM Frame I/O

**Files:**
- Modify: `lib/libtetrissh/src/session.c`
- Create: `lib/libtetrissh/tests/test_session_frames.c`

**Interfaces:**
- Consumes: exact I/O helpers from Task 2 and public `t_session` fields.
- Produces: `session_send`, `session_recv`, `session_close` final behavior for encrypted frames.

- [ ] **Step 1: Write failing frame tests**

Create `lib/libtetrissh/tests/test_session_frames.c` with:

```c
#include "tetrissh.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void	init_pair(t_session *client, t_session *server, int fds[2])
{
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(client, 0, sizeof(*client));
	memset(server, 0, sizeof(*server));
	client->fd = fds[0];
	client->role = TETRISSH_ROLE_CLIENT;
	client->established = 1;
	server->fd = fds[1];
	server->role = TETRISSH_ROLE_SERVER;
	server->established = 1;
	for (size_t i = 0; i < TETRISSH_KEY_LEN; ++i)
	{
		client->aes_key[i] = (unsigned char)(i + 1u);
		server->aes_key[i] = (unsigned char)(i + 1u);
	}
}

static void	test_client_to_server_round_trip(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		buf[64];

	init_pair(&client, &server, fds);
	assert(session_send(&client, "JOIN /room/a HTTTP/1.0\r\n\r\n", 27) == 27);
	assert(session_recv(&server, buf, sizeof(buf)) == 27);
	buf[27] = '\0';
	assert(strcmp(buf, "JOIN /room/a HTTTP/1.0\r\n\r\n") == 0);
	assert(client.send_seq == 1);
	assert(server.recv_seq == 1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_client_to_server_round_trip\n");
}

static void	test_server_to_client_round_trip(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		buf[64];

	init_pair(&client, &server, fds);
	assert(session_send(&server, "HTTTP/1.0 200 OK\r\n\r\n", 23) == 23);
	assert(session_recv(&client, buf, sizeof(buf)) == 23);
	buf[23] = '\0';
	assert(strcmp(buf, "HTTTP/1.0 200 OK\r\n\r\n") == 0);
	assert(server.send_seq == 1);
	assert(client.recv_seq == 1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_server_to_client_round_trip\n");
}

static void	test_oversized_plaintext_rejected(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		too_big[TETRISSH_MAX_PLAINTEXT + 1u];

	init_pair(&client, &server, fds);
	memset(too_big, 'A', sizeof(too_big));
	assert(session_send(&client, too_big, sizeof(too_big)) == -1);
	assert(client.send_seq == 0);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_oversized_plaintext_rejected\n");
}

static void	test_replay_same_frame_fails(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		buf[64];

	init_pair(&client, &server, fds);
	assert(session_send(&client, "first", 5) == 5);
	assert(session_recv(&server, buf, sizeof(buf)) == 5);
	assert(session_send(&client, "second", 6) == 6);
	server.recv_seq = 0;
	assert(session_recv(&server, buf, sizeof(buf)) == -1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_replay_same_frame_fails\n");
}

int	main(void)
{
	test_client_to_server_round_trip();
	test_server_to_client_round_trip();
	test_oversized_plaintext_rejected();
	test_replay_same_frame_fails();
	return (0);
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
make -C lib/libtetrissh test
```

Expected: `test_session_frames` fails because `session_send` stub returns `-1`.

- [ ] **Step 3: Implement AES-256-GCM frames**

Replace `lib/libtetrissh/src/session.c` with implementation containing these exact helpers and public functions:

```c
#include "internal.h"
#include <limits.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>

static int	valid_session(const t_session *sess)
{
	return (sess != NULL && sess->fd >= 0 && sess->established
		&& (sess->role == TETRISSH_ROLE_CLIENT
			|| sess->role == TETRISSH_ROLE_SERVER));
}

static void	build_aad(uint64_t seq, unsigned char marker, unsigned char aad[9])
{
	tsh_u64_be(seq, aad);
	aad[8] = marker;
}
```

Then add `gcm_encrypt()`:

```c
static int	gcm_encrypt(t_session *sess, const unsigned char *plain,
		size_t plain_len, unsigned char **frame, uint32_t *frame_len)
{
	EVP_CIPHER_CTX	*ctx;
	unsigned char	*token;
	unsigned char	aad[9];
	int				out_len;
	int				final_len;

	if (plain_len > TETRISSH_MAX_PLAINTEXT || plain_len > UINT32_MAX)
		return (-1);
	*frame_len = (uint32_t)(TSH_FRAME_OVERHEAD + plain_len);
	token = malloc(*frame_len);
	if (token == NULL)
		return (-1);
	if (RAND_bytes(token, TSH_GCM_NONCE_LEN) != 1)
		return (free(token), -1);
	build_aad(sess->send_seq, tsh_send_marker(sess->role), aad);
	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return (free(token), -1);
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1
		|| EVP_EncryptInit_ex(ctx, NULL, NULL, sess->aes_key, token) != 1
		|| EVP_EncryptUpdate(ctx, NULL, &out_len, aad, sizeof(aad)) != 1
		|| EVP_EncryptUpdate(ctx, token + TSH_FRAME_OVERHEAD, &out_len,
			plain, (int)plain_len) != 1
		|| EVP_EncryptFinal_ex(ctx, token + TSH_FRAME_OVERHEAD + out_len,
			&final_len) != 1
		|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG,
			TSH_GCM_TAG_LEN, token + TSH_GCM_NONCE_LEN) != 1)
	{
		EVP_CIPHER_CTX_free(ctx);
		OPENSSL_cleanse(token, *frame_len);
		free(token);
		return (-1);
	}
	EVP_CIPHER_CTX_free(ctx);
	*frame = token;
	return (0);
}
```

Then add `gcm_decrypt()` and public functions:

```c
static int	gcm_decrypt(t_session *sess, const unsigned char *frame,
		uint32_t frame_len, unsigned char **plain, size_t *plain_len)
{
	EVP_CIPHER_CTX	*ctx;
	unsigned char	*out;
	unsigned char	aad[9];
	int				out_len;
	int				final_ok;

	if (frame_len < TSH_FRAME_OVERHEAD)
		return (-1);
	*plain_len = (size_t)frame_len - TSH_FRAME_OVERHEAD;
	if (*plain_len > TETRISSH_MAX_PLAINTEXT)
		return (-1);
	out = malloc(*plain_len == 0 ? 1 : *plain_len);
	if (out == NULL)
		return (-1);
	build_aad(sess->recv_seq, tsh_recv_marker(sess->role), aad);
	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return (free(out), -1);
	if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1
		|| EVP_DecryptInit_ex(ctx, NULL, NULL, sess->aes_key, frame) != 1
		|| EVP_DecryptUpdate(ctx, NULL, &out_len, aad, sizeof(aad)) != 1
		|| EVP_DecryptUpdate(ctx, out, &out_len,
			frame + TSH_FRAME_OVERHEAD, (int)*plain_len) != 1
		|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
			TSH_GCM_TAG_LEN, (void *)(frame + TSH_GCM_NONCE_LEN)) != 1)
	{
		EVP_CIPHER_CTX_free(ctx);
		OPENSSL_cleanse(out, *plain_len);
		free(out);
		return (-1);
	}
	final_ok = EVP_DecryptFinal_ex(ctx, out + out_len, &out_len);
	EVP_CIPHER_CTX_free(ctx);
	if (final_ok != 1)
		return (OPENSSL_cleanse(out, *plain_len), free(out), -1);
	*plain = out;
	return (0);
}

ssize_t	session_send(t_session *sess, const void *buf, size_t len)
{
	unsigned char	*frame;
	uint32_t		frame_len;

	if (!valid_session(sess) || (buf == NULL && len != 0)
		|| len > TETRISSH_MAX_PLAINTEXT)
		return (-1);
	if (gcm_encrypt(sess, buf, len, &frame, &frame_len) != 0)
		return (-1);
	if (tsh_write_u32(sess->fd, frame_len) != 0
		|| tsh_write_exact(sess->fd, frame, frame_len) != 0)
	{
		OPENSSL_cleanse(frame, frame_len);
		free(frame);
		return (-1);
	}
	OPENSSL_cleanse(frame, frame_len);
	free(frame);
	sess->send_seq++;
	return ((ssize_t)len);
}

ssize_t	session_recv(t_session *sess, void *buf, size_t max_len)
{
	uint32_t		frame_len;
	unsigned char	*frame;
	unsigned char	*plain;
	size_t			plain_len;
	t_tsh_io_result	res;

	if (!valid_session(sess) || buf == NULL)
		return (-1);
	res = tsh_read_u32(sess->fd, &frame_len);
	if (res == TSH_IO_EOF)
		return (0);
	if (res != TSH_IO_OK || frame_len < TSH_FRAME_OVERHEAD
		|| frame_len > TSH_FRAME_OVERHEAD + TETRISSH_MAX_PLAINTEXT)
		return (-1);
	frame = malloc(frame_len);
	if (frame == NULL)
		return (-1);
	if (tsh_read_exact(sess->fd, frame, frame_len) != TSH_IO_OK
		|| gcm_decrypt(sess, frame, frame_len, &plain, &plain_len) != 0)
		return (OPENSSL_cleanse(frame, frame_len), free(frame), -1);
	OPENSSL_cleanse(frame, frame_len);
	free(frame);
	if (plain_len > max_len)
		return (OPENSSL_cleanse(plain, plain_len), free(plain), -1);
	memcpy(buf, plain, plain_len);
	OPENSSL_cleanse(plain, plain_len);
	free(plain);
	sess->recv_seq++;
	return ((ssize_t)plain_len);
}

void	session_close(t_session *sess)
{
	if (sess == NULL)
		return;
	OPENSSL_cleanse(sess->aes_key, sizeof(sess->aes_key));
	sess->fd = -1;
	sess->role = TETRISSH_ROLE_NONE;
	sess->send_seq = 0;
	sess->recv_seq = 0;
	sess->established = 0;
}
```

- [ ] **Step 4: Run tests to verify pass**

Run:

```bash
make -C lib/libtetrissh test
```

Expected: `test_io` and `test_session_frames` pass.

- [ ] **Step 5: Commit and push**

Run:

```bash
git status --short
git diff --stat
git add lib/libtetrissh/src/session.c lib/libtetrissh/tests/test_session_frames.c
git commit -m "[libtetrissh] add AES-256 frame io"
git push origin MacMiniSSH
```

Expected: branch pushes one commit.

---

### Task 4: Test Certificates And Handshake Test

**Files:**
- Create: `lib/libtetrissh/scripts/generate_test_certs.sh`
- Create: `lib/libtetrissh/tests/test_handshake_socketpair.c`
- Modify: `lib/libtetrissh/Makefile`

**Interfaces:**
- Consumes: final public handshake signatures from `tetrissh.h`.
- Produces: failing socketpair integration test that drives both handshake sides in threads.

- [ ] **Step 1: Add certificate generation script**

Create `lib/libtetrissh/scripts/generate_test_certs.sh` with:

```bash
#!/bin/sh
set -eu

out_dir="${1:-tests/tmp/certs}"
mkdir -p "$out_dir"

openssl genrsa -out "$out_dir/ca.key" 1024 >/dev/null 2>&1
openssl req -x509 -new -nodes -key "$out_dir/ca.key" -sha256 -days 1 \
    -subj "/C=SG/ST=Singapore/L=Singapore/O=SUTD/CN=tetrish-test-ca" \
    -out "$out_dir/ca.crt" >/dev/null 2>&1

openssl genrsa -out "$out_dir/server.key" 1024 >/dev/null 2>&1
openssl req -new -key "$out_dir/server.key" \
    -subj "/C=SG/ST=Singapore/L=Singapore/O=SUTD/CN=sutd.edu.sg" \
    -out "$out_dir/server.csr" >/dev/null 2>&1
openssl x509 -req -in "$out_dir/server.csr" -CA "$out_dir/ca.crt" \
    -CAkey "$out_dir/ca.key" -CAcreateserial -out "$out_dir/server.crt" \
    -days 1 -sha256 >/dev/null 2>&1
```

- [ ] **Step 2: Write failing handshake test**

Create `lib/libtetrissh/tests/test_handshake_socketpair.c` with:

```c
#include "tetrissh.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_server_args
{
	int			fd;
	t_session	sess;
	int			result;
} t_server_args;

static void	*server_thread(void *arg)
{
	t_server_args	*server;
	char			buf[64];

	server = arg;
	server->result = session_handshake_server(server->fd, &server->sess,
		"tests/tmp/certs/server.crt", "tests/tmp/certs/server.key");
	if (server->result == 0)
	{
		assert(session_recv(&server->sess, buf, sizeof(buf)) == 5);
		assert(memcmp(buf, "hello", 5) == 0);
		assert(session_send(&server->sess, "world", 5) == 5);
	}
	return (NULL);
}

static void	test_handshake_and_frames(void)
{
	int				fds[2];
	t_session		client;
	t_server_args	server;
	pthread_t		thread;
	char			buf[64];

	assert(system("./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(&server, 0, sizeof(server));
	server.fd = fds[1];
	assert(pthread_create(&thread, NULL, server_thread, &server) == 0);
	assert(session_handshake_client(fds[0], &client,
		"tests/tmp/certs/ca.crt") == 0);
	assert(session_send(&client, "hello", 5) == 5);
	assert(session_recv(&client, buf, sizeof(buf)) == 5);
	assert(memcmp(buf, "world", 5) == 0);
	assert(pthread_join(thread, NULL) == 0);
	assert(server.result == 0);
	session_close(&client);
	session_close(&server.sess);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_handshake_and_frames\n");
}

int	main(void)
{
	test_handshake_and_frames();
	return (0);
}
```

- [ ] **Step 3: Ensure test runs from library root**

Modify `lib/libtetrissh/Makefile` test target if needed so tests execute with working directory `lib/libtetrissh`. Keep:

```make
test: $(NAME) $(TEST_BINS)
	@./scripts/run_tests.sh $(TEST_BINS)
```

No change required if Task 1 Makefile already contains that target.

- [ ] **Step 4: Run test to verify failure**

Run:

```bash
make -C lib/libtetrissh test
```

Expected: `test_handshake_socketpair` fails because handshake functions return `-1`.

- [ ] **Step 5: Commit and push failing integration test**

Run:

```bash
git status --short
git diff --stat
git add lib/libtetrissh/scripts/generate_test_certs.sh lib/libtetrissh/tests/test_handshake_socketpair.c lib/libtetrissh/Makefile
git commit -m "[libtetrissh] add handshake integration test"
git push origin MacMiniSSH
```

Expected: branch pushes one commit containing a known failing test. Mention failure in progress update.

---

### Task 5: Client And Server Handshake

**Files:**
- Modify: `lib/libtetrissh/src/handshake.c`

**Interfaces:**
- Consumes: PA2 common functions `load_cert_file`, `load_cert_bytes`, `verify_server_cert`, `load_private_key`, `sign_message_pss`, `verify_message_pss`, `rsa_encrypt_block`, `rsa_decrypt_block`.
- Produces: working `session_handshake_server` and `session_handshake_client`.

- [ ] **Step 1: Replace handshake implementation**

Replace `lib/libtetrissh/src/handshake.c` with implementation containing these helper functions and public functions:

```c
#include "internal.h"
#include "libs/common.h"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int	read_file_bytes(const char *path, unsigned char **out, uint32_t *len)
{
	FILE	*fp;
	long	size;

	if (path == NULL || out == NULL || len == NULL)
		return (-1);
	fp = fopen(path, "rb");
	if (fp == NULL)
		return (-1);
	if (fseek(fp, 0, SEEK_END) != 0)
		return (fclose(fp), -1);
	size = ftell(fp);
	if (size <= 0 || size > UINT32_MAX)
		return (fclose(fp), -1);
	rewind(fp);
	*out = malloc((size_t)size);
	if (*out == NULL)
		return (fclose(fp), -1);
	if (fread(*out, 1, (size_t)size, fp) != (size_t)size)
		return (free(*out), *out = NULL, fclose(fp), -1);
	*len = (uint32_t)size;
	return (fclose(fp), 0);
}

static void	init_session(t_session *sess, int fd, t_tetrissh_role role)
{
	memset(sess, 0, sizeof(*sess));
	sess->fd = fd;
	sess->role = role;
}
```

Then add `session_handshake_server()`:

```c
int	session_handshake_server(int fd, t_session *sess,
		const char *cert_path, const char *key_path)
{
	unsigned char	client_nonce[TSH_NONCE_LEN];
	unsigned char	*cert_bytes;
	unsigned char	*sig;
	unsigned char	*wrapped;
	unsigned char	*key_plain;
	uint32_t		cert_len;
	uint32_t		sig_len_u32;
	uint32_t		wrapped_len;
	size_t			sig_len;
	size_t			key_len;
	EVP_PKEY		*priv;
	int				ok;

	cert_bytes = NULL;
	sig = NULL;
	wrapped = NULL;
	key_plain = NULL;
	priv = NULL;
	ok = -1;
	if (sess == NULL || cert_path == NULL || key_path == NULL)
		return (-1);
	init_session(sess, fd, TETRISSH_ROLE_SERVER);
	if (tsh_read_exact(fd, client_nonce, sizeof(client_nonce)) != TSH_IO_OK)
		goto cleanup;
	if (read_file_bytes(cert_path, &cert_bytes, &cert_len) != 0)
		goto cleanup;
	priv = load_private_key(key_path);
	if (priv == NULL)
		goto cleanup;
	sig = sign_message_pss(priv, client_nonce, sizeof(client_nonce), &sig_len);
	if (sig == NULL || sig_len > UINT32_MAX)
		goto cleanup;
	sig_len_u32 = (uint32_t)sig_len;
	if (tsh_write_u32(fd, cert_len) != 0
		|| tsh_write_exact(fd, cert_bytes, cert_len) != 0
		|| tsh_write_u32(fd, sig_len_u32) != 0
		|| tsh_write_exact(fd, sig, sig_len_u32) != 0)
		goto cleanup;
	if (tsh_read_u32(fd, &wrapped_len) != TSH_IO_OK || wrapped_len == 0)
		goto cleanup;
	wrapped = malloc(wrapped_len);
	if (wrapped == NULL)
		goto cleanup;
	if (tsh_read_exact(fd, wrapped, wrapped_len) != TSH_IO_OK)
		goto cleanup;
	key_plain = rsa_decrypt_block(priv, wrapped, wrapped_len, &key_len, 1);
	if (key_plain == NULL || key_len != TETRISSH_KEY_LEN)
		goto cleanup;
	memcpy(sess->aes_key, key_plain, TETRISSH_KEY_LEN);
	sess->established = 1;
	ok = 0;
cleanup:
	EVP_PKEY_free(priv);
	free(cert_bytes);
	free(sig);
	if (wrapped != NULL)
		OPENSSL_cleanse(wrapped, wrapped_len);
	free(wrapped);
	if (key_plain != NULL)
		OPENSSL_cleanse(key_plain, key_len);
	free(key_plain);
	OPENSSL_cleanse(client_nonce, sizeof(client_nonce));
	if (ok != 0)
		session_close(sess);
	return (ok);
}
```

Then add `session_handshake_client()`:

```c
int	session_handshake_client(int fd, t_session *sess, const char *ca_path)
{
	unsigned char	client_nonce[TSH_NONCE_LEN];
	unsigned char	*cert_bytes;
	unsigned char	*sig;
	unsigned char	*wrapped;
	uint32_t		cert_len;
	uint32_t		sig_len;
	size_t			wrapped_len;
	X509			*cert;
	EVP_PKEY		*pub;
	int				ok;

	cert_bytes = NULL;
	sig = NULL;
	wrapped = NULL;
	cert = NULL;
	pub = NULL;
	ok = -1;
	if (sess == NULL || ca_path == NULL)
		return (-1);
	init_session(sess, fd, TETRISSH_ROLE_CLIENT);
	if (RAND_bytes(client_nonce, sizeof(client_nonce)) != 1)
		goto cleanup;
	if (tsh_write_exact(fd, client_nonce, sizeof(client_nonce)) != 0)
		goto cleanup;
	if (tsh_read_u32(fd, &cert_len) != TSH_IO_OK || cert_len == 0)
		goto cleanup;
	cert_bytes = malloc(cert_len);
	if (cert_bytes == NULL)
		goto cleanup;
	if (tsh_read_exact(fd, cert_bytes, cert_len) != TSH_IO_OK)
		goto cleanup;
	if (tsh_read_u32(fd, &sig_len) != TSH_IO_OK || sig_len == 0)
		goto cleanup;
	sig = malloc(sig_len);
	if (sig == NULL)
		goto cleanup;
	if (tsh_read_exact(fd, sig, sig_len) != TSH_IO_OK)
		goto cleanup;
	cert = load_cert_bytes(cert_bytes, (int)cert_len);
	if (cert == NULL || verify_server_cert(cert, ca_path) != 1)
		goto cleanup;
	if (verify_message_pss(cert, sig, sig_len,
			client_nonce, sizeof(client_nonce)) != 1)
		goto cleanup;
	pub = X509_get_pubkey(cert);
	if (pub == NULL)
		goto cleanup;
	if (RAND_bytes(sess->aes_key, TETRISSH_KEY_LEN) != 1)
		goto cleanup;
	wrapped = rsa_encrypt_block(pub, sess->aes_key, TETRISSH_KEY_LEN,
		&wrapped_len, 1);
	if (wrapped == NULL || wrapped_len > UINT32_MAX)
		goto cleanup;
	if (tsh_write_u32(fd, (uint32_t)wrapped_len) != 0
		|| tsh_write_exact(fd, wrapped, wrapped_len) != 0)
		goto cleanup;
	sess->established = 1;
	ok = 0;
cleanup:
	EVP_PKEY_free(pub);
	X509_free(cert);
	free(cert_bytes);
	free(sig);
	if (wrapped != NULL)
		OPENSSL_cleanse(wrapped, wrapped_len);
	free(wrapped);
	OPENSSL_cleanse(client_nonce, sizeof(client_nonce));
	if (ok != 0)
		session_close(sess);
	return (ok);
}
```

- [ ] **Step 2: Run tests to verify pass**

Run:

```bash
make -C lib/libtetrissh clean
make -C lib/libtetrissh test
```

Expected: `test_io`, `test_session_frames`, and `test_handshake_socketpair` pass. If `generate_test_certs.sh` lacks executable bit, run `chmod +x lib/libtetrissh/scripts/generate_test_certs.sh` and re-run tests.

- [ ] **Step 3: Run umbrella libs build**

Run:

```bash
make libs
```

Expected: top-level `libs` target builds `lib/libtetrisbrain` and `lib/libtetrissh`.

- [ ] **Step 4: Commit and push**

Run:

```bash
git status --short
git diff --stat
git add lib/libtetrissh/src/handshake.c lib/libtetrissh/scripts/generate_test_certs.sh
git commit -m "[libtetrissh] implement secure handshake"
git push origin MacMiniSSH
```

Expected: branch pushes one commit.

---

### Task 6: Failure Tests And README Update

**Files:**
- Modify: `lib/libtetrissh/tests/test_handshake_socketpair.c`
- Modify: `README.md`

**Interfaces:**
- Consumes: working handshake and frame APIs.
- Produces: explicit failure coverage and final docs for checkoff explanation.

- [ ] **Step 1: Add invalid CA failure test**

Append this function to `lib/libtetrissh/tests/test_handshake_socketpair.c` before `main`:

```c
static void	*server_fail_thread(void *arg)
{
	t_server_args	*server;

	server = arg;
	server->result = session_handshake_server(server->fd, &server->sess,
		"tests/tmp/certs/server.crt", "tests/tmp/certs/server.key");
	return (NULL);
}

static void	test_invalid_ca_fails_client(void)
{
	int				fds[2];
	t_session		client;
	t_server_args	server;
	pthread_t		thread;

	assert(system("./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(&server, 0, sizeof(server));
	server.fd = fds[1];
	assert(pthread_create(&thread, NULL, server_fail_thread, &server) == 0);
	assert(session_handshake_client(fds[0], &client,
		"tests/tmp/certs/missing-ca.crt") == -1);
	shutdown(fds[0], SHUT_RDWR);
	shutdown(fds[1], SHUT_RDWR);
	assert(pthread_join(thread, NULL) == 0);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_invalid_ca_fails_client\n");
}
```

Modify `main` to call both tests:

```c
int	main(void)
{
	test_handshake_and_frames();
	test_invalid_ca_fails_client();
	return (0);
}
```

- [ ] **Step 2: Run tests**

Run:

```bash
make -C lib/libtetrissh test
```

Expected: all tests pass; invalid CA test prints `PASS test_invalid_ca_fails_client`.

- [ ] **Step 3: Update README secure-session section**

Modify `README.md` lines under `## Secure Session: libtetrissh` so the key points read:

```markdown
`lib/libtetrissh` is a self-contained static library. It builds with:

```bash
make -C lib/libtetrissh
make -C lib/libtetrissh test
```

Handshake:

1. Client sends a fresh 32-byte nonce.
2. Server sends its PEM X.509 certificate with a 4-byte big-endian length.
3. Server signs the client nonce with RSA-PSS/SHA-256 using its private key.
4. Client verifies the cert against `ca_path` and verifies the nonce signature.
5. Client generates a fresh 32-byte AES-256 key and wraps it with RSA-OAEP/SHA-256.
6. Server unwraps the AES key with its private key.
7. Every HTTTP message after that is sent as one AES-256-GCM frame.

Encrypted frame format:

```text
frame_len[4] || nonce[12] || tag[16] || ciphertext
```

`frame_len` counts bytes after the length field. Plaintext is capped at 64 KiB. GCM authenticates a per-direction sequence number, so reordered or replayed frames fail tag verification.
```

Keep existing security assumptions and replace `CBC or GCM — [document your choice]` with `AES-256-GCM with per-frame random nonce and 16-byte tag`.

- [ ] **Step 4: Run verification**

Run:

```bash
make -C lib/libtetrissh clean
make -C lib/libtetrissh test
make libs
```

Expected: all commands exit `0`.

- [ ] **Step 5: Commit and push**

Run:

```bash
git status --short
git diff --stat
git add lib/libtetrissh/tests/test_handshake_socketpair.c README.md
git commit -m "[libtetrissh] document secure frames"
git push origin MacMiniSSH
```

Expected: branch pushes one commit.

---

## Self-Review

Spec coverage:

- Self-contained `lib/libtetrissh`: Task 1.
- PA2 common copied unchanged into normal library paths: Task 1 Step 1.
- Public API: Task 1 Step 2.
- Exact socket I/O: Task 2.
- AES-256-GCM frames, 64 KiB limit, sequence AAD: Task 3.
- Client/server handshake with nonce, cert, RSA-PSS, RSA-OAEP: Task 5.
- Socketpair tests: Tasks 2, 3, 4, 6.
- README security docs: Task 6.
- Commit and push per increment: every task includes commit and push step.

Red-flag scan:

- No incomplete sections, empty function names, or unnamed files remain in this plan.
- Every listed failure path returns `-1`, `0`, or test failure explicitly.

Type consistency:

- Public type is `t_session` in all tasks.
- Public constants are `TETRISSH_KEY_LEN` and `TETRISSH_MAX_PLAINTEXT` in tests and implementation.
- Private helpers use `tsh_` prefix in `internal.h`, `io.c`, `session.c`, and tests.
