/* ************************************************************************** */
/*                                                                            */
/*   test_reactor.c - the event loop's own framing and its handshake pool     */
/*                                                                            */
/*   session_recv used to find the frame boundaries, one blocking call per    */
/*   client thread. The reactor finds them itself, out of a buffer a peer     */
/*   fills at whatever rhythm it likes, so these cases feed it the rhythms    */
/*   a well-behaved client never produces: two requests in one segment, one   */
/*   request split across two, and a length prefix that is a lie.             */
/*                                                                            */
/*   The last case is the pool's deadline, which is the difference between    */
/*   a bounded pool and a denial of service.                                  */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

/*
** The handshake budget these cases run the server with. Far below the shipped
** default so the suite is quick, but well clear of a real RSA handshake, which
** costs single-digit milliseconds natively and about 0.8 s under valgrind - the
** honest client has to fit inside this budget, not just the attacker.
*/
# define HANDSHAKE_BUDGET_MS	2000

/*
** How long the dribbling peer keeps feeding its worker one byte at a time. Long
** enough that a login completing sooner can only mean the budget cut the
** dribbler off, and not that it happened to stop of its own accord.
*/
# define DRIBBLE_MS				10000

// Static Variables
static int				dribble_fd = -1;
static volatile bool	dribble_stop = false;

// Static Functions
static void	test_two_requests_in_one_segment_are_both_answered(void);
static void	test_a_request_split_across_segments_is_answered(void);
static void	test_an_impossible_length_prefix_ends_the_connection(void);
static void	test_a_silent_peer_does_not_hold_the_pool_forever(void);
static void	test_a_dribbling_peer_does_not_hold_the_pool_forever(void);

static int	player(t_fixture *fx, t_harness *hc, const char *name);
static int	seal_request(t_harness *hc, const char *method, const char *path, unsigned char *out, size_t cap);
static int	expect_status(t_harness *hc);
static int	connect_tcp(int port);
static void	*dribble_main(void *arg);

int	main(void)
{
	test_two_requests_in_one_segment_are_both_answered();
	test_a_request_split_across_segments_is_answered();
	test_an_impossible_length_prefix_ends_the_connection();
	test_a_silent_peer_does_not_hold_the_pool_forever();
	test_a_dribbling_peer_does_not_hold_the_pool_forever();
	return (0);
}

