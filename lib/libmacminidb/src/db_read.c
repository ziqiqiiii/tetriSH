/* ************************************************************************** */
/*                                                                            */
/*   db_read.c — credential + profile reads (§3 reads, read lock)             */
/*                                                                            */
/*   login, get_player, and the two ownership probes. Each takes the read      */
/*   lock, reads the in-memory index, and copies out — no log I/O on the read  */
/*   path. login matches the stored password hash; the hashing itself is the   */
/*   caller's job (common.c), this only compares the precomputed hashes.       */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Verify a username/password-hash pair and return the player.
 *
 * Looks the username up under the read lock and compares the stored hash to the
 * supplied one. A missing user and a hash mismatch both report DB_BAD_CREDS, so
 * the caller cannot distinguish them (no username enumeration). On success the
 * player document is copied into out.
 *
 * @param db The handle.
 * @param username The login username.
 * @param password_hashed The precomputed password hash to match.
 * @param out Receives the player document on success.
 * @return DB_OK, DB_INVALID, or DB_BAD_CREDS.
 */
t_db_result	db_login(t_db *db, const char *username, const char *password_hashed, t_player *out)
{
	t_player	*p;
	t_db_result	r;

	if (!db || !username || !password_hashed || !out)
		return (DB_INVALID);
	pthread_rwlock_rdlock(&db->lock);
	p = hashmap_get(db->players, username);
	if (!p || memcmp(p->password_hashed, password_hashed, DB_HASH_LEN) != 0)
		r = DB_BAD_CREDS;
	else
	{
		*out = *p;
		r = DB_OK;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}

/**
 * @brief Copy a player's document out by id.
 *
 * Read-locked snapshot for the profile / loadout page. A missing id reports
 * DB_NOT_FOUND and leaves out untouched.
 *
 * @param db The handle.
 * @param id The player's id.
 * @param out Receives the player document on success.
 * @return DB_OK, DB_INVALID, or DB_NOT_FOUND.
 */
t_db_result	db_get_player(t_db *db, t_player_id id, t_player *out)
{
	t_player	*p;
	t_db_result	r;

	if (!db || !out)
		return (DB_INVALID);
	pthread_rwlock_rdlock(&db->lock);
	p = db_find_by_id(db, id);
	if (!p)
		r = DB_NOT_FOUND;
	else
	{
		*out = *p;
		r = DB_OK;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}

/**
 * @brief Copy a player's stored salt out by username.
 *
 * Login hashes the entered password with the account's own salt before calling
 * db_login, but at that point no player id exists yet — hence the username key.
 * Read-locked; copies exactly DB_SALT_LEN bytes, which are not NUL-terminated
 * when the salt fills the field. A cap below DB_SALT_LEN is rejected rather
 * than truncated: a short salt silently produces a wrong hash and an
 * unexplainable 401.
 *
 * An unknown username reports DB_NOT_FOUND. That is deliberately distinguish-
 * able here (see macminidb.h); hiding it is the authentication caller's job.
 *
 * @param db The handle.
 * @param username The login username.
 * @param out_salt Receives DB_SALT_LEN bytes of salt on success.
 * @param cap Size of out_salt; must be at least DB_SALT_LEN.
 * @return DB_OK, DB_INVALID, or DB_NOT_FOUND.
 */
t_db_result	db_get_salt(t_db *db, const char *username, char *out_salt, size_t cap)
{
	t_player	*p;
	t_db_result	r;

	if (!db || !username || !out_salt || cap < DB_SALT_LEN)
		return (DB_INVALID);
	pthread_rwlock_rdlock(&db->lock);
	p = hashmap_get(db->players, username);
	if (!p)
		r = DB_NOT_FOUND;
	else
	{
		memcpy(out_salt, p->salt, DB_SALT_LEN);
		r = DB_OK;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}

/**
 * @brief Report whether a player owns a given character.
 *
 * Read-locked membership probe (serves does_player_own_*). An unknown player
 * reports DB_FALSE; only a NULL handle is undeterminable.
 *
 * @param db The handle (may be NULL).
 * @param id The player's id.
 * @param cid The character id to test.
 * @return DB_TRUE if the player exists and owns the character, DB_FALSE if not,
 *         DB_UNKNOWN if the handle is NULL.
 */
t_db_bool	db_player_owns_character(t_db *db, t_player_id id, t_item_id cid)
{
	t_player	*p;
	t_db_bool	owns;

	if (!db)
		return (DB_UNKNOWN);
	pthread_rwlock_rdlock(&db->lock);
	p = db_find_by_id(db, id);
	if (p && owned_has(p->owned_characters, p->owned_characters_count, cid))
		owns = DB_TRUE;
	else
		owns = DB_FALSE;
	pthread_rwlock_unlock(&db->lock);
	return (owns);
}

/**
 * @brief Report whether a player owns a given theme.
 *
 * Read-locked membership probe, as for characters.
 *
 * @param db The handle (may be NULL).
 * @param id The player's id.
 * @param tid The theme id to test.
 * @return DB_TRUE if the player exists and owns the theme, DB_FALSE if not,
 *         DB_UNKNOWN if the handle is NULL.
 */
t_db_bool	db_player_owns_theme(t_db *db, t_player_id id, t_item_id tid)
{
	t_player	*p;
	t_db_bool	owns;

	if (!db)
		return (DB_UNKNOWN);
	pthread_rwlock_rdlock(&db->lock);
	p = db_find_by_id(db, id);
	if (p && owned_has(p->owned_themes, p->owned_themes_count, tid))
		owns = DB_TRUE;
	else
		owns = DB_FALSE;
	pthread_rwlock_unlock(&db->lock);
	return (owns);
}
