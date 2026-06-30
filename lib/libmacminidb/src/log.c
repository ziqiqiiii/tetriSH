/* ************************************************************************** */
/*                                                                            */
/*   log.c — append-only player log, the one source of truth on disk          */
/*                                                                            */
/*   §5, §7.2: Bitcask-style whole-record LWW — every write appends the full  */
/*   player document. Record framing:                                         */
/*       [magic/ver u32][key_len u32][val_len u32][username][payload]         */
/*   Replay scans front->back so later records for a key overwrite earlier    */
/*   ones. A record may not exceed DB_FRAME_MAX (64 KiB).                     */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// TODO: define struct s_dblog (fd/FILE* + path)
// TODO: log_open / log_close
// TODO: log_append (frame the record, write, do NOT fsync) / log_fsync
// TODO: log_replay (read frames in order, deserialise, invoke cb)