/*
** Nagle is off and the harness sends one frame per write, so the coalescing a
** real network does has to be produced deliberately: two sealed frames, one
** write. The reactor must answer both, which it can only do by looping over
** its buffer rather than assuming one readable event means one message.
*/
static void	test_two_requests_in_one_segment_are_both_answered(void)
{
	unsigned char	wire[8192];
	t_fixture		fx;
	t_harness		hc;
	int				len;
	int				total;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	total = seal_request(&hc, "LIST", TETRISD_ROUTE_ROOMS, wire, sizeof(wire));
	assert(total > 0);
	len = seal_request(&hc, "LIST", TETRISD_ROUTE_ROOMS, wire + total, sizeof(wire) - (size_t)total);
	assert(len > 0);
	total += len;
	assert(write(hc.fd, wire, (size_t)total) == total);
	assert(expect_status(&hc) == 200);
	assert(expect_status(&hc) == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_two_requests_in_one_segment_are_both_answered\n");
}

/*
** The other half of the same property: a frame that arrives in pieces must be
** held, not answered early and not dropped. The prefix goes out on its own so
** the reactor is woken with a header and nothing behind it.
*/
static void	test_a_request_split_across_segments_is_answered(void)
{
	unsigned char	wire[8192];
	t_fixture		fx;
	t_harness		hc;
	int				total;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	total = seal_request(&hc, "LIST", TETRISD_ROUTE_ROOMS, wire, sizeof(wire));
	assert(total > TETRISD_LENGTH_PREFIX_BYTES + 1);
	assert(write(hc.fd, wire, TETRISD_LENGTH_PREFIX_BYTES + 1)
		== TETRISD_LENGTH_PREFIX_BYTES + 1);
	usleep(50 * 1000);
	assert(write(hc.fd, wire + TETRISD_LENGTH_PREFIX_BYTES + 1,
			(size_t)total - TETRISD_LENGTH_PREFIX_BYTES - 1)
		== total - TETRISD_LENGTH_PREFIX_BYTES - 1);
	assert(expect_status(&hc) == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_request_split_across_segments_is_answered\n");
}

/*
** The stream is encrypted, so a prefix no frame could carry means the peer is
** not speaking the protocol. Nothing later would resynchronise, so the
** connection ends rather than the server hunting for the next plausible frame.
*/
static void	test_an_impossible_length_prefix_ends_the_connection(void)
{
	t_htttp_message	resp;
	unsigned char	prefix[TETRISD_LENGTH_PREFIX_BYTES];
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	memset(prefix, 0xFF, sizeof(prefix));
	assert(write(hc.fd, prefix, sizeof(prefix)) == (ssize_t)sizeof(prefix));
	assert(hc_recv(&hc, &resp, HC_TIMEOUT_MS) == -1);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_an_impossible_length_prefix_ends_the_connection\n");
}

/*
** With one worker and one peer that connects and says nothing, every login on
** the server queues behind it - which is exactly the denial of service a
** bounded pool introduces and the deadline removes. The second client is
** expected to be delayed, not refused.
*/
static void	test_a_silent_peer_does_not_hold_the_pool_forever(void)
{
	t_fixture	fx;
	t_harness	hc;
	int			silent;

	assert(fx_start(&fx) == 0);
	server_stop(fx.srv);
	fx.cfg.handshake_workers = 1;
	fx.cfg.handshake_timeout_ms = HANDSHAKE_BUDGET_MS;
	assert(server_start(&fx.cfg, &fx.srv) == 0);
	silent = connect_tcp(server_port(fx.srv));
	assert(silent >= 0);
	usleep(20 * 1000);
	assert(player(&fx, &hc, "amber") == 0);
	close(silent);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_silent_peer_does_not_hold_the_pool_forever\n");
}

/*
** The case SO_RCVTIMEO alone does not cover, and the reason the pool carries a
** wall-clock budget as well. A peer that sends one byte just before every
** receive timeout never trips it - libtetrissh loops until it has the bytes it
** asked for - so the socket option alone would let this one connection hold
** the only worker for as long as it cares to keep dribbling.
**
** The assertion is therefore about time, not just success: the honest login
** has to finish while the dribbler is still going. With a per-read timeout
** alone it finishes only once the dribbler stops, which is DRIBBLE_MS away.
*/
static void	test_a_dribbling_peer_does_not_hold_the_pool_forever(void)
{
	t_fixture	fx;
	t_harness	hc;
	pthread_t	dribbler;
	uint64_t	started;

	assert(fx_start(&fx) == 0);
	server_stop(fx.srv);
	fx.cfg.handshake_workers = 1;
	fx.cfg.handshake_timeout_ms = HANDSHAKE_BUDGET_MS;
	assert(server_start(&fx.cfg, &fx.srv) == 0);
	dribble_fd = connect_tcp(server_port(fx.srv));
	dribble_stop = false;
	assert(dribble_fd >= 0);
	assert(pthread_create(&dribbler, NULL, dribble_main, NULL) == 0);
	usleep(20 * 1000);
	started = clock_now_ms();
	assert(player(&fx, &hc, "amber") == 0);
	assert(clock_now_ms() - started < DRIBBLE_MS / 2);
	dribble_stop = true;
	pthread_join(dribbler, NULL);
	close(dribble_fd);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_dribbling_peer_does_not_hold_the_pool_forever\n");
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
 * @brief Builds one request as wire bytes: length prefix then sealed frame.
 *
 * Sealing without sending is what lets a test choose how the bytes reach the
 * server, which is the only way to produce a boundary a well-behaved client
 * never would.
 *
 * @param hc Connected client whose session seals the frame.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param out Buffer receiving the wire bytes.
 * @param cap Size of out.
 * @return Number of bytes written, or -1 on failure.
 */
static int	seal_request(t_harness *hc, const char *method, const char *path,
			unsigned char *out, size_t cap)
{
	t_htttp_message	req;
	unsigned char	*plain;
	char			pid[32];
	size_t			len;
	ssize_t			frame_len;

	htttp_message_init(&req);
	snprintf(pid, sizeof(pid), "%llu", (unsigned long long)hc->player_id);
	if (htttp_message_make_request(&req, method, path) != HTTTP_OK
		|| htttp_message_set_header(&req, "Player-Id", pid) != HTTTP_OK
		|| htttp_serialize(&req, &plain, &len) != HTTTP_OK)
		return (htttp_message_free(&req), -1);
	htttp_message_free(&req);
	frame_len = session_frame_seal(&hc->sess, plain, len,
			out + TETRISD_LENGTH_PREFIX_BYTES,
			cap - TETRISD_LENGTH_PREFIX_BYTES);
	free(plain);
	if (frame_len < 0)
		return (-1);
	buffer_put_u32(out, (uint32_t)frame_len);
	return ((int)frame_len + TETRISD_LENGTH_PREFIX_BYTES);
}

/**
 * @brief Waits for the next response and reports its status code.
 *
 * @param hc Connected client.
 * @return The status code, or -1 on timeout or a parse failure.
 */
static int	expect_status(t_harness *hc)
{
	t_htttp_message	resp;
	int				status;

	if (hc_recv(hc, &resp, HC_TIMEOUT_MS) != 0)
		return (-1);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Sends one byte at a time, slowly, until told to stop or DRIBBLE_MS.
 *
 * Each byte arrives inside the socket's receive timeout, so the read the
 * handshake is blocked in keeps making progress and never times out. The stop
 * flag is what keeps the passing run quick: the test sets it as soon as the
 * honest login is through. A run without the wall-clock budget never gets
 * there, waits out DRIBBLE_MS, and fails the elapsed-time assertion instead of
 * hanging.
 *
 * @param arg Unused; the descriptor and the stop flag are file statics.
 * @return Always NULL.
 */
static void	*dribble_main(void *arg)
{
	uint64_t	deadline;

	(void)arg;
	deadline = clock_now_ms() + DRIBBLE_MS;
	while (!dribble_stop && clock_now_ms() < deadline)
	{
		if (send(dribble_fd, "x", 1, MSG_NOSIGNAL) != 1)
			break ;
		usleep((HANDSHAKE_BUDGET_MS / 8) * 1000);
	}
	return (NULL);
}

/**
 * @brief Opens a TCP connection and does nothing else with it.
 *
 * @param port Port to connect to.
 * @return The connected descriptor, or -1 on failure.
 */
static int	connect_tcp(int port)
{
	struct sockaddr_in	addr;
	int					fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return (-1);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		return (close(fd), -1);
	return (fd);
}
