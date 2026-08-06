/* ************************************************************************** */
/*                                                                            */
/*   test_shutdown.c - lifecycle: limits, signals, and a clean stop           */
/*                                                                            */
/*   Shutdown is where a threaded server usually leaks or hangs, so these     */
/*   cases stop a server with clients connected, with a game in progress,     */
/*   and by signal - and they are meant to be run under valgrind, where a     */
/*   thread that never joined or a buffer never freed becomes a failure.      */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_stopping_disconnects_every_client(void);
static void	test_stopping_mid_game_is_clean(void);
static void	test_sigterm_asks_the_server_to_stop(void);
static void	test_sighup_rereads_the_configuration(void);
static void	test_clients_over_the_limit_are_refused(void);
static void	test_a_failed_boot_leaves_stdin_open(void);
static void	test_sigusr1_dumps_the_whole_server_state(void);

static int	player(t_fixture *fx, t_harness *hc, const char *name);
static int	simple(t_harness *hc, const char *method, const char *path, const char *body);
static void	write_rc(const char *path, const char *text);
static int	slurp_after(const char *path, const char *needle, char *out, size_t cap, int timeout_ms);

int	main(void)
{
	test_stopping_disconnects_every_client();
	test_stopping_mid_game_is_clean();
	test_sigterm_asks_the_server_to_stop();
	test_sighup_rereads_the_configuration();
	test_clients_over_the_limit_are_refused();
	test_a_failed_boot_leaves_stdin_open();
	test_sigusr1_dumps_the_whole_server_state();
	return (0);
}

/*
** SIGUSR1 is the "what is this server doing right now" question, answered
** without restarting it. The dump has to reach the log by the ordinary path,
** so with no tetrislogd listening it lands on stderr like any other record.
*/
static void	test_sigusr1_dumps_the_whole_server_state(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		out[192];
	char		text[8192];
	int			saved;
	int			fd;

	assert(fx_start(&fx) == 0);
	atomic_store(&fx.srv->log.level, COREIPC_LOG_INFO);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_join_new(&hc, "single", out, sizeof(out)) == 201);
	snprintf(out, sizeof(out), "%s/stderr.txt", fx.dir);
	fd = open(out, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	assert(fd >= 0);
	saved = dup(STDERR_FILENO);
	assert(saved >= 0 && dup2(fd, STDERR_FILENO) >= 0);
	signals_install(fx.srv);
	assert(raise(SIGUSR1) == 0);
	assert(slurp_after(out, "logs dropped", text, sizeof(text), 3000) == 0);
	signals_restore();
	assert(dup2(saved, STDERR_FILENO) >= 0);
	close(saved);
	close(fd);
	assert(strstr(text, "state dump") != NULL);
	assert(strstr(text, "amber") != NULL);
	assert(strstr(text, "S-01") != NULL);
	assert(strstr(text, "single") != NULL);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_sigusr1_dumps_the_whole_server_state\n");
}

static void	test_stopping_disconnects_every_client(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &amber, "amber") == 0);
	assert(player(&fx, &blake, "blake") == 0);
	server_stop(fx.srv);
	fx.srv = NULL;
	assert(hc_request(&amber, "LIST", TETRISD_ROUTE_ROOMS, NULL, &resp) == -1);
	assert(hc_recv(&blake, &resp, 500) == -1);
	hc_close(&blake);
	hc_close(&amber);
	fx_stop(&fx);
	printf("PASS test_stopping_disconnects_every_client\n");
}

static void	test_stopping_mid_game_is_clean(void)
{
	t_body_state	state;
	t_fixture	fx;
	t_harness	hc;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&hc, "START", path, NULL) == 200);
	assert(hc_wait_state(&hc, &state, HC_TIMEOUT_MS) == 0);
	server_stop(fx.srv);
	fx.srv = NULL;
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_stopping_mid_game_is_clean\n");
}

static void	test_sigterm_asks_the_server_to_stop(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	signals_install(fx.srv);
	assert(raise(SIGTERM) == 0);
	server_wait(fx.srv);
	signals_restore();
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_sigterm_asks_the_server_to_stop\n");
}

