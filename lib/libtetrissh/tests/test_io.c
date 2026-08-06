#include "internal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void	test_u32_round_trip(void)
{
	int			fds[2];
	uint32_t	value;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	assert(sessionio_write_u32(fds[0], 0x01020304u) == 0);
	assert(sessionio_read_u32(fds[1], &value) == SESSIONIO_OK);
	assert(value == 0x01020304u);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_u32_round_trip\n");
}

static void	test_u32_wire_bytes(void)
{
	int				fds[2];
	unsigned char	buf[4];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	assert(sessionio_write_u32(fds[0], 0x01020304u) == 0);
	assert(sessionio_read_exact(fds[1], buf, sizeof(buf)) == SESSIONIO_OK);
	assert(buf[0] == 0x01 && buf[1] == 0x02);
	assert(buf[2] == 0x03 && buf[3] == 0x04);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_u32_wire_bytes\n");
}

static void	test_exact_buffer_round_trip(void)
{
	int			fds[2];
	const char	*msg;
	char		buf[64];

	msg = "split-safe socket payload";
	memset(buf, 0, sizeof(buf));
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	assert(sessionio_write_exact(fds[0], msg, strlen(msg)) == 0);
	assert(sessionio_read_exact(fds[1], buf, strlen(msg)) == SESSIONIO_OK);
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
	assert(sessionio_write_exact(fds[0], "abc", 3) == 0);
	close(fds[0]);
	assert(sessionio_read_exact(fds[1], buf, sizeof(buf)) == SESSIONIO_ERR);
	close(fds[1]);
	printf("PASS test_eof_before_payload_is_error\n");
}

static void	test_clean_eof_before_payload(void)
{
	int		fds[2];
	char	buf[1];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	close(fds[0]);
	assert(sessionio_read_exact(fds[1], buf, sizeof(buf)) == SESSIONIO_EOF);
	close(fds[1]);
	printf("PASS test_clean_eof_before_payload\n");
}

static void	test_u64_big_endian(void)
{
	unsigned char	out[8];

	sessionio_u64_be(0x0102030405060708ull, out);
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
	test_u32_wire_bytes();
	test_exact_buffer_round_trip();
	test_eof_before_payload_is_error();
	test_clean_eof_before_payload();
	test_u64_big_endian();
	return (0);
}
