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

static void	append_user_create(t_coredb *db, uint32_t player_id,
		const char *username)
{
	t_coredb_ev_user_create	ev;

	memset(&ev, 0, sizeof(ev));
	ev.player_id = player_id;
	strcpy(ev.username, username);
	ev.password_iters = 10000;
	coredb_storage_append_record(db, COREDB_EV_USER_CREATE, sizeof(ev),
		&ev, NULL);
}

static int	test_user_create_persists_across_reopen(void)
{
	const char		*path;
	t_coredb		*db;
	t_coredb_user	*user;
	int				ok;

	path = "/tmp/coredb_replay_user_test.db";
	(void)unlink(path);
	coredb_open(&db, path);
	append_user_create(db, 1, "alice");
	coredb_close(db);
	coredb_open(&db, path);
	user = coredb_hash_find_by_username(db->table, "alice");
	ok = assert_true(user != NULL && user->player_id == 1
		&& strcmp(user->username, "alice") == 0,
		"user_create event replays into the hash table on reopen");
	if (assert_true(coredb_hash_find_by_player_id(db->table, 1) == user,
			"replayed user is reachable by player_id too") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(db->next_player_id == 2,
			"next_player_id resumes after the highest replayed id")
		!= EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	coredb_close(db);
	unlink(path);
	return (ok);
}

static int	test_points_grant_equip_score_persist(void)
{
	const char					*path;
	t_coredb					*db;
	t_coredb_ev_points_delta	points_ev;
	t_coredb_ev_grant_item		grant_ev;
	t_coredb_ev_equip_item		equip_ev;
	t_coredb_ev_high_score		score_ev;
	t_coredb_user				*user;
	int							ok;

	path = "/tmp/coredb_replay_state_test.db";
	(void)unlink(path);
	coredb_open(&db, path);
	append_user_create(db, 5, "bob");
	points_ev.player_id = 5;
	points_ev.points_delta = 50;
	points_ev.reason = 0;
	coredb_storage_append_record(db, COREDB_EV_POINTS_DELTA,
		sizeof(points_ev), &points_ev, NULL);
	points_ev.points_delta = -10;
	coredb_storage_append_record(db, COREDB_EV_POINTS_DELTA,
		sizeof(points_ev), &points_ev, NULL);
	grant_ev.player_id = 5;
	grant_ev.item_id = 2;
	coredb_storage_append_record(db, COREDB_EV_GRANT_CHARACTER,
		sizeof(grant_ev), &grant_ev, NULL);
	grant_ev.item_id = 3;
	coredb_storage_append_record(db, COREDB_EV_GRANT_THEME,
		sizeof(grant_ev), &grant_ev, NULL);
	equip_ev.player_id = 5;
	equip_ev.item_id = 3;
	coredb_storage_append_record(db, COREDB_EV_EQUIP_THEME,
		sizeof(equip_ev), &equip_ev, NULL);
	score_ev.player_id = 5;
	score_ev.high_score = 9001;
	coredb_storage_append_record(db, COREDB_EV_SET_HIGH_SCORE,
		sizeof(score_ev), &score_ev, NULL);
	coredb_close(db);
	coredb_open(&db, path);
	user = coredb_hash_find_by_player_id(db->table, 5);
	ok = assert_true(user != NULL && user->points == 40,
		"points deltas accumulate and persist across reopen");
	if (assert_true(user != NULL && (user->owned_characters & (1ULL << 2)),
			"granted character bit persists") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(user != NULL && (user->owned_themes & (1ULL << 3)),
			"granted theme bit persists") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(user != NULL && user->equipped_theme == 3,
			"equipped theme persists") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	if (assert_true(user != NULL && user->high_score == 9001,
			"high score persists") != EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	coredb_close(db);
	unlink(path);
	return (ok);
}

static int	test_corrupt_tail_stops_replay_but_keeps_prefix(void)
{
	const char	*path;
	t_coredb	*db;
	int			ok;

	path = "/tmp/coredb_replay_corrupt_test.db";
	(void)unlink(path);
	coredb_open(&db, path);
	append_user_create(db, 1, "alice");
	append_user_create(db, 2, "carol");
	fwrite("XX", 1, 2, db->file);
	fflush(db->file);
	coredb_close(db);
	ok = assert_true(coredb_open(&db, path) == EXIT_SUCCESS,
		"open still succeeds despite a corrupt trailing record");
	if (assert_true(coredb_hash_find_by_username(db->table, "alice") != NULL
			&& coredb_hash_find_by_username(db->table, "carol") != NULL,
			"records before the corrupt tail are still replayed")
		!= EXIT_SUCCESS)
		ok = EXIT_FAILURE;
	coredb_close(db);
	unlink(path);
	return (ok);
}

int	main(void)
{
	if (test_user_create_persists_across_reopen() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_points_grant_equip_score_persist() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	if (test_corrupt_tail_stops_replay_but_keeps_prefix() != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