static void	test_sighup_rereads_the_configuration(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		rc[192];

	assert(fx_start(&fx) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", fx.dir);
	write_rc(rc, "export TETRISD_LOG_LEVEL=error\nexport TETRISD_TICK_MS=25\n");
	snprintf(fx.srv->cfg.rc_path, TETRISD_FILESYSTEM_PATH_MAX, "%s", rc);
	signals_install(fx.srv);
	assert(raise(SIGHUP) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(simple(&hc, "LIST", TETRISD_ROUTE_ROOMS, NULL) == 200);
	assert(atomic_load(&fx.srv->tick_ms) == 25);
	assert(fx.srv->cfg.log_level == COREIPC_LOG_ERROR);
	signals_restore();
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_sighup_rereads_the_configuration\n");
}

static void	test_clients_over_the_limit_are_refused(void)
{
	t_fixture	fx;
	t_harness	first;
	t_harness	second;

	assert(fx_start(&fx) == 0);
	server_stop(fx.srv);
	fx.cfg.max_clients = 1;
	assert(server_start(&fx.cfg, &fx.srv) == 0);
	assert(hc_connect(&first, &fx) == 0);
	assert(hc_connect(&second, &fx) == -1);
	hc_close(&second);
	hc_close(&first);
	fx_stop(&fx);
	printf("PASS test_clients_over_the_limit_are_refused\n");
}

/*
** A boot that fails before the logger is open still runs the whole teardown
** path, so every descriptor that teardown closes must be one the server
** actually opened. An uninitialised descriptor reads as 0, which is stdin.
*/
static void	test_a_failed_boot_leaves_stdin_open(void)
{
	t_fixture	fx;
	t_server	*srv;
	char		blocker[192];
	int			fd;

	assert(fx_start(&fx) == 0);
	server_stop(fx.srv);
	fx.srv = NULL;
	snprintf(blocker, sizeof(blocker), "%s/blocker", fx.dir);
	fd = open(blocker, O_CREAT | O_WRONLY, 0600);
	assert(fd >= 0);
	close(fd);
	snprintf(fx.cfg.data_dir, TETRISD_FILESYSTEM_PATH_MAX, "%s/sub", blocker);
	srv = NULL;
	assert(server_start(&fx.cfg, &srv) == -1);
	assert(fcntl(STDIN_FILENO, F_GETFD) != -1);
	fx_stop(&fx);
	printf("PASS test_a_failed_boot_leaves_stdin_open\n");
}

/**
 * @brief Connects, registers, and logs in one player.
 *
 * @param fx Running fixture.
 * @param hc Client to bring up.
 * @param name Username to register.
 * @return 0 on success, -1 otherwise.
 */
static int	player(t_fixture *fx, t_harness *hc, const char *name)
{
	if (hc_connect(hc, fx) != 0)
		return (-1);
	if (hc_signup(hc, name, "hunter2") != 201)
		return (-1);
	if (hc_login(hc, name, "hunter2") != 200)
		return (-1);
	return (0);
}

/**
 * @brief Sends a request and returns only its status code.
 *
 * @param hc Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body, or NULL.
 * @return The status code, or -1 when the exchange failed.
 */
static int	simple(t_harness *hc, const char *method, const char *path,
			const char *body)
{
	t_htttp_message	resp;
	int				status;

	if (hc_request(hc, method, path, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Reads a file once it contains a marker, or gives up.
 *
 * The dump travels the ordinary log path - ring buffer, then shipper thread -
 * so it arrives shortly after the signal rather than during it. Waiting for
 * the dump's last line is what makes the test wait exactly long enough.
 *
 * @param path File to read.
 * @param needle Marker that means the write is complete.
 * @param out Buffer receiving the contents.
 * @param cap Size of out.
 * @param timeout_ms How long to keep looking.
 * @return 0 when the marker appeared, -1 on timeout.
 */
static int	slurp_after(const char *path, const char *needle, char *out,
			size_t cap, int timeout_ms)
{
	struct timespec	nap;
	FILE			*f;
	size_t			n;
	int				waited;

	nap.tv_sec = 0;
	nap.tv_nsec = 20 * 1000000L;
	waited = 0;
	while (waited < timeout_ms)
	{
		f = fopen(path, "r");
		if (f != NULL)
		{
			n = fread(out, 1, cap - 1, f);
			out[n] = '\0';
			fclose(f);
			if (strstr(out, needle) != NULL)
				return (0);
		}
		nanosleep(&nap, NULL);
		waited += 20;
	}
	return (-1);
}

/**
 * @brief Writes a start-up file for the reload test to read.
 *
 * @param path File to write.
 * @param text Contents to write.
 */
static void	write_rc(const char *path, const char *text)
{
	FILE	*f;

	f = fopen(path, "w");
	assert(f != NULL);
	fputs(text, f);
	fclose(f);
}
