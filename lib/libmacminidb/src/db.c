#include "internal.h"

/*
** db.c — lifecycle + public API (operations 3, rwlock 7.5).
**
** The only file external code (tetrisd) calls. Every public db_* function
** acquires this handle's rwlock (read lock for reads, write lock for writes),
** drives the internal modules, and never holds the lock across a blocking
** syscall (CLAUDE.md). db_record_game is the only write that touches the skip
** list.
*/

/**
 * @brief Opens the store and brings the in-memory state online.
 *
 * Loads the character/theme catalogues read-only from config_dir, replays
 * <data_dir>/players.log front->back (LWW) to rebuild the hash map, builds the
 * skip list from it, then starts the flusher thread. Paths come from
 * .tetrishrc — never hard-coded here.
 *
 * @param data_dir Directory holding the append-only player log.
 * @param config_dir Directory holding the read-only character/theme config.
 * @param out Receives the opened handle on success.
 * @return DB_OK on success, DB_IO_ERROR on a log/catalogue failure,
 *         DB_INVALID on a NULL argument.
 */
t_db_result	db_open(const char *data_dir, const char *config_dir, t_db **out)
{
	(void)data_dir;
	(void)config_dir;
	(void)out;
	return (DB_OK);
}

/**
 * @brief Stops the flusher, fsyncs, and frees the handle.
 *
 * Safe to call with a NULL handle.
 *
 * @param db The handle to close (may be NULL).
 */
void	db_close(t_db *db)
{
	(void)db;
}

/*
** TODO: writes  — db_signup, db_buy_*, db_equip_*, db_record_game (write lock)
** TODO: reads   — db_login, db_get_player, db_player_owns_*,
**                 db_leaderboard, db_rank (read lock)
** TODO: catalogue passthrough — db_get_character, db_get_theme
*/
