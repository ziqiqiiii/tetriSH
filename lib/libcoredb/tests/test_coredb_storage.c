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

static int	test_append_then_read_round_trips(void)
{
	const char				*path;
	t_coredb				*db;
	t_coredb_ev_user_create	create_payload;
	t_coredb_ev_high_score	score_payload;
	uint64_t				seq0;
	uint64_t				seq1;
	t_coredb_record_header	hdr;
	t_coredb_event_payload	buf;
	int						ok;

	path = "/tmp/coredb_storage_test.db";
	(void)unlink(path);
	ok = EXIT_SUCCESS;
	if (coredb_open(&db, path) != EXIT_SUCCESS)
		return (assert_true(0, "coredb_open succeeds for storage test"));
	memset(&create_payload, 0, sizeof(create_payload));
	create_payload.player_id = 1;
	strcpy(create_payload.username, "alice");
	score_payload.player_id = 1;
	score_payload.high_score = 555;
	if (coredb_storage_append_record(db, COREDB_EV_USER_CREATE,
			sizeof(create_payload), &create_payload, &seq0) != EXIT_SUCCESS)
		ok = assert_true(0, "append user_create record succeeds");
	if (coredb_storage_append_record(db, COREDB_EV_SET_HIGH_SCORE,
			sizeof(score_payload), &score_payload, &seq1) != EXIT_SUCCESS)
		ok = assert_true(0, "append high_score record succeeds");
	if (assert_true(seq1 == seq0 + 1,
			"seq_no increments per record") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	rewind(db->file);
	if (assert_true(coredb_storage_read_record(db->file, &hdr, &buf,
				sizeof(buf)) == COREDB_IO_OK
			&& hdr.event_type == COREDB_EV_USER_CREATE
			&& memcmp(&buf.user_create, &create_payload,
				sizeof(create_payload)) == 0,
			"first record reads back identical to what was written")
		!= EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(coredb_storage_read_record(db->file, &hdr, &buf,
				sizeof(buf)) == COREDB_IO_OK
			&& hdr.event_type == COREDB_EV_SET_HIGH_SCORE
			&& memcmp(&buf.high_score, &score_payload,
				sizeof(score_payload)) == 0,
			"second record reads back identical to what was written")
		!= EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(coredb_storage_read_record(db->file, &hdr, &buf,
				sizeof(buf)) == COREDB_IO_EOF,
			"reading past the last record reports clean EOF") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	coredb_close(db);
	unlink(path);
	return (ok);
}

static int	test_truncated_trailing_record_detected(void)
{
	const char				*path;
	t_coredb				*db;
	t_coredb_ev_high_score	score_payload;
	t_coredb_record_header	hdr;
	t_coredb_event_payload	buf;
	int						ok;

	path = "/tmp/coredb_storage_truncated_test.db";
	(void)unlink(path);
	ok = EXIT_SUCCESS;
	if (coredb_open(&db, path) != EXIT_SUCCESS)
		return (assert_true(0, "coredb_open succeeds for truncation test"));
	score_payload.player_id = 9;
	score_payload.high_score = 42;
	coredb_storage_append_record(db, COREDB_EV_SET_HIGH_SCORE,
		sizeof(score_payload), &score_payload, NULL);
	if (fwrite("XX", 1, 2, db->file) != 2)
		ok = assert_true(0, "wrote garbage tail to simulate crash mid-write");
	fflush(db->file);
	rewind(db->file);
	if (assert_true(coredb_storage_read_record(db->file, &hdr, &buf,
				sizeof(buf)) == COREDB_IO_OK,
			"good record before the crash still reads cleanly")
		!= EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(coredb_storage_read_record(db->file, &hdr, &buf,
				sizeof(buf)) == COREDB_IO_TRUNCATED,
			"half-written trailing record reports truncated, not EOF")
		!= EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	coredb_close(db);
	unlink(path);
	return (ok);
}

int	main(void)
{
	if (test_append_then_read_round_trips() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_truncated_trailing_record_detected() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
