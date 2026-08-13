/* ************************************************************************** */
/*                                                                            */
/*   bot_brain.c — where a bot decides where its piece goes                    */
/*                                                                            */
/*   Pure: a snapshot goes in, a rotation and a column come out. No session,   */
/*   no socket, no terminal - the same discipline libtetrisbrain is held to,   */
/*   and what lets the tiers be tested with no server in the room.             */
/*                                                                            */
/*   The search itself was written for tests/match_smoke.c and has been        */
/*   playing against a real tetrisd for as long as Double has existed. Two     */
/*   bugs are already paid for in it and are worth not re-paying:              */
/*                                                                            */
/*   - The column has to be chosen *after* rotating, because a rotation near   */
/*     a wall kicks the piece sideways and the plan made before it names a     */
/*     column the piece is no longer measured from. That is why bot_plan takes */
/*     may_rotate and is called twice per piece.                               */
/*   - The caller has to wait for the board that took the drop. Planning the   */
/*     next piece from the snapshot before the last one landed stacks straight */
/*     to the ceiling without ever completing a row.                           */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu_bot.h"

static int		column_profile(const t_board *board, int heights[BOARD_WIDTH]);
static int		tallest_column(const t_board *board);
static int		attack_value(t_bot_level level, int cleared,
					const t_board *board);
static int		surface_value(const t_board *board, t_bot_level level);
static int		row_transitions(const t_board *board);
static int		column_transitions(const t_board *board);
static int		well_sums(const t_board *board, bool forgive);
static int		well_column(const t_board *board, int col);
static int		landing_height(const t_piece *piece);
static int		eroded_cells(const t_board *board, const t_piece *piece);
static int		placement_value(t_bot_level level, const t_board *board,
					const t_piece *piece);
static int		best_reply(t_bot_level level, const t_board *board, int type);
static uint32_t	bot_random(t_bot *bot);
static int		pace_for_level(int base, int level);

/*
** One rotation scan in progress. It is a struct rather than eight arguments
** because the scan is walked from three places - the careful plan, the sloppy
** one, and the reply - and a signature they all have to agree on is easier to
** keep right than one they each spell out.
*/
typedef struct s_bot_scan
{
	t_bot_level		level;
	const t_board	*board;
	int				type;
	int				row;
	int				next;
	int				best;
	int				rotation;
	int				col;
	int				seen;
	bool			found;
}	t_bot_scan;

static void		scan_rotation(t_bot_scan *scan, int rotation);
static void		scan_rotation_sloppy(t_bot *bot, t_bot_scan *scan,
					int rotation);
static bool		scan_entry(const t_bot_scan *scan, int rotation, int col,
					t_piece *out);
static bool		piece_entry(const t_board *board, t_piece *piece, int from);
static int		scan_score(const t_bot_scan *scan, const t_piece *piece);
static void		scan_init(t_bot_scan *scan, t_bot_level level,
					const t_board *board, const t_body_state *snap);

/**
 * @brief Start a bot at a difficulty with a seed of its own.
 *
 * The seed is the caller's so that two bots in one process are independent and
 * a test can replay a game exactly. A zero seed would leave the generator
 * stuck on zero forever, so it is nudged off it.
 *
 * @param bot The bot to initialise.
 * @param level The difficulty it plays at.
 * @param seed Its random source.
 */
void	bot_init(t_bot *bot, t_bot_level level, uint32_t seed)
{
	if (bot == NULL)
		return ;
	bot->level = level;
	bot->rng = seed;
	if (bot->rng == 0)
		bot->rng = BAG_FALLBACK_SEED;
	bot->sloppy = false;
}

/**
 * @brief Roll whether this piece is one the easy tier throws away.
 *
 * Called once per piece, not once per plan. A piece is planned twice - for its
 * rotation, then for its column - and rolling inside bot_plan would give every
 * piece two chances to be thrown and let the second plan place carefully a
 * rotation the first one chose at random.
 *
 * @param bot The bot about to place a piece.
 */
void	bot_begin_piece(t_bot *bot)
{
	if (bot == NULL)
		return ;
	bot->sloppy = bot->level == BOT_EASY
		&& (int)(bot_random(bot) % 100u) < BOT_SLOPPY_PERCENT;
}

