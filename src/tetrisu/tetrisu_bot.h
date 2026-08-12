# ifndef TETRISU_BOT_H
# define TETRISU_BOT_H

# include <stdbool.h>
# include <stdint.h>

# include "statusbody.h"
# include "tetrisbrain.h"

/*
** The opponent a player can add to their own room, and nothing else.
**
** This header pulls in no notcurses, for the same reason tetrisu_net.h does
** not: a bot has no screen. It signs in, joins a room and plays, and the
** binary that does that must be linkable without a terminal library in it.
**
** The brain here is pure - a board and a piece go in, a placement comes out.
** It performs no I/O and holds no session, which is the discipline
** libtetrisbrain is held to and the reason this can be unit-tested with no
** server in the room. Everything that talks to a socket is in bot_main.c, and
** everything that forks is in bot_proc.c.
*/

/*
** The weights. The first four are the well-known tuned set for this feature
** set, scaled to integers: lines are worth having, and total height, buried
** holes and an uneven surface are each worth avoiding.
**
** BOT_W_BUMP is the one that cannot be dropped. Without it a scorer that
** counts only holes and height happily builds a single column to the ceiling
** rather than accept one hole, which is how the first version of this bot
** topped out with two lines to its name.
*/
# define BOT_W_LINE				760
# define BOT_W_HEIGHT			510
# define BOT_W_HOLE				357
# define BOT_W_BUMP				184

/*
** What a sent row is worth to a bot that is playing to attack rather than to
** survive. A Tetris is three rows and so scores 3000 against a double's 1000,
** which is what makes the difference between the two visible at all: the
** surface terms already price a clear generously, because placement_score
** measures the board *after* the clear and four vanished rows take forty
** cells of height with them.
*/
# define BOT_W_GARBAGE			1000

/*
** The cost of taking a clear that sends nothing - which is exactly a single,
** since garbage_lines_from_clear is {0, 0, 1, 2, 3}.
**
** It has to be this large. Clearing one row drops every column by one, worth
** up to BOARD_WIDTH * BOT_W_HEIGHT = 5100 to the surface terms, and it is
** that windfall rather than BOT_W_LINE that makes the old scorer take every
** single the instant one is available. A penalty smaller than the windfall
** changes nothing at all.
*/
# define BOT_W_WASTED_CLEAR		6000

/*
** Above this height the wasted-clear penalty stops applying and the bot takes
** whatever it can get.
**
** Without the ceiling the penalty is unconditional, and a bot that refuses
** singles unconditionally refuses them at row 19 too: every candidate is
** equally tall by then, so the clearing one still loses by the penalty and the
** bot tops out holding a row it could have taken. Building for a Tetris is a
** luxury of having room.
*/
# define BOT_DANGER_HEIGHT		12

/* how often the easy tier throws a piece away, in percent */
# define BOT_SLOPPY_PERCENT		25

/* how far a scan runs past each wall - see scan_rotation */
# define BOT_SCAN_MARGIN		3

/* how far below the piece's own row a placement may be entered - see scan_entry */
# define BOT_ENTRY_ROWS			2

/*
** The pool's credentials, kept here as a second copy of the server's.
**
** tetrisu cannot link libmacminidb and does not include tetrisd.h, so these
** are duplicated the same way auth_form.c duplicates the username charset
** rule, and for the same reason: the client needs to know what the server
** will accept before it asks. The originals are DB_RESERVED_PREFIX in
** macminidb.h and TETRISD_BOT_SECRET in tetrisd.h; change either and this has
** to move with it, which is what the shared test in tests/integration is for.
**
** BOT_POOL_MAX is how far up the pool a bot will look for a free account, not
** how many the server keeps - the server's count is its own setting, and a
** name past the end simply answers 401 and ends the walk.
*/
# define BOT_ACCOUNT_PREFIX		"BOT_"
# define BOT_ACCOUNT_SECRET		"tetrish-bot-"
# define BOT_POOL_MAX			256

/* how long a bot waits for the board that took its drop, in poll rounds */
# define BOT_SETTLE_TRIES		600
# define BOT_POLL_MS			5

/*
** How many idle poll rounds pass before a bot asks the room whether a select
** window has opened. At BOT_POLL_MS that is about a fifth of a second, which
** is fast against TETRISD_MATCH_SELECT_MS and quiet enough that a bot sitting
** in a waiting room is not a request per frame.
*/
# define BOT_SELECT_POLL_ROUNDS	40

