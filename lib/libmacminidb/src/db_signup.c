/* ************************************************************************** */
/*                                                                            */
/*   db_signup.c — create a new player (§3 write, write lock)                  */
/*                                                                            */
/*   Allocates the next id, builds a fresh player document (starter character  */
/*   and theme owned + equipped), indexes it in the hash map and the           */
/*   leaderboard, and appends it to the log. The whole op runs under the write */
/*   lock; the log append is a page-cache write, so no blocking syscall is     */
/*   held under the lock (CLAUDE.md).                                          */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static t_player	*make_player(t_db *db, const char *username, const char *pw, const char *salt);

/**
 * @brief Create a new player under a unique username.
 *
 * Rejects an empty/oversized username (DB_INVALID) or one already taken
 * (DB_EXISTS), else builds the player, indexes it in both structures, persists
 * it, and returns its freshly allocated id. On a persist failure the in-memory
 * insert stands (it will be re-logged on the next write) but the id is still
 * reported so the caller sees the live player.
 *
 * @param db The handle.
 * @param username Desired unique username.
 * @param password_hashed The pre-hashed password to store.
 * @param salt The salt that was used, stored alongside.
 * @param out_id Receives the new player's id on success.
 * @return DB_OK, DB_INVALID, DB_EXISTS, or DB_IO_ERROR.
 */
t_db_result	db_signup(t_db *db, const char *username, const char *password_hashed, const char *salt, t_player_id *out_id)
{
	t_player	*p;
	size_t		len;

	if (!db || !username || !password_hashed || !salt || !out_id)
		return (DB_INVALID);
	len = strnlen(username, DB_MAX_USERNAME);
	if (len == 0 || len >= DB_MAX_USERNAME)
		return (DB_INVALID);
	pthread_rwlock_wrlock(&db->lock);
	if (hashmap_get(db->players, username))
		return (pthread_rwlock_unlock(&db->lock), DB_EXISTS);
	p = make_player(db, username, password_hashed, salt);
	if (!p)
		return (pthread_rwlock_unlock(&db->lock), DB_IO_ERROR);
	hashmap_put(db->players, p);
	skiplist_insert(db->board, p);
	*out_id = p->player_id;
	db_persist(db, p);
	pthread_rwlock_unlock(&db->lock);
	return (DB_OK);
}

/**
 * @brief Allocate and initialise a new player document.
 *
 * Assigns the next monotonic id, copies the credentials at their full field
 * width, and grants the free starter character and theme (id 1 each), owned and
 * equipped. The score and wallet start at zero.
 *
 * @param db The handle (its next_id allocator is advanced).
 * @param username The validated username.
 * @param pw The pre-hashed password.
 * @param salt The salt.
 * @return The new player, or NULL on allocation failure.
 */
static t_player	*make_player(t_db *db, const char *username, const char *pw, const char *salt)
{
	t_player	*p;

	p = calloc(1, sizeof(*p));
	if (!p)
		return (NULL);
	p->player_id = db->next_id++;
	memcpy(p->username, username, strnlen(username, DB_MAX_USERNAME - 1));
	memcpy(p->password_hashed, pw, DB_HASH_LEN);
	memcpy(p->salt, salt, DB_SALT_LEN);
	p->owned_characters[0] = 1;
	p->owned_characters_count = 1;
	p->current_equipped_character = 1;
	p->owned_themes[0] = 1;
	p->owned_themes_count = 1;
	p->current_equipped_theme = 1;
	return (p);
}