/**
 * @brief How long this bot should spend on the piece it is about to place.
 *
 * The tempo, and the whole of a bot's answer to "how fast may I play". A bot
 * has no hands and no eyes, so nothing about placing a piece takes it any time
 * at all - left alone it plays at the speed of the socket, which measured at
 * six to thirteen pieces a second against a person's one or two. It is here
 * rather than in bot_main.c because it is a decision about difficulty, and
 * difficulty is what this file holds.
 *
 * The answer is a budget for the whole piece and not a pause added to it, so
 * a bot on a slow link plays the same tempo as one on a fast one - it simply
 * has less of the budget left to wait out.
 *
 * The game's level shortens it, because a bot that kept level 1's tempo at
 * level 15 would be the only thing on the board that had not sped up. The
 * floor is what keeps the curve from turning back into the bug this file has
 * already paid for once.
 *
 * Jittered, because three bots sharing a period attack in one pulse. The
 * spread is drawn from the bot's own generator, so two bots in one process
 * still diverge and a seeded test still replays exactly.
 *
 * @param bot The bot about to place a piece.
 * @param level The level its game is on; 0 or less is read as the first.
 * @return The budget in milliseconds, 0 for a bot that is not there.
 */
int	bot_piece_pace_ms(t_bot *bot, int level)
{
	int	base;
	int	spread;

	if (bot == NULL)
		return (0);
	if (bot->level == BOT_EASY)
		base = BOT_PACE_EASY_MS;
	else if (bot->level == BOT_ULTRA)
		base = BOT_PACE_ULTRA_MS;
	else
		base = BOT_PACE_NORMAL_MS;
	base = pace_for_level(base, level);
	spread = base * BOT_PACE_JITTER_PCT / 100;
	return (base - spread + (int)(bot_random(bot) % (uint32_t)(2 * spread + 1)));
}

/**
 * @brief Shortens a tier's tempo by the level the game has reached.
 *
 * @param base The tier's tempo at level 1.
 * @param level The level; anything under 1 is read as 1.
 * @return The tempo for that level, never under BOT_PACE_FLOOR_PCT of base.
 */
static int	pace_for_level(int base, int level)
{
	int	floor;
	int	scaled;

	if (level < 1)
		level = 1;
	floor = base * BOT_PACE_FLOOR_PCT / 100;
	scaled = base - base * BOT_PACE_LEVEL_PCT * (level - 1) / 100;
	if (scaled < floor)
		return (floor);
	return (scaled);
}

/**
 * @brief Advance the bot's generator and return the value it produced.
 *
 * xorshift32: cheap, self-contained, and reproducible from a seed, which is
 * all a bot needs from a random source.
 *
 * @param bot The bot whose state advances.
 * @return The next value.
 */
static uint32_t	bot_random(t_bot *bot)
{
	bot->rng ^= bot->rng << 13;
	bot->rng ^= bot->rng >> 17;
	bot->rng ^= bot->rng << 5;
	return (bot->rng);
}

/**
 * @brief Turn the board on the wire back into one libtetrisbrain can plan on.
 *
 * @param out Receives the board.
 * @param snap Snapshot to read it from.
 */
void	bot_board_from_snapshot(t_board *out, const t_body_state *snap)
{
	int	row;
	int	col;

	if (out == NULL || snap == NULL)
		return ;
	board_init(out);
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			out->cells[row][col].type
				= (t_cell_type)snap->cells[row][col].type;
			out->cells[row][col].color = snap->cells[row][col].color;
			col++;
		}
		row++;
	}
}

/**
 * @brief Measures how tall each column stands and how much is buried.
 *
 * A hole is any empty cell with a filled one somewhere above it in the same
 * column. It is the feature that cannot be undone in place: every row above a
 * hole has to be cleared before the hole can be reached.
 *
 * @param board Board to measure.
 * @param heights Receives each column's height.
 * @return How many holes the board carries.
 */
