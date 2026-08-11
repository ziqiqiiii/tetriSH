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
**   body_*_encode -> bytes written, or -1 with errno = EINVAL (bad field /
**                  NULL) or ERANGE (cap too small); never writes past cap.
**   body_*_decode -> 0, or -1 with errno = EBADMSG (missing key, malformed
**                  or out-of-range value, trailing junk) or EINVAL (NULL).
** Round-trip law: decode(encode(x)) == x; encode is deterministic.
**
** Rooms travel by name (S-01, D-02, BR-10), never by their numeric id:
** ids run per mode in tetrisd, so only the prefixed name is unique, and
** it is what the client displays and sends back. Mode still rides as its
** own field so clients never parse the prefix.
*/

# define BODY_NAME_MAX		16
# define BODY_USER_MAX		32
# define BODY_OWNED_MAX		64
# define BODY_BOARD_ROWS		20
# define BODY_BOARD_COLS		10
# define BODY_NEXT_COUNT		3
# define BODY_CLEARING_MAX	4
# define BODY_CHARGE_MAX		10
# define BODY_COLOR_MAX		15
/* one store front: the catalogue caps in libmacminidb are 64 per kind, but
** what tetrisd sells is a fixed roster of four characters and seven themes */
# define BODY_CATALOGUE_MAX	16
# define BODY_ITEM_NAME_MAX	32
# define BODY_ROOM_MEMBERS_MAX	99
/*
** How many other players' boards one STATE snapshot carries.
**
** One, because that is Double. It is a deliberate cap and not a placeholder:
** an opponent costs a whole board inside t_body_state, and tetrisd collects
** one t_body_state per slot on the stack for every tick, so the struct is
** multiplied by TD_MAX_GAMES before it is ever encoded. Battle Royale's 98
** cannot be carried this way at all - it needs the 1-bit-per-cell projection
** the field order below leaves room for, and a tick that encodes per slot
** rather than collecting first.
*/
# define BODY_OPPONENTS_MAX	1
/* the longest chat line a room will carry, sender excluded */
# define BODY_CHAT_TEXT_MAX	256
/* what t_body_state.hold reads when the player is holding nothing */
# define BODY_HOLD_EMPTY		(-1)

/*
** COUNTDOWN is last so the four that came before it keep their values.
**
** It is the room holding a dealt board still before a match begins: every
** game is active and none of them is advancing. It is not PAUSED, which is one
** player stopping their own clock and is refused outside Single for exactly
** that reason - a countdown stops everybody's, which is the only way two
** players can be made to start on the same tick.
*/
typedef enum e_body_phase
{
	BODY_PHASE_ACTIVE,
	BODY_PHASE_CLEARING,
	BODY_PHASE_PAUSED,
	BODY_PHASE_TOP_OUT,
	BODY_PHASE_COUNTDOWN
}	t_body_phase;

typedef enum e_body_clear_label
{
	BODY_CLEAR_NONE,
	BODY_CLEAR_SINGLE,
	BODY_CLEAR_DOUBLE,
	BODY_CLEAR_TRIPLE,
	BODY_CLEAR_TETRIS,
	BODY_CLEAR_TSPIN,
	BODY_CLEAR_TSPIN_MINI,
	BODY_CLEAR_PERFECT
}	t_body_clear_label;

/*
** How a match ended for the player being sent this snapshot.
**
** It is deliberately not a phase. A phase says what the board is doing, and
** the winner's board is doing nothing unusual - it is simply still there,
** active, with a piece on it. Nothing about the board distinguishes "still
** playing" from "playing when everybody else stopped", so the outcome has to
** be its own field or the winner is never told they won.
**
** Single sends NONE always: a solo game ends by topping out, and the top-out
** phase already says so.
*/
typedef enum e_body_result
{
	BODY_RESULT_NONE,
	BODY_RESULT_WON,
	BODY_RESULT_LOST
}	t_body_result;

typedef enum e_body_mode
{
	BODY_MODE_SINGLE,
	BODY_MODE_DOUBLE,
	BODY_MODE_BATTLE_ROYALE
}	t_body_mode;

