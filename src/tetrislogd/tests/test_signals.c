/* ************************************************************************** */
/*                                                                            */
/*   test_signals.c - the four signals the logger answers to                  */
/*                                                                            */
/*   Handlers only record and wake; every consequence happens on the loop     */
/*   thread. These cases pin that split, the coalescing of repeats, and what  */
/*   each signal actually does to the sink once the loop notices it.          */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

// Static Functions
static void	test_take_starts_empty_and_clears(void);
static void	test_repeated_signals_coalesce(void);
static void	test_term_and_int_stop_the_loop(void);
static void	test_hup_rotates_the_sink(void);
static void	test_usr1_reports_the_counters(void);
static void	test_sigpipe_is_ignored(void);

static int	boot(t_fixture *fx, t_logd *lg);

// Static Variables
static int	g_wake[2] = {-1, -1};

int	main(void)
{
	assert(selfpipe_open(g_wake) == 0);
	assert(signals_install(g_wake[1]) == 0);
	test_take_starts_empty_and_clears();
	test_repeated_signals_coalesce();
	test_term_and_int_stop_the_loop();
	test_hup_rotates_the_sink();
	test_usr1_reports_the_counters();
	test_sigpipe_is_ignored();
	close(g_wake[0]);
	close(g_wake[1]);
	return (0);
}

/*
** A handler that opened a file or wrote a record would be running
** non-async-signal-safe code between two arbitrary instructions. All it may
** do is set a flag and wake the loop, so signals_take is where the signal
** becomes visible - and reading it must clear it.
*/
static void	test_take_starts_empty_and_clears(void)
{
	assert(signals_take() == 0);
	raise(SIGHUP);
	assert(signals_take() == TETRISLOGD_SIGNAL_HUP);
	assert(signals_take() == 0);
	raise(SIGUSR1);
	assert(signals_take() == TETRISLOGD_SIGNAL_DUMP);
	raise(SIGTERM);
	assert(signals_take() == TETRISLOGD_SIGNAL_STOP);
	raise(SIGINT);
	assert(signals_take() == TETRISLOGD_SIGNAL_STOP);
	assert(signals_take() == 0);
	printf("PASS test_take_starts_empty_and_clears\n");
}

/*
** Two SIGHUPs between iterations are one rotation: the file only needs
** reopening once. Different signals in the same window must both survive,
** because a shutdown that swallowed a pending rotation would write its final
** records into the file the operator just moved away.
*/
static void	test_repeated_signals_coalesce(void)
{
	raise(SIGHUP);
	raise(SIGHUP);
	assert(signals_take() == TETRISLOGD_SIGNAL_HUP);
	raise(SIGHUP);
	raise(SIGUSR1);
	assert(signals_take() == (TETRISLOGD_SIGNAL_HUP | TETRISLOGD_SIGNAL_DUMP));
	assert(signals_take() == 0);
	printf("PASS test_repeated_signals_coalesce\n");
}

static void	test_term_and_int_stop_the_loop(void)
{
	t_fixture	fx;
	t_logd		lg;

	assert(boot(&fx, &lg) == 0);
	raise(SIGTERM);
	assert(logd_run_once(&lg) == 0);
	assert(lg.running == false);
	logd_stop(&lg);
	fx_destroy(&fx);
	assert(boot(&fx, &lg) == 0);
	raise(SIGINT);
	assert(logd_run_once(&lg) == 0);
	assert(lg.running == false);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_term_and_int_stop_the_loop\n");
}

/*
** The full rotation contract, end to end: an external mover renames the file,
** SIGHUP arrives, and the next record lands in a fresh one while everything
** written before the signal stays in the moved file.
*/
static void	test_hup_rotates_the_sink(void)
{
	t_fixture	fx;
	t_logd		lg;
	char		moved[256];
	int			tx;

	assert(boot(&fx, &lg) == 0);
	snprintf(moved, sizeof(moved), "%s.1", fx.file_path);
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send(tx, COREIPC_LOG_INFO, "before the move") == 0);
	assert(logd_run_once(&lg) == 0);
	assert(rename(fx.file_path, moved) == 0);
	raise(SIGHUP);
	assert(logd_run_once(&lg) == 0);
	assert(lg.running == true);
	assert(sink_is_open(&lg.sink) == true);
	assert(fx_send(tx, COREIPC_LOG_INFO, "after the move") == 0);
	assert(logd_run_once(&lg) == 0);
	assert(fx_contains(moved, "before the move") == 1);
	assert(fx_contains(fx.file_path, "after the move") == 1);
	assert(fx_contains(fx.file_path, "before the move") == 0);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_hup_rotates_the_sink\n");
}

/*
** With no control channel in this milestone, SIGUSR1 is the only way to read
** the counters while the daemon is running - it stands in for the query
** tetrisctl will eventually make.
*/
static void	test_usr1_reports_the_counters(void)
{
	t_fixture	fx;
	t_logd		lg;
	int			tx;

	assert(boot(&fx, &lg) == 0);
	tx = fx_producer(&fx);
	assert(tx >= 0);
	assert(fx_send(tx, COREIPC_LOG_INFO, "one record") == 0);
	assert(logd_run_once(&lg) == 0);
	raise(SIGUSR1);
	assert(logd_run_once(&lg) == 0);
	assert(lg.running == true);
	assert(fx_contains(fx.file_path, "written") == 1);
	assert(fx_contains(fx.file_path, "rejected") == 1);
	assert(fx_contains(fx.file_path, "degraded") == 1);
	close(tx);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_usr1_reports_the_counters\n");
}

/*
** The degraded path writes to stderr, so a closed pipe must not be able to
** kill the process that is recording why everything else died.
*/
static void	test_sigpipe_is_ignored(void)
{
	struct sigaction	sa;
	t_fixture			fx;
	t_logd				lg;

	assert(boot(&fx, &lg) == 0);
	memset(&sa, 0, sizeof(sa));
	assert(sigaction(SIGPIPE, NULL, &sa) == 0);
	assert(sa.sa_handler == SIG_IGN);
	logd_stop(&lg);
	fx_destroy(&fx);
	printf("PASS test_sigpipe_is_ignored\n");
}

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