static int	column_profile(const t_board *board, int heights[BOARD_WIDTH])
{
	int	holes;
	int	col;
	int	row;
	int	top;

	holes = 0;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		top = BOARD_HEIGHT;
		row = 0;
		while (row < BOARD_HEIGHT)
		{
			if (board_get(board, col, row).type != CELL_EMPTY)
			{
				if (top == BOARD_HEIGHT)
					top = row;
			}
			else if (top != BOARD_HEIGHT)
				holes++;
			row++;
		}
		heights[col] = BOARD_HEIGHT - top;
		col++;
	}
	return (holes);
}

/**
 * @brief What a clear is worth beyond the rows it erodes.
 *
 * ERODED already prices a clear by how much of the placed piece it took away,
 * which is what makes a Tetris beat four singles without a rule saying so.
 * This is the attack on top of it: a row that lands on somebody is worth
 * having, and a row that lands on nobody is worth waiting past.
 *
 * Easy does not play this game at all - it takes whatever it can get, which is
 * most of what makes it easy.
 *
 * @param level The tier asking.
 * @param cleared How many rows the placement completed.
 * @param board The board after the clear, for the danger-height gate.
 * @return The attack's value, positive or negative.
 */
static int	attack_value(t_bot_level level, int cleared, const t_board *board)
{
	int	sent;

	if (cleared <= 0 || level == BOT_EASY)
		return (0);
	sent = garbage_lines_from_clear(cleared);
	if (sent > 0)
		return (sent * BOT_W_GARBAGE);
	if (tallest_column(board) < BOT_DANGER_HEIGHT)
		return (-BOT_W_WASTED_CLEAR);
	return (0);
}

/**
 * @brief How tall this board's tallest column stands.
 *
 * @param board The board to measure.
 * @return The height of its tallest column, 0 for an empty board.
 */
static int	tallest_column(const t_board *board)
{
	int	heights[BOARD_WIDTH];
	int	tallest;
	int	col;

	column_profile(board, heights);
	tallest = 0;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		if (heights[col] > tallest)
			tallest = heights[col];
		col++;
	}
	return (tallest);
}

/**
 * @brief Price a settled board on the four features that need no piece.
 *
 * Row and column transitions, buried holes, and wells. There is deliberately
 * no bumpiness term: the four-feature scorer this replaced had one, and two
 * units of it were worth more than the hole that covering a notch buried, so
 * the bot filled its own board with them. A column transition *is* the
 * filled-over-empty boundary a hole makes, and it is weighted twice anything
 * else here.
 *
 * @param board The board to measure.
 * @return Its cost, as a number that is better when larger.
 */
static int	surface_value(const t_board *board, t_bot_level level)
{
	int	heights[BOARD_WIDTH];
	int	holes;

	holes = column_profile(board, heights);
	return (-holes * BOT_W_HOLE
		- row_transitions(board) * BOT_W_ROW_TRANS
		- column_transitions(board) * BOT_W_COL_TRANS
		- well_sums(board, level != BOT_EASY) * BOT_W_WELL);
}

/**
 * @brief Counts filled/empty changes scanning across each row.
 *
 * Both walls count as filled, so a row with a single cell in the middle of it
 * scores two transitions and a full row scores none. That is the feature that
 * prefers rows which are nearly done to rows which are half full.
 *
 * @param board The board to measure.
 * @return The transition count.
 */
static int	row_transitions(const t_board *board)
{
	int	count;
	int	row;
	int	col;
	int	previous;
	int	here;

	count = 0;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		previous = 1;
		col = 0;
		while (col <= BOARD_WIDTH)
		{
			here = 1;
			if (col < BOARD_WIDTH)
				here = board_get(board, col, row).type != CELL_EMPTY;
			count += (here != previous);
			previous = here;
			col++;
		}
		row++;
	}
	return (count);
}

/**
 * @brief Counts filled/empty changes scanning down each column.
 *
 * The floor counts as filled and everything above the stack as empty, so a
 * clean column scores one and a column with a hole in it scores three. This is
 * the largest weight in the set and it is the one that makes burying a hole
 * cost what it should.
 *
 * @param board The board to measure.
 * @return The transition count.
 */
