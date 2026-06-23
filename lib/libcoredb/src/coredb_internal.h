#ifndef COREDB_INTERNAL_H
# define COREDB_INTERNAL_H

# include "coredb.h"

# include <pthread.h>
# include <stdint.h>
# include <stdio.h>

/* On-disk record framing. Internal only — never exposed in coredb.h. */
# define COREDB_RECORD_MAGIC 0x434F5244u /* "CORD" */
# define COREDB_RECORD_VERSION 1

typedef enum e_coredb_event_type
{
	COREDB_EV_USER_CREATE = 1,
	COREDB_EV_PASSWORD_SET = 2,
	COREDB_EV_POINTS_DELTA = 3,
	COREDB_EV_GRANT_CHARACTER = 4,
	COREDB_EV_GRANT_THEME = 5,
	COREDB_EV_EQUIP_THEME = 6,
	COREDB_EV_SET_HIGH_SCORE = 7
}	t_coredb_event_type;

typedef struct s_coredb_record_header
{
	uint32_t	magic;
	uint16_t	version;
	uint16_t	event_type;
	uint64_t	seq_no;
	uint64_t	timestamp_ms;
	uint32_t	payload_len;
	uint32_t	payload_crc;
}	t_coredb_record_header;

typedef struct s_coredb_ev_user_create
{
	uint32_t	player_id;
	char		username[COREDB_USERNAME_MAX];
	uint8_t		password_salt[COREDB_PASSWORD_SALT_SIZE];
	uint8_t		password_hash[COREDB_PASSWORD_HASH_SIZE];
	uint32_t	password_iters;
}	t_coredb_ev_user_create;

typedef struct s_coredb_ev_points_delta
{
	uint32_t	player_id;
	int32_t		points_delta;
	uint32_t	reason;
}	t_coredb_ev_points_delta;

typedef struct s_coredb_ev_grant_item
{
	uint32_t	player_id;
	uint32_t	item_id;
}	t_coredb_ev_grant_item;

typedef struct s_coredb_ev_equip_item
{
	uint32_t	player_id;
	uint32_t	item_id;
}	t_coredb_ev_equip_item;

typedef struct s_coredb_ev_high_score
{
	uint32_t	player_id;
	uint32_t	high_score;
}	t_coredb_ev_high_score;

/* Largest possible payload — sized so replay can read into one fixed buffer
 * without knowing the event type in advance. */
typedef union u_coredb_event_payload
{
	t_coredb_ev_user_create		user_create;
	t_coredb_ev_points_delta	points_delta;
	t_coredb_ev_grant_item		grant_item;
	t_coredb_ev_equip_item		equip_item;
	t_coredb_ev_high_score		high_score;
}	t_coredb_event_payload;

# define COREDB_MAX_PAYLOAD_SIZE sizeof(t_coredb_event_payload)

/* Outcome of reading one record off disk. Lets replay tell a clean end of
 * file apart from a half-written trailing record (crash mid-write). */
typedef enum e_coredb_io_status
{
	COREDB_IO_OK = 0,
	COREDB_IO_EOF,
	COREDB_IO_TRUNCATED,
	COREDB_IO_BAD_HEADER
}	t_coredb_io_status;

/* In-memory live user state. Separate chaining, two lookup paths sharing
 * the same nodes: most callers have a player_id (public API takes
 * player_id everywhere except create/verify_user), auth needs username. */
# define COREDB_HASH_INITIAL_BUCKETS 256

typedef struct s_coredb_hash_node
{
	t_coredb_user				user;
	struct s_coredb_hash_node	*next_by_username;
	struct s_coredb_hash_node	*next_by_player_id;
}	t_coredb_hash_node;

typedef struct s_coredb_hash_table
{
	t_coredb_hash_node	**buckets_by_username;
	t_coredb_hash_node	**buckets_by_player_id;
	size_t				bucket_count;
	size_t				user_count;
}	t_coredb_hash_table;

struct s_coredb
{
	FILE				*file; // File handle for the database file
	pthread_mutex_t		mutex; // serialises concurrent access from multiple threads
	char				*path; // copy of the filename (for errror messages / reopening)
	uint32_t			next_player_id; // auto-incrementing: next new user gets ID = N
	uint64_t			next_seq_no; // auto-incrementing: next new record gets seq_no = N
	t_coredb_hash_table	*table; // live user state, replayed from disk on open
};

/* db_record.c — record header build/validate + crc32 over a payload. */
uint32_t	coredb_crc32(const void *data, size_t len);
uint64_t	coredb_now_ms(void);
void		coredb_record_header_build(t_coredb_record_header *hdr,
				uint16_t event_type, uint64_t seq_no,
				uint32_t payload_len, const void *payload);
int			coredb_record_header_check(const t_coredb_record_header *hdr);
int			coredb_record_payload_check(const t_coredb_record_header *hdr,
				const void *payload);

/* db_storage.c — raw append/read of one record. No locking, no replay
 * logic, no CRC policy decisions: caller holds db->mutex and decides what
 * a bad status means. */
int	coredb_storage_append_record(t_coredb *db, uint16_t event_type,
		uint32_t payload_len, const void *payload, uint64_t *out_seq_no);
t_coredb_io_status	coredb_storage_read_record(FILE *file,
		t_coredb_record_header *out_hdr, void *payload_buf,
		size_t payload_buf_cap);

/* db_hash.c — separate-chaining hash table over t_coredb_user, indexed by
 * both username and player_id. Finders return a pointer into the live
 * node, so callers (db_points.c, db_inventory.c, ...) mutate fields
 * in place rather than copy-modify-reinsert. */
int		coredb_hash_table_init(t_coredb_hash_table **out_table);
void	coredb_hash_table_destroy(t_coredb_hash_table *table);
int		coredb_hash_insert(t_coredb_hash_table *table,
			const t_coredb_user *user, t_coredb_user **out_user);
t_coredb_user	*coredb_hash_find_by_username(t_coredb_hash_table *table,
					const char *username);
t_coredb_user	*coredb_hash_find_by_player_id(t_coredb_hash_table *table,
					uint32_t player_id);
size_t	coredb_hash_user_count(const t_coredb_hash_table *table);
size_t	coredb_hash_bucket_count(const t_coredb_hash_table *table);

/* db_replay.c — rebuilds db->table from db->file on open. First version:
 * on a corrupt/truncated trailing record, stop and report the warning
 * status, keep whatever was already replayed. Does not truncate the file
 * (later version, per plan: "truncate file to last good offset"). */
typedef enum e_coredb_replay_status
{
	COREDB_REPLAY_OK = 0,
	COREDB_REPLAY_CORRUPT_TAIL
}	t_coredb_replay_status;

t_coredb_replay_status	coredb_replay(t_coredb *db);

#endif
