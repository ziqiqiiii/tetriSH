#include "body_util.h"

// Static Variables
static const char *const	g_phases[] = {
								"active",
								"clearing",
								"paused",
								"topout",
								"countdown"
							};

static const char *const	g_results[] = {
								"none",
								"won",
								"lost"
							};

static const char *const	g_clears[] = {
								"none", 
								"single", 
								"double", 
								"triple",
								"tetris", 
								"tspin", 
								"tspin_mini", 
								"perfect"
							};

// Static Functions
static int	validate_state(const t_body_state *in);
static int	validate_cells(const t_body_state *in);
static int	validate_cell_block(const t_body_cell (*cells)[BODY_BOARD_COLS]);
static int	validate_opponents(const t_body_state *in);
static int	validate_username(const char *name);
static int	encode_head(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	encode_stats(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	encode_timers(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	encode_board(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	encode_cells(const t_body_cell (*cells)[BODY_BOARD_COLS],
				char *out, size_t cap, size_t *off);
static int	encode_opponents(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	decode_head(t_body_cursor *c, t_body_state *out);
static int	decode_stats(t_body_cursor *c, t_body_state *out);
static int	decode_flags(t_body_cursor *c, t_body_state *out);
static int	decode_timers(t_body_cursor *c, t_body_state *out);
static int	decode_effects(t_body_cursor *c, t_body_state *out);
static int	validate_effects(const t_body_state *in);
static int	decode_result(t_body_cursor *c, t_body_state *out);
static int	decode_board(t_body_cursor *c, t_body_state *out);
static int	decode_opponents(t_body_cursor *c, t_body_state *out);
static int	decode_opponent(t_body_cursor *c, t_body_opponent *out);
static int	decode_cells(t_body_cursor *c, t_body_cell (*cells)[BODY_BOARD_COLS]);
static int	decode_row(const char *line, t_body_cell *cells);
static int	hex_nibble(char ch);

/**
 * @brief Serialises one STATE snapshot into the application/tetris-state
 * body (key order and board block per the header comment).
 *
 * Deterministic: identical frames produce identical bytes.
 *
 * @param in The frame to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes, or -1 with errno = EINVAL (NULL args or a
 *         field out of range) or ERANGE (cap too small).
 */
int	body_state_encode(const t_body_state *in, char *out, size_t cap)
{
	size_t	off;

	if (!in || !out)
		return (body_fail(EINVAL));
	if (validate_state(in) != 0)
		return (body_fail(EINVAL));
	off = 0;
	if (encode_head(in, out, cap, &off) != 0
		|| encode_stats(in, out, cap, &off) != 0
		|| encode_timers(in, out, cap, &off) != 0
		|| encode_board(in, out, cap, &off) != 0
		|| encode_opponents(in, out, cap, &off) != 0)
		return (body_fail(ERANGE));
	return ((int)off);
}

/**
 * @brief Parses an application/tetris-state body back into a frame.
 *
 * Strict: keys must appear in encode order, every key present, exactly 20
 * board rows of 20 hex chars, and nothing after the last row.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param out Receives the decoded frame, fully overwritten on success.
 * @return 0 on success, -1 with errno = EINVAL (NULL args) or EBADMSG
 *         (missing/misordered key, malformed value, out-of-range number,
 *         bad board row, trailing junk).
 */
int	body_state_decode(const char *buf, size_t len, t_body_state *out)
{
	t_body_cursor	c;

	if (!buf || !out)
		return (body_fail(EINVAL));
	memset(out, 0, sizeof(*out));
	c.p = buf;
	c.end = buf + len;
	if (decode_head(&c, out) != 0
		|| decode_stats(&c, out) != 0
		|| decode_timers(&c, out) != 0
		|| decode_board(&c, out) != 0
		|| decode_opponents(&c, out) != 0
		|| !body_at_end(&c))
		return (body_fail(EBADMSG));
	return (0);
}

/**
 * @brief Checks every frame field an encoder cannot represent on the wire.
 *
 * @param in The frame to validate.
 * @return 0 when the frame is encodable, -1 otherwise.
 */
static int	validate_state(const t_body_state *in)
{
	if (in->phase > BODY_PHASE_COUNTDOWN || in->last_clear > BODY_CLEAR_PERFECT)
		return (-1);
	if (in->charge < 0 || in->charge > BODY_CHARGE_MAX)
		return (-1);
	if (in->clearing_count < 0 || in->clearing_count > BODY_CLEARING_MAX)
		return (-1);
	if (in->lines < 0 || in->level < 0 || in->combo < 0)
		return (-1);
	if (in->clearing_ms < 0 || in->last_ability.level < 0)
		return (-1);
	if (in->countdown_ms < 0 || in->rank < 0 || in->pending < 0)
		return (-1);
	if (validate_effects(in) != 0)
		return (-1);
	if (in->result > BODY_RESULT_LOST)
		return (-1);
	if (in->piece.type < 0 || in->piece.rotation < 0)
		return (-1);
	if (in->hold < BODY_HOLD_EMPTY || in->hold > BODY_COLOR_MAX)
		return (-1);
	if (validate_cells(in) != 0)
		return (-1);
	return (validate_opponents(in));
}

/**
 * @brief Checks the next queue and every board cell fit their nibbles.
 *
 * @param in The frame to validate.
 * @return 0 when every cell and next entry is representable, -1 otherwise.
 */
static int	validate_cells(const t_body_state *in)
{
	int	i;

	i = 0;
	while (i < BODY_NEXT_COUNT)
	{
		if (in->next[i] < 0 || in->next[i] > BODY_COLOR_MAX)
			return (-1);
		i++;
	}
	return (validate_cell_block(in->cells));
}

/**
 * @brief Checks one 20x10 board fits the two nibbles a cell is written as.
 *
 * Shared by the frame's own board and by every opponent's, because they are
 * the same block in the same encoding and one of them silently accepting a
 * cell the other rejects is how a body becomes undecodable by its own reader.
 *
 * @param cells The board to check.
 * @return 0 when every cell is representable, -1 otherwise.
 */
static int	validate_cell_block(const t_body_cell (*cells)[BODY_BOARD_COLS])
{
	int	row;
	int	col;

	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			if (cells[row][col].type > 2
				|| cells[row][col].color > BODY_COLOR_MAX)
				return (-1);
			col++;
		}
		row++;
	}
	return (0);
}

/**
 * @brief Checks every opponent carried by this frame is encodable.
 *
 * @param in The frame to validate.
 * @return 0 when the count and every opponent is representable, -1 otherwise.
 */
static int	validate_opponents(const t_body_state *in)
{
	const t_body_opponent	*opponent;
	size_t					i;

	if (in->opponent_count > BODY_OPPONENTS_MAX)
		return (-1);
	i = 0;
	while (i < in->opponent_count)
	{
		opponent = &in->opponents[i];
		if (opponent->slot < 0 || opponent->lines < 0 || opponent->pending < 0)
			return (-1);
		if (opponent->piece.type < 0 || opponent->piece.rotation < 0)
			return (-1);
		if (opponent->phase > BODY_PHASE_COUNTDOWN)
			return (-1);
		if (validate_username(opponent->username) != 0)
			return (-1);
		if (validate_cell_block(opponent->cells) != 0)
			return (-1);
		i++;
	}
	return (0);
}

/**
 * @brief Checks a name can be the last, space-delimited field of its line.
 *
 * Every field before the name is positional, so one space inside it shifts
 * all of them and the whole line decodes as something else - which is the
 * failure db_username_valid exists to prevent at the source. A name that
 * reached here with a space in it came from somewhere that skipped that
 * check, so it is refused rather than written out.
 *
 * @param name The username to check.
 * @return 0 when the name is one printable, space-free word, -1 otherwise.
 */
static int	validate_username(const char *name)
{
	size_t	i;

	if (name[0] == '\0')
		return (-1);
	i = 0;
	while (name[i] != '\0')
	{
		if (i >= BODY_USER_MAX - 1)
			return (-1);
		if (name[i] <= ' ' || name[i] > '~')
			return (-1);
		i++;
	}
	return (0);
}

/**
 * @brief Writes the seq, phase, piece, next, and hold lines.
 *
 * @param in The frame being serialised.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_head(const t_body_state *in, char *out, size_t cap,
		size_t *off)
{
	if (body_append(out, cap, off, "seq %" PRIu64 "\n", in->seq) != 0)
		return (-1);
	if (body_append(out, cap, off, "phase %s\n", g_phases[in->phase]) != 0)
		return (-1);
	if (body_append(out, cap, off, "piece %d %d %d %d\n", in->piece.type,
			in->piece.rotation, in->piece.col, in->piece.row) != 0)
		return (-1);
	if (body_append(out, cap, off, "next %d %d %d\n", in->next[0], in->next[1],
			in->next[2]) != 0)
		return (-1);
	if (body_append(out, cap, off, "hold %d %d\n", in->hold,
			in->hold_used) != 0)
		return (-1);
	return (0);
}

/**
 * @brief Writes the score, counters, flags, ability, and clear lines.
 *
 * @param in The frame being serialised.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_stats(const t_body_state *in, char *out, size_t cap,
		size_t *off)
{
	if (body_append(out, cap, off, "score %" PRIu64 "\n", in->score) != 0)
		return (-1);
	if (body_append(out, cap, off, "lines %d\n", in->lines) != 0)
		return (-1);
	if (body_append(out, cap, off, "level %d\n", in->level) != 0)
		return (-1);
	if (body_append(out, cap, off, "combo %d\n", in->combo) != 0)
		return (-1);
	if (body_append(out, cap, off, "b2b %d\n", in->back_to_back) != 0)
		return (-1);
	if (body_append(out, cap, off, "charge %d\n", in->charge) != 0)
		return (-1);
	if (body_append(out, cap, off, "ability %d %d\n", in->last_ability.level,
			in->last_ability.accepted) != 0)
		return (-1);
	return (body_append(out, cap, off, "clear %s\n", g_clears[in->last_clear]));
}

/**
 * @brief Writes the two hold timers: the clearing line and the countdown.
 *
 * Both say how far through something the server is rather than how long is
 * left, and both are written whether or not one is running - a count of 0 and
 * a countdown of 0 are the "nothing is happening" values, so the line order
 * never depends on what the frame contains.
 *
 * @param in The frame being serialised.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_timers(const t_body_state *in, char *out, size_t cap,
		size_t *off)
{
	int	i;

	if (body_append(out, cap, off, "clearing %d %d", in->clearing_count,
			in->clearing_ms) != 0)
		return (-1);
	i = 0;
	while (i < in->clearing_count)
	{
		if (body_append(out, cap, off, " %d", in->clearing_rows[i]) != 0)
			return (-1);
		i++;
	}
	if (body_append(out, cap, off, "\n") != 0)
		return (-1);
	if (body_append(out, cap, off, "countdown %d\n", in->countdown_ms) != 0)
		return (-1);
	if (body_append(out, cap, off, "pending %d\n", in->pending) != 0)
		return (-1);
	if (body_append(out, cap, off, "effects %d %d %d %d %d %d %d %d\n",
			in->effect_paralysis, in->effect_inversion, in->effect_nue,
			in->effect_thwack, in->effect_fry, in->effect_dark,
			in->effect_pals, in->effect_mirror) != 0)
		return (-1);
	return (body_append(out, cap, off, "result %s %d\n",
			g_results[in->result], in->rank));
}

/**
 * @brief Writes the board block: 20 rows of 10 type/color hex pairs.
 *
 * @param in The frame being serialised.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_board(const t_body_state *in, char *out, size_t cap,
		size_t *off)
{
	if (body_append(out, cap, off, "board\n") != 0)
		return (-1);
	return (encode_cells(in->cells, out, cap, off));
}

/**
 * @brief Writes one 20x10 board as 20 lines of 20 hex characters.
 *
 * @param cells The board to write.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_cells(const t_body_cell (*cells)[BODY_BOARD_COLS],
		char *out, size_t cap, size_t *off)
{
	int	row;
	int	col;

	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			if (body_append(out, cap, off, "%x%x", cells[row][col].type,
					cells[row][col].color) != 0)
				return (-1);
			col++;
		}
		if (body_append(out, cap, off, "\n") != 0)
			return (-1);
		row++;
	}
	return (0);
}

/**
 * @brief Writes the opponents section: a count, then a header and a board
 *        apiece.
 *
 * @param in The frame being serialised.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_opponents(const t_body_state *in, char *out, size_t cap,
		size_t *off)
{
	const t_body_opponent	*opponent;
	size_t					i;

	if (body_append(out, cap, off, "opponents %zu\n",
			in->opponent_count) != 0)
		return (-1);
	i = 0;
	while (i < in->opponent_count)
	{
		opponent = &in->opponents[i];
		if (body_append(out, cap, off,
				"opp %d %" PRIu64 " %d %s %" PRIu64 " %d %d %d %d %d %d %s\n",
				opponent->slot, opponent->player_id, opponent->alive,
				g_phases[opponent->phase], opponent->score, opponent->lines,
				opponent->pending, opponent->piece.type,
				opponent->piece.rotation, opponent->piece.col,
				opponent->piece.row, opponent->username) != 0)
			return (-1);
		if (encode_cells(opponent->cells, out, cap, off) != 0)
			return (-1);
		i++;
	}
	return (0);
}

/**
 * @brief Reads the seq, phase, piece, next, and hold lines in order.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a missing, misordered, or malformed line.
 */
static int	decode_head(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];
	char	word[BODY_LINE_MAX];
	int		used;
	int		n;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| strncmp(line, "seq ", 4) != 0
		|| body_parse_u64(line + 4, &out->seq) != 0)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "phase %1023s%n", word, &n) != 1 || line[n] != '\0')
		return (-1);
	n = body_word_index(word, g_phases, 5);
	if (n < 0)
		return (-1);
	out->phase = (t_body_phase)n;
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "piece %d %d %d %d%n", &out->piece.type,
			&out->piece.rotation, &out->piece.col, &out->piece.row, &n) != 4
		|| line[n] != '\0' || out->piece.type < 0 || out->piece.rotation < 0)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "next %d %d %d%n", &out->next[0], &out->next[1],
			&out->next[2], &n) != 3 || line[n] != '\0')
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "hold %d %d%n", &out->hold, &used, &n) != 2
		|| line[n] != '\0' || out->hold < BODY_HOLD_EMPTY
		|| out->hold > BODY_COLOR_MAX || (used != 0 && used != 1))
		return (-1);
	out->hold_used = (used == 1);
	return (0);
}

