/* ************************************************************************** */
/*                                                                            */
/*   test_logd.c - the daemon loop                                            */
/*                                                                            */
/*   Records arrive as real datagrams on a real socket, exactly as tetrisd's  */
/*   shipper sends them. These cases pin the three fates of a record          */
/*   (written, rejected, degraded), the single-instance guard, and the        */
/*   promise that a stop still drains what the kernel already accepted.       */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

// Static Functions
static void	test_start_binds_and_opens_the_sink(void);
static void	test_a_valid_record_is_written_and_counted(void);
static void	test_a_malformed_record_is_rejected(void);
static void	test_a_short_datagram_is_rejected(void);
static void	test_an_oversized_datagram_is_rejected(void);
static void	test_a_burst_drains_in_one_iteration(void);
static void	test_records_degrade_when_the_sink_is_gone(void);
static void	test_the_sink_recovers_on_the_idle_tick(void);
static void	test_a_deleted_log_file_is_reclaimed(void);
static void	test_a_second_instance_refuses_to_start(void);
static void	test_stop_drains_what_is_still_queued(void);
static void	test_stop_reclaims_a_deleted_log_before_signing_off(void);
static void	test_stop_is_safe_on_a_blank_daemon(void);

static int	boot(t_fixture *fx, t_logd *lg);

int	main(void)
{
	test_start_binds_and_opens_the_sink();
	test_a_valid_record_is_written_and_counted();
	test_a_malformed_record_is_rejected();
	test_a_short_datagram_is_rejected();
	test_an_oversized_datagram_is_rejected();
	test_a_burst_drains_in_one_iteration();
	test_records_degrade_when_the_sink_is_gone();
	test_the_sink_recovers_on_the_idle_tick();
	test_a_deleted_log_file_is_reclaimed();
	test_a_second_instance_refuses_to_start();
	test_stop_drains_what_is_still_queued();
	test_stop_reclaims_a_deleted_log_before_signing_off();
	test_stop_is_safe_on_a_blank_daemon();
	return (0);
}

/*
** The sink is claimed before the socket is bound, so the boot line is already
** in the file by the time anything can send to it. That line is also the only
** record of which socket this logger is listening on.
*/
static void	test_start_binds_and_opens_the_sink(void)
{
	t_fixture	fx;
	struct stat	st;
	t_logd		lg;

	assert(boot(&fx, &lg) == 0);
	assert(stat(fx.sock_path, &st) == 0 && S_ISSOCK(st.st_mode));
	assert(sink_is_open(&lg.sink) == true);
	assert(lg.running == true);
	assert(fx_contains(fx.file_path, TETRISLOGD_COMPONENT_NAME) == 1);
	logd_stop(&lg);
	assert(stat(fx.sock_path, &st) == -1);
	fx_destroy(&fx);
	printf("PASS test_start_binds_and_opens_the_sink\n");
}

static void	test_a_valid_record_is_written_and_counted(void)
{
	t_fixture	fx;
	uint64_t	before;
	t_logd		lg;
	int			tx;

	assert(boot(&fx, &lg) == 0);
	before = lg.count.written;
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send(tx, COREIPC_LOG_INFO, "room 3 started") == 0);
	assert(logd_run_once(&lg) == 0);
	assert(lg.count.written == before + 1);
	assert(lg.count.rejected == 0 && lg.count.degraded == 0);
	assert(fx_contains(fx.file_path, "room 3 started") == 1);
	assert(fx_contains(fx.file_path, "INFO") == 1);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_a_valid_record_is_written_and_counted\n");
}

/*
** The sink is a file operators read line by line, so a record that fails
** validation is discarded rather than written - one malformed datagram must
** not be able to corrupt a line of it.
*/
static void	test_a_malformed_record_is_rejected(void)
{
	t_log_record	rec;
	t_fixture		fx;
	uint64_t		before;
	t_logd			lg;
	int				tx;

	assert(boot(&fx, &lg) == 0);
	before = lg.count.written;
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(logrecord_make(&rec, COREIPC_LOG_ERROR, 1, 2, "tetrisd", "bad") == 0);
	rec.magic = 0xDEADBEEFu;
	assert(fx_send_raw(tx, &rec, sizeof(rec)) == 0);
	assert(logd_run_once(&lg) == 0);
	assert(lg.count.rejected == 1);
	assert(lg.count.written == before);
	assert(fx_contains(fx.file_path, "bad") == 0);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_a_malformed_record_is_rejected\n");
}

