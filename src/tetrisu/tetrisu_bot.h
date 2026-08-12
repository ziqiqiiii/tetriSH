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