typedef enum e_body_room_status
{
	BODY_ROOM_WAITING,
	BODY_ROOM_READY,
	BODY_ROOM_IN_GAME,
	BODY_ROOM_FINISHED
}	t_body_room_status;

/* one board cell on the wire: type 0-2, color 0-15 (one hex nibble each) */
typedef struct s_body_cell
{
	uint8_t	type;
	uint8_t	color;
}	t_body_cell;

typedef struct s_body_piece
{
	int	type;
	int	rotation;
	int	col;
	int	row;
}	t_body_piece;

/* last ability activation feedback; level 0 = none */
typedef struct s_body_ability
{
	int		level;
	bool	accepted;
}	t_body_ability;

/*
** One other player's board, as the snapshot's recipient is shown it.
**
** It rides inside the recipient's own snapshot rather than arriving as a
** snapshot of its own, because tetrisd's outbox holds exactly one STATE
** mailbox slot per client and a second push would destroy the first. Carrying
** both boards in one message also makes them the same instant by
** construction: a client can never draw its own board from one tick beside
** its opponent's from three ticks ago.
**
** `pending` is the garbage queued against this player and not yet landed - it
** is applied at their next piece lock, so it is a warning the recipient can
** see coming rather than a change to the board below it.
**
** The username is last on its line and may not contain a space, which is the
** format's rule rather than a policy: every field before it is positional, so
** one space inside a name shifts all of them. db_username_valid is where that
** rule is enforced at the source.
*/
typedef struct s_body_opponent
{
	int			slot;
	uint64_t	player_id;
	bool		alive;
	t_body_phase	phase;
	uint64_t	score;
	int			lines;
	int			pending;
	/*
	** The falling piece, carried separately from the settled board for the
	** same reason the recipient's own is: a board without it only changes
	** when something locks, so an opponent would appear to sit motionless and
	** then jump. Sending it as a piece rather than stamping it into the cells
	** is what lets the client draw an opponent through the path it already
	** draws a board with.
	*/
	t_body_piece	piece;
	char		username[BODY_USER_MAX];
	t_body_cell	cells[BODY_BOARD_ROWS][BODY_BOARD_COLS];
}	t_body_opponent;