static void	test_a_short_datagram_is_rejected(void)
{
	t_fixture	fx;
	uint64_t	before;
	t_logd		lg;
	int			tx;

	assert(boot(&fx, &lg) == 0);
	before = lg.count.written;
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send_raw(tx, "truncated", 9) == 0);
	assert(logd_run_once(&lg) == 0);
	assert(lg.count.rejected == 1);
	assert(lg.count.written == before);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_a_short_datagram_is_rejected\n");
}

/*
** A datagram longer than a record must not be quietly truncated into one that
** validates. The loop receives into a buffer one byte larger than a record
** precisely so an over-long datagram comes back over-long and fails the size
** check, instead of having its tail cut off and its head accepted.
*/
static void	test_an_oversized_datagram_is_rejected(void)
{
	unsigned char	buf[sizeof(t_log_record) + 64];
	t_fixture		fx;
	uint64_t		before;
	t_logd			lg;
	int				tx;

	assert(boot(&fx, &lg) == 0);
	before = lg.count.written;
	tx = fx_producer(&fx);
	assert(tx >= 0);
	memset(buf, 0, sizeof(buf));
	assert(logrecord_make((t_log_record *)buf, COREIPC_LOG_INFO, 1, 2,
			"tetrisd", "valid head, junk tail") == 0);
	assert(fx_send_raw(tx, buf, sizeof(buf)) == 0);
	assert(logd_run_once(&lg) == 0);
	assert(lg.count.rejected == 1);
	assert(lg.count.written == before);
	assert(fx_contains(fx.file_path, "valid head") == 0);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_an_oversized_datagram_is_rejected\n");
}

/*
** One wake-up drains everything the socket holds. Handling a single record
** per poll would let a burst outrun the loop and fill the receive buffer,
** which pushes the sender into its stderr fallback for no reason.
*/
static void	test_a_burst_drains_in_one_iteration(void)
{
	t_fixture	fx;
	uint64_t	before;
	t_logd		lg;
	int			tx;
	int			i;

	assert(boot(&fx, &lg) == 0);
	before = lg.count.written;
	tx = fx_producer(&fx);
	assert(tx >= 0);
	i = 0;
	while (i < 8)
	{
		assert(fx_send(tx, COREIPC_LOG_DEBUG, "burst") == 0);
		i++;
	}
	assert(logd_run_once(&lg) == 0);
	assert(lg.count.written == before + 8);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_a_burst_drains_in_one_iteration\n");
}

/*
** Degraded is counted separately from Rejected because the two blame different
** things: a Rejected record was malformed, a Degraded one was fine and the sink
** was not. Under dspawn stderr is the daemon's own .err file, so the record
** survives and this counter says how many took that route.
*/
static void	test_records_degrade_when_the_sink_is_gone(void)
{
	t_fixture	fx;
	uint64_t	before;
	t_logd		lg;
	int			tx;

	assert(boot(&fx, &lg) == 0);
	before = lg.count.written;
	sink_close(&lg.sink);
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send(tx, COREIPC_LOG_WARNING, "sink is gone") == 0);
	assert(logd_run_once(&lg) == 0);
	assert(lg.count.degraded == 1);
	assert(lg.count.written == before);
	assert(lg.count.rejected == 0);
	assert(lg.running == true);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_records_degrade_when_the_sink_is_gone\n");
}

/*
** The retry rides the idle tick, so it costs nothing on a busy logger and
** still recovers a sink that came back. An iteration with a record waiting
** does not retry - the timeout branch is the only place it happens.
*/
static void	test_the_sink_recovers_on_the_idle_tick(void)
{
	t_fixture	fx;
	t_logd		lg;
	int			tx;

	assert(boot(&fx, &lg) == 0);
	sink_close(&lg.sink);
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send(tx, COREIPC_LOG_ERROR, "while degraded") == 0);
	assert(logd_run_once(&lg) == 0);
	assert(sink_is_open(&lg.sink) == false);
	assert(logd_run_once(&lg) == 0);
	assert(sink_is_open(&lg.sink) == true);
	assert(fx_send(tx, COREIPC_LOG_ERROR, "after recovery") == 0);
	assert(logd_run_once(&lg) == 0);
	assert(fx_contains(fx.file_path, "after recovery") == 1);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_the_sink_recovers_on_the_idle_tick\n");
}

