/* ************************************************************************** */
/*                                                                            */
/*   player_read.c — on-disk record -> player_t (§4 schema), read side        */
/*                                                                            */
/*   Reverses the write side: rebuilds a player document from the byte image  */
/*   stored by log.c. Every read is bounds-checked against the buffer length  */
/*   so a short or corrupt record is rejected, never read out of bounds. See  */
/*   player_write.c for the on-disk layout this mirrors.                      */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static int			get_u32(const uint8_t *buf, size_t len, size_t *off, uint32_t *out);
static int			get_u64(const uint8_t *buf, size_t len, size_t *off, uint64_t *out);
static int			get_bytes(const uint8_t *buf, size_t len, size_t *off, void *dst, size_t n);
static t_db_result	read_owned(const uint8_t *buf, size_t len, size_t *off, t_item_id *ids, size_t *count);

/**
 * @brief Rebuild a player document from a serialised byte image.
 *
 * Reverses player_serialise, validating every read against len so a short or
 * malformed buffer is rejected rather than read out of bounds. Owned counts
 * are bounded by DB_MAX_OWNED. *out is only partially valid on failure.
 *
 * @param buf Source buffer holding the serialised record.
 * @param len Length of buf in bytes.
 * @param out Destination player document.
 * @return DB_OK on success, DB_INVALID on a short or malformed buffer.
 */
t_db_result	player_deserialise(const uint8_t *buf, size_t len, t_player *out)
{
	size_t		off;
	uint64_t	scratch;

	off = 0;
	memset(out, 0, sizeof(*out));
	if (!get_u64(buf, len, &off, &out->player_id)
		|| !get_bytes(buf, len, &off, out->username, DB_MAX_USERNAME)
		|| !get_bytes(buf, len, &off, out->password_hashed, DB_HASH_LEN)
		|| !get_bytes(buf, len, &off, out->salt, DB_SALT_LEN))
		return (DB_INVALID);
	if (!get_u64(buf, len, &off, &scratch))
		return (DB_INVALID);
	out->leaderboard_score = (int64_t)scratch;
	if (!get_u64(buf, len, &off, &scratch))
		return (DB_INVALID);
	out->wallet_points = (int64_t)scratch;
	if (!get_u32(buf, len, &off, &out->current_equipped_character)
		|| !get_u32(buf, len, &off, &out->current_equipped_theme))
		return (DB_INVALID);
	if (read_owned(buf, len, &off, out->owned_characters,
			&out->owned_characters_count) != DB_OK
		|| read_owned(buf, len, &off, out->owned_themes,
			&out->owned_themes_count) != DB_OK)
		return (DB_INVALID);
	if (!get_u32(buf, len, &off, &out->games_played)
		|| !get_u32(buf, len, &off, &out->games_won))
		return (DB_INVALID);
	return (DB_OK);
}

/**
 * @brief Read a little-endian u32, advancing the cursor if it fits.
 *
 * @param buf Source buffer.
 * @param len Length of buf.
 * @param off In/out byte cursor, advanced by 4 on success.
 * @param out Where the decoded value is stored.
 * @return 1 on success, 0 if fewer than 4 bytes remain.
 */
static int	get_u32(const uint8_t *buf, size_t len, size_t *off, uint32_t *out)
{
	if (*off + sizeof(uint32_t) > len)
		return (0);
	*out = (uint32_t)buf[*off]
		| ((uint32_t)buf[*off + 1] << 8)
		| ((uint32_t)buf[*off + 2] << 16)
		| ((uint32_t)buf[*off + 3] << 24);
	*off += sizeof(uint32_t);
	return (1);
}

/**
 * @brief Read a little-endian u64, advancing the cursor if it fits.
 *
 * @param buf Source buffer.
 * @param len Length of buf.
 * @param off In/out byte cursor, advanced by 8 on success.
 * @param out Where the decoded value is stored.
 * @return 1 on success, 0 if fewer than 8 bytes remain.
 */
static int	get_u64(const uint8_t *buf, size_t len, size_t *off, uint64_t *out)
{
	uint32_t	lo;
	uint32_t	hi;

	if (!get_u32(buf, len, off, &lo) || !get_u32(buf, len, off, &hi))
		return (0);
	*out = (uint64_t)lo | ((uint64_t)hi << 32);
	return (1);
}

/**
 * @brief Copy n raw bytes out of buf, advancing the cursor if they fit.
 *
 * @param buf Source buffer.
 * @param len Length of buf.
 * @param off In/out byte cursor, advanced by n on success.
 * @param dst Destination for the copied bytes.
 * @param n Number of bytes to copy.
 * @return 1 on success, 0 if fewer than n bytes remain.
 */
static int	get_bytes(const uint8_t *buf, size_t len, size_t *off, void *dst, size_t n)
{
	if (*off + n > len)
		return (0);
	memcpy(dst, buf + *off, n);
	*off += n;
	return (1);
}

/**
 * @brief Read an owned list (u32 count then that many u32 ids) from buf.
 *
 * The count is rejected if it exceeds DB_MAX_OWNED, guarding the fixed-size
 * destination array against an oversized or corrupt record.
 *
 * @param buf Source buffer.
 * @param len Length of buf.
 * @param off In/out byte cursor, advanced past the list on success.
 * @param ids Destination array of at least DB_MAX_OWNED ids.
 * @param count Where the decoded element count is stored.
 * @return DB_OK on success, DB_INVALID on a short or oversized list.
 */
static t_db_result	read_owned(const uint8_t *buf, size_t len, size_t *off, t_item_id *ids, size_t *count)
{
	uint32_t	n;
	uint32_t	i;

	if (!get_u32(buf, len, off, &n))
		return (DB_INVALID);
	if (n > DB_MAX_OWNED)
		return (DB_INVALID);
	i = 0;
	while (i < n)
	{
		if (!get_u32(buf, len, off, &ids[i]))
			return (DB_INVALID);
		i++;
	}
	*count = n;
	return (DB_OK);
}
