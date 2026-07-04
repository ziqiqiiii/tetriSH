/* ************************************************************************** */
/*                                                                            */
/*   log_append.c — frame and append one player record (§5, §7.2)             */
/*                                                                            */
/*   Write path for log.c. Builds [magic][key_len][val_len][username]         */
/*   [payload] in a single stack frame and emits it with one contiguous       */
/*   write so a concurrent append cannot interleave. Header fields are        */
/*   little-endian to match player_write.c, so the log is portable across     */
/*   hosts. No fsync here — durability is the flusher's 1s job (§7.3).         */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static void	put_u32_le(uint8_t *buf, uint32_t v);
static int	write_all(int fd, const uint8_t *buf, size_t n);

/**
 * @brief Append one whole-record player document to the log.
 *
 * Serialises the payload straight into the frame after the header and key,
 * fills in the little-endian header, then writes the whole frame at once. The
 * record is rejected if the payload would not fit under DB_FRAME_MAX (a 0 from
 * player_serialise). No fsync is performed — that is the flusher's job (§7.3).
 *
 * @param log The open log handle.
 * @param p The player document to append.
 * @return DB_OK on success, DB_INVALID if the record exceeds DB_FRAME_MAX,
 *         DB_IO_ERROR on a write failure.
 */
t_db_result	log_append(t_dblog *log, const t_player *p)
{
	uint8_t		frame[DB_FRAME_MAX];
	uint32_t	key_len;
	size_t		val_len;

	key_len = (uint32_t)strnlen(p->username, DB_MAX_USERNAME);
	val_len = player_serialise(p, frame + DB_FRAME_HDR + key_len, sizeof(frame) - DB_FRAME_HDR - key_len);
	if (val_len == 0)
		return (DB_INVALID);
	put_u32_le(frame, DB_LOG_MAGIC);
	put_u32_le(frame + 4, key_len);
	put_u32_le(frame + 8, (uint32_t)val_len);
	memcpy(frame + DB_FRAME_HDR, p->username, key_len);
	if (write_all(log->fd, frame, DB_FRAME_HDR + key_len + val_len) != 0)
		return (DB_IO_ERROR);
	return (DB_OK);
}

/**
 * @brief Write a u32 to buf little-endian.
 *
 * @param buf Destination of at least 4 bytes.
 * @param v The value to encode.
 */
static void	put_u32_le(uint8_t *buf, uint32_t v)
{
	buf[0] = (uint8_t)(v & 0xFF);
	buf[1] = (uint8_t)((v >> 8) & 0xFF);
	buf[2] = (uint8_t)((v >> 16) & 0xFF);
	buf[3] = (uint8_t)((v >> 24) & 0xFF);
}

/**
 * @brief Write exactly n bytes, retrying short writes and EINTR.
 *
 * @param fd Destination file descriptor.
 * @param buf Source bytes.
 * @param n Number of bytes to write.
 * @return 0 once all n bytes are written, -1 on a write error.
 */
static int	write_all(int fd, const uint8_t *buf, size_t n)
{
	ssize_t	w;
	size_t	done;

	done = 0;
	while (done < n)
	{
		w = write(fd, buf + done, n - done);
		if (w < 0)
		{
			if (errno == EINTR)
				continue ;
			return (-1);
		}
		done += (size_t)w;
	}
	return (0);
}