/*
** `make reset` deletes tmp/ under a running logger. Writes to the unlinked
** inode keep succeeding, so nothing fails and the log file simply does not
** exist - which is what "dspawn tetrislogd writes no boot line" actually was.
** The idle tick has to notice and reopen, and the records that follow have to
** land in the file an operator can read.
*/
static void	test_a_deleted_log_file_is_reclaimed(void)
{
	t_fixture	fx;
	struct stat	st;
	t_logd		lg;
	int			tx;

	assert(boot(&fx, &lg) == 0);
	assert(unlink(fx.file_path) == 0);
	assert(stat(fx.file_path, &st) == -1);
	assert(sink_is_stale(&lg.sink) == true);
	assert(logd_run_once(&lg) == 0);
	assert(sink_is_stale(&lg.sink) == false);
	assert(stat(fx.file_path, &st) == 0);
	assert(fx_contains(fx.file_path, "sink replaced") == 1);
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send(tx, COREIPC_LOG_INFO, "after the wipe") == 0);
	assert(logd_run_once(&lg) == 0);
	assert(fx_contains(fx.file_path, "after the wipe") == 1);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_a_deleted_log_file_is_reclaimed\n");
}

/*
** unixsock_dgram_bind unlinks the path before binding, so a second launch that got
** as far as logd_start would silently steal the socket and leave the first
** logger deaf. Nothing inside logd_start prevents that any more - the guard
** is the pidfile main.c claims first (docs/adr/0007), and this case pins the
** ordering that makes it a guard at all: the claim fails, so the bind that
** would have done the damage is never reached.
**
** flock is held per open file description, so a second claim fails even from
** this same process - which is what lets the case stay in-process.
*/
static void	test_a_second_instance_refuses_to_start(void)
{
	t_fixture	fx;
	struct stat	st;
	t_pidfile	held;
	t_pidfile	loser;
	t_logd		first;

	assert(boot(&fx, &first) == 0);
	daemon_pid_blank(&held);
	daemon_pid_blank(&loser);
	assert(daemon_pid_claim(&held, fx.cfg.pid_path) == 0);
	errno = 0;
	assert(daemon_pid_claim(&loser, fx.cfg.pid_path) == -1);
	assert(errno == EWOULDBLOCK || errno == EAGAIN);
	assert(stat(fx.sock_path, &st) == 0 && S_ISSOCK(st.st_mode));
	assert(sink_is_open(&first.sink) == true);
	daemon_pid_release(&held);
	logd_stop(&first);
	fx_destroy(&fx);
	printf("PASS test_a_second_instance_refuses_to_start\n");
}

/*
** SIGTERM arrives while records are still in the receive buffer. Those are
** records the kernel already accepted on this process's behalf, so dropping
** them at shutdown would lose exactly the last few lines before a restart -
** the ones worth reading.
*/
static void	test_stop_drains_what_is_still_queued(void)
{
	t_fixture	fx;
	t_logd		lg;
	int			tx;

	assert(boot(&fx, &lg) == 0);
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send(tx, COREIPC_LOG_INFO, "last words") == 0);
	close(tx);
	logd_stop(&lg);
	assert(fx_contains(fx.file_path, "last words") == 1);
	fx_destroy(&fx);
	printf("PASS test_stop_drains_what_is_still_queued\n");
}

/*
** A shutdown can arrive before any idle tick has run, so the stop path cannot
** assume the sink is still on a file that exists. The exit line is the one an
** operator goes looking for after a daemon disappears - signing off into a
** deleted inode loses exactly that.
*/
static void	test_stop_reclaims_a_deleted_log_before_signing_off(void)
{
	t_fixture	fx;
	struct stat	st;
	t_logd		lg;

	assert(boot(&fx, &lg) == 0);
	assert(unlink(fx.file_path) == 0);
	logd_stop(&lg);
	assert(stat(fx.file_path, &st) == 0);
	assert(fx_contains(fx.file_path, "exit") == 1);
	fx_destroy(&fx);
	printf("PASS test_stop_reclaims_a_deleted_log_before_signing_off\n");
}

static void	test_stop_is_safe_on_a_blank_daemon(void)
{
	t_logd	lg;

	logd_blank(&lg);
	assert(lg.sock_fd == -1);
	assert(lg.idle_ms == TETRISLOGD_IDLE_MS);
	logd_stop(&lg);
	logd_stop(&lg);
	printf("PASS test_stop_is_safe_on_a_blank_daemon\n");
}

/*
** Every case starts the same way; the shortened idle tick keeps the timeout
** path in the tests measured in milliseconds rather than seconds.
*/
static int	boot(t_fixture *fx, t_logd *lg)
{
	if (fx_make(fx) != 0)
		return (-1);
	logd_blank(lg);
	if (logd_start(lg, &fx->cfg) != 0)
		return (-1);
	lg->idle_ms = FIXTURE_IDLE_MS;
	return (0);
}
