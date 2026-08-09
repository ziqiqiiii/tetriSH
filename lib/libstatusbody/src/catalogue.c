#include "body_util.h"

// Static Functions
static int	encode_section(const char *key, const t_body_catalogue_item *items,
				size_t count, char *out, size_t cap, size_t *off);
static int	decode_section(t_body_cursor *c, const char *key,
				t_body_catalogue_item *items, size_t *out_count);
static int	decode_count(const char *line, const char *key, size_t *out);
static int	decode_item(const char *line, t_body_catalogue_item *item);

/**
 * @brief Serialises the store front: both catalogues, each count-prefixed.
 *
 * Characters come first because that is the tab the Marketplace opens on, and
 * a decoder that reads the sections in a fixed order needs no lookahead.
 *
 * @param in The catalogue to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes, or -1 with errno = EINVAL (NULL args, a count
 *         past BODY_CATALOGUE_MAX, an empty or overlong name) or ERANGE (cap
 *         too small).
 */
int	body_catalogue_encode(const t_body_catalogue *in, char *out, size_t cap)
{
	size_t	off;

	if (!in || !out)
		return (body_fail(EINVAL));
	if (in->character_count > BODY_CATALOGUE_MAX
		|| in->theme_count > BODY_CATALOGUE_MAX)
		return (body_fail(EINVAL));
	if (cap == 0)
		return (body_fail(ERANGE));
	out[0] = '\0';
	off = 0;
	if (encode_section("characters", in->characters, in->character_count,
			out, cap, &off) != 0)
		return (-1);
	if (encode_section("themes", in->themes, in->theme_count,
			out, cap, &off) != 0)
		return (-1);
	return ((int)off);
}

/**
 * @brief Parses a store-front body back into both catalogues.
 *
 * Strict: both sections present, in encode order, each holding exactly the
 * number of rows its count line promised, and nothing after them.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param out Receives the decoded catalogue, fully overwritten on success.
 * @return 0 on success, -1 with errno = EINVAL (NULL args) or EBADMSG
 *         (missing section, wrong row count, malformed row, trailing junk).
 */
int	body_catalogue_decode(const char *buf, size_t len, t_body_catalogue *out)
{
	t_body_cursor	c;

	if (!buf || !out)
		return (body_fail(EINVAL));
	memset(out, 0, sizeof(*out));
	c.p = buf;
	c.end = buf + len;
	if (decode_section(&c, "characters", out->characters,
			&out->character_count) != 0
		|| decode_section(&c, "themes", out->themes, &out->theme_count) != 0
		|| !body_at_end(&c))
		return (body_fail(EBADMSG));
	return (0);
}

/**
 * @brief Writes one count line followed by exactly that many item lines.
 *
 * @param key The section's key, characters or themes.
 * @param items The items to write.
 * @param count How many items to write.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 with errno already set by body_fail.
 */
static int	encode_section(const char *key, const t_body_catalogue_item *items,
		size_t count, char *out, size_t cap, size_t *off)
{
	size_t	i;

	if (body_append(out, cap, off, "%s %zu\n", key, count) != 0)
		return (body_fail(ERANGE));
	i = 0;
	while (i < count)
	{
		if (items[i].name[0] == '\0'
			|| strlen(items[i].name) >= BODY_ITEM_NAME_MAX
			|| memchr(items[i].name, '\n', strlen(items[i].name)))
			return (body_fail(EINVAL));
		if (body_append(out, cap, off, "%" PRIu32 " %" PRIu64 " %s\n",
				items[i].id, items[i].price, items[i].name) != 0)
			return (body_fail(ERANGE));
		i++;
	}
	return (0);
}

/**
 * @brief Reads one count line and the item lines it promised.
 *
 * The count is trusted only as far as BODY_CATALOGUE_MAX: a body claiming
 * more rows than the struct holds is refused before a single row is read,
 * rather than filling the array and overrunning it.
 *
 * @param c The cursor, advanced past the whole section on success.
 * @param key The section's expected key.
 * @param items Destination array of BODY_CATALOGUE_MAX items.
 * @param out_count Receives the number of items decoded.
 * @return 0 on success, -1 on a missing, oversized, or malformed section.
 */
static int	decode_section(t_body_cursor *c, const char *key,
		t_body_catalogue_item *items, size_t *out_count)
{
	char	line[BODY_LINE_MAX];
	size_t	count;
	size_t	i;

	if (body_take_line(c, line, sizeof(line)) != 0
		|| decode_count(line, key, &count) != 0)
		return (-1);
	i = 0;
	while (i < count)
	{
		if (body_take_line(c, line, sizeof(line)) != 0
			|| decode_item(line, &items[i]) != 0)
			return (-1);
		i++;
	}
	*out_count = count;
	return (0);
}

/**
 * @brief Parses a `<key> <count>` section header.
 *
 * @param line The line text, newline already stripped.
 * @param key The key the line must open with.
 * @param out Receives the count.
 * @return 0 on success, -1 on the wrong key or a count past the array.
 */
static int	decode_count(const char *line, const char *key, size_t *out)
{
	uint64_t	value;
	size_t		key_len;

	key_len = strlen(key);
	if (strncmp(line, key, key_len) != 0 || line[key_len] != ' ')
		return (-1);
	if (body_parse_u64(line + key_len + 1, &value) != 0)
		return (-1);
	if (value > BODY_CATALOGUE_MAX)
		return (-1);
	*out = (size_t)value;
	return (0);
}

/**
 * @brief Parses one `<id> <price> <name>` item line.
 *
 * The name is whatever remains after the two numbers, spaces included, so it
 * is taken by offset rather than scanned as a word.
 *
 * @param line The line text, newline already stripped.
 * @param item The item to fill.
 * @return 0 on success, -1 on a malformed id, price, or name.
 */
static int	decode_item(const char *line, t_body_catalogue_item *item)
{
	char		field[BODY_LINE_MAX];
	uint64_t	id;
	int			used;

	memset(item, 0, sizeof(*item));
	used = 0;
	if (sscanf(line, "%1023s %n", field, &used) != 1 || used == 0)
		return (-1);
	if (body_parse_u64(field, &id) != 0 || id > UINT32_MAX)
		return (-1);
	item->id = (uint32_t)id;
	line += used;
	used = 0;
	if (sscanf(line, "%1023s %n", field, &used) != 1 || used == 0)
		return (-1);
	if (body_parse_u64(field, &item->price) != 0)
		return (-1);
	line += used;
	if (line[0] == '\0' || strlen(line) >= BODY_ITEM_NAME_MAX)
		return (-1);
	strcpy(item->name, line);
	return (0);
}
