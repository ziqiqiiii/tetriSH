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

struct s_coredb
{
	FILE			*file; // File handle for the database file
	pthread_mutex_t	mutex; // serialises concurrent access from multiple threads
	char			*path; // copy of the filename (for errror messages / reopening)
	uint32_t		next_player_id; // auto-incrementing: next new user gets ID = N
	uint64_t		next_seq_no; // auto-incrementing: next new record gets seq_no = N
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

#endif