/**
 * @brief Reads the score, counters, flags, ability, and clear lines.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a missing, misordered, or malformed line.
 */
static int	decode_stats(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];
	int		n;
	int		b2b;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| strncmp(line, "score ", 6) != 0
		|| body_parse_u64(line + 6, &out->score) != 0)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "lines %d%n", &out->lines, &n) != 1 || line[n] != '\0'
		|| out->lines < 0)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "level %d%n", &out->level, &n) != 1 || line[n] != '\0'
		|| out->level < 0)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "combo %d%n", &out->combo, &n) != 1 || line[n] != '\0'
		|| out->combo < 0)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "b2b %d%n", &b2b, &n) != 1 || line[n] != '\0'
		|| (b2b != 0 && b2b != 1))
		return (-1);
	out->back_to_back = (b2b == 1);
	return (decode_flags(c, out));
}

/**
 * @brief Reads the charge, ability, and clear lines.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a missing, misordered, or malformed line.
 */
static int	decode_flags(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];
	char	word[BODY_LINE_MAX];
	int		accepted;
	int		n;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "charge %d%n", &out->charge, &n) != 1
		|| line[n] != '\0' || out->charge < 0 || out->charge > BODY_CHARGE_MAX)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "ability %d %d%n", &out->last_ability.level,
			&accepted, &n) != 2 || line[n] != '\0'
		|| out->last_ability.level < 0 || (accepted != 0 && accepted != 1))
		return (-1);
	out->last_ability.accepted = (accepted == 1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "clear %1023s%n", word, &n) != 1 || line[n] != '\0')
		return (-1);
	n = body_word_index(word, g_clears, 8);
	if (n < 0)
		return (-1);
	out->last_clear = (t_body_clear_label)n;
	return (0);
}