static int	column_transitions(const t_board *board)
{
	int	count;
	int	row;
	int	col;
	int	previous;
	int	here;

	count = 0;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		previous = 0;
		row = 0;
		while (row <= BOARD_HEIGHT)
		{
			here = 1;
			if (row < BOARD_HEIGHT)
				here = board_get(board, col, row).type != CELL_EMPTY;
			count += (here != previous);
			previous = here;
			row++;
		}
		col++;
	}
	return (count);
}

/**
 * @brief Sums the depth of every well, triangularly, forgiving one of them.
 *
 * A well is a run of empty cells with filled ones - or a wall - on both sides.
 * A well two deep is worth 1 + 2 rather than 2, because the second row of it
 * costs more than the first: only an I reaches the bottom of it.
 *
 * `forgive` is the whole of a bot's ability to attack, and without it the two
 * attacking tiers cannot. Dellacherie's set is a *survival* evaluator: it
 * prices every well as damage, so a bot under it keeps a flat board, takes
 * whatever single is in front of it, and sends nothing all game. Measured,
 * that was 1197 lines and 270 rows of garbage over 3000 pieces with two
 * Tetrises in it. A Tetris needs a well four deep held open on purpose, so one
 * well - the deepest, which is the one it is keeping - is charged nothing.
 *
 * Only one, and only the deepest. Forgiving all of them is a bot that never
 * fills anything in.
 *
 * @param board The board to measure.
 * @param forgive Whether the deepest well is free.
 * @return The summed cost.
 */
static int	well_sums(const t_board *board, bool forgive)
{
	int	total;
	int	deepest;
	int	column;
	int	col;

	total = 0;
	deepest = 0;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		column = well_column(board, col);
		total += column;
		if (column > deepest)
			deepest = column;
		col++;
	}
	if (forgive)
		return (total - deepest);
	return (total);
}

/**
 * @brief One column's triangular well cost.
 *
 * @param board The board to measure.
 * @param col The column.
 * @return Its cost.
 */
static int	well_column(const t_board *board, int col)
{
	int	total;
	int	row;
	int	depth;

	total = 0;
	depth = 0;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		if (board_get(board, col, row).type == CELL_EMPTY
			&& board_get(board, col - 1, row).type != CELL_EMPTY
			&& board_get(board, col + 1, row).type != CELL_EMPTY)
			total += ++depth;
		else
			depth = 0;
		row++;
	}
	return (total);
}

/**
 * @brief How high the piece came to rest, measured from the floor.
 *
 * The middle of the piece rather than its bottom, which is Dellacherie's own
 * definition and the reason a flat placement beats a standing one at the same
 * base: standing puts half the piece two rows higher.
 *
 * @param piece The placed piece.
 * @return Its height in rows, 0 when the piece has no cells.
 */
static int	landing_height(const t_piece *piece)
{
	int	cols[4];
	int	rows[4];
	int	top;
	int	bottom;
	int	index;

	if (!piece_cells(piece, cols, rows))
		return (0);
	top = rows[0];
	bottom = rows[0];
	index = 1;
	while (index < 4)
	{
		if (rows[index] < top)
			top = rows[index];
		if (rows[index] > bottom)
			bottom = rows[index];
		index++;
	}
	return (BOARD_HEIGHT - (top + bottom) / 2);
}

/**
 * @brief How many of this piece's own cells sit in rows that are now full.
 *
 * Measured on the stamped board *before* the clear, because afterwards the
 * rows and the cells in them are both gone. Multiplied by the number of rows
 * cleared, it is the ERODED feature: a Tetris erodes four times what the same
 * piece erodes completing a single, so nothing has to say that a Tetris is
 * better.
 *
 * @param board The board with the piece already stamped, before clearing.
 * @param piece The piece that was stamped.
 * @return The count, 0 to 4.
 */
static int	eroded_cells(const t_board *board, const t_piece *piece)
{
	int	cols[4];
	int	rows[4];
	int	count;
	int	index;
	int	col;

	if (!piece_cells(piece, cols, rows))
		return (0);
	count = 0;
	index = 0;
	while (index < 4)
	{
		col = 0;
		while (col < BOARD_WIDTH
			&& board_get(board, col, rows[index]).type != CELL_EMPTY)
			col++;
		count += (col == BOARD_WIDTH);
		index++;
	}
	return (count);
}

