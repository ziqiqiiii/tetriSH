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
/* an hour, which no select window is; a bound, not a policy */
# define BODY_SELECT_MS_MAX		3600000
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
/*
** How many cards one STATE snapshot's arena carries: every seat of the largest
** room the domain allows.
**
** This is the other half of the cap above, and it is affordable for exactly
** the reason that one is not. A t_body_opponent is a whole board at full
** fidelity - 496 bytes - because Double draws the rival at nearly the size of
** your own. An arena card is a thumbnail, so it carries a 1-bit-per-cell
** occupancy mask instead: the shape of the stack, and nothing about colour or
** piece type that a card that size could show anyway. 40 bytes against 496.
**
** So the arena is a second detail level rather than a wider opponents section.
** Ninety-nine cards cost ~4 KB inside t_body_state; ninety-nine opponents would
** have cost 48 KB, which is most of the frame cap before anything else is said.
*/
# define BODY_ARENA_MAX		99
/*
** Bits of an arena card's `flags` field.
**
** MASK_PRESENT is the one that is not about the player. A dead board never
** changes again, so its card carries the mask only occasionally and the client
** keeps the last one it holds; this bit is how a decoder knows whether the
** field is on the line at all, rather than inferring it from ALIVE and having
** the two disagree.
*/
# define BODY_ARENA_ALIVE			0x01
# define BODY_ARENA_ATTACKING_YOU	0x02
# define BODY_ARENA_TARGETED_BY_YOU	0x04
# define BODY_ARENA_CLEARING		0x08
# define BODY_ARENA_MASK_PRESENT	0x10
# define BODY_ARENA_FLAGS_MAX		0x1F
/* 20 rows of 10 bits, 4 bits to the hex char */
# define BODY_ARENA_CELL_CHARS	(BODY_BOARD_ROWS * BODY_BOARD_COLS)
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

/*
** Kept positionally identical to libtetrisroom's t_room_status: tetrisd casts
** one to the other rather than mapping them, so a value inserted in one has to
** be inserted at the same place in the other.
*/
typedef enum e_body_room_status
{
	BODY_ROOM_WAITING,
	BODY_ROOM_READY,
	BODY_ROOM_SELECTING,
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
	/*
	** What the recipient needs to draw the other side of the screen and
	** cannot work out for itself: how much power they are holding, and who
	** they are holding it as. The charge is the same 0-10 the frame's own
	** carries; the character is a catalogue id, so 0 means they have not
	** chosen one rather than naming the first of them.
	*/
	int			charge;
	uint32_t	character;
	char		username[BODY_USER_MAX];
	t_body_cell	cells[BODY_BOARD_ROWS][BODY_BOARD_COLS];
}	t_body_opponent;

