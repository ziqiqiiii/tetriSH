# ifndef STATUSBODY_H
# define STATUSBODY_H

# include <errno.h>
# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

/*
** Shared HTTTP message-body codecs - the formats tetrisd encodes and
** tetrisu decodes (STATE snapshots, room list rows, profile view,
** leaderboard rows). Self-contained: no brain/room/db headers; the daemons
** map their structs into these wire-facing ones. All bodies are plaintext
** `key value` lines. Contract:
**   sb_*_encode -> bytes written, or -1 with errno = EINVAL (bad field /
**                  NULL) or ERANGE (cap too small); never writes past cap.
**   sb_*_decode -> 0, or -1 with errno = EBADMSG (missing key, malformed
**                  or out-of-range value, trailing junk) or EINVAL (NULL).
** Round-trip law: decode(encode(x)) == x; encode is deterministic.
**
** Rooms travel by name (S-01, D-02, BR-10), never by their numeric id:
** ids run per mode in tetrisd, so only the prefixed name is unique, and
** it is what the client displays and sends back. Mode still rides as its
** own field so clients never parse the prefix.
*/

# define SB_NAME_MAX		16
# define SB_USER_MAX		32
# define SB_OWNED_MAX		64
# define SB_BOARD_ROWS		20
# define SB_BOARD_COLS		10
# define SB_NEXT_COUNT		3
# define SB_CLEARING_MAX	4
# define SB_CHARGE_MAX		10
# define SB_COLOR_MAX		15

typedef enum e_sb_phase
{
	SB_PHASE_ACTIVE,
	SB_PHASE_CLEARING,
	SB_PHASE_PAUSED,
	SB_PHASE_TOP_OUT
}	t_sb_phase;

typedef enum e_sb_clear_label
{
	SB_CLEAR_NONE,
	SB_CLEAR_SINGLE,
	SB_CLEAR_DOUBLE,
	SB_CLEAR_TRIPLE,
	SB_CLEAR_TETRIS,
	SB_CLEAR_TSPIN,
	SB_CLEAR_TSPIN_MINI,
	SB_CLEAR_PERFECT
}	t_sb_clear_label;

typedef enum e_sb_mode
{
	SB_MODE_SINGLE,
	SB_MODE_DOUBLE,
	SB_MODE_BATTLE_ROYALE
}	t_sb_mode;

typedef enum e_sb_room_status
{
	SB_ROOM_WAITING,
	SB_ROOM_READY,
	SB_ROOM_IN_GAME,
	SB_ROOM_FINISHED
}	t_sb_room_status;

/* one board cell on the wire: type 0-2, color 0-15 (one hex nibble each) */
typedef struct s_sb_cell
{
	uint8_t	type;
	uint8_t	color;
}	t_sb_cell;

typedef struct s_sb_piece
{
	int	type;
	int	rotation;
	int	col;
	int	row;
}	t_sb_piece;

/* last ability activation feedback; level 0 = none */
typedef struct s_sb_ability
{
	int		level;
	bool	accepted;
}	t_sb_ability;

/*
** application/tetris-state body, in encode order:
**   seq <u64>
**   phase <active|clearing|paused|topout>
**   piece <type> <rotation> <col> <row>
**   next <t0> <t1> <t2>
**   score <u64>
**   lines <n>  level <n>  combo <n>  b2b <0|1>  charge <0-10>
**   ability <level> <0|1>
**   clear <none|single|double|triple|tetris|tspin|tspin_mini|perfect>
**   clearing <count> <ms> [<rows>...]
**   board            (then exactly 20 lines of 20 hex chars: 10 cells x
**                     type nibble + color nibble)
*/
typedef struct s_sb_state
{
	uint64_t			seq;
	t_sb_phase			phase;
	t_sb_cell			cells[SB_BOARD_ROWS][SB_BOARD_COLS];
	t_sb_piece			piece;
	int					next[SB_NEXT_COUNT];
	uint64_t			score;
	int					lines;
	int					level;
	int					combo;
	bool				back_to_back;
	int					charge;
	t_sb_ability		last_ability;
	int					clearing_rows[SB_CLEARING_MAX];
	int					clearing_count;
	int					clearing_ms;
	t_sb_clear_label	last_clear;
}	t_sb_state;

/* one LIST /rooms line: <name> <mode> <players>/<slots> <status> <owner> */
typedef struct s_sb_room_row
{
	char				name[SB_NAME_MAX];
	t_sb_mode			mode;
	int					players;
	int					slot_count;
	t_sb_room_status	status;
	char				owner[SB_USER_MAX];
}	t_sb_room_row;

/* UC-20 ProfileView body, one key per line; owned lists are count-prefixed */
typedef struct s_sb_profile
{
	char		username[SB_USER_MAX];
	uint64_t	wallet;
	uint64_t	score;
	int			rank;
	uint32_t	equipped_character;
	uint32_t	equipped_theme;
	uint32_t	owned_characters[SB_OWNED_MAX];
	size_t		owned_character_count;
	uint32_t	owned_themes[SB_OWNED_MAX];
	size_t		owned_theme_count;
}	t_sb_profile;

/* one leaderboard line: <rank> <username> <score> (UC-21) */
typedef struct s_sb_lb_row
{
	int			rank;
	char		username[SB_USER_MAX];
	uint64_t	score;
}	t_sb_lb_row;

/* STATE.C */
int	sb_state_encode(const t_sb_state *in, char *out, size_t cap);
int	sb_state_decode(const char *buf, size_t len, t_sb_state *out);

/* ROOMS.C */
int	sb_rooms_encode(const t_sb_room_row *rows, size_t count, char *out, size_t cap);
int	sb_rooms_decode(const char *buf, size_t len, t_sb_room_row *rows, size_t cap, size_t *count);

/* PROFILE.C */
int	sb_profile_encode(const t_sb_profile *in, char *out, size_t cap);
int	sb_profile_decode(const char *buf, size_t len, t_sb_profile *out);

/* LEADERBOARD.C */
int	sb_leaderboard_encode(const t_sb_lb_row *rows, size_t count, char *out, size_t cap);
int	sb_leaderboard_decode(const char *buf, size_t len, t_sb_lb_row *rows, size_t cap, size_t *count);

# endif
