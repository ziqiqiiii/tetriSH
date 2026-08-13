// Unit tests for flusher.c — the 1s background fsync thread (§7.3). The flusher
// API is internal, so this suite pulls in internal.h directly. fsync durability
// can't be observed from userspace without crash injection, so these cases
// assert the lifecycle contract instead: the thread starts, ticks, and stops
// cleanly (joins without hanging), and the data written under it stays readable.
#include "internal.h"
#include <assert.h>

#define TEST_DIR	"tests/bin"

// Static Functions
static t_player	sample_player(const char *name, t_player_id id, int64_t score);
static t_dblog	*fresh_log(void);
static size_t	replay_count(t_dblog *log);

// A NULL log yields no flusher, and a NULL flusher_stop is a safe no-op.
void	test_null_inputs_are_safe(void)
{
	assert(flusher_start(NULL) == NULL);
	flusher_stop(NULL);
	printf("PASS test_null_inputs_are_safe\n");
}

// start -> stop with no writes joins cleanly and frees without hanging.
void	test_start_stop_no_writes(void)
{
	t_dblog		*log;
	t_flusher	*f;

	log = fresh_log();
	f = flusher_start(log);
	assert(f != NULL);
	flusher_stop(f);
	log_close(log);
	printf("PASS test_start_stop_no_writes\n");
}

// A record appended while the flusher runs survives the stop barrier and is
// replayable afterwards (flusher_stop performs the final fsync, then we re-read).
void	test_record_durable_after_stop(void)
{
	t_dblog		*log;
	t_flusher	*f;
	t_player	p;

	log = fresh_log();
	f = flusher_start(log);
	assert(f != NULL);
	p = sample_player("amber", 1, 42);
	assert(log_append(log, &p) == DB_OK);
	flusher_stop(f);
	assert(replay_count(log) == 1);
	log_close(log);
	printf("PASS test_record_durable_after_stop\n");
}

// The worker keeps ticking: across a span longer than the flush interval the
// thread stays alive (no early exit, no deadlock) and stops cleanly after.
void	test_survives_a_full_tick(void)
{
	t_dblog		*log;
	t_flusher	*f;
	t_player	p;

	log = fresh_log();
	f = flusher_start(log);
	assert(f != NULL);
	p = sample_player("zoe", 2, 7);
	assert(log_append(log, &p) == DB_OK);
	usleep((DB_FLUSH_INTERVAL_S * 1000 * 1000) + 200000);
	flusher_stop(f);
	assert(replay_count(log) == 1);
	log_close(log);
	printf("PASS test_survives_a_full_tick\n");
}

int	main(void)
{
	test_null_inputs_are_safe();
	test_start_stop_no_writes();
	test_record_durable_after_stop();
	test_survives_a_full_tick();
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

// Replay sink that just tallies the records on disk.
static void	count_cb(const t_player *p, void *ctx)
{
	(void)p;
	(*(size_t *)ctx)++;
}

static size_t	replay_count(t_dblog *log)
{
	size_t	n;

	n = 0;
	assert(log_replay(log, count_cb, &n) == DB_OK);
	return (n);
}
