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
** eight full boards would be 46 KB of body; ninety-eight masks are 4.8 KB.
**
** The mask is optional, and its flags say so. A board that has topped out never
** changes again, so re-sending its mask five times a second is the largest
** avoidable cost in the mode. A card without one means "keep the mask you
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
static int	encode_card(const t_body_arena_slot *card, char *out, size_t cap,
				size_t *off);
static int	encode_mask(const t_body_arena_slot *card, char *out, size_t cap,
				size_t *off);
static int	decode_card(const char *line, t_body_arena_slot *out);
static int	decode_mask(const char *text, t_body_arena_slot *out);
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
 * @brief Writes one card, with its mask only when the flags claim one.
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
static int	encode_card(const t_body_arena_slot *card, char *out, size_t cap,
		size_t *off)
{
	if (body_append(out, cap, off, "a %d %" PRIx64 " %u %d %d %d %d",
			card->slot, card->player_id, card->flags, card->lines,
			card->pending, card->ko, card->rank) != 0)
		return (-1);
	if ((card->flags & BODY_ARENA_MASK_PRESENT) != 0)
	{
		if (body_append(out, cap, off, " ") != 0
			|| encode_mask(card, out, cap, off) != 0)
			return (-1);
	}
	return (body_append(out, cap, off, "\n"));
}

/**
 * @brief Writes a board mask as hex, row 0 first, leftmost column highest bit.
 *
 * @param card The card whose mask is written.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_mask(const t_body_arena_slot *card, char *out, size_t cap,
		size_t *off)
{
	unsigned	nibble;
	int			cell;
	int			row;
	int			col;

	cell = 0;
	nibble = 0;
	while (cell < BODY_BOARD_ROWS * BODY_BOARD_COLS)
	{
		row = cell / BODY_BOARD_COLS;
		col = cell % BODY_BOARD_COLS;
		nibble = (nibble << 1) | ((card->mask[row][col / 8] >> (col % 8)) & 1u);
		cell++;
		if (cell % 4 == 0)
		{
			if (body_append(out, cap, off, "%x", nibble) != 0)
				return (-1);
			nibble = 0;
		}
	}
	return (0);
}

/**
 * @brief Parses one `a` line, with or without its trailing mask.
 *
 * @param line The card text, newline already stripped.
 * @param out The card to fill.
 * @return 0 on success, -1 on a malformed line or a mask that disagrees with
 *         the flags that announced it.
 */
static int	decode_card(const char *line, t_body_arena_slot *out)
{
	char	mask[BODY_LINE_MAX];
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
	if (sscanf(line + head, " %1023s%n", mask, &tail) != 1
		|| line[head + tail] != '\0'
		|| strlen(mask) != (size_t)BODY_ARENA_MASK_CHARS)
		return (-1);
	out->mask_valid = true;
	return (decode_mask(mask, out));
}

/**
 * @brief Unpacks a hex mask back into one bit per cell.
 *
 * @param text The mask characters, already length-checked.
 * @param out The card whose mask is filled.
 * @return 0 on success, -1 on a non-hex character.
 */
static int	decode_mask(const char *text, t_body_arena_slot *out)
{
	int	value;
	int	cell;
	int	row;
	int	col;

	cell = 0;
	while (cell < BODY_BOARD_ROWS * BODY_BOARD_COLS)
	{
		value = hex_value(text[cell / 4]);
		if (value < 0)
			return (-1);
		row = cell / BODY_BOARD_COLS;
		col = cell % BODY_BOARD_COLS;
		if ((value >> (3 - cell % 4)) & 1)
			out->mask[row][col / 8] |= (unsigned char)(1u << (col % 8));
		cell++;
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
	if (card->slot < 0 || card->slot >= BODY_ARENA_MAX)
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
