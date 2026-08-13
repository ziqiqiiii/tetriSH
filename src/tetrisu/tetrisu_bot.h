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
** The evaluator: Dellacherie's six features, scaled to integers.
**
** These replace a four-feature set - lines, aggregate height, holes,
** bumpiness - that had one arithmetic property nobody had done the sum on and
** that cost the bot every game it played:
**
**     covering a one-cell notch buried a hole (-357) and removed two units of
**     bumpiness (+368), for a net gain of 11.
**
** So the scorer paid itself to bury a hole in every notch it could reach. The
** boards it left were swiss cheese - one gap per row, in a different column
** each time - and it topped out in 49 to 99 pieces. Not a tuning problem: no
** value of those four weights fixes it while a flat surface is worth more than
** an unbroken one.
**
** Dellacherie's set has no bumpiness term at all. Surface roughness is carried
** by the two transition counts instead, and COL_TRANS - the largest weight
** here by a factor of two - is exactly the filled-over-empty boundary that a
** buried hole creates. Covering a notch now costs 9348 + 7899 and saves
** nothing, which is the correct answer.
**
** LANDING and ERODED are the two that need the piece rather than the settled
** board: how high it came to rest, and how many of its own cells the clear it
** completed took away with it. ERODED is what makes a Tetris worth four times
** a single without any rule saying so.
*/
# define BOT_W_LANDING			4500
# define BOT_W_ERODED			3418
# define BOT_W_ROW_TRANS		3217
# define BOT_W_COL_TRANS		9348
# define BOT_W_HOLE				7899
# define BOT_W_WELL				3386

/*
** What a sent row is worth on top of the evaluator, and what a clear that
** sends nothing costs.
**
** garbage_lines_from_clear is {0, 0, 1, 2, 3}, so a single sends nothing at
** all and a Tetris sends three. ERODED already makes a Tetris worth four
** singles by itself; this is the extra nudge that makes it worth *waiting*
** for one, and it is deliberately far smaller than the old -6000 was against
** the old scale. That penalty is what the player saw as a bot which "avoids
** finishing a line way too much": it outweighed every board consideration
** there was, so the bot stacked to the danger height in silence rather than
** take a row.
*/
# define BOT_W_GARBAGE			14000
# define BOT_W_WASTED_CLEAR		2500

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
** How long a bot spends on one piece, per tier, in milliseconds.
**
** This is the difference between an opponent and a machine gun, and it is not
** a nicety. Nothing else in the loop paces a bot: it plans, moves and hard
** drops as fast as the socket and the input rate limit allow, which measured
** at six to thirteen pieces a second - past the fastest human alive, and past
** it by every bot in the room at once. Three of them put seventeen rows of
** garbage on a player who did nothing in twenty-two seconds, and a Battle
** Royale that a person could not survive to play was the result.
**
** The numbers are pieces per second a person actually reaches: about 0.75 for
** somebody learning, 1.4 for somebody good, 2.5 for somebody who competes.
** They are a tempo and not a handicap - the scoring weights are what make a
** tier good or bad at Tetris, and these are what make it fast or slow at it.
**
** The jitter is not decoration either. Three bots on the same fixed period
** send their garbage in one pulse, and a pulse of three is what a board cannot
** answer; spread out, the same rows arrive as a stream a player can dig
** through.
**
** Every tier is slower than it was (1700/1300/950), because three of them at
** once is the case the numbers are actually felt in: what a player reads off
** an opponent is how often rows arrive, and three bots on the old normal put a
** row on the board more than twice a second between them.
**
** How much slower is bounded, and the bound is the interesting part. The
** jitter runs to +30%, and test_a_tier_is_a_tempo_as_well_as_a_price refuses a
** piece slower than one every 2.5 s at either end of it - so the slowest base
** this file may carry is 1923, and easy sits just under it. A bot placing a
** piece every three seconds does not read as easy, it reads as broken, which
** is the same reason that bound exists at all.
*/
# define BOT_PACE_EASY_MS		1900
# define BOT_PACE_NORMAL_MS		1400
# define BOT_PACE_ULTRA_MS		1000
# define BOT_PACE_JITTER_PCT	30

