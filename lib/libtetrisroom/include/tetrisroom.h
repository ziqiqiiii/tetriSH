# ifndef TETRISROOM_H
# define TETRISROOM_H

# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>
# include <stdio.h>
# include <string.h>

/*
** Pure lobby/room/slot domain - no sockets, no locks, no I/O. All locking
** and broadcasting live in the callers (tetrisd). Slot indices are 1-based,
** matching the room UI. The connection probe is the only seam: a callback
** `bool (*probe)(void *ctx, t_player_id pid)`; NULL means always connected.
**
** A room has both a numeric id and a display name, and neither is ever
** caller-supplied. t_lobby owns one monotonic counter per mode and hands
** the next id to room_init, which derives the name as "<prefix>-<id>" from
** the mode (S / D / BR), zero-padded to two digits: S-01, D-02, BR-10.
** Since ids run per mode, only the name is unique lobby-wide - it is the
** lookup key and what clients display and send back.
*/

# define ROOM_NAME_MAX		16
# define ROOM_USER_MAX		32
# define ROOM_MAX_SLOTS		99
# define LOBBY_MAX_ROOMS	64

typedef uint64_t	t_player_id;

typedef enum e_game_mode
{
	MODE_SINGLE,
	MODE_DOUBLE,
	MODE_BATTLE_ROYALE
}	t_game_mode;

/*
** SELECTING sits between READY and IN_GAME: everybody has declared, nobody is
** playing yet, and the room is holding a window open for them to choose a
** fighter in. tetrisd casts this enum to libstatusbody's t_body_room_status,
** so the two must keep the same values in the same order.
*/
typedef enum e_room_status
{
	ROOM_WAITING,
	ROOM_READY,
	ROOM_SELECTING,
	ROOM_IN_GAME,
	ROOM_FINISHED
}	t_room_status;

typedef enum e_slot_status
{
	SLOT_WAITING,
	SLOT_JOINING,
	SLOT_READY,
	SLOT_LEAVING
}	t_slot_status;

typedef enum e_room_role
{
	ROLE_OWNER,
	ROLE_PLAYER
}	t_room_role;

/* enum, never a bool (UT-34/35) */
typedef enum e_join_verdict
{
	JOIN_ACCEPTED,
	JOIN_FULL,
	JOIN_IN_GAME
}	t_join_verdict;

typedef enum e_start_verdict
{
	START_ACCEPTED,
	START_NOT_OWNER,
	START_TOO_FEW_PLAYERS,
	START_ALREADY_STARTED
}	t_start_verdict;

typedef struct s_membership
{
	t_player_id	player_id;
	char		username[ROOM_USER_MAX];
	t_room_role	role;
	bool		muted;
}	t_membership;

typedef struct s_slot
{
	int				index;
	t_slot_status	status;
	bool			occupied;
	t_membership	membership;
}	t_slot;

typedef struct s_room
{
	int				id;
	char			name[ROOM_NAME_MAX];
	t_game_mode		mode;
	int				slot_count;
	int				min_to_start;
	int				number_of_players;
	t_room_status	status;
	t_slot			slots[ROOM_MAX_SLOTS];
}	t_room;

/* lobby row projection */
typedef struct s_room_summary
{
	int				id;
	char			name[ROOM_NAME_MAX];
	t_game_mode		mode;
	int				players;
	int				slot_count;
	t_room_status	status;
	char			owner_name[ROOM_USER_MAX];
}	t_room_summary;

/* admin row projection */
typedef struct s_room_snapshot
{
	t_room_summary	summary;
	t_player_id		member_pids[ROOM_MAX_SLOTS];
	size_t			member_count;
}	t_room_snapshot;

/* filled by room_release so the caller broadcasts after the role change */
typedef struct s_release_result
{
	bool		released;
	bool		owner_changed;
	t_player_id	new_owner;
	char		new_owner_name[ROOM_USER_MAX];
	bool		room_empty;
}	t_release_result;

typedef struct s_lobby
{
	t_room			rooms[LOBBY_MAX_ROOMS];
	bool			in_use[LOBBY_MAX_ROOMS];
	size_t			max_rooms;
	int				br_slot_count;
	int				next_id[3];
}	t_lobby;

/* MEMBERSHIP.C */
t_membership	membership_make(t_player_id pid, const char *username, t_room_role role);
void			membership_set_role(t_membership *m, t_room_role role);
bool			membership_is_owner(const t_membership *m);

/* SLOT.C */
void			slot_init(t_slot *s, int index);
int				slot_occupy(t_slot *s, t_membership m);
void			slot_clear(t_slot *s);

/* ROOM.C */
int				room_init(t_room *r, t_game_mode mode, int id, int br_slots);
t_join_verdict	room_can_accept(const t_room *r);
int				room_seat(t_room *r, t_player_id pid, const char *username, bool (*probe)(void *ctx, t_player_id pid), void *probe_ctx);
void			room_recompute_status(t_room *r);
t_membership	*room_find_member(t_room *r, t_player_id pid);
int				room_set_ready(t_room *r, t_player_id pid, bool ready);

/* RELEASE.C */
int				room_release(t_room *r, t_player_id pid, bool (*probe)(void *ctx, t_player_id pid), void *probe_ctx, t_release_result *out);
t_membership	*room_select_successor(const t_room *r, bool (*probe)(void *ctx, t_player_id pid), void *probe_ctx);

/* START.C */
t_start_verdict	room_can_start(const t_room *r, t_player_id requester);
t_start_verdict	room_start(t_room *r, t_player_id requester);
void			room_abort_start(t_room *r);
void			room_finish(t_room *r);
void			room_rematch(t_room *r);
int				room_begin_selection(t_room *r);
void			room_abort_selection(t_room *r);
const char		*room_state_message(const t_room *r);

/* LOBBY.C */
int				lobby_init(t_lobby *l, size_t max_rooms, int br_slot_count);
int				lobby_create_room(t_lobby *l, t_game_mode mode, t_room **out);
t_room			*lobby_find_room(t_lobby *l, const char *name);
int				lobby_destroy_room(t_lobby *l, const char *name);
size_t			lobby_list_open(const t_lobby *l, t_room_summary *out, size_t cap);
size_t			lobby_list_all(const t_lobby *l, t_room_snapshot *out, size_t cap);
size_t			lobby_room_count(const t_lobby *l);

/* PROJECTION.C */
void			room_to_summary(const t_room *r, t_room_summary *out);
void			room_to_snapshot(const t_room *r, t_room_snapshot *out);

# endif
