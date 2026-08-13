/* ************************************************************************** */
/*                                                                            */
/*   log_replay.c — replay the append-only log front->back (§5, §6)           */
/*                                                                            */
/*   Overflow file for log.c: reads each [magic][key_len][val_len][username]  */
/*   [payload] frame in write order and hands the decoded player to cb. Order */
/*   is preserved so a later record for a key supersedes an earlier one (LWW) */
/*   — the dedup itself is the caller's job (recovery.c). A truncated tail    */
/*   (torn last write) stops the scan cleanly rather than failing the boot,   */
/*   and the offset it stopped at is reported so the caller can cut it off:   */
/*   the fd is O_APPEND, so a tail left in place is one that the next append  */
/*   writes a valid frame *after*, and the boot after that meets the garbage  */
/*   mid-file, where a short read no longer looks like EOF.                   */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static int			read_all(int fd, uint8_t *buf, size_t n);
static t_db_result	replay_frame(int fd, void (*cb)(const t_player *, void *), void *ctx, int *done);

/**
 * @brief Replay every record in the log into cb, in write order.
 *
 * Seeks to the start and walks frame by frame, decoding each payload back into
 * a t_player and invoking cb. A clean EOF — including a torn final frame whose
 * header or body is short — ends the replay with DB_OK; only a read error or a
 * frame that fails validation aborts with DB_IO_ERROR. out_clean_end receives
 * the offset just past the last whole frame, which is where the file ends if
 * nothing was torn and where the torn tail begins if something was.
 *
 * @param log The open log handle.
 * @param cb Callback invoked once per decoded record, in order.
 * @param ctx Opaque context forwarded to cb.
 * @param out_clean_end Receives the end of the last whole frame (may be NULL).
 * @return DB_OK on a full clean replay, DB_IO_ERROR on a read/decode failure.
 */
t_db_result	log_replay(t_dblog *log, void (*cb)(const t_player *, void *), void *ctx, off_t *out_clean_end)
{
	t_db_result	r;
	off_t		clean;
	int			done;

	if (lseek(log->fd, 0, SEEK_SET) < 0)
		return (DB_IO_ERROR);
	clean = 0;
	done = 0;
	while (!done)
	{
		r = replay_frame(log->fd, cb, ctx, &done);
		if (r != DB_OK)
			return (r);
		if (!done)
		{
			clean = lseek(log->fd, 0, SEEK_CUR);
			if (clean < 0)
				return (DB_IO_ERROR);
		}
	}
	if (out_clean_end)
		*out_clean_end = clean;
	return (DB_OK);
}

/**
 * @brief Read, validate, and dispatch one frame from the log.
 *
 * Reads the fixed header, checks the magic and the DB_FRAME_MAX bound, then
 * reads the key and payload and decodes the payload into a t_player passed to
 * cb. A short read at any point is treated as a clean end of log (sets *done),
 * distinguishing a torn tail from genuine corruption mid-frame.
 *
 * @param fd The log file descriptor, positioned at a frame boundary.
 * @param cb Callback invoked with the decoded record.
 * @param ctx Opaque context forwarded to cb.
 * @param done Set to 1 when EOF (or a torn tail) is reached.
 * @return DB_OK on success or clean EOF, DB_IO_ERROR on a corrupt frame.
 */
static t_db_result	replay_frame(int fd, void (*cb)(const t_player *, void *), void *ctx, int *done)
{
	uint8_t		hdr[DB_FRAME_HDR];
	uint8_t		body[DB_FRAME_MAX];
	uint32_t	key_len;
	uint32_t	val_len;
	t_player	p;

	if (read_all(fd, hdr, DB_FRAME_HDR) != 0)
		return (*done = 1, DB_OK);
	if (((uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16)
			| ((uint32_t)hdr[3] << 24)) != DB_LOG_MAGIC)
		return (DB_IO_ERROR);
	memcpy(&key_len, hdr + 4, sizeof(key_len));
	memcpy(&val_len, hdr + 8, sizeof(val_len));
	if (key_len > DB_MAX_USERNAME
		|| (size_t)key_len + val_len > sizeof(body))
		return (DB_IO_ERROR);
	if (read_all(fd, body, (size_t)key_len + val_len) != 0)
		return (*done = 1, DB_OK);
	if (player_deserialise(body + key_len, val_len, &p) != DB_OK)
		return (DB_IO_ERROR);
	cb(&p, ctx);
	return (DB_OK);
}

/**
 * @brief Read exactly n bytes, retrying EINTR and reporting any short read.
 *
 * A short read (including immediate EOF) returns -1 so the caller can treat a
 * truncated tail as the end of the log rather than read past it.
 *
 * @param fd Source file descriptor.
 * @param buf Destination buffer of at least n bytes.
 * @param n Number of bytes to read.
 * @return 0 if all n bytes were read, -1 on EOF, short read, or error.
 */
static int	read_all(int fd, uint8_t *buf, size_t n)
{
	ssize_t	r;
	size_t	done;

	done = 0;
	while (done < n)
	{
		r = read(fd, buf + done, n - done);
		if (r < 0)
		{
			if (errno == EINTR)
				continue ;
			return (-1);
		}
		if (r == 0)
			return (-1);
		done += (size_t)r;
	}
	return (0);
}
