#ifndef MACMINIDB_INTERNAL_H
# define MACMINIDB_INTERNAL_H

/*
** Private declarations for libmacminidb. NOT installed, not part of the public
** API — consumers include only <macminidb.h>. Every src .c file includes this
** header (and only this one, per code_style 10); it wires the modules — hash
** map, skip list, log, flusher, recovery, catalogue, player serialisation — to
** db.c, which is the single place that owns the rwlock.
*/

# include "macminidb.h"

# include <errno.h>
# include <pthread.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define DB_LOG_NAME		"players.log"
# define DB_LOG_MAGIC		0x4D4D4442
# define DB_HASH_BUCKETS	512
# define DB_FLUSH_INTERVAL_S	1
# define DB_FRAME_MAX		65536

typedef struct s_hashmap	t_hashmap;
typedef struct s_skiplist	t_skiplist;
typedef struct s_dblog		t_dblog;
typedef struct s_flusher	t_flusher;
typedef struct s_catalogue	t_catalogue;

typedef struct s_macminidb
{
	pthread_rwlock_t	lock;		/* read-write lock, held in db.c only */
	t_hashmap			*players;	/* username -> t_player* */
	t_skiplist			*board;		/* leaderboard, ordered by (score, id) */
	t_dblog				*log;		/* append-only durability */
	t_flusher			*flusher;	/* 1s fsync thread */
	t_catalogue			*cat;		/* characters + themes */
	t_player_id			next_id;	/* monotonic id allocator */
}	t_macminidb;

/* HASHMAP.C */

t_hashmap			*hashmap_create(size_t buckets);
void				hashmap_destroy(t_hashmap *m);
t_player			*hashmap_get(t_hashmap *m, const char *username);
t_player			*hashmap_put(t_hashmap *m, t_player *p);
void				hashmap_foreach(t_hashmap *m,
						void (*fn)(t_player *, void *), void *ctx);

/* SKIPLIST.C */

t_skiplist			*skiplist_create(void);
void				skiplist_destroy(t_skiplist *s);
void				skiplist_insert(t_skiplist *s, t_player *p);
void				skiplist_remove(t_skiplist *s, t_player *p);
void				skiplist_update(t_skiplist *s, t_player *p, int64_t score);
size_t				skiplist_topn(t_skiplist *s, t_rank_entry *out, size_t cap);
size_t				skiplist_rank(t_skiplist *s, t_player *p);

/* PLAYER.C */

size_t				player_serialise(const t_player *p, uint8_t *buf, size_t cap);
t_db_result			player_deserialise(const uint8_t *buf, size_t len,
						t_player *out);

/* LOG.C */

t_dblog				*log_open(const char *data_dir);
void				log_close(t_dblog *log);
t_db_result			log_append(t_dblog *log, const t_player *p);
t_db_result			log_fsync(t_dblog *log);
t_db_result			log_replay(t_dblog *log,
						void (*cb)(const t_player *, void *), void *ctx);

/* FLUSHER.C */

t_flusher			*flusher_start(t_dblog *log);
void				flusher_stop(t_flusher *f);

/* RECOVERY.C */

t_db_result			recovery_run(t_dblog *log, t_hashmap *hm, t_skiplist *sl,
						t_player_id *out_next_id);

/* CATALOGUE.C */

t_catalogue			*catalogue_load(const char *config_dir);
void				catalogue_free(t_catalogue *c);
const t_character	*catalogue_character(t_catalogue *c, t_item_id id);
const t_theme		*catalogue_theme(t_catalogue *c, t_item_id id);

#endif
