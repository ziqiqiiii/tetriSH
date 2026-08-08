#include "body_util.h"

// Static Variables
static const char *const	g_phases[] = {
								"active", 
								"clearing", 
								"paused",
								"topout"
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
static int	encode_head(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	encode_stats(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	encode_clearing(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	encode_board(const t_body_state *in, char *out, size_t cap,
				size_t *off);
static int	decode_head(t_body_cursor *c, t_body_state *out);
static int	decode_stats(t_body_cursor *c, t_body_state *out);
static int	decode_flags(t_body_cursor *c, t_body_state *out);
static int	decode_clearing(t_body_cursor *c, t_body_state *out);
static int	decode_board(t_body_cursor *c, t_body_state *out);
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
		|| encode_clearing(in, out, cap, &off) != 0
		|| encode_board(in, out, cap, &off) != 0)
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
		|| decode_clearing(&c, out) != 0
		|| decode_board(&c, out) != 0
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
	if (in->phase > BODY_PHASE_TOP_OUT || in->last_clear > BODY_CLEAR_PERFECT)
		return (-1);
	if (in->charge < 0 || in->charge > BODY_CHARGE_MAX)
		return (-1);
	if (in->clearing_count < 0 || in->clearing_count > BODY_CLEARING_MAX)
		return (-1);
	if (in->lines < 0 || in->level < 0 || in->combo < 0)
		return (-1);
	if (in->clearing_ms < 0 || in->last_ability.level < 0)
		return (-1);
	if (in->piece.type < 0 || in->piece.rotation < 0)
		return (-1);
	if (in->hold < BODY_HOLD_EMPTY || in->hold > BODY_COLOR_MAX)
		return (-1);
	return (validate_cells(in));
}

/**
 * @brief Checks the next queue and every board cell fit their nibbles.
 *
 * @param in The frame to validate.
 * @return 0 when every cell and next entry is representable, -1 otherwise.
 */
static int	validate_cells(const t_body_state *in)
{
	int	row;
	int	col;

	row = 0;
	while (row < BODY_NEXT_COUNT)
	{
		if (in->next[row] < 0 || in->next[row] > BODY_COLOR_MAX)
			return (-1);
		row++;
	}
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			if (in->cells[row][col].type > 2
				|| in->cells[row][col].color > BODY_COLOR_MAX)
				return (-1);
			col++;
		}
		row++;
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
 * @brief Writes the clearing line, whose row list is count-length.
 *
 * @param in The frame being serialised.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_clearing(const t_body_state *in, char *out, size_t cap,
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
	return (body_append(out, cap, off, "\n"));
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
	int	row;
	int	col;

	if (body_append(out, cap, off, "board\n") != 0)
		return (-1);
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			if (body_append(out, cap, off, "%x%x", in->cells[row][col].type,
					in->cells[row][col].color) != 0)
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
	n = body_word_index(word, g_phases, 4);
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
 * @brief Reads the clearing line and its count-length row list.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a malformed line or a bad row count.
 */
static int	decode_clearing(t_body_cursor *c, t_body_state *out)
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
	int		row;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| strcmp(line, "board") != 0)
		return (-1);
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		if (body_take_line(c, line, sizeof(line)) != 0
			|| decode_row(line, out->cells[row]) != 0)
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
