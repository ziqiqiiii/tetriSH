#ifndef COREDB_H
# define COREDB_H

# include <stdint.h>

# define COREDB_USERNAME_MAX 32
# define COREDB_PASSWORD_SALT_SIZE 16
# define COREDB_PASSWORD_HASH_SIZE 32

/* Opaque handle keeps disk/hash internals private to libcoredb. */
typedef struct s_coredb	 t_coredb;

typedef struct s_coredb_user
{
	uint32_t	player_id;
	char		username[COREDB_USERNAME_MAX];
	uint8_t		password_salt[COREDB_PASSWORD_SALT_SIZE];
	uint8_t		password_hash[COREDB_PASSWORD_HASH_SIZE];
	uint32_t	password_iters;
	uint32_t	points;
	uint64_t	owned_characters;
	uint64_t	owned_themes;
	uint32_t	equipped_theme;
	uint32_t	high_score;
} 	t_coredb_user;

int	coredb_open(t_coredb **out_db, const char *path);
int	coredb_close(t_coredb *db);

int	coredb_create_user(t_coredb *db, const char *username,
		const char *password, uint32_t *out_player_id);
int	coredb_verify_user(t_coredb *db, const char *username,
		const char *password, uint32_t *out_player_id);
int	coredb_credit_points(t_coredb *db, uint32_t player_id,
		int32_t delta, uint32_t reason);
int	coredb_get_balance(t_coredb *db, uint32_t player_id,
		uint32_t *out_balance);
int	coredb_grant_character(t_coredb *db, uint32_t player_id,
		uint32_t character_id);
int	coredb_grant_theme(t_coredb *db, uint32_t player_id,
		uint32_t theme_id);
int	coredb_equip_theme(t_coredb *db, uint32_t player_id,
		uint32_t theme_id);
int	coredb_set_high_score(t_coredb *db, uint32_t player_id, uint32_t score);
int	coredb_get_user(t_coredb *db, uint32_t player_id,
		t_coredb_user *out_user);

#endif