/*
** How the tempo answers the level, as a percentage shaved off the base per
** level above the first and a floor it never goes under.
**
** A bot that played level 1's tempo at level 15 would be the only thing on
** the board that had not sped up: gravity is five times what it was, the
** player is placing pieces to keep up with it, and the opponent would be
** visibly idling. The floor is what stops the curve turning back into the bug
** this file already paid for.
**
** The floor is also what keeps the tiers apart, which 45% did not. Easy's
** floor was 765 ms and ultra's base is 950, so a long game turned easy into
** something faster than ultra had ever been - the difficulty a player chose
** stopped meaning anything at exactly the point the game got hard. At 65%
** every tier keeps its place at every level, and shaving 3% a level rather
** than 4% reaches the floor later, so the speed-up is something a match grows
** into rather than something it arrives at.
*/
# define BOT_PACE_LEVEL_PCT		3
# define BOT_PACE_FLOOR_PCT		65

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
** Two limits, because there are two reasons to add a bot.
**
** BOT_FARM_HAND_MAX is the one a player meets: four is enough to reach a
** Battle Royale's minimum of four from a single person, and one is enough for
** a Double. It is what B offers and what B refuses past.
**
** BOT_FARM_MAX is the array, and it is fifty because filling a room to see
** what fifty boards cost is a thing worth doing and nothing else in the client
** can do it. It is not reachable from B - only F1 goes past the first limit -
** so a player cannot walk into fifty processes by holding a key down.
**
** Fifty rather than the ninety-nine a Battle Royale seats: every bot is a
** process of this client's own, and the server's account pool is the other
** ceiling (TETRISD_BOT_ACCOUNTS, 64 by default), so the number that fits is
** smaller than the number of seats and always will be.
*/
# define BOT_FARM_HAND_MAX		4
# define BOT_FARM_MAX			50
# define BOT_PATH_MAX			4096

/*
** The server coordinates a bot is handed on its command line, and the room
** for them. BOT_HOST_MAX matches the session config's own host field; the
** argv budget is the seven fixed arguments, six for the server, and NULL.
*/
# define BOT_HOST_MAX			256
# define BOT_NAME_MAX			32
# define BOT_ARGV_MAX			18
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
	/*
	** The other pipe, and it runs the other way: the child writes the account
	** it managed to claim and the parent reads it.
	**
	** Without it the parent knows a pid and the roster knows a name, and there
	** is nothing joining the two - which is why kicking could only ever drop
	** the bot added last. Which account a bot gets is not decidable here: it
	** walks the pool and takes the first one free, and who else is connected
	** decides where that lands.
	**
	** `report` is the parent's *read* end, non-blocking, and FD_CLOEXEC for
	** the reason the deadman's write end is - a later child must not inherit
	** an earlier one's. `username` is empty until the child has logged in,
	** which is a state the kick path has to handle rather than wait out.
	*/
	int			report;
	char		username[BOT_NAME_MAX];
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
int				bot_farm_drop_named(t_bot_farm *farm, const char *username);
void			bot_farm_collect_names(t_bot_farm *farm);
bool			bot_farm_holds(const t_bot_farm *farm, const char *username);
void			bot_farm_clear(t_bot_farm *farm);
int				bot_farm_reap_exited(t_bot_farm *farm);

/* BOT_BRAIN.C */
void			bot_init(t_bot *bot, t_bot_level level, uint32_t seed);
void			bot_begin_piece(t_bot *bot);
int				bot_piece_pace_ms(t_bot *bot, int level);
bool			bot_plan(t_bot *bot, const t_body_state *snap, bool may_rotate,
					int *rotation, int *col);
bool			bot_level_parse(const char *name, t_bot_level *out);
const char		*bot_level_word(t_bot_level level);
void			bot_board_from_snapshot(t_board *out, const t_body_state *snap);
bool			bot_entry(const t_body_state *snap, int rotation, int col,
					t_piece *out);
int				bot_placement_score(t_bot_level level, t_board *board);

# endif
