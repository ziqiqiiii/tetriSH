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

typedef uint64_t	t_player_id;
typedef uint32_t	t_item_id;

typedef struct s_player
{
	t_player_id	player_id;
	char		username[DB_MAX_USERNAME];
	char		password_hashed[DB_HASH_LEN];
	char		salt[DB_SALT_LEN];
	int64_t		leaderboard_score;
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
t_db_result			db_buy_character(t_db *db, t_player_id id, t_item_id cid);
t_db_result			db_buy_theme(t_db *db, t_player_id id, t_item_id tid);
t_db_result			db_equip_character(t_db *db, t_player_id id, t_item_id cid);
t_db_result			db_equip_theme(t_db *db, t_player_id id, t_item_id tid);
t_db_result			db_record_game(t_db *db, t_player_id id, int64_t score_delta, int64_t points_delta, bool won);
t_db_result			db_login(t_db *db, const char *username, const char *password_hashed, t_player *out);
t_db_result			db_get_player(t_db *db, t_player_id id, t_player *out);
bool				db_player_owns_character(t_db *db, t_player_id id, t_item_id cid);
bool				db_player_owns_theme(t_db *db, t_player_id id, t_item_id tid);
t_db_result			db_leaderboard(t_db *db, t_rank_entry *out, size_t cap, size_t *out_count);
t_db_result			db_rank(t_db *db, t_player_id id, size_t *out_rank);
const t_character	*db_get_character(t_db *db, t_item_id cid);
const t_theme		*db_get_theme(t_db *db, t_item_id tid);

#endif
