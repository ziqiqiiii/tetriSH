/* ************************************************************************** */
/*                                                                            */
/*   flusher.c — background thread, fsync the log every 1s (§7.3)             */
/*                                                                            */
/*   Chosen durability: lose at most ~1s of writes on a crash. The thread     */
/*   holds no db rwlock; it only calls log_fsync. flusher_stop signals the    */
/*   thread, joins it, and performs a final fsync.                            */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// TODO: define struct s_flusher (pthread_t, dblog_t*, stop flag/condvar)
// TODO: static helper — flusher_loop(void *arg): 1s tick -> log_fsync
// TODO: flusher_start (spawn pthread) / flusher_stop (signal, join, final fsync)
