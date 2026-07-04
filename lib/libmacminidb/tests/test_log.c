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
	assert(log_replay(log, collect_cb, &got) == DB_OK);
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
	assert(log_replay(log, collect_cb, &got) == DB_OK);
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
	assert(log_replay(log, collect_cb, &got) == DB_OK);
	assert(got.count == 0);
	log_close(log);
	printf("PASS test_replay_empty_log\n");
}

void	test_replay_stops_on_torn_tail(void)
{
	t_dblog		*log;
	t_collected	got;
	t_player	dave;
	uint8_t		junk[5];

	log = fresh_log();
	dave = sample_player("dave", 9, 50);
	assert(log_append(log, &dave) == DB_OK);
	// A partial header written after a complete record: a torn last write.
	memset(junk, 0, sizeof(junk));
	assert(write(log->fd, junk, sizeof(junk)) == (ssize_t)sizeof(junk));
	got.count = 0;
	assert(log_replay(log, collect_cb, &got) == DB_OK);
	assert(got.count == 1);
	assert(got.players[0].player_id == 9);
	log_close(log);
	printf("PASS test_replay_stops_on_torn_tail\n");
}

int	main(void)
{
	test_append_replay_preserves_order();
	test_replay_roundtrips_full_record();
	test_replay_empty_log();
	test_replay_stops_on_torn_tail();
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
