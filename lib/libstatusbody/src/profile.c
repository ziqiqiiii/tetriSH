#include "sb_util.h"

#include <inttypes.h>

// Static Functions
static int	encode_owned(const char *key, const uint32_t *ids, size_t count, char *out, size_t cap, size_t *off);
static int	decode_identity(t_sb_cursor *c, t_sb_profile *out);
static int	decode_equipped(t_sb_cursor *c, t_sb_profile *out);
static int	decode_owned(t_sb_cursor *c, const char *key, uint32_t *ids, size_t *out_count);
static int	key_value(const char *line, const char *key, const char **value);
static int	take_number(const char *line, int *used, uint64_t *out);

/**
 * @brief Serialises the UC-20 ProfileView body: one key per line, owned
 * lists count-prefixed (format per the header comment).
 *
 * @param in The profile to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes, or -1 with errno = EINVAL (NULL args,
 *         empty username, owned counts > SB_OWNED_MAX) or ERANGE (cap
 *         too small).
 */
int	sb_profile_encode(const t_sb_profile *in, char *out, size_t cap)
{
	size_t	off;

	if (!in || !out)
		return (sb_fail(EINVAL));
	if (in->username[0] == '\0' || in->rank < 0
		|| in->owned_character_count > SB_OWNED_MAX
		|| in->owned_theme_count > SB_OWNED_MAX)
		return (sb_fail(EINVAL));
	off = 0;
	if (sb_append(out, cap, &off, "username %s\n", in->username) != 0
		|| sb_append(out, cap, &off, "wallet %" PRIu64 "\n", in->wallet) != 0
		|| sb_append(out, cap, &off, "score %" PRIu64 "\n", in->score) != 0
		|| sb_append(out, cap, &off, "rank %d\n", in->rank) != 0
		|| sb_append(out, cap, &off, "equipped_character %" PRIu32 "\n",
			in->equipped_character) != 0
		|| sb_append(out, cap, &off, "equipped_theme %" PRIu32 "\n",
			in->equipped_theme) != 0
		|| encode_owned("owned_characters", in->owned_characters,
			in->owned_character_count, out, cap, &off) != 0
		|| encode_owned("owned_themes", in->owned_themes,
			in->owned_theme_count, out, cap, &off) != 0)
		return (sb_fail(ERANGE));
	return ((int)off);
}

/**
 * @brief Parses a ProfileView body back into a profile.
 *
 * Strict: keys in encode order, every key present, owned counts within
 * SB_OWNED_MAX, username within SB_USER_MAX.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param out Receives the decoded profile, fully overwritten on success.
 * @return 0 on success, -1 with errno = EINVAL (NULL args) or EBADMSG
 *         (missing key, malformed value, overlong name, list overflow).
 */
int	sb_profile_decode(const char *buf, size_t len, t_sb_profile *out)
{
	t_sb_cursor	c;

	if (!buf || !out)
		return (sb_fail(EINVAL));
	memset(out, 0, sizeof(*out));
	c.p = buf;
	c.end = buf + len;
	if (decode_identity(&c, out) != 0
		|| decode_equipped(&c, out) != 0
		|| decode_owned(&c, "owned_characters", out->owned_characters,
			&out->owned_character_count) != 0
		|| decode_owned(&c, "owned_themes", out->owned_themes,
			&out->owned_theme_count) != 0
		|| !sb_at_end(&c))
		return (sb_fail(EBADMSG));
	return (0);
}

/**
 * @brief Writes one count-prefixed owned-id list line.
 *
 * @param key The line's key, owned_characters or owned_themes.
 * @param ids The ids to write.
 * @param count How many ids to write.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_owned(const char *key, const uint32_t *ids, size_t count,
		char *out, size_t cap, size_t *off)
{
	size_t	i;

	if (sb_append(out, cap, off, "%s %zu", key, count) != 0)
		return (-1);
	i = 0;
	while (i < count)
	{
		if (sb_append(out, cap, off, " %" PRIu32, ids[i]) != 0)
			return (-1);
		i++;
	}
	return (sb_append(out, cap, off, "\n"));
}

/**
 * @brief Reads the username, wallet, score, and rank lines in order.
 *
 * @param c The body cursor.
 * @param out The profile being filled.
 * @return 0 on success, -1 on a missing, misordered, or malformed line.
 */