/**
 * @brief Prices one placement: stamp it, clear what it completes, measure.
 *
 * The two piece-dependent features have to be taken here and in this order -
 * the landing height from the piece, the eroded cells from the board while the
 * full rows are still on it - which is why this exists rather than a scorer
 * that takes a settled board alone.
 *
 * @param level The tier asking.
 * @param board The board before the piece lands.
 * @param piece The piece, already dropped to where it rests.
 * @return The placement's score, better when larger.
 */
static int	placement_value(t_bot_level level, const t_board *board,
			const t_piece *piece)
{
	t_board	work;
	int		eroded;
	int		cleared;

	board_copy(&work, board);
	piece_stamp(&work, piece);
	eroded = eroded_cells(&work, piece);
	cleared = board_clear_lines(&work);
	return (surface_value(&work, level) - landing_height(piece) * BOT_W_LANDING
		+ cleared * eroded * BOT_W_ERODED
		+ attack_value(level, cleared, &work));
}

/**
 * @brief Price one candidate board end to end: its clear, then its surface.
 *
 * @param level The difficulty deciding what a clear is worth.
 * @param board Board with the candidate stamped in; cleared in place.
 * @return The score; higher is better.
 */
int	bot_placement_score(t_bot_level level, t_board *board)
{
	int	cleared;

	if (board == NULL)
		return (0);
	cleared = board_clear_lines(board);
	return (surface_value(board, level) + attack_value(level, cleared, board));
}

/**
 * @brief Price one candidate landing, one piece deep or two.
 *
 * With no lookahead the candidate is worth its own clear plus the surface it
 * leaves. With one, the surface that matters is the one *after* the next piece
 * has been placed as well - so the candidate's own clear is still counted, but
 * the board it leaves is judged by the best thing that can be done to it
 * rather than by how it looks standing still. That is what stops a bot taking
 * a placement that scores well and leaves nowhere for the piece it already
 * knows is coming.
 *
 * @param scan The scan in progress.
 * @param piece The candidate, already dropped to where it lands.
 * @return The score; higher is better.
 */
static int	scan_score(const t_bot_scan *scan, const t_piece *piece)
{
	t_board	work;
	int		score;

	score = placement_value(scan->level, scan->board, piece);
	if (scan->next < 0)
		return (score);
	board_copy(&work, scan->board);
	piece_stamp(&work, piece);
	board_clear_lines(&work);
	return (score + best_reply(scan->level, &work, scan->next));
}

/**
 * @brief Score every column this rotation can be dropped from, keeping the best.
 *
 * The scan runs past both walls because a piece's anchor is not its leftmost
 * cell: an I in one rotation is legal at a column no cell of it occupies, and
 * stopping the scan at the wall would lose that placement.
 *
 * @param scan The scan in progress; its best is updated in place.
 * @param rotation The rotation to try.
 */
static void	scan_rotation(t_bot_scan *scan, int rotation)
{
	t_piece	piece;
	int		score;
	int		candidate;

	candidate = -BOT_SCAN_MARGIN;
	while (candidate <= BOARD_WIDTH)
	{
		if (scan_entry(scan, rotation, candidate, &piece))
		{
			piece_hard_drop(scan->board, &piece);
			score = scan_score(scan, &piece);
			if (!scan->found || score > scan->best)
			{
				scan->found = true;
				scan->best = score;
				scan->col = candidate;
				scan->rotation = rotation;
			}
		}
		candidate++;
	}
}

/**
 * @brief Find the row a candidate can enter the board at, if it can at all.
 *
 * A placement is not tried only at the row the piece is on now, and the reason
 * is the I piece: it spawns at row -1, and its two vertical rotations reach a
 * row above their anchor. At the spawn row that puts a cell at row -2, where
 * board_get answers CELL_FILLED for a wall - so piece_is_valid refuses a
 * vertical I at every column of the board, and a bot that only ever asked at
 * the spawn row could not place one at all. That is not a small loss: the
 * vertical I is the piece that empties a well, so the bot could never take a
 * Tetris and cleared almost only singles.
 *
 * Looking a couple of rows down fixes it honestly rather than by relaxing the
 * bounds check, because by the time the caller has sent the rotation the piece
 * really has fallen: a plan is a place to aim for, and gravity has moved the
 * piece before the first input lands. The window is small so that a placement
 * under an overhang - which no sequence of moves could reach - is still
 * refused.
 *
 * @param scan The scan in progress.
 * @param rotation The rotation to try.
 * @param col The anchor column to try.
 * @param out Receives the piece at the row it enters on.
 * @return true when the candidate can be entered, false otherwise.
 */
