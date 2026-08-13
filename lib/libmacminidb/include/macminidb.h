#ifndef MACMINIDB_H
# define MACMINIDB_H

# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>

# define DB_MAX_USERNAME	32
# define DB_HASH_LEN		64
# define DB_SALT_LEN		32
# define DB_MAX_OWNED		64
# define DB_THEME_DESC_LEN	128

typedef enum e_db_result
{
	DB_OK = 0,
	DB_NOT_FOUND,		/* no such player / row */
	DB_EXISTS,			/* username already taken (signup) */
	DB_BAD_CREDS,		/* login: username/password mismatch */
	DB_INSUFFICIENT,	/* buy: not enough wallet_points */
	DB_NOT_OWNED,		/* equip: player does not own that item */
	DB_IO_ERROR,		/* log append / fsync / read failed */
	DB_FULL,			/* owned list at capacity */
	DB_INVALID			/* malformed argument */
}	t_db_result;

/*
** Predicate answers. Kept separate from t_db_result: a probe that returns
** DB_FALSE succeeded — "no" is an answer, not a failure. DB_FALSE is 0 so a
** bare truth test still reads correctly.
*/
typedef enum e_db_bool
{
	DB_FALSE = 0,		/* predicate does not hold */
	DB_TRUE,			/* predicate holds */
	DB_UNKNOWN			/* could not determine (bad handle) */
}	t_db_bool;

typedef uint64_t	t_player_id;
typedef uint32_t	t_item_id;

/*
** The three running numbers are three different questions, and conflating any
** two of them is a bug that has already happened here once:
**
**   leaderboard_score  the best single game this player has ever had. It is
**                      what the board ranks on, and it only ever goes up by
**                      being beaten - never by being played more.
**   lifetime_points    every point ever scored, added up. Nothing ranks on it;
**                      it exists so the wallet's exchange rate can be charged
**                      against a running total instead of one game at a time,
**                      which is what keeps a game worth less than the rate
**                      from rounding away to nothing.
**   wallet_points      what is left to spend, after everything bought.
*/
typedef struct s_player
{
	t_player_id	player_id;
	char		username[DB_MAX_USERNAME];
	char		password_hashed[DB_HASH_LEN];
	char		salt[DB_SALT_LEN];
	int64_t		leaderboard_score;
	int64_t		lifetime_points;
	int64_t		wallet_points;
	t_item_id	current_equipped_character;
	t_item_id	current_equipped_theme;
	t_item_id	owned_characters[DB_MAX_OWNED];
	size_t		owned_characters_count;
	t_item_id	owned_themes[DB_MAX_OWNED];
	size_t		owned_themes_count;
	uint32_t	games_played;
	uint32_t	games_won;
}	t_player;

typedef struct s_character
{
	t_item_id	character_id;
	char		name[DB_MAX_USERNAME];
	uint32_t	abilities;		/* bitfield of ability ids */
	int64_t		cost_points;
}	t_character;

typedef struct s_theme
{
	t_item_id	theme_id;
	char		name[DB_MAX_USERNAME];
	int64_t		cost_points;
	char		description[DB_THEME_DESC_LEN];
}	t_theme;

typedef struct s_rank_entry
{
	t_player_id	player_id;
	char		username[DB_MAX_USERNAME];
	int64_t		leaderboard_score;
}	t_rank_entry;

typedef struct s_macminidb	t_db;

/* DB.C */

t_db_result			db_open(const char *data_dir, const char *config_dir, t_db **out);
void				db_close(t_db *db);
t_db_result			db_signup(t_db *db, const char *username, const char *password_hashed, const char *salt, t_player_id *out_id);

/*
** USERNAME.C
**
** What a username may contain, asked without a handle. db_signup applies it
** itself, so a caller never has to; it is public so a server can refuse a
** name at the edge with a reason of its own rather than reading DB_INVALID
** and guessing which field was wrong.
*/
bool				db_username_valid(const char *username);
t_db_result			db_buy_character(t_db *db, t_player_id id, t_item_id cid);
t_db_result			db_buy_theme(t_db *db, t_player_id id, t_item_id tid);
t_db_result			db_equip_character(t_db *db, t_player_id id, t_item_id cid);
t_db_result			db_equip_theme(t_db *db, t_player_id id, t_item_id tid);
t_db_result			db_record_game(t_db *db, t_player_id id, int64_t game_score, int64_t points_delta, bool won);
t_db_result			db_login(t_db *db, const char *username, const char *password_hashed, t_player *out);
t_db_result			db_get_player(t_db *db, t_player_id id, t_player *out);

/*
** Fetch a player's salt so the caller can hash a login attempt before calling
** db_login (which compares precomputed hashes only). Keyed by username because
** at login time no player id is known yet.
**
** Unlike db_login, this DOES distinguish an unknown username (DB_NOT_FOUND)
** from a successful read: the store's job is to answer accurately, and folding
** the two would leave the caller unable to tell a missing user from an I/O
** failure. That makes the distinction the CALLER's to hide — on DB_NOT_FOUND
** an authentication path must still hash against a dummy salt and return the
** same 401 on the same code path, so neither the message nor the timing
** reveals whether the username exists.
*/
t_db_result			db_get_salt(t_db *db, const char *username, char *out_salt, size_t cap);
t_db_bool			db_player_owns_character(t_db *db, t_player_id id, t_item_id cid);
t_db_bool			db_player_owns_theme(t_db *db, t_player_id id, t_item_id tid);
t_db_result			db_leaderboard(t_db *db, t_rank_entry *out, size_t cap, size_t *out_count);
t_db_result			db_rank(t_db *db, t_player_id id, size_t *out_rank);
const t_character	*db_get_character(t_db *db, t_item_id cid);
const t_theme		*db_get_theme(t_db *db, t_item_id tid);

/*
** Enumerate the whole catalogue rather than one row of it. The by-id getters
** answer "what is this item"; a store front asks "what is for sale", and could
** only reach that by probing ids until one came back NULL — which would read a
** gap in the numbering as the end of the roster. Ids are never renumbered
** (they are written into players' owned lists), so gaps are expected.
**
** Rows are copied, so the answer does not borrow from the handle. DB_FULL
** means cap could not hold the whole roster; nothing is written, because a
** truncated catalogue would be indistinguishable from a short one.
*/
t_db_result			db_characters(t_db *db, t_character *out, size_t cap, size_t *out_count);
t_db_result			db_themes(t_db *db, t_theme *out, size_t cap, size_t *out_count);

#endif