static int	decode_identity(t_sb_cursor *c, t_sb_profile *out)
{
	char		line[SB_LINE_MAX];
	const char	*value;
	uint64_t	rank;

	if (sb_take_line(c, line, sizeof(line)) != 0
		|| key_value(line, "username", &value) != 0
		|| *value == '\0' || strlen(value) >= SB_USER_MAX
		|| strchr(value, ' '))
		return (-1);
	strcpy(out->username, value);
	if (sb_take_line(c, line, sizeof(line)) != 0
		|| key_value(line, "wallet", &value) != 0
		|| sb_parse_u64(value, &out->wallet) != 0)
		return (-1);
	if (sb_take_line(c, line, sizeof(line)) != 0
		|| key_value(line, "score", &value) != 0
		|| sb_parse_u64(value, &out->score) != 0)
		return (-1);
	if (sb_take_line(c, line, sizeof(line)) != 0
		|| key_value(line, "rank", &value) != 0
		|| sb_parse_u64(value, &rank) != 0 || rank > INT32_MAX)
		return (-1);
	out->rank = (int)rank;
	return (0);
}

/**
 * @brief Reads the equipped_character and equipped_theme lines.
 *
 * @param c The body cursor.
 * @param out The profile being filled.
 * @return 0 on success, -1 on a missing, misordered, or malformed line.
 */
static int	decode_equipped(t_sb_cursor *c, t_sb_profile *out)
{
	char		line[SB_LINE_MAX];
	const char	*value;
	uint64_t	id;

	if (sb_take_line(c, line, sizeof(line)) != 0
		|| key_value(line, "equipped_character", &value) != 0
		|| sb_parse_u64(value, &id) != 0 || id > UINT32_MAX)
		return (-1);
	out->equipped_character = (uint32_t)id;
	if (sb_take_line(c, line, sizeof(line)) != 0
		|| key_value(line, "equipped_theme", &value) != 0
		|| sb_parse_u64(value, &id) != 0 || id > UINT32_MAX)
		return (-1);
	out->equipped_theme = (uint32_t)id;
	return (0);
}

/**
 * @brief Reads one count-prefixed owned-id list line.
 *
 * The count leads the line, so a list claiming more ids than SB_OWNED_MAX
 * is rejected before a single id is stored.
 *
 * @param c The body cursor.
 * @param key The expected key, owned_characters or owned_themes.
 * @param ids The array receiving the ids.
 * @param out_count Receives how many ids were read.
 * @return 0 on success, -1 on a malformed line or an over-long list.
 */
static int	decode_owned(t_sb_cursor *c, const char *key, uint32_t *ids,
		size_t *out_count)
{
	char		line[SB_LINE_MAX];
	uint64_t	value;
	int			used;
	int			i;

	if (sb_take_line(c, line, sizeof(line)) != 0
		|| strncmp(line, key, strlen(key)) != 0)
		return (-1);
	used = (int)strlen(key);
	if (take_number(line, &used, &value) != 0 || value > SB_OWNED_MAX)
		return (-1);
	*out_count = (size_t)value;
	i = 0;
	while ((size_t)i < *out_count)
	{
		if (take_number(line, &used, &value) != 0 || value > UINT32_MAX)
			return (-1);
		ids[i] = (uint32_t)value;
		i++;
	}
	if (line[used] != '\0')
		return (-1);
	return (0);
}

/**
 * @brief Splits a `key value` line, checking the key is the expected one.
 *
 * @param line The line text, newline already stripped.
 * @param key The key the line must carry.
 * @param value Receives a pointer into line, just past "key ".
 * @return 0 on success, -1 when the key does not match.
 */
static int	key_value(const char *line, const char *key, const char **value)
{
	size_t	n;

	n = strlen(key);
	if (strncmp(line, key, n) != 0 || line[n] != ' ')
		return (-1);
	*value = line + n + 1;
	return (0);
}

/**
 * @brief Reads the next space-separated number from a line, in place.
 *
 * @param line The line being walked.
 * @param used In/out offset into line, advanced past the number.
 * @param out Receives the parsed value.
 * @return 0 on success, -1 when no unsigned number follows.
 */
static int	take_number(const char *line, int *used, uint64_t *out)
{
	unsigned long long	v;
	int					n;

	if (line[*used] != ' ' || line[*used + 1] < '0' || line[*used + 1] > '9')
		return (-1);
	if (sscanf(line + *used, " %llu%n", &v, &n) != 1)
		return (-1);
	*used += n;
	*out = (uint64_t)v;
	return (0);
}
