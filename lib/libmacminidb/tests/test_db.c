/* ************************************************************************** */
/*                                                                            */
/*   test_db.c — end-to-end: signup -> buy -> equip -> record_game -> rank    */
/*                                                                            */
/*   Exercises the public API only (macminidb.h), against a fresh temp data    */
/*   dir under tests/bin/ and the real config/ catalogue. The headline case    */
/*   is the durability round-trip: write, close, reopen, and assert the state  */
/*   was recovered from the log.                                               */
/*                                                                            */
/* ************************************************************************** */

#include "macminidb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DATA_DIR	"tests/bin"
#define CFG_DIR		"config"
#define LOG_PATH	DATA_DIR "/players.log"

// Static Functions
static t_db	*fresh_db(void);

// signup creates a player; a duplicate username is rejected; bad creds fail.
void	test_signup_and_login(void)
{
	t_db		*db;
	t_player_id	id;
	t_player	out;

	db = fresh_db();
	assert(db_signup(db, "amber", "hashAAA", "saltAAA", &id) == DB_OK);
	assert(db_signup(db, "amber", "hashAAA", "saltAAA", &id) == DB_EXISTS);
	assert(db_login(db, "amber", "hashAAA", &out) == DB_OK);
	assert(out.player_id == id);
	assert(db_login(db, "amber", "wrong", &out) == DB_BAD_CREDS);
	assert(db_login(db, "ghost", "hashAAA", &out) == DB_BAD_CREDS);
	db_close(db);
	printf("PASS test_signup_and_login\n");
}

// get_salt returns the salt stored at signup so a caller can hash a login
// attempt; it rejects an undersized buffer rather than truncating, and reports
// an unknown username distinguishably (hiding that is the caller's job).
void	test_get_salt(void)
{
	t_db		*db;
	t_player_id	id;
	char		salt[DB_SALT_LEN];
	char		again[DB_SALT_LEN];
	char		small[DB_SALT_LEN - 1];

	db = fresh_db();
	assert(db_signup(db, "amber", "hashAAA", "saltAAA", &id) == DB_OK);
	assert(db_get_salt(db, "amber", salt, sizeof(salt)) == DB_OK);
	assert(memcmp(salt, "saltAAA", strlen("saltAAA")) == 0);
	// Unknown user is reported, not folded into a credentials failure.
	assert(db_get_salt(db, "ghost", salt, sizeof(salt)) == DB_NOT_FOUND);
	// A short buffer would yield a wrong hash and an unexplainable 401.
	assert(db_get_salt(db, "amber", small, sizeof(small)) == DB_INVALID);
	assert(db_get_salt(NULL, "amber", salt, sizeof(salt)) == DB_INVALID);
	assert(db_get_salt(db, NULL, salt, sizeof(salt)) == DB_INVALID);
	assert(db_get_salt(db, "amber", NULL, sizeof(salt)) == DB_INVALID);
	// The read leaves the record intact: same salt, login still works.
	assert(db_get_salt(db, "amber", again, sizeof(again)) == DB_OK);
	assert(memcmp(salt, again, DB_SALT_LEN) == 0);
	db_close(db);
	printf("PASS test_get_salt\n");
}

// buy charges the wallet, rejects re-purchase / insufficient funds, and equip
// requires ownership.
void	test_buy_and_equip(void)
{
	t_db		*db;
	t_player_id	id;

	db = fresh_db();
	assert(db_signup(db, "zoe", "h", "s", &id) == DB_OK);
	// Character 2 (Mirurun) costs 10; fresh wallet is 0.
	assert(db_buy_character(db, id, 2) == DB_INSUFFICIENT);
	assert(db_record_game(db, id, 0, 800, false) == DB_OK);
	assert(db_buy_character(db, id, 2) == DB_OK);
	assert(db_buy_character(db, id, 2) == DB_EXISTS);
	assert(db_equip_character(db, id, 2) == DB_OK);
	assert(db_equip_character(db, id, 3) == DB_NOT_OWNED);
	assert(db_player_owns_character(db, id, 2) == DB_TRUE);
	assert(db_player_owns_character(db, id, 3) == DB_FALSE);
	// A NULL handle is undeterminable, not a plain "does not own".
	assert(db_player_owns_character(NULL, id, 2) == DB_UNKNOWN);
	assert(db_player_owns_theme(NULL, id, 1) == DB_UNKNOWN);
	db_close(db);
	printf("PASS test_buy_and_equip\n");
}