/**
 * @brief Reads the clearing line with its count-length row list, then the
 *        countdown line.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a malformed line or a bad row count.
 */
static int	decode_timers(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];
	int		used;
	int		n;
	int		i;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "clearing %d %d%n", &out->clearing_count,
			&out->clearing_ms, &used) != 2
		|| out->clearing_count < 0 || out->clearing_count > BODY_CLEARING_MAX
		|| out->clearing_ms < 0)
		return (-1);
	i = 0;
	while (i < out->clearing_count)
	{
		if (sscanf(line + used, " %d%n", &out->clearing_rows[i], &n) != 1)
			return (-1);
		used += n;
		i++;
	}
	if (line[used] != '\0')
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "countdown %d%n", &out->countdown_ms, &n) != 1
		|| line[n] != '\0' || out->countdown_ms < 0)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "pending %d%n", &out->pending, &n) != 1
		|| line[n] != '\0' || out->pending < 0)
		return (-1);
	if (decode_effects(c, out) != 0)
		return (-1);
	return (decode_result(c, out));
}

/**
 * @brief Reads the result line: how the match ended, and at what placing.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on an unknown result word or a negative rank.
 */
static int	decode_result(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];
	char	word[BODY_LINE_MAX];
	int		n;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "result %1023s %d%n", word, &out->rank, &n) != 2
		|| line[n] != '\0' || out->rank < 0)
		return (-1);
	n = body_word_index(word, g_results, 3);
	if (n < 0)
		return (-1);
	out->result = (t_body_result)n;
	return (0);
}

