#include "coredb.h"
#include "../src/coredb_internal.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t	test_strlen(const char *s)
{
	size_t	len;

	len = 0;
	while (s[len] != '\0')
		len++;
	return (len);
}

static void	write_result(const char *prefix, const char *name)
{
	(void)write(STDOUT_FILENO, prefix, test_strlen(prefix));
	(void)write(STDOUT_FILENO, name, test_strlen(name));
	(void)write(STDOUT_FILENO, "\n", 1);
}

static int	assert_true(int condition, const char *name)
{
	if (condition)
	{
		write_result("PASS ", name);
		return (EXIT_SUCCESS);
	}
	write_result("FAIL ", name);
	return (EXIT_FAILURE);
}

static int	test_crc32_known_vector(void)
{
	return (assert_true(coredb_crc32("123456789", 9) == 0xCBF43926u,
			"crc32 matches known test vector"));
}

static int	test_crc32_detects_corruption(void)
{
	const char	good[] = "alice:password123";
	char		bad[sizeof(good)];

	memcpy(bad, good, sizeof(good));
	bad[3] = 'X';
	return (assert_true(
			coredb_crc32(good, sizeof(good)) != coredb_crc32(bad, sizeof(bad)),
			"crc32 changes when payload is corrupted"));
}

static int	test_header_build_round_trips(void)
{
	t_coredb_ev_high_score	payload;
	t_coredb_record_header	hdr;

	payload.player_id = 42;
	payload.high_score = 9001;
	coredb_record_header_build(&hdr, COREDB_EV_SET_HIGH_SCORE, 7,
		sizeof(payload), &payload);
	if (assert_true(coredb_record_header_check(&hdr) == EXIT_SUCCESS,
			"built header passes magic/version check") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(hdr.event_type == COREDB_EV_SET_HIGH_SCORE,
			"built header keeps event type") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (assert_true(hdr.seq_no == 7, "built header keeps seq_no") != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (assert_true(
			coredb_record_payload_check(&hdr, &payload) == EXIT_SUCCESS,
			"built header validates its own payload"));
}

static int	test_header_check_rejects_bad_magic(void)
{
	t_coredb_record_header	hdr;

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic = 0xDEADBEEFu;
	hdr.version = COREDB_RECORD_VERSION;
	return (assert_true(coredb_record_header_check(&hdr) == EXIT_FAILURE,
			"header check rejects wrong magic"));
}

static int	test_payload_check_rejects_tampered_payload(void)
{
	t_coredb_ev_points_delta	payload;
	t_coredb_record_header		hdr;

	payload.player_id = 1;
	payload.points_delta = 100;
	payload.reason = 0;
	coredb_record_header_build(&hdr, COREDB_EV_POINTS_DELTA, 1,
		sizeof(payload), &payload);
	payload.points_delta = 999;
	return (assert_true(
			coredb_record_payload_check(&hdr, &payload) == EXIT_FAILURE,
			"payload check rejects tampered payload"));
}

int	main(void)
{
	if (test_crc32_known_vector() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_crc32_detects_corruption() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_header_build_round_trips() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_header_check_rejects_bad_magic() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_payload_check_rejects_tampered_payload() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
