// Unit tests for log.c / log_append.c / log_replay.c — append + replay
// round-trip. The log API is internal, so this suite pulls in internal.h
// directly. Each case opens a fresh temp log under tests/bin/ so runs do not
// interfere, and unlinks it on the way out.
#include "internal.h"
#include <assert.h>

#define TEST_DIR	"tests/bin"

// Static Functions
static t_player	sample_player(const char *name, t_player_id id, int64_t score);
static void		collect_cb(const t_player *p, void *ctx);
static t_dblog	*fresh_log(void);

// Replay sink: records are appended in arrival order.
typedef struct s_collected
{
	t_player	players[16];
	size_t		count;
}	t_collected;

void	test_append_replay_preserves_order(void)
{
	t_dblog		*log;
	t_collected	got;
	t_player	p;

	log = fresh_log();
	p = sample_player("zoe", 0, 10);
	assert(log_append(log, &p) == DB_OK);
	p = sample_player("alice", 1, 100);
	assert(log_append(log, &p) == DB_OK);
	p = sample_player("bob", 2, 200);
	assert(log_append(log, &p) == DB_OK);
	got.count = 0;
	assert(log_replay(log, collect_cb, &got, NULL) == DB_OK);
	assert(got.count == 3);
	assert(strcmp(got.players[0].username, "zoe") == 0);
	assert(got.players[1].player_id == 1);
	assert(got.players[2].leaderboard_score == 200);
	assert(strcmp(got.players[2].username, "bob") == 0);
	log_close(log);
	printf("PASS test_append_replay_preserves_order\n");
}

void	test_replay_roundtrips_full_record(void)
{
	t_dblog		*log;
	t_collected	got;
	t_player	in;

	log = fresh_log();
	in = sample_player("carol", 42, -7);
	in.owned_characters_count = 2;
	in.owned_characters[0] = 11;
	in.owned_characters[1] = 22;
	in.games_won = 5;
	assert(log_append(log, &in) == DB_OK);
	got.count = 0;
	assert(log_replay(log, collect_cb, &got, NULL) == DB_OK);
	assert(got.count == 1);
	assert(memcmp(&in, &got.players[0], sizeof(t_player)) == 0);
	log_close(log);
	printf("PASS test_replay_roundtrips_full_record\n");
}

void	test_replay_empty_log(void)
{
	t_dblog		*log;
	t_collected	got;

	log = fresh_log();
	got.count = 0;
	assert(log_replay(log, collect_cb, &got, NULL) == DB_OK);
	assert(got.count == 0);
	log_close(log);
	printf("PASS test_replay_empty_log\n");
}

void	test_replay_stops_on_torn_tail(void)
{
	t_dblog		*log;
	t_collected	got;
	t_player	dave;
	struct stat	st;
	off_t		clean;

	log = fresh_log();
	dave = sample_player("dave", 9, 50);
	assert(log_append(log, &dave) == DB_OK);
	// A partial header written after a complete record: a torn last write.
	assert(write(log->fd, "\0\0\0\0\0", 5) == 5);
	got.count = 0;
	clean = -1;
	assert(log_replay(log, collect_cb, &got, &clean) == DB_OK);
	assert(got.count == 1);
	assert(got.players[0].player_id == 9);
	// The reported end is the frame boundary, so the caller can cut the 5 bytes
	// of tail off rather than let the next O_APPEND bury a valid frame behind it.
	assert(fstat(log->fd, &st) == 0);
	assert(clean == st.st_size - 5);
	log_close(log);
	printf("PASS test_replay_stops_on_torn_tail\n");
}

// A log with no torn tail reports its own size, so the caller's truncate is a
// no-op on the ordinary boot rather than something it has to special-case.
void	test_replay_reports_the_end_of_a_clean_log(void)
{
	t_dblog		*log;
	t_collected	got;
	t_player	eve;
	struct stat	st;
	off_t		clean;

	log = fresh_log();
	eve = sample_player("eve", 4, 20);
	assert(log_append(log, &eve) == DB_OK);
	got.count = 0;
	clean = -1;
	assert(log_replay(log, collect_cb, &got, &clean) == DB_OK);
	assert(fstat(log->fd, &st) == 0);
	assert(clean == st.st_size);
	assert(log_truncate_tail(log, clean) == DB_OK);
	assert(fstat(log->fd, &st) == 0 && st.st_size == clean);
	log_close(log);
	printf("PASS test_replay_reports_the_end_of_a_clean_log\n");
}

int	main(void)
{
	test_append_replay_preserves_order();
	test_replay_roundtrips_full_record();
	test_replay_empty_log();
	test_replay_stops_on_torn_tail();
	test_replay_reports_the_end_of_a_clean_log();
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

static void	collect_cb(const t_player *p, void *ctx)
{
	t_collected	*c;

	c = ctx;
	if (c->count < sizeof(c->players) / sizeof(c->players[0]))
		c->players[c->count++] = *p;
}

static t_dblog	*fresh_log(void)
{
	t_dblog	*log;

	unlink(TEST_DIR "/" DB_LOG_NAME);
	log = log_open(TEST_DIR);
	assert(log != NULL);
	return (log);
}
