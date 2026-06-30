/* ************************************************************************** */
/*                                                                            */
/*   flusher.c — background thread, fsync the log every 1s (§7.3)             */
/*                                                                            */
/*   Chosen durability: lose at most ~1s of writes on a crash. The thread     */
/*   holds no db rwlock; it only calls log_fsync. flusher_stop signals the    */
/*   thread, joins it, and performs a final fsync. The fsync runs outside the */
/*   flusher's own mutex, so no lock is ever held across the blocking syscall. */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static void	*flusher_loop(void *arg);
static int	wait_tick(t_flusher *f);

/**
 * @brief Spawn the background thread that fsyncs the log once a second.
 *
 * Allocates the handle, initialises its mutex and condition variable, and
 * starts the worker. Any failure unwinds whatever was set up so far and yields
 * NULL; the log itself is borrowed, not owned, so it is left untouched.
 *
 * @param log The open log handle to sync (must outlive the flusher).
 * @return The running flusher handle, or NULL on alloc/init/spawn failure.
 */
t_flusher	*flusher_start(t_dblog *log)
{
	t_flusher	*f;

	if (!log)
		return (NULL);
	f = malloc(sizeof(*f));
	if (!f)
		return (NULL);
	f->log = log;
	f->stop = 0;
	if (pthread_mutex_init(&f->lock, NULL) != 0)
		return (free(f), NULL);
	if (pthread_cond_init(&f->cond, NULL) != 0)
		return (pthread_mutex_destroy(&f->lock), free(f), NULL);
	if (pthread_create(&f->thread, NULL, flusher_loop, f) != 0)
	{
		pthread_cond_destroy(&f->cond);
		pthread_mutex_destroy(&f->lock);
		return (free(f), NULL);
	}
	return (f);
}

/**
 * @brief Signal the worker, join it, do a final fsync, and free the handle.
 *
 * Sets the stop flag under the lock and wakes the worker so it returns without
 * waiting out the current tick, joins it, then performs one last fsync so no
 * buffered write is lost at shutdown. Safe to call with a NULL handle.
 *
 * @param f The flusher to stop (may be NULL).
 */
void	flusher_stop(t_flusher *f)
{
	if (!f)
		return ;
	pthread_mutex_lock(&f->lock);
	f->stop = 1;
	pthread_cond_signal(&f->cond);
	pthread_mutex_unlock(&f->lock);
	pthread_join(f->thread, NULL);
	log_fsync(f->log);
	pthread_cond_destroy(&f->cond);
	pthread_mutex_destroy(&f->lock);
	free(f);
}

/**
 * @brief Worker body: wait up to 1s, fsync, repeat until stopped.
 *
 * wait_tick blocks under the lock; the fsync then runs with the lock released,
 * so the blocking syscall never holds the mutex. A failed fsync is left for the
 * next tick (or the final fsync in flusher_stop) to retry rather than aborting.
 *
 * @param arg The t_flusher handle.
 * @return NULL; the thread exits once stop is observed.
 */
static void	*flusher_loop(void *arg)
{
	t_flusher	*f;

	f = arg;
	while (!wait_tick(f))
		log_fsync(f->log);
	return (NULL);
}

/**
 * @brief Wait one tick (~1s) or until stopped, under the flusher's lock.
 *
 * Sleeps on the condvar until either the 1s deadline elapses or flusher_stop
 * signals it. The lock is released around the implicit blocking wait by
 * pthread_cond_timedwait and is dropped before returning, so the caller's fsync
 * runs lock-free.
 *
 * @param f The flusher handle.
 * @return 1 if the worker should stop, 0 to perform another fsync.
 */
static int	wait_tick(t_flusher *f)
{
	struct timespec	deadline;
	int				stop;

	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += DB_FLUSH_INTERVAL_S;
	pthread_mutex_lock(&f->lock);
	while (!f->stop)
		if (pthread_cond_timedwait(&f->cond, &f->lock, &deadline) == ETIMEDOUT)
			break ;
	stop = f->stop;
	pthread_mutex_unlock(&f->lock);
	return (stop);
}
