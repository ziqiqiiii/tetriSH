#include "body_util.h"

/*
** The Battle Royale arena: one line per occupied seat, each carrying a
** thumbnail of that player's board rather than the board itself.
**
** Three things make this section unlike every other one in this codec, and all
** three are deliberate:
**
** The payload is bit-packed. A card is drawn a few terminal cells wide, so what
** it can show is the silhouette of a stack and nothing else - one bit per cell
** against the two hex nibbles a full board spends on type and colour. Ninety-
** eight full boards would be 46 KB of body; ninety-eight cards are 19 KB.
**
** The cells are optional, and the flags say so. A board that has topped out
** never changes again, so re-sending it five times a second is the largest
** avoidable cost in the mode. A card without them means "keep the ones you
** have", which is safe here and only here: the value is known to be frozen, so
** a client that misses a push is stale by nothing at all. This is not a delta -
** every push is still the complete roster - and the difference matters, because
** the transport underneath cannot support a delta at all.
**
** Absent is not empty. `arena absent 0` says this frame carries no news about
** the arena, and `arena full 0` says the room has nobody in it. Collapsing the
** two would have every client between pushes clear a screen full of live cards.
*/

// Static Functions
static int	encode_card(const t_body_arena_slot *card, char *out, size_t cap, size_t *off);
static int	encode_cells(const t_body_arena_slot *card, char *out, size_t cap, size_t *off);
static int	decode_card(const char *line, t_body_arena_slot *out);
static int	decode_cells(const char *text, t_body_arena_slot *out);
static int	validate_card(const t_body_arena_slot *card);
static int	hex_value(char ch);

/**
 * @brief Writes the counts line and the arena section.
 *
 * The counts go out on every frame, arena or not. They are the room's own
 * numbers and cannot be recovered from the cards: most frames carry no cards,
 * so a HUD that counted them would read `ALIVE 0/0` between pushes.
 *
 * @param in The frame being serialised.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
int	body_arena_encode(const t_body_state *in, char *out, size_t cap,
		size_t *off)
{
	size_t	i;

	if (body_append(out, cap, off, "counts %d %d\n", in->players,
			in->alive) != 0)
		return (-1);
	if (!in->arena_present)
		return (body_append(out, cap, off, "arena absent 0\n"));
	if (body_append(out, cap, off, "arena full %zu\n", in->arena_count) != 0)
		return (-1);
	i = 0;
	while (i < in->arena_count)
	{
		if (encode_card(&in->arena[i], out, cap, off) != 0)
			return (-1);
		i++;
	}
	return (0);
}

/**
 * @brief Reads the counts line and the arena section.
 *
 * @param c The body cursor.
 * @param out The frame being filled.
 * @return 0 on success, -1 on a missing, misordered or malformed line.
 */
int	body_arena_decode(t_body_cursor *c, t_body_state *out)
{
	char	line[BODY_LINE_MAX];
	char	word[BODY_LINE_MAX];
	size_t	count;
	int		n;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "counts %d %d", &out->players, &out->alive) != 2
		|| out->players < 0 || out->alive < 0 || out->alive > out->players)
		return (-1);
	if (body_take_line(c, line, sizeof(line)) != 0
		|| sscanf(line, "arena %1023s %d", word, &n) != 2
		|| n < 0 || n > BODY_ARENA_MAX)
		return (-1);
	count = (size_t)n;
	if (strcmp(word, "absent") == 0)
	{
		if (count != 0)
			return (-1);
		return (0);
	}
	if (strcmp(word, "full") != 0)
		return (-1);
	out->arena_present = true;
	out->arena_count = count;
	count = 0;
	while (count < out->arena_count)
	{
		if (body_take_line(c, line, sizeof(line)) != 0
			|| decode_card(line, &out->arena[count]) != 0)
			return (-1);
		count++;
	}
	return (0);
}

/**
 * @brief Checks every arena field an encoder cannot represent on the wire.
 *
 * @param in The frame to validate.
 * @return 0 when the arena is encodable, -1 otherwise.
 */
int	body_arena_validate(const t_body_state *in)
{
	size_t	i;

	if (in->players < 0 || in->alive < 0 || in->alive > in->players)
		return (-1);
	if (in->arena_count > BODY_ARENA_MAX)
		return (-1);
	if (!in->arena_present && in->arena_count != 0)
		return (-1);
	i = 0;
	while (i < in->arena_count)
	{
		if (validate_card(&in->arena[i]) != 0)
			return (-1);
		i++;
	}
	return (0);
}