/*
** application/tetris-state body, in encode order:
**   seq <u64>
**   phase <active|clearing|paused|topout|countdown>
**   piece <type> <rotation> <col> <row>
**   next <t0> <t1> <t2>
**   hold <type|-1> <0|1>
**   score <u64>
**   lines <n>  level <n>  combo <n>  b2b <0|1>  charge <0-10>
**   ability <level> <0|1>
**   clear <none|single|double|triple|tetris|tspin|tspin_mini|perfect>
**   clearing <count> <elapsed_ms> [<rows>...]
**   countdown <ms>
**   pending <rows>
**   effects <paralysis> <inversion> <nue> <thwack> <fry> <dark> <pals>
**           <mirror>
**   result <none|won|lost> <rank>
**   board          (then exactly 20 lines of 20 hex chars: 10 cells x
**                     type nibble + color nibble)
**   opponents <n>
**   opp <slot> <pid> <alive> <phase> <score> <lines> <pending>
**       <ptype> <protation> <pcol> <prow> <username>
**                    (then that opponent's 20 board lines; both repeated n
**                     times)
**
** `hold` is BODY_HOLD_EMPTY until the player has held something. Its second
** field says the hold has already been spent on the falling piece, which is
** what stops a player swapping back and forth forever; it is the server's
** answer, not a request the client can make.
**
** `clearing` is how far through the clear the server is, not how long is
** left: the rows named are still filled in the board that follows, and the
** count is 0 whenever no clear is running. The client draws the animation
** from that offset rather than timing one of its own, so it cannot still be
** flashing rows the server has taken away.
**
** `countdown`, `result` and `opponents` are always written, carrying 0 or
** `none` when there is nothing to say. That is this codec's existing idiom -
** `clearing` has always been present with a count of 0 - and it is what keeps
** the line order fixed: no line's presence depends on another line's value, so
** a decoder never has to look ahead to know what it is reading. Single sends
** all three empty and is otherwise unchanged.
**
** `rank` rides with the result because Battle Royale's loss is a placing
** rather than a bare defeat. It is 0 whenever the result is NONE, and 1 for a
** win.
*/
typedef struct s_body_state
{
	uint64_t			seq;
	t_body_phase			phase;
	t_body_cell			cells[BODY_BOARD_ROWS][BODY_BOARD_COLS];
	t_body_piece			piece;
	int					next[BODY_NEXT_COUNT];
	int					hold;
	bool				hold_used;
	uint64_t			score;
	int					lines;
	int					level;
	int					combo;
	bool				back_to_back;
	int					charge;
	t_body_ability		last_ability;
	int					clearing_rows[BODY_CLEARING_MAX];
	int					clearing_count;
	int					clearing_ms;
	int					countdown_ms;
	/*
	** Garbage queued against the recipient and not yet landed. It is the same
	** number the opponents section carries for everyone else, said about the
	** subject of the frame, because a player needs to see what is coming at
	** them at least as much as what is coming at the other player.
	**
	** It is a count of rows and not a board change: garbage lands at the
	** receiver's next piece lock, never on arrival, so between the two this is
	** all there is to show. Single always sends 0.
	*/
	int					pending;
	/*
	** The status effects riding on this player, as counts rather than flags:
	** the piece-counted ones (Paralysis, Inversion, Nue, Thwack) say how many
	** of this player's pieces are left under them, and the rest say 1 or 0.
	**
	** They are on the wire because an effect nobody can see is indis-
	** tinguishable from a bug. Paralysis worked perfectly and looked exactly
	** like a rotate key that had stopped responding, because the server
	** refused the input and the client was never told why; Dark could not be
	** drawn at all, since blacking out a field is something only the renderer
	** can do.
	*/
	int					effect_paralysis;
	int					effect_inversion;
	int					effect_nue;
	int					effect_thwack;
	int					effect_fry;
	int					effect_dark;
	int					effect_pals;
	int					effect_mirror;
	t_body_result		result;
	int					rank;
	t_body_clear_label	last_clear;
	size_t				opponent_count;
	t_body_opponent		opponents[BODY_OPPONENTS_MAX];
}	t_body_state;

/* one LIST /rooms line: <name> <mode> <players>/<slots> <status> <owner> */
typedef struct s_body_room_row
{
	char				name[BODY_NAME_MAX];
	t_body_mode			mode;
	int					players;
	int					slot_count;
	t_body_room_status	status;
	char				owner[BODY_USER_MAX];
}	t_body_room_row;

/* one occupied waiting-room seat, kept in server slot order */
typedef struct s_body_room_member
{
	int			slot;
	uint64_t	player_id;
	bool		owner;
	bool		ready;
	char		username[BODY_USER_MAX];
}	t_body_room_member;

/* detailed LIST /room/<name> snapshot used by the waiting room */
typedef struct s_body_room
{
	char				name[BODY_NAME_MAX];
	t_body_mode			mode;
	t_body_room_status	status;
	int				min_to_start;
	int				slot_count;
	size_t				member_count;
	t_body_room_member	members[BODY_ROOM_MEMBERS_MAX];
}	t_body_room;

/* UC-20 ProfileView body, one key per line; owned lists are count-prefixed */
typedef struct s_body_profile
{
	char		username[BODY_USER_MAX];
	uint64_t	wallet;
	uint64_t	score;
	int			rank;
	uint32_t	equipped_character;
	uint32_t	equipped_theme;
	uint32_t	owned_characters[BODY_OWNED_MAX];
	size_t		owned_character_count;
	uint32_t	owned_themes[BODY_OWNED_MAX];
	size_t		owned_theme_count;
}	t_body_profile;