// record_game re-sorts the leaderboard; rank and top-N reflect the scores.
void	test_leaderboard_and_rank(void)
{
	t_db		*db;
	t_player_id	a;
	t_player_id	b;
	t_player_id	c;
	t_rank_entry	top[8];
	size_t		n;
	size_t		rank;

	db = fresh_db();
	assert(db_signup(db, "alice", "h", "s", &a) == DB_OK);
	assert(db_signup(db, "bob", "h", "s", &b) == DB_OK);
	assert(db_signup(db, "carol", "h", "s", &c) == DB_OK);
	assert(db_record_game(db, a, 100, 0, true) == DB_OK);
	assert(db_record_game(db, b, 300, 0, true) == DB_OK);
	assert(db_record_game(db, c, 200, 0, true) == DB_OK);
	assert(db_leaderboard(db, top, 8, &n) == DB_OK);
	assert(n == 3);
	assert(strcmp(top[0].username, "bob") == 0 && top[0].leaderboard_score == 300);
	assert(strcmp(top[2].username, "alice") == 0);
	assert(db_rank(db, b, &rank) == DB_OK && rank == 1);
	assert(db_rank(db, a, &rank) == DB_OK && rank == 3);
	db_close(db);
	printf("PASS test_leaderboard_and_rank\n");
}

// The durability contract: state written before close is recovered on reopen,
// including the wallet, owned/equipped items, and the leaderboard score.
void	test_durability_roundtrip(void)
{
	t_db		*db;
	t_player_id	id;
	t_player	out;
	size_t		rank;

	db = fresh_db();
	assert(db_signup(db, "amber", "hashZ", "saltZ", &id) == DB_OK);
	assert(db_record_game(db, id, 250, 1000, true) == DB_OK);
	assert(db_buy_character(db, id, 2) == DB_OK);
	assert(db_equip_character(db, id, 2) == DB_OK);
	db_close(db);
	assert(db_open(DATA_DIR, CFG_DIR, &db) == DB_OK);
	assert(db_get_player(db, id, &out) == DB_OK);
	assert(out.leaderboard_score == 250);
	assert(out.wallet_points == 1000 - 10);
	assert(out.current_equipped_character == 2);
	assert(db_player_owns_character(db, id, 2) == DB_TRUE);
	assert(db_rank(db, id, &rank) == DB_OK && rank == 1);
	db_close(db);
	printf("PASS test_durability_roundtrip\n");
}

// The catalogue is reachable through the db handle.
void	test_catalogue_passthrough(void)
{
	t_db				*db;
	const t_character	*ch;
	const t_theme		*th;

	db = fresh_db();
	ch = db_get_character(db, 1);
	assert(ch != NULL && strcmp(ch->name, "Halloween") == 0);
	th = db_get_theme(db, 1);
	assert(th != NULL && strcmp(th->name, "Default") == 0);
	assert(db_get_character(db, 999) == NULL);
	db_close(db);
	printf("PASS test_catalogue_passthrough\n");
}

int	main(void)
{
	test_signup_and_login();
	test_get_salt();
	test_buy_and_equip();
	test_leaderboard_and_rank();
	test_durability_roundtrip();
	test_catalogue_passthrough();
	return (0);
}

// Open a brand-new store: unlink any prior log so each suite starts clean.
static t_db	*fresh_db(void)
{
	t_db	*db;

	unlink(LOG_PATH);
	assert(db_open(DATA_DIR, CFG_DIR, &db) == DB_OK);
	return (db);
}