/**
 * @brief Reads the board block: exactly 20 rows of 20 hex characters.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a missing, short, or non-hex row.
 */
static int	decode_board(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];

	if (body_take_line(c, line, sizeof(line)) != 0
		|| strcmp(line, "board") != 0)
		return (-1);
	return (decode_cells(c, out->cells));
}

/**
 * @brief Reads the opponents section, count first.
 *
 * The count is what bounds the read, so a body claiming more opponents than
 * it carries runs out of lines and is rejected rather than leaving the tail
 * of somebody else's frame in the array.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a bad count or a malformed opponent.
 */
static int	decode_opponents(t_body_cursor *c, t_body_state *out)
{
	char		line[BODY_LINE_MAX];
	size_t		count;
	uint64_t	value;
	size_t		i;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| strncmp(line, "opponents ", 10) != 0
		|| body_parse_u64(line + 10, &value) != 0
		|| value > BODY_OPPONENTS_MAX)
		return (-1);
	count = (size_t)value;
	i = 0;
	while (i < count)
	{
		if (decode_opponent(c, &out->opponents[i]) != 0)
			return (-1);
		i++;
	}
	out->opponent_count = count;
	return (0);
}

/**
 * @brief Reads one opponent: its header line, then its board.
 *
 * @param c The body cursor.
 * @param out The opponent being filled.
 * @return 0 on success, -1 on a malformed header or board.
 */
