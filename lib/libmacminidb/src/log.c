/* ************************************************************************** */
/*                                                                            */
/*   log.c — append-only player log: open / close / fsync lifecycle           */
/*                                                                            */
/*   §5, §7.2: Bitcask-style whole-record LWW. This file owns the log handle  */
/*   lifecycle; the write path lives in log_append.c and replay in            */
/*   log_replay.c. Record framing (shared across the three files):            */
/*       [magic/ver u32][key_len u32][val_len u32][username][payload]         */
/*   A record may not exceed DB_FRAME_MAX (64 KiB).                           */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Open (creating if absent) the append-only log under data_dir.
 *
 * The file is opened O_RDWR|O_APPEND|O_CREAT so appends land atomically at the
 * end-of-file and the same fd can be re-read front->back by log_replay. The
 * resolved path <data_dir>/players.log is kept for diagnostics only.
 *
 * @param data_dir Directory that holds (or will hold) the log file.
 * @return The opened log handle, or NULL on an allocation or open failure.
 */
t_dblog	*log_open(const char *data_dir)
{
	t_dblog	*log;
	int		n;

	log = malloc(sizeof(*log));
	if (!log)
		return (NULL);
	n = snprintf(log->path, sizeof(log->path), "%s/%s", data_dir, DB_LOG_NAME);
	if (n < 0 || (size_t)n >= sizeof(log->path))
	{
		free(log);
		return (NULL);
	}
	log->fd = open(log->path, O_RDWR | O_APPEND | O_CREAT, 0644);
	if (log->fd < 0)
	{
		free(log);
		return (NULL);
	}
	return (log);
}

/**
 * @brief Close the log file and free the handle.
 *
 * Safe to call with a NULL handle. Does not fsync — the caller (flusher /
 * db_close) is responsible for the final durability barrier beforehand.
 *
 * @param log The log handle to close (may be NULL).
 */
void	log_close(t_dblog *log)
{
	if (!log)
		return ;
	if (log->fd >= 0)
		close(log->fd);
	free(log);
}

/**
 * @brief Force the log's buffered writes through to stable storage.
 *
 * The single durability barrier in the library; called on the flusher's 1s
 * tick and once more at shutdown. fdatasync suffices — only the data and the
 * length need to be durable, not the inode timestamps.
 *
 * @param log The open log handle.
 * @return DB_OK on success, DB_IO_ERROR if the sync failed.
 */
t_db_result	log_fsync(t_dblog *log)
{
	if (fdatasync(log->fd) != 0)
		return (DB_IO_ERROR);
	return (DB_OK);
}