/**
 * @brief Writes one card, with its cells only when the flags claim them.
 *
 * The player id goes out in hex. It is the widest field on the line and the
 * only one with no natural bound - as decimal its worst case is 20 characters
 * against 16 - and nothing reads it as text.
 *
 * @param card The card to write.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_card(const t_body_arena_slot *card, char *out, size_t cap, size_t *off)
{
	if (body_append(out, cap, off, "a %d %" PRIx64 " %u %d %d %d %d",
			card->slot, card->player_id, card->flags, card->lines,
			card->pending, card->ko, card->rank) != 0)
		return (-1);
	if ((card->flags & BODY_ARENA_MASK_PRESENT) != 0)
	{
		if (body_append(out, cap, off, " ") != 0
			|| encode_cells(card, out, cap, off) != 0)
			return (-1);
	}
	return (body_append(out, cap, off, "\n"));
}

/**
 * @brief Writes a board as hex, one nibble per cell, row 0 first.
 *
 * @param card The card whose cells are written.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_cells(const t_body_arena_slot *card, char *out, size_t cap, size_t *off)
{
	int	row;
	int	col;

	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			if (body_append(out, cap, off, "%x",
					card->cells[row][col] & 0xFu) != 0)
				return (-1);
			col++;
		}
		row++;
	}
	return (0);
}

/**
 * @brief Parses one `a` line, with or without its trailing cells.
 *
 * @param line The card text, newline already stripped.
 * @param out The card to fill.
 * @return 0 on success, -1 on a malformed line, or cells that disagree with
 *         the flags that announced them.
 */
static int	decode_card(const char *line, t_body_arena_slot *out)
{
	char	cells[BODY_LINE_MAX];
	int		head;
	int		tail;

	memset(out, 0, sizeof(*out));
	head = 0;
	if (sscanf(line, "a %d %" SCNx64 " %u %d %d %d %d%n", &out->slot,
			&out->player_id, &out->flags, &out->lines, &out->pending,
			&out->ko, &out->rank, &head) != 7)
		return (-1);
	if (validate_card(out) != 0)
		return (-1);
	if ((out->flags & BODY_ARENA_MASK_PRESENT) == 0)
	{
		if (line[head] != '\0')
			return (-1);
		return (0);
	}
	tail = 0;
	if (sscanf(line + head, " %1023s%n", cells, &tail) != 1
		|| line[head + tail] != '\0'
		|| strlen(cells) != (size_t)BODY_ARENA_CELL_CHARS)
		return (-1);
	out->cells_valid = true;
	return (decode_cells(cells, out));
}

/**
 * @brief Unpacks the hex back into one code per cell.
 *
 * @param text The cell characters, already length-checked.
 * @param out The card whose cells are filled.
 * @return 0 on success, -1 on a non-hex character.
 */
static int	decode_cells(const char *text, t_body_arena_slot *out)
{
	int	value;
	int	row;
	int	col;

	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			value = hex_value(text[row * BODY_BOARD_COLS + col]);
			if (value < 0)
				return (-1);
			out->cells[row][col] = (unsigned char)value;
			col++;
		}
		row++;
	}
	return (0);
}

/**
 * @brief Checks one card's fields against what the wire can carry.
 *
 * @param card The card to check.
 * @return 0 when the card is encodable, -1 otherwise.
 */
static int	validate_card(const t_body_arena_slot *card)
{
	/*
	 * A slot is the seat number a room hands out, and a room numbers its
	 * seats from 1 - so the last seat of a full room is BODY_ARENA_MAX
	 * itself. Read as a 0-based index into an array of that size, seat 99
	 * failed to validate, and because one bad card fails the whole body, a
	 * full ninety-nine player room encoded no arena at all: every client
	 * drew an empty grid for the whole match, and the only symptom was
	 * silence.
	 */
	if (card->slot < 1 || card->slot > BODY_ARENA_MAX)
		return (-1);
	if (card->lines < 0 || card->pending < 0 || card->ko < 0)
		return (-1);
	if (card->rank < 0 || card->rank > BODY_ARENA_MAX)
		return (-1);
	if (card->flags > BODY_ARENA_FLAGS_MAX)
		return (-1);
	return (0);
}

/**
 * @brief Reads one lower-case hex digit.
 *
 * @param ch The character to read.
 * @return Its value, or -1 when it is not a hex digit.
 */
static int	hex_value(char ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0');
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a' + 10);
	return (-1);
}