/* one leaderboard line: <rank> <username> <score> (UC-21) */
typedef struct s_body_leaderboard_row
{
	int			rank;
	char		username[BODY_USER_MAX];
	uint64_t	score;
}	t_body_leaderboard_row;

/*
** One item on the store front: what it is, what it costs, what to call it.
**
** Ownership is deliberately absent - it is a fact about a player, not about
** the catalogue, and it arrives in t_body_profile's owned lists. Keeping the
** two apart is what lets the catalogue be the same answer for everybody.
**
** The id is the catalogue id and is never renumbered, because it is written
** into players' owned lists. Gaps in it are expected, so a client must not
** treat the id as a position in the array.
*/
typedef struct s_body_catalogue_item
{
	uint32_t	id;
	uint64_t	price;
	char		name[BODY_ITEM_NAME_MAX];
}	t_body_catalogue_item;

/*
** application/tetris-status body for LIST /store, in encode order:
**   characters <n>
**   <id> <price> <name>          (exactly n lines)
**   themes <n>
**   <id> <price> <name>          (exactly n lines)
**
** The name runs to end of line and may contain spaces ("Do u wanna build a
** snowman?"), which is why it is last on the line and why nothing may follow
** it. Both counts may be 0; a store with nothing in it is a valid answer.
*/
typedef struct s_body_catalogue
{
	t_body_catalogue_item	characters[BODY_CATALOGUE_MAX];
	size_t					character_count;
	t_body_catalogue_item	themes[BODY_CATALOGUE_MAX];
	size_t					theme_count;
}	t_body_catalogue;

/*
** One line of a room's feed (UC-09), in encode order:
**   seq <n>
**   at <ms>
**   kind player|system
**   sender <username>            (absent when kind is system)
**   text <one line>
**
** Player chat and system narration are one type with two authors, not two
** mechanisms: narration is a message the server wrote, so `system` is true
** and there is no sender. That is what lets a client draw one ordered feed
** instead of merging two.
**
** `text` runs to end of line and may contain spaces, so it is last and
** nothing follows it. A newline or any other control character in it would
** end the line early and re-decode as a different message, so the codec
** rejects them outright rather than escaping - one message has exactly one
** representation.
**
** `seq` is the room's own counter. The chat lane in tetrisd's outbox drops
** its oldest message rather than closing a slow client, so a client that
** cares can see the gap instead of silently believing it read everything.
*/
typedef struct s_body_chat
{
	uint64_t	seq;
	uint64_t	at;
	bool		system;
	char		sender[BODY_USER_MAX];
	char		text[BODY_CHAT_TEXT_MAX];
}	t_body_chat;

/* STATE.C */
int	body_state_encode(const t_body_state *in, char *out, size_t cap);
int	body_state_decode(const char *buf, size_t len, t_body_state *out);

/* ROOMS.C */
int	body_rooms_encode(const t_body_room_row *rows, size_t count, char *out, size_t cap);
int	body_rooms_decode(const char *buf, size_t len, t_body_room_row *rows, size_t cap, size_t *count);

/* ROOM.C */
int	body_room_encode(const t_body_room *in, char *out, size_t cap);
int	body_room_decode(const char *buf, size_t len, t_body_room *out);

/* CHAT.C */
int	body_chat_encode(const t_body_chat *in, char *out, size_t cap);
int	body_chat_decode(const char *buf, size_t len, t_body_chat *out);

/* PROFILE.C */
int	body_profile_encode(const t_body_profile *in, char *out, size_t cap);
int	body_profile_decode(const char *buf, size_t len, t_body_profile *out);

/* LEADERBOARD.C */
int	body_leaderboard_encode(const t_body_leaderboard_row *rows, size_t count, char *out, size_t cap);
int	body_leaderboard_decode(const char *buf, size_t len, t_body_leaderboard_row *rows, size_t cap, size_t *count);

/* CATALOGUE.C */
int	body_catalogue_encode(const t_body_catalogue *in, char *out, size_t cap);
int	body_catalogue_decode(const char *buf, size_t len, t_body_catalogue *out);

# endif