/*
** Difficulty. The split is decided by two facts about the game rather than by
** taste, and both are worth stating where the enum is read.
**
** EASY is the scorer as it has always been, plus noise. The weights alone are
** a near-perfect *survival* set: left to itself the bot does not die, it
** merely never attacks, and in a Battle Royale that is not an easy opponent
** but a stalemate that survives to the final few and does nothing. The thrown
** pieces are what make it lose.
**
** NORMAL prices clears by what they send instead of by how many rows they
** are, so it holds rows back and builds.
**
** ULTRA cannot hit harder - garbage is flat, and back-to-back and combo feed
** the score only, never the attack - so it hits oftener and better aimed
** instead: it reads one piece further ahead, and it steers its Target.
*/
typedef enum
{
	BOT_EASY,
	BOT_NORMAL,
	BOT_ULTRA
}	t_bot_level;

/*
** A bot's own state between pieces.
**
** `rng` is caller-owned for the same reason a t_piece_bag's is: no global
** generator, so two bots in one process are independent and a test can pin a
** seed and get the same game twice.
**
** `sloppy` is rolled once per piece rather than per plan, and that matters
** because a piece is planned twice - once to choose a rotation, and again
** after the rotation to re-choose the column. Rolling inside bot_plan would
** give a piece two chances to be thrown away and let the second plan be
** careful about a rotation the first one picked at random.
*/
typedef struct s_bot
{
	t_bot_level	level;
	uint32_t	rng;
	bool		sloppy;
}	t_bot;

/*
** How many bots one player may add to one room. Four is enough to reach a
** Battle Royale's minimum of four from a single person, and one is enough for
** a Double.
*/
# define BOT_FARM_MAX			4
# define BOT_PATH_MAX			4096

/*
** The server coordinates a bot is handed on its command line, and the room
** for them. BOT_HOST_MAX matches the session config's own host field; the
** argv budget is the seven fixed arguments, six for the server, and NULL.
*/
# define BOT_HOST_MAX			256
# define BOT_ARGV_MAX			16
# define BOT_BINARY_NAME		"tetrisu-bot"
# define BOT_LOG_NAME			"tetrisu-bot.log"
# define BOT_LOG_DEFAULT		"tmp/" BOT_LOG_NAME
# define BOT_LOG_TEMP_DIR		"/tmp"

/*
** One spawned bot, from the parent's side.
**
** `deadman` is the *write* end of a pipe whose read end the child holds.
** Closing it is what tells an orphaned bot to go: a parent that exits by any
** means - cleanly, killed, crashed - closes it, and the child's read returns
** EOF. That is the hole D2 would otherwise leave, because SIGTERM only works
** while there is somebody left to send it.
*/
typedef struct s_bot_handle
{
	int			pid;
	int			deadman;
	t_bot_level	level;
}	t_bot_handle;

typedef struct s_bot_farm
{
	t_bot_handle	bots[BOT_FARM_MAX];
	int				count;
}	t_bot_farm;

/* BOT_PROC.C — spawning, holding and letting go of bots */
void			bot_farm_init(t_bot_farm *farm);
void			bot_farm_remember_self(const char *argv0);
void			bot_farm_remember_server(const char *host, int port,
					const char *ca_path);
int				bot_farm_binary(char *out, size_t cap);
int				bot_farm_add(t_bot_farm *farm, const char *room,
					t_bot_level level);
int				bot_farm_drop(t_bot_farm *farm);
void			bot_farm_clear(t_bot_farm *farm);
int				bot_farm_reap_exited(t_bot_farm *farm);

/* BOT_BRAIN.C */
void			bot_init(t_bot *bot, t_bot_level level, uint32_t seed);
void			bot_begin_piece(t_bot *bot);
bool			bot_plan(t_bot *bot, const t_body_state *snap, bool may_rotate,
					int *rotation, int *col);
bool			bot_level_parse(const char *name, t_bot_level *out);
const char		*bot_level_word(t_bot_level level);
void			bot_board_from_snapshot(t_board *out, const t_body_state *snap);
bool			bot_entry(const t_body_state *snap, int rotation, int col,
					t_piece *out);
int				bot_placement_score(t_bot_level level, t_board *board);

# endif