static int	decode_opponent(t_body_cursor *c, t_body_opponent *out)
{
	char	line[BODY_LINE_MAX];
	char	phase[BODY_LINE_MAX];
	int		alive;
	int		n;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line,
			"opp %d %" SCNu64 " %d %1023s %" SCNu64 " %d %d %d %d %d %d %31s%n",
			&out->slot, &out->player_id, &alive, phase, &out->score,
			&out->lines, &out->pending, &out->piece.type, &out->piece.rotation,
			&out->piece.col, &out->piece.row, out->username, &n) != 12
		|| line[n] != '\0')
		return (-1);
	if (out->slot < 0 || out->lines < 0 || out->pending < 0)
		return (-1);
	if (out->piece.type < 0 || out->piece.rotation < 0)
		return (-1);
	if (alive != 0 && alive != 1)
		return (-1);
	out->alive = (alive == 1);
	n = body_word_index(phase, g_phases, 5);
	if (n < 0)
		return (-1);
	out->phase = (t_body_phase)n;
	return (decode_cells(c, out->cells));
}

/**
 * @brief Reads exactly 20 board rows of 20 hex characters into one board.
 *
 * @param c The body cursor.
 * @param cells The board to fill.
 * @return 0 on success, -1 on a missing, short, or non-hex row.
 */
