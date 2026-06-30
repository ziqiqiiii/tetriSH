// Unit tests for recovery.c — replay the log into the hash map (LWW), seed the
// skip list from it, and derive next_id. The recovery API is internal, so this
// suite pulls in internal.h directly. Each case builds a fresh temp log under
// tests/bin/ via log_append, recovers from it, then frees the rebuilt state.
#include "internal.h"
#include <assert.h>

#define TEST_DIR	"tests/bin"

// Static Functions
static t_player	sample_player(const char *name, t_player_id id, int64_t score);
static t_dblog	*fresh_log(void);

// Latest record for a username supersedes the earlier one (LWW), and next_id is
// the highest player_id seen plus one.
void	test_lww_latest_wins(void)
{
	t_dblog		*log;
	t_hashmap	*hm;
	t_skiplist	*sl;
	t_player_id	next_id;
	t_player	p;

	log = fresh_log();
	p = sample_player("zoe", 7, 10);
	assert(log_append(log, &p) == DB_OK);
	p = sample_player("zoe", 7, 250);
	assert(log_append(log, &p) == DB_OK);
	hm = hashmap_create(DB_HASH_BUCKETS);
	sl = skiplist_create();
	assert(recovery_run(log, hm, sl, &next_id) == DB_OK);
	assert(hm->size == 1);
	assert(hashmap_get(hm, "zoe")->leaderboard_score == 250);
	assert(next_id == 8);
	skiplist_destroy(sl);
	hashmap_destroy(hm);
	log_close(log);
	printf("PASS test_lww_latest_wins\n");
}

// Distinct players each survive recovery, the skip list is seeded in
// (score desc, id asc) order, and next_id tracks the highest id across them.
void	test_seeds_skiplist_and_next_id(void)
{
	t_dblog		*log;
	t_hashmap	*hm;
	t_skiplist	*sl;
	t_player_id	next_id;
	t_rank_entry	top[4];
	t_player	p;

	log = fresh_log();
	p = sample_player("alice", 1, 100);
	assert(log_append(log, &p) == DB_OK);
	p = sample_player("bob", 5, 300);
	assert(log_append(log, &p) == DB_OK);
	p = sample_player("carol", 3, 200);
	assert(log_append(log, &p) == DB_OK);
	hm = hashmap_create(DB_HASH_BUCKETS);
	sl = skiplist_create();
	assert(recovery_run(log, hm, sl, &next_id) == DB_OK);
	assert(hm->size == 3 && sl->size == 3);
	assert(next_id == 6);
	assert(skiplist_topn(sl, top, 4) == 3);
	assert(strcmp(top[0].username, "bob") == 0);
	assert(strcmp(top[1].username, "carol") == 0);
	assert(strcmp(top[2].username, "alice") == 0);
	skiplist_destroy(sl);
	hashmap_destroy(hm);
	log_close(log);
	printf("PASS test_seeds_skiplist_and_next_id\n");
}

// An empty log recovers to an empty store, with next_id starting at 1.
void	test_empty_log_starts_fresh(void)
{
	t_dblog		*log;
	t_hashmap	*hm;
	t_skiplist	*sl;
	t_player_id	next_id;

	log = fresh_log();
	hm = hashmap_create(DB_HASH_BUCKETS);
	sl = skiplist_create();
	assert(recovery_run(log, hm, sl, &next_id) == DB_OK);
	assert(hm->size == 0 && sl->size == 0);
	assert(next_id == 1);
	skiplist_destroy(sl);
	hashmap_destroy(hm);
	log_close(log);
	printf("PASS test_empty_log_starts_fresh\n");
}

int	main(void)
{
	test_lww_latest_wins();
	test_seeds_skiplist_and_next_id();
	test_empty_log_starts_fresh();
	return (0);
}

static t_player	sample_player(const char *name, t_player_id id, int64_t score)
{
	t_player	p;

	memset(&p, 0, sizeof(p));
	p.player_id = id;
	strncpy(p.username, name, DB_MAX_USERNAME - 1);
	memset(p.password_hashed, 'h', DB_HASH_LEN);
	memset(p.salt, 's', DB_SALT_LEN);
	p.leaderboard_score = score;
	p.wallet_points = 500;
	return (p);
}

static t_dblog	*fresh_log(void)
{
	t_dblog	*log;

	unlink(TEST_DIR "/" DB_LOG_NAME);
	log = log_open(TEST_DIR);
	assert(log != NULL);
	return (log);
}
