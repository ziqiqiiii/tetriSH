/* ************************************************************************** */
/*                                                                            */
/*   player_write.c — player_t -> on-disk record (§4 schema), write side      */
/*                                                                            */
/*   Serialises one player document to the byte image stored by log.c. The    */
/*   read side (player_deserialise + decode helpers) lives in player_read.c.  */
/*                                                                            */
/*   The layout is fixed and little-endian so the log survives reboots and is */
/*   portable across hosts: integers are written byte by byte (never a raw    */
/*   struct memcpy, which would leak padding and host endianness), and the    */
/*   fixed-width char fields are stored at their full DB_* widths. Only the   */
/*   owned lists are variable: a count followed by exactly that many ids.     */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static void		put_u32(uint8_t *buf, size_t *off, uint32_t v);
static void		put_u64(uint8_t *buf, size_t *off, uint64_t v);
static void		put_bytes(uint8_t *buf, size_t *off, const void *src, size_t n);
static size_t	player_size(const t_player *p);

/**
 * @brief Serialise a player document into the byte image stored by log.c.
 *
 * Writes a fixed little-endian layout: the scalar fields, the fixed-width
 * string fields at full width, then each owned list as a u32 count followed
 * by that many u32 ids. Nothing is written unless the whole record fits, so
 * a return of 0 leaves buf untouched.
 *
 * @param p The player document to serialise.
 * @param buf Destination buffer.
 * @param cap Capacity of buf in bytes.
 * @return The number of bytes written, or 0 if the record would exceed cap.
 */
size_t	player_serialise(const t_player *p, uint8_t *buf, size_t cap)
{
	size_t	off;
	size_t	i;

	off = 0;
	if (player_size(p) > cap)
		return (0);
	put_u64(buf, &off, p->player_id);
	put_bytes(buf, &off, p->username, DB_MAX_USERNAME);
	put_bytes(buf, &off, p->password_hashed, DB_HASH_LEN);
	put_bytes(buf, &off, p->salt, DB_SALT_LEN);
	put_u64(buf, &off, (uint64_t)p->leaderboard_score);
	put_u64(buf, &off, (uint64_t)p->wallet_points);
	put_u32(buf, &off, p->current_equipped_character);
	put_u32(buf, &off, p->current_equipped_theme);
	put_u32(buf, &off, (uint32_t)p->owned_characters_count);
	i = 0;
	while (i < p->owned_characters_count)
		put_u32(buf, &off, p->owned_characters[i++]);
	put_u32(buf, &off, (uint32_t)p->owned_themes_count);
	i = 0;
	while (i < p->owned_themes_count)
		put_u32(buf, &off, p->owned_themes[i++]);
	put_u32(buf, &off, p->games_played);
	put_u32(buf, &off, p->games_won);
	return (off);
}

/**
 * @brief Append a u32 to buf little-endian and advance the cursor.
 *
 * @param buf Destination buffer (assumed large enough; checked by caller).
 * @param off In/out byte cursor, advanced by 4.
 * @param v The value to write.
 */
static void	put_u32(uint8_t *buf, size_t *off, uint32_t v)
{
	buf[(*off)++] = (uint8_t)(v & 0xFF);
	buf[(*off)++] = (uint8_t)((v >> 8) & 0xFF);
	buf[(*off)++] = (uint8_t)((v >> 16) & 0xFF);
	buf[(*off)++] = (uint8_t)((v >> 24) & 0xFF);
}

/**
 * @brief Append a u64 to buf little-endian and advance the cursor.
 *
 * @param buf Destination buffer (assumed large enough; checked by caller).
 * @param off In/out byte cursor, advanced by 8.
 * @param v The value to write.
 */
static void	put_u64(uint8_t *buf, size_t *off, uint64_t v)
{
	put_u32(buf, off, (uint32_t)(v & 0xFFFFFFFF));
	put_u32(buf, off, (uint32_t)(v >> 32));
}

/**
 * @brief Append n raw bytes to buf and advance the cursor.
 *
 * @param buf Destination buffer (assumed large enough; checked by caller).
 * @param off In/out byte cursor, advanced by n.
 * @param src Source bytes.
 * @param n Number of bytes to copy.
 */
static void	put_bytes(uint8_t *buf, size_t *off, const void *src, size_t n)
{
	memcpy(buf + *off, src, n);
	*off += n;
}

/**
 * @brief Compute the exact serialised size of a player document.
 *
 * Mirrors player_serialise's layout so the caller can bounds-check once up
 * front. The two owned lists contribute a u32 count plus 4 bytes per id.
 *
 * @param p The player document to measure.
 * @return The number of bytes player_serialise would write.
 */
static size_t	player_size(const t_player *p)
{
	size_t	n;

	n = sizeof(uint64_t);
	n += DB_MAX_USERNAME + DB_HASH_LEN + DB_SALT_LEN;
	n += 2 * sizeof(uint64_t);
	n += 2 * sizeof(uint32_t);
	n += sizeof(uint32_t) + p->owned_characters_count * sizeof(uint32_t);
	n += sizeof(uint32_t) + p->owned_themes_count * sizeof(uint32_t);
	n += 2 * sizeof(uint32_t);
	return (n);
}