static int	decode_cells(t_body_cursor *c, t_body_cell (*cells)[BODY_BOARD_COLS])
{
	char	line[BODY_LINE_MAX];
	int		row;

	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		if (body_take_line(c, line, sizeof(line)) != 0
			|| decode_row(line, cells[row]) != 0)
			return (-1);
		row++;
	}
	return (0);
}

/**
 * @brief Decodes one board row of type/color hex pairs.
 *
 * @param line The row text, newline already stripped.
 * @param cells The row of cells to fill.
 * @return 0 on success, -1 on a wrong length, non-hex, or bad cell type.
 */
static int	decode_row(const char *line, t_body_cell *cells)
{
	int	col;
	int	type;
	int	color;

	if (strlen(line) != (size_t)(BODY_BOARD_COLS * 2))
		return (-1);
	col = 0;
	while (col < BODY_BOARD_COLS)
	{
		type = hex_nibble(line[col * 2]);
		color = hex_nibble(line[col * 2 + 1]);
		if (type < 0 || color < 0 || type > 2)
			return (-1);
		cells[col].type = (uint8_t)type;
		cells[col].color = (uint8_t)color;
		col++;
	}
	return (0);
}

/**
 * @brief Converts one hexadecimal character to its value.
 *
 * @param ch The character to convert.
 * @return The value 0-15, or -1 when ch is not a hex digit.
 */
static int	hex_nibble(char ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0');
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a' + 10);
	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A' + 10);
	return (-1);
}

/**
 * @brief Reads the effects line: eight counts in a fixed order.
 *
 * Positional and always present, like every other line here, so a decoder
 * never has to look ahead. The four piece-counted effects carry how many of
 * the player's pieces are left under them and the other four carry 1 or 0,
 * which is why they share one line rather than being flags somewhere.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a malformed line or a negative count.
 */
static int	decode_effects(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];
	int		n;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "effects %d %d %d %d %d %d %d %d%n",
			&out->effect_paralysis, &out->effect_inversion, &out->effect_nue,
			&out->effect_thwack, &out->effect_fry, &out->effect_dark,
			&out->effect_pals, &out->effect_mirror, &n) != 8
		|| line[n] != '\0')
		return (-1);
	return (validate_effects(out));
}

/**
 * @brief Refuses a negative effect count, in either direction.
 *
 * Shared by the encoder and the decoder so an unencodable frame and an
 * unreadable one are the same frame.
 *
 * @param in The frame to check.
 * @return 0 when every count is zero or positive, -1 otherwise.
 */
static int	validate_effects(const t_body_state *in)
{
	if (in->effect_paralysis < 0 || in->effect_inversion < 0
		|| in->effect_nue < 0 || in->effect_thwack < 0)
		return (-1);
	if (in->effect_fry < 0 || in->effect_dark < 0
		|| in->effect_pals < 0 || in->effect_mirror < 0)
		return (-1);
	return (0);
}