/*
** One card of a Battle Royale arena: what a player can tell about one rival
** from a thumbnail, and nothing more.
**
** `cells` is the board at one nibble each, row 0 first, leftmost column first:
**
**     0        empty
**     1        garbage
**     2 + type a piece, in the seven types' own order
**
** It was one *bit* per cell until a player pointed out that every block in the
** mode drew grey. It could not have drawn anything else - the wire carried a
** silhouette and nothing more - so the client painted every filled cell one
** colour and garbage was indistinguishable from a piece somebody had placed.
** A nibble is four times a bit and still a quarter of the two nibbles a full
** board spends, which keeps the arena an order of magnitude off a board while
** letting a rival's stack read as a stack.
**
** `cells_valid` is not on the wire; it is what the decoder writes after
** reading the flags, so a caller can tell "this card carried fresh cells" from
** "this card left them to whatever you already had". A client that keeps its
** arena slot-indexed simply leaves the old ones in place when it is false.
**
** No score. Nothing at this size draws one, the tiering sorts on placing and
** on who is attacking, and as a decimal uint64 it was the single widest field
** on the line.
*/
typedef struct s_body_arena_slot
{
	int			slot;
	uint64_t	player_id;
	unsigned	flags;
	int			lines;
	int			pending;
	int			ko;
	int			rank;
	bool		cells_valid;
	unsigned char	cells[BODY_BOARD_ROWS][BODY_BOARD_COLS];
}	t_body_arena_slot;

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
**   counts <players> <alive>
**   arena <full|absent> <n>
**   a <slot> <pid-hex> <flags> <lines> <pending> <ko> <rank> [cells]
**                    (repeated n times; no lines at all when absent)
**
** The arena section is appended after the opponents, so every line that came
** before it keeps its position and Double's frame is unchanged in every field
** it reads. Single and Double send `arena absent 0`.
**
** `arena absent` and `arena full 0` are different answers and a decoder must
** not collapse them: absent is "this frame says nothing about the arena", and
** full 0 is "the arena is empty". The first happens on almost every frame,
** because the arena rides a slower clock than the board does.
**
** A card's `cells` are present only when its flags say so, which is the one
** place in this body where a field's presence depends on a value earlier on
** the same line. It is worth the exception: a dead board is finished changing,
** so re-sending it five times a second is the largest avoidable cost in the
** mode, and the alternative - sending only the cards that changed - is a delta
** this transport cannot support.
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
	/*
	** How many players are in this match and how many are still in it. Both are
	** the room's own counts and neither can be derived from the arena, which is
	** the point: the arena rides its own clock, so most frames carry no cards at
	** all, and a HUD that counted them would read ALIVE 0/0 between pushes.
	**
	** Always written. Single sends 1 1 and Double 2 2.
	*/
	int					players;
	int					alive;
	/*
	** The arena, and whether this frame is carrying one. `arena_present` false
	** means "nothing about the arena this frame, keep what you have" - it is not
	** the same as a count of 0, which would mean "the room is empty" and would
	** have every client clear a screen full of live cards on every frame between
	** pushes.
	*/
	bool				arena_present;
	size_t				arena_count;
	t_body_arena_slot	arena[BODY_ARENA_MAX];
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
	/*
	** The fighter this seat has settled on, as a catalogue id, and 0 for
	** "still choosing". Locking in is not a second flag beside it: a seat is
	** locked exactly when it names a character, so the two facts cannot
	** disagree about whether the room may start.
	*/
	uint32_t	character;
	char		username[BODY_USER_MAX];
}	t_body_room_member;

/* detailed LIST /room/<name> snapshot used by the waiting room */
typedef struct s_body_room
{
	char				name[BODY_NAME_MAX];
	t_body_mode			mode;
	t_body_room_status	status;
	/*
	** Milliseconds left in the character-select window, 0 when none is
	** running. It is the room's clock and not each client's, so two players
	** browsing the same roster are shown the same number and the match starts
	** for both at the same instant.
	*/
	int				select_ms;
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

/*
** The widest a state body can encode to, derived from the constants that
** produce it rather than chosen and hoped for.
**
** One arena card, worst case, is 241 bytes: `a ` plus a 2-digit slot, a 16-hex
** player id, 2-digit flags, 4-digit lines, 3-digit pending, 2-digit ko,
** 2-digit rank, each with its space, then the 200 cell nibbles and a newline.
** The cells count even though they are often elided - a buffer is sized by its
** worst case, and eliding them saves bandwidth rather than bytes of buffer.
**
** They were 50 characters while a card was one bit per cell. Four times that
** is what colour costs, and a full arena is still a quarter of what ninety-
** eight real boards would be.
**
** BODY_STATE_HEAD_MAX covers everything before the arena: the fixed lines, the
** subject's own 20 board rows, one full opponent, and the two count lines. It
** is rounded up generously because it is not the term that grows.
**
** Whoever sizes a buffer for one of these must assert against this, not
** against a number that happened to be big enough when it was written.
*/
# define BODY_ARENA_LINE_MAX	241
# define BODY_STATE_HEAD_MAX	2048
# define BODY_STATE_MAX_BYTES	(BODY_STATE_HEAD_MAX \
									+ BODY_ARENA_MAX * BODY_ARENA_LINE_MAX)

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