static bool	scan_entry(const t_bot_scan *scan, int rotation, int col,
		t_piece *out)
{
	out->type = (t_piece_type)scan->type;
	out->rotation = rotation;
	out->col = col;
	return (piece_entry(scan->board, out, scan->row));
}

/**
 * @brief Settle a piece onto the first row at or below `from` that takes it.
 *
 * @param board The board it has to fit on.
 * @param piece Type, rotation and column set by the caller; its row is written.
 * @param from The row to start looking at.
 * @return true when a row within the window took it, false otherwise.
 */
static bool	piece_entry(const t_board *board, t_piece *piece, int from)
{
	int	drop;

	drop = 0;
	while (drop <= BOT_ENTRY_ROWS)
	{
		piece->row = from + drop;
		if (piece_is_valid(board, piece))
			return (true);
		drop++;
	}
	return (false);
}

/**
 * @brief Resolve a plan into the piece it names, at the row it enters on.
 *
 * The entry rule is not something a caller should work out for itself. A
 * caller that checked the plan at the piece's own row would reject every
 * vertical I the brain proposed - which reads exactly like a bot that has
 * planned an illegal move, and is how this was first got wrong.
 *
 * A client playing against tetrisd does not need this: it sends the rotation
 * and the moves, and the server owns the board. It is for anyone simulating a
 * placement locally, which is what makes a tier assertable without a server.
 *
 * @param snap The board and the piece the plan was made from.
 * @param rotation The rotation the plan named.
 * @param col The column the plan named.
 * @param out Receives the piece, ready to be hard-dropped.
 * @return true when the plan resolves, false when nothing takes it.
 */
bool	bot_entry(const t_body_state *snap, int rotation, int col, t_piece *out)
{
	t_board	board;

	if (snap == NULL || out == NULL)
		return (false);
	bot_board_from_snapshot(&board, snap);
	out->type = (t_piece_type)snap->piece.type;
	out->rotation = rotation;
	out->col = col;
	return (piece_entry(&board, out, snap->piece.row));
}

/**
 * @brief Draw one legal placement of this rotation at random, uniformly.
 *
 * Reservoir sampling: the nth legal placement seen replaces the one held with
 * probability 1/n, which leaves every placement equally likely without knowing
 * in advance how many there are and without a second walk to reach the one
 * that was drawn. The count carries across rotations, so a rotation with more
 * legal columns is drawn from more often - which is right, since what is being
 * drawn is a placement rather than a rotation.
 *
 * @param bot The bot whose generator advances.
 * @param scan The scan in progress; its held placement is updated in place.
 * @param rotation The rotation to try.
 */
static void	scan_rotation_sloppy(t_bot *bot, t_bot_scan *scan, int rotation)
{
	t_piece	piece;
	int		candidate;

	candidate = -BOT_SCAN_MARGIN;
	while (candidate <= BOARD_WIDTH)
	{
		if (scan_entry(scan, rotation, candidate, &piece))
		{
			scan->seen++;
			if (bot_random(bot) % (uint32_t)scan->seen == 0)
			{
				scan->found = true;
				scan->col = candidate;
				scan->rotation = rotation;
			}
		}
		candidate++;
	}
}

/**
 * @brief The best that can be done to this board with the piece known to be next.
 *
 * One ply, and deliberately not two: it reuses the same walker with lookahead
 * switched off, so the recursion is one level deep by construction rather than
 * by a depth counter somebody has to remember to decrement.
 *
 * A board nothing fits on is not scored specially. It is already so tall that
 * surface_value has priced it out of contention.
 *
 * @param level The difficulty deciding what a clear is worth.
 * @param board The board the next piece falls on.
 * @param type The piece type it will be.
 * @return The best score reachable, or the board's own if nothing fits.
 */
