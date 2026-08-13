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

# include <ctype.h>
# include <errno.h>
# include <fcntl.h>
# include <limits.h>
# include <pthread.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>
# include <time.h>
# include <unistd.h>

# define DB_LOG_NAME			"players.log"
# define DB_CHAR_CFG_NAME		"characters.cfg"
# define DB_THEME_CFG_NAME		"themes.cfg"
# define DB_MAX_CHARACTERS		64		/* catalogue cap: roster is ~tens of rows */
# define DB_MAX_THEMES			64		/* catalogue cap: roster is ~tens of rows */
# define DB_CFG_LINE_MAX		256		/* longest config line accepted */
# define DB_LOG_MAGIC			0x4D4D4442
# define DB_HASH_BUCKETS		512
# define DB_FLUSH_INTERVAL_S	1
# define DB_FRAME_MAX			65536
# define DB_FRAME_HDR			12		/* magic u32 + key_len u32 + val_len u32 */
# define DB_SKIP_MAXLVL			16		/* tower cap: > log_4(300 users) headroom */
# define DB_SKIP_P				4		/* 1-in-P promotion; level grows ~log_P n */

typedef struct s_catalogue
{
	t_character		characters[DB_MAX_CHARACTERS];	/* loaded char rows */
	size_t			char_count;						/* rows in characters[] */
	t_theme			themes[DB_MAX_THEMES];			/* loaded theme rows */
	size_t			theme_count;					/* rows in themes[] */
}	t_catalogue;

typedef struct s_skipnode
{
	t_player			*player;		/* shared by ref, owned by the hash map */
	int					height;			/* number of forward links in this tower */
	struct s_skipnode	*forward[];		/* flexible tower: [0..height-1] */
}	t_skipnode;

typedef struct s_skiplist
{
	t_skipnode			*head;			/* sentinel tower of DB_SKIP_MAXLVL links */
	int					level;			/* highest occupied level, 1-based count */
	size_t				size;			/* number of live nodes (excl. sentinel) */
	uint64_t			rng;			/* xorshift state for random_level */
}	t_skiplist;

typedef struct s_hm_entry
{
	t_player			*player;		/* owned here, shared by ref with skiplist */
	struct s_hm_entry	*next;			/* separate-chaining bucket list */
}	t_hm_entry;

typedef struct s_hashmap
{
	t_hm_entry			**buckets;		/* array of bucket-list heads */
	size_t				bucket_count;	/* length of the buckets array */
	size_t				size;			/* number of live entries */
}	t_hashmap;

typedef struct s_dblog
{
	int					fd;					/* append/read fd on the log file */
	char				path[PATH_MAX];		/* <data_dir>/players.log */

}	t_dblog;

typedef struct s_flusher
{
	pthread_t			thread;			/* the 1s fsync worker */
	t_dblog				*log;			/* log to sync; not owned by the flusher */
	pthread_mutex_t		lock;			/* guards stop; paired with the condvar */
	pthread_cond_t		cond;			/* timed 1s wait, signalled early on stop */
	int					stop;			/* set by flusher_stop to end the loop */
}	t_flusher;

typedef struct s_macminidb
{
	pthread_rwlock_t	lock;				/* read-write lock, held in db.c only */
	t_hashmap			*players;			/* username -> t_player* */
	t_skiplist			*board;				/* leaderboard, ordered by (score, id) */
	t_dblog				*log;				/* append-only durability */
	t_flusher			*flusher;			/* 1s fsync thread */
	t_catalogue			*cat;				/* characters + themes */
	t_player_id			next_id;			/* monotonic id allocator */
	
}	t_macminidb;

/* HASHMAP.C */

t_hashmap			*hashmap_create(size_t buckets);
void				hashmap_destroy(t_hashmap *m);
void				hashmap_foreach(t_hashmap *m, void (*fn)(t_player *, void *), void *ctx);

/* HASHMAP_OPS.C */

t_player			*hashmap_get(t_hashmap *m, const char *username);
t_player			*hashmap_put(t_hashmap *m, t_player *p);
size_t				hashmap_hash(const char *username, size_t bucket_count);
t_hm_entry			*hashmap_find(t_hashmap *m, const char *username, size_t *out_idx);

/* SKIPLIST.C */

t_skiplist			*skiplist_create(void);
void				skiplist_destroy(t_skiplist *s);
t_skipnode			*node_new(t_player *p, int height);
int					random_level(t_skiplist *s);

/* SKIPLIST_OPS.C */

int					skiplist_cmp(const t_player *a, const t_player *b);
t_skipnode			**skiplist_seek(t_skiplist *s, const t_player *p, \
						t_skipnode **upd);
void				skiplist_insert(t_skiplist *s, t_player *p);
void				skiplist_remove(t_skiplist *s, t_player *p);

/* SKIPLIST_READ.C */

size_t				skiplist_rank(t_skiplist *s, t_player *p);
size_t				skiplist_topn(t_skiplist *s, t_rank_entry *out, size_t cap);
void				skiplist_update(t_skiplist *s, t_player *p, int64_t score);

/* PLAYER_WRITE.C */

size_t				player_serialise(const t_player *p, uint8_t *buf, size_t cap);

/* PLAYER_READ.C */

t_db_result			player_deserialise(const uint8_t *buf, size_t len, t_player *out);

/* LOG.C */

t_dblog				*log_open(const char *data_dir);
void				log_close(t_dblog *log);
t_db_result			log_fsync(t_dblog *log);
t_db_result			log_truncate_tail(t_dblog *log, off_t clean_end);

/* LOG_APPEND.C */

t_db_result			log_append(t_dblog *log, const t_player *p);

/* LOG_REPLAY.C */

t_db_result			log_replay(t_dblog *log, void (*cb)(const t_player *, void *), void *ctx, off_t *out_clean_end);

/* FLUSHER.C */

t_flusher			*flusher_start(t_dblog *log);
void				flusher_stop(t_flusher *f);

/* RECOVERY.C */

t_db_result			recovery_run(t_dblog *log, t_hashmap *hm, t_skiplist *sl, t_player_id *out_next_id);

/* DB.C — shared db-internal helpers (lock held by the caller) */

typedef struct s_find_ctx
{
	t_player_id	id;			/* id being searched for */
	t_player	*match;		/* the located player, or NULL */
}	t_find_ctx;

t_player			*db_find_by_id(t_db *db, t_player_id id);
t_db_result			db_persist(t_db *db, const t_player *p);
bool				owned_has(const t_item_id *ids, size_t count, t_item_id id);
t_db_result			owned_add(t_item_id *ids, size_t *count, t_item_id id);

/* CATALOGUE.C */

t_catalogue			*catalogue_load(const char *config_dir);
void				catalogue_free(t_catalogue *c);
const t_character	*catalogue_character(t_catalogue *c, t_item_id id);
const t_theme		*catalogue_theme(t_catalogue *c, t_item_id id);

/* CATALOGUE_CHARS.C */

t_db_result			catalogue_parse_chars(const char *config_dir, t_catalogue *c);

/* CATALOGUE_THEMES.C */

t_db_result			catalogue_parse_themes(const char *config_dir, t_catalogue *c);

/* CATALOGUE_PARSE.C */

FILE				*catalogue_open(const char *config_dir, const char *name);
int					catalogue_next_line(FILE *f, char *line, size_t cap);
int					catalogue_split(char *line, char **fields, int max);

#endif
