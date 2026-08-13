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
static size_t	recover_and_release(t_dblog *log, const char *expect);

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

// A row whose username the wire cannot carry is dropped rather than recovered.
// It can only predate db_signup's charset check, and one of them is enough to
// have the leaderboard body rejected as malformed for every player at once, so
// leaving it in the index would keep the bug alive across a restart.
//
// Its id is still counted: the row is gone, but handing its number to the next
// account would be a second bug stacked on the first.
void	test_a_name_the_wire_cannot_carry_is_not_recovered(void)
{
	t_dblog		*log;
	t_hashmap	*hm;
	t_skiplist	*sl;
	t_player_id	next_id;
	t_player	p;

	log = fresh_log();
	p = sample_player("amber", 1, 100);
	assert(log_append(log, &p) == DB_OK);
	p = sample_player("amber lee", 9, 999);
	assert(log_append(log, &p) == DB_OK);
	hm = hashmap_create(DB_HASH_BUCKETS);
	sl = skiplist_create();
	assert(recovery_run(log, hm, sl, &next_id) == DB_OK);
	assert(hm->size == 1 && sl->size == 1);
	assert(hashmap_get(hm, "amber") != NULL);
	assert(hashmap_get(hm, "amber lee") == NULL);
	// The dropped row's id is spent, not reissued to whoever signs up next.
	assert(next_id == 10);
	skiplist_destroy(sl);
	hashmap_destroy(hm);
	log_close(log);
	printf("PASS test_a_name_the_wire_cannot_carry_is_not_recovered\n");
}

// Recovery cuts a torn tail off instead of leaving it in the file. The log is
// O_APPEND, so the next write cannot overwrite the tail — it lands after it, and
// the boot after that meets the garbage mid-file, where a short read is
// corruption rather than EOF. That failed db_open for good, and the only way
// back was deleting everybody's accounts.
void	test_a_torn_tail_is_cut_off_rather_than_buried(void)
{
	t_dblog		*log;
	t_player	p;

	log = fresh_log();
	p = sample_player("alice", 1, 100);
	assert(log_append(log, &p) == DB_OK);
	// A partial header behind a whole record: the torn last write of a crash.
	assert(write(log->fd, "\0\0\0\0\0", 5) == 5);
	assert(recover_and_release(log, "alice") == 1);
	p = sample_player("bob", 2, 200);
	assert(log_append(log, &p) == DB_OK);
	log_close(log);
	// The second boot: the append above has to be readable, which it only is if
	// it landed on the frame boundary rather than behind the five junk bytes.
	log = log_open(TEST_DIR);
	assert(log != NULL);
	assert(recover_and_release(log, "bob") == 2);
	log_close(log);
	printf("PASS test_a_torn_tail_is_cut_off_rather_than_buried\n");
}

int	main(void)
{
	test_lww_latest_wins();
	test_seeds_skiplist_and_next_id();
	test_empty_log_starts_fresh();
	test_a_name_the_wire_cannot_carry_is_not_recovered();
	test_a_torn_tail_is_cut_off_rather_than_buried();
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

/**
 * @brief Recover into throwaway indexes, check one name, free them, report size.
 *
 * Lets a case boot the same log twice without carrying two sets of indexes in
 * its own frame. The log handle is left open for the caller.
 *
 * @param log The open log handle to recover from.
 * @param expect A username that must survive the recovery, or NULL for none.
 * @return The number of players recovered into the map.
 */
static size_t	recover_and_release(t_dblog *log, const char *expect)
{
	t_hashmap	*hm;
	t_skiplist	*sl;
	t_player_id	next_id;
	size_t		size;

	hm = hashmap_create(DB_HASH_BUCKETS);
	sl = skiplist_create();
	assert(recovery_run(log, hm, sl, &next_id) == DB_OK);
	size = hm->size;
	assert(!expect || hashmap_get(hm, expect) != NULL);
	skiplist_destroy(sl);
	hashmap_destroy(hm);
	return (size);
}