static int	best_reply(t_bot_level level, const t_board *board, int type)
{
	t_bot_scan	scan;
	int			rotation;

	memset(&scan, 0, sizeof(scan));
	scan.level = level;
	scan.board = board;
	scan.type = type;
	scan.row = piece_spawn((t_piece_type)type).row;
	scan.next = -1;
	rotation = 0;
	while (rotation <= 3)
	{
		scan_rotation(&scan, rotation);
		rotation++;
	}
	if (!scan.found)
		return (surface_value(board, level));
	return (scan.best);
}

/**
 * @brief Prepare a scan of the piece this snapshot is holding.
 *
 * Lookahead is switched on for every tier but EASY, and only when the frame
 * actually named a next piece - the field is an ordinary piece type on the
 * wire and nothing guarantees a server that has not dealt one yet writes a
 * value this side can plan against.
 *
 * @param scan Receives the prepared scan.
 * @param level The difficulty it runs at.
 * @param board The board to plan on.
 * @param snap The snapshot the piece came from.
 */
static void	scan_init(t_bot_scan *scan, t_bot_level level, const t_board *board,
		const t_body_state *snap)
{
	memset(scan, 0, sizeof(*scan));
	scan->level = level;
	scan->board = board;
	scan->type = snap->piece.type;
	scan->row = snap->piece.row;
	scan->next = -1;
	if (level != BOT_EASY && snap->next[0] >= 0 && snap->next[0] <= PIECE_L)
		scan->next = snap->next[0];
}

/**
 * @brief Pick where the piece in this snapshot should go.
 *
 * Called twice per piece. The first call chooses a rotation; the caller turns
 * to it, and the second call - with may_rotate false - re-chooses the column
 * on the board as it is *after* the turn, because a rotation near a wall kicks
 * the piece sideways and a column chosen before it is measured from an anchor
 * the piece no longer has.
 *
 * @param bot The bot planning; its generator advances only when sloppy.
 * @param snap The board and the piece to place on it.
 * @param may_rotate Whether rotations other than the current one are open.
 * @param rotation Receives the rotation to turn to.
 * @param col Receives the column to slide to.
 * @return true when at least one placement was legal, false otherwise.
 */
bool	bot_plan(t_bot *bot, const t_body_state *snap, bool may_rotate,
		int *rotation, int *col)
{
	t_board		board;
	t_bot_scan	scan;
	int			rot;
	int			last;

	if (bot == NULL || snap == NULL || rotation == NULL || col == NULL)
		return (false);
	bot_board_from_snapshot(&board, snap);
	scan_init(&scan, bot->level, &board, snap);
	rot = 0;
	last = 3;
	if (!may_rotate)
	{
		rot = snap->piece.rotation;
		last = rot;
	}
	scan.rotation = rot;
	while (rot <= last)
	{
		if (bot->sloppy)
			scan_rotation_sloppy(bot, &scan, rot);
		else
			scan_rotation(&scan, rot);
		rot++;
	}
	*rotation = scan.rotation;
	*col = scan.col;
	return (scan.found);
}

/**
 * @brief Read a difficulty out of the word a command line or a setting used.
 *
 * @param name The word.
 * @param out Receives the level on success.
 * @return true when the word named a level, false otherwise.
 */
bool	bot_level_parse(const char *name, t_bot_level *out)
{
	if (name == NULL || out == NULL)
		return (false);
	if (strcmp(name, "easy") == 0)
		return (*out = BOT_EASY, true);
	if (strcmp(name, "normal") == 0)
		return (*out = BOT_NORMAL, true);
	if (strcmp(name, "ultra") == 0)
		return (*out = BOT_ULTRA, true);
	return (false);
}

/**
 * @brief The word for a difficulty, for a command line and for the seat list.
 *
 * @param level The level.
 * @return Its word; "normal" for a value that is not one.
 */
const char	*bot_level_word(t_bot_level level)
{
	if (level == BOT_EASY)
		return ("easy");
	if (level == BOT_ULTRA)
		return ("ultra");
	return ("normal");
}
