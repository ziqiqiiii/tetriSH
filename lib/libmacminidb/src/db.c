/* ************************************************************************** */
/*                                                                            */
/*   db.c — lifecycle + shared helpers (operations §3, rwlock §7.5)           */
/*                                                                            */
/*   The only file external code (tetrisd) calls. Every public db_* function   */
/*   takes this handle's rwlock — a read lock for reads, a write lock for      */
/*   writes — drives the internal modules, and persists writes through the     */
/*   append-only log. The only blocking syscall is the flusher's fdatasync,    */
/*   which runs on its own thread and never under the rwlock (CLAUDE.md).      */
/*   db_open replays the log (recovery) before the flusher is started.         */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static t_db	*db_create(const char *data_dir, const char *config_dir);
static void	find_by_id_cb(t_player *p, void *ctx);

/**
 * @brief Open the store and bring the in-memory state online.
 *
 * Builds the handle, replays <data_dir>/players.log (LWW) into the hash map and
 * skip list, loads the catalogues from config_dir, then starts the 1s flusher.
 * Any failure tears the partial handle back down and reports it. Paths come from
 * .tetrishrc — never hard-coded here.
 *
 * @param data_dir Directory holding the append-only player log.
 * @param config_dir Directory holding the read-only character/theme config.
 * @param out Receives the opened handle on success.
 * @return DB_OK on success, DB_INVALID on a NULL argument, DB_IO_ERROR else.
 */
t_db_result	db_open(const char *data_dir, const char *config_dir, t_db **out)
{
	t_db	*db;

	if (!data_dir || !config_dir || !out)
		return (DB_INVALID);
	db = db_create(data_dir, config_dir);
	if (!db)
		return (DB_IO_ERROR);
	db->flusher = flusher_start(db->log);
	if (!db->flusher)
		return (db_close(db), DB_IO_ERROR);
	*out = db;
	return (DB_OK);
}

/**
 * @brief Stop the flusher (final fsync), then free every owned resource.
 *
 * Tears down in reverse of construction and is NULL-safe at every step, so it
 * doubles as the unwind path for a half-built handle from db_open. Safe to call
 * with a NULL handle.
 *
 * @param db The handle to close (may be NULL).
 */
void	db_close(t_db *db)
{
	if (!db)
		return ;
	flusher_stop(db->flusher);
	catalogue_free(db->cat);
	skiplist_destroy(db->board);
	hashmap_destroy(db->players);
	log_close(db->log);
	pthread_rwlock_destroy(&db->lock);
	free(db);
}

/**
 * @brief Allocate the handle and bring every module online except the flusher.
 *
 * Inits the rwlock and the empty index, opens the log, replays it into the
 * index (deriving next_id), and loads the catalogue. On any failure the partial
 * handle is closed and NULL returned; the flusher is started by the caller once
 * recovery is complete so it never races the replay.
 *
 * @param data_dir Directory holding the log.
 * @param config_dir Directory holding the catalogue config.
 * @return The built (flusher-less) handle, or NULL on failure.
 */
static t_db	*db_create(const char *data_dir, const char *config_dir)
{
	t_db	*db;

	db = calloc(1, sizeof(*db));
	if (!db || pthread_rwlock_init(&db->lock, NULL) != 0)
		return (free(db), NULL);
	db->players = hashmap_create(DB_HASH_BUCKETS);
	db->board = skiplist_create();
	db->log = log_open(data_dir);
	if (!db->players || !db->board || !db->log)
		return (db_close(db), NULL);
	if (recovery_run(db->log, db->players, db->board, &db->next_id) != DB_OK)
		return (db_close(db), NULL);
	db->cat = catalogue_load(config_dir);
	if (!db->cat)
		return (db_close(db), NULL);
	return (db);
}

/**
 * @brief Find a live player by id (linear scan of the username index).
 *
 * The hash map is keyed by username, so a by-id lookup walks every entry. At
 * ~300 users this is trivial; a second index would not pay for itself. The
 * caller must hold the rwlock.
 *
 * @param db The handle to search.
 * @param id The player id to find.
 * @return The owned player pointer, or NULL if no player has that id.
 */
t_player	*db_find_by_id(t_db *db, t_player_id id)
{
	t_find_ctx	ctx;

	ctx.id = id;
	ctx.match = NULL;
	hashmap_foreach(db->players, find_by_id_cb, &ctx);
	return (ctx.match);
}

/**
 * @brief Append a player record to the log under the write lock.
 *
 * The single persistence point for writes: each mutated player is appended
 * whole (LWW). The append is a page-cache write only — durability is the
 * flusher's fdatasync — so no blocking syscall runs under the rwlock.
 *
 * @param db The handle whose log is appended to.
 * @param p The player document to persist.
 * @return DB_OK on success, DB_IO_ERROR on a log write failure.
 */
t_db_result	db_persist(t_db *db, const t_player *p)
{
	return (log_append(db->log, p));
}

/**
 * @brief hashmap_foreach sink: capture the player whose id matches ctx->id.
 *
 * Visiting every entry is fine — the map is small and foreach has no early
 * exit; a later duplicate id cannot occur since ids are unique.
 *
 * @param p The player being visited.
 * @param ctx The t_find_ctx carrying the target id and the out match.
 */
static void	find_by_id_cb(t_player *p, void *ctx)
{
	t_find_ctx	*fc;

	fc = ctx;
	if (p->player_id == fc->id)
		fc->match = p;
}
