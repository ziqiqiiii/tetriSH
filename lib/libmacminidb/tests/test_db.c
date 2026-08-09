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

// The two equips write two different fields. db_equip_theme used to set
// current_equipped_character, so choosing a theme silently swapped the
// player's character - and the character is what decides which Gaiden
// abilities they have, so a cosmetic choice changed how the game played.
// Nothing caught it because no test had ever equipped a theme.
void	test_equipping_a_theme_leaves_the_character_alone(void)
{
	t_db		*db;
	t_player_id	id;
	t_player	out;

	db = fresh_db();
	assert(db_signup(db, "zoe", "h", "s", &id) == DB_OK);
	assert(db_record_game(db, id, 0, 800, false) == DB_OK);
	assert(db_buy_character(db, id, 2) == DB_OK);
	assert(db_equip_character(db, id, 2) == DB_OK);
	assert(db_buy_theme(db, id, 3) == DB_OK);
	assert(db_equip_theme(db, id, 3) == DB_OK);
	assert(db_get_player(db, id, &out) == DB_OK);
	assert(out.current_equipped_theme == 3);
	assert(out.current_equipped_character == 2);
	// And the reverse: equipping a character leaves the theme alone.
	assert(db_equip_character(db, id, 1) == DB_OK);
	assert(db_get_player(db, id, &out) == DB_OK);
	assert(out.current_equipped_character == 1);
	assert(out.current_equipped_theme == 3);
	assert(db_equip_theme(db, id, 4) == DB_NOT_OWNED);
	db_close(db);
	printf("PASS test_equipping_a_theme_leaves_the_character_alone\n");
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

// leaderboard_score is a personal best, not a running total. It used to add
// every game to it, which ranked whoever played most rather than whoever
// played best: a player grinding 100-point games passed one who had scored
// 5,000 once, and no amount of skill could catch up with somebody who simply
// kept playing. lifetime_points is where the running total went, because the
// wallet still needs one.
void	test_the_board_ranks_your_best_game_not_your_total(void)
{
	t_db			*db;
	t_player_id		grinder;
	t_player_id		ace;
	t_rank_entry	top[8];
	t_player		out;
	size_t			n;
	size_t			rank;
	int				i;

	db = fresh_db();
	assert(db_signup(db, "grinder", "h", "s", &grinder) == DB_OK);
	assert(db_signup(db, "ace", "h", "s", &ace) == DB_OK);
	i = 0;
	while (i < 20)
	{
		assert(db_record_game(db, grinder, 100, 0, false) == DB_OK);
		i++;
	}
	assert(db_record_game(db, ace, 500, 0, true) == DB_OK);
	// 20 x 100 = 2000 scored in total, but the best of them is still 100.
	assert(db_get_player(db, grinder, &out) == DB_OK);
	assert(out.leaderboard_score == 100);
	assert(out.lifetime_points == 2000);
	assert(out.games_played == 20);
	assert(db_leaderboard(db, top, 8, &n) == DB_OK && n == 2);
	assert(strcmp(top[0].username, "ace") == 0);
	assert(db_rank(db, ace, &rank) == DB_OK && rank == 1);
	// A worse game afterwards cannot take the record away.
	assert(db_record_game(db, ace, 10, 0, false) == DB_OK);
	assert(db_get_player(db, ace, &out) == DB_OK);
	assert(out.leaderboard_score == 500);
	assert(out.lifetime_points == 510);
	// A better one replaces it outright rather than adding to it.
	assert(db_record_game(db, ace, 800, 0, true) == DB_OK);
	assert(db_get_player(db, ace, &out) == DB_OK);
	assert(out.leaderboard_score == 800);
	assert(out.lifetime_points == 1310);
	db_close(db);
	printf("PASS test_the_board_ranks_your_best_game_not_your_total\n");
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

// The whole roster comes out in one read, gaps in the numbering included: a
// caller that probed ids until one came back NULL would stop at the cut theme
// (id 5) and never see the three after it.
void	test_catalogue_enumeration(void)
{
	t_db		*db;
	t_character	chars[8];
	t_theme		themes[8];
	t_theme		tight[2];
	size_t		count;

	db = fresh_db();
	count = 0;
	assert(db_characters(db, chars, 8, &count) == DB_OK);
	assert(count == 4);
	assert(strcmp(chars[0].name, "Halloween") == 0);
	assert(chars[0].cost_points == 10);
	count = 0;
	assert(db_themes(db, themes, 8, &count) == DB_OK);
	assert(count == 7);
	assert(themes[0].theme_id == 1 && themes[0].cost_points == 0);
	// The row after Haaland is id 6, not id 5: enumeration is positional in
	// the array but not in the ids.
	assert(themes[3].theme_id == 4);
	assert(themes[4].theme_id == 6);
	// A buffer too small for the roster is refused whole; a truncated
	// catalogue would be indistinguishable from a short one.
	assert(db_themes(db, tight, 2, &count) == DB_FULL);
	assert(db_characters(NULL, chars, 8, &count) == DB_INVALID);
	assert(db_themes(db, themes, 8, NULL) == DB_INVALID);
	db_close(db);
	printf("PASS test_catalogue_enumeration\n");
}

int	main(void)
{
	test_signup_and_login();
	test_get_salt();
	test_buy_and_equip();
	test_equipping_a_theme_leaves_the_character_alone();
	test_leaderboard_and_rank();
	test_the_board_ranks_your_best_game_not_your_total();
	test_durability_roundtrip();
	test_catalogue_passthrough();
	test_catalogue_enumeration();
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
