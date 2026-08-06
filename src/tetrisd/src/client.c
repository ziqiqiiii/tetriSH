#include "tetrisd.h"

// Static Functions
static int	start_reader(t_client *cli);
static void	*reader_main(void *arg);
static void	*writer_main(void *arg);
static void	teardown(t_client *cli);

/**
 * @brief Takes ownership of an accepted connection and starts serving it.
 *
 * The client is registered before its handshake runs, so a shutdown can
 * interrupt a peer that never finishes one. From here on the connection
 * belongs to its own reader thread, which frees it when the peer goes away.
 *
 * @param srv Server the connection belongs to.
 * @param fd Accepted socket descriptor; closed here on failure.
 * @return 0 when a reader thread took over, -1 when the client was refused.
 */
int	client_spawn(t_server *srv, int fd)
{
	t_client	*cli;

	if (srv == NULL || fd < 0)
		return (-1);
	cli = calloc(1, sizeof(*cli));
	if (cli == NULL)
	{
		close(fd);
		return (-1);
	}
	cli->fd = fd;
	cli->srv = srv;
	cli->index = -1;
	cli->room_index = -1;
	cli->slot_index = -1;
	cli->state = CLI_HANDSHAKE;
	if (outbox_init(&cli->outbox) != 0 || registry_add(&srv->reg, cli) != 0)
	{
		outbox_destroy(&cli->outbox);
		close(fd);
		free(cli);
		return (-1);
	}
	if (start_reader(cli) != 0)
	{
		registry_remove(&srv->reg, cli);
		outbox_destroy(&cli->outbox);
		close(fd);
		free(cli);
		return (-1);
	}
	return (0);
}

/**
 * @brief Serialises a message and queues it for this client's writer thread.
 *
 * Handlers and tickers never touch the socket: they hand bytes to the outbox
 * and move on, which is what keeps one slow peer from blocking a room.
 *
 * @param cli Client to send to.
 * @param msg Message to serialise; the caller still owns and frees it.
 * @param is_state true for a STATE push (latest-wins mailbox).
 */
void	client_send(t_client *cli, t_htttp_message *msg, bool is_state)
{
	unsigned char	*bytes;
	size_t			len;
	int				rc;

	if (cli == NULL || msg == NULL)
		return ;
	if (htttp_serialize(msg, &bytes, &len) != HTTTP_OK)
		return ;
	if (is_state)
		rc = outbox_push_state(&cli->outbox, bytes, len);
	else
		rc = outbox_push(&cli->outbox, bytes, len);
	if (rc != 0)
	{
		free(bytes);
		shutdown(cli->fd, SHUT_RDWR);
	}
}

/**
 * @brief Starts the detached reader thread that owns the client from now on.
 *
 * @param cli Client to serve.
 * @return 0 on success, -1 when the thread could not be created.
 */
static int	start_reader(t_client *cli)
{
	pthread_attr_t	attr;
	int				rc;

	if (pthread_attr_init(&attr) != 0)
		return (-1);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&cli->reader, &attr, reader_main, cli);
	pthread_attr_destroy(&attr);
	if (rc != 0)
		return (-1);
	return (0);
}

/**
 * @brief Reader thread: handshake, then one request at a time until the end.
 *
 * It blocks in session_recv, which is why every client needs its own thread;
 * the matching writer thread is only started once the session is established,
 * so nothing can be sent over an unauthenticated connection.
 *
 * @param arg The client.
 * @return Always NULL.
 */
static void	*reader_main(void *arg)
{
	t_client		*cli;
	unsigned char	*buf;
	ssize_t			n;

	cli = arg;
	buf = malloc(TETRISSH_MAX_PLAINTEXT);
	if (buf != NULL && session_handshake_server(cli->fd, &cli->sess,
			cli->srv->cfg.cert_path, cli->srv->cfg.key_path) == 0)
	{
		registry_mark_state(&cli->srv->reg, cli, CLI_ANONYMOUS);
		if (pthread_create(&cli->writer, NULL, writer_main, cli) == 0)
			cli->writer_started = true;
		while (cli->writer_started && atomic_load(&cli->srv->running))
		{
			n = session_recv(&cli->sess, buf, TETRISSH_MAX_PLAINTEXT);
			if (n <= 0 || atomic_load(&cli->outbox.overflowed))
				break ;
			client_handle_frame(cli, buf, (size_t)n);
		}
	}
	else
		logger_emit(&cli->srv->log, CIPC_LOG_WARNING,
			"handshake failed on fd %d", cli->fd);
	free(buf);
	teardown(cli);
	return (NULL);
}

/**
 * @brief Writer thread: the one caller of session_send for this connection.
 *
 * Keeping sends on a single thread is what makes the frame sequence numbers
 * meaningful - two threads encrypting into the same session would interleave.
 *
 * @param arg The client.
 * @return Always NULL.
 */
static void	*writer_main(void *arg)
{
	t_client	*cli;
	t_outbound_message	msg;
	ssize_t		sent;

	cli = arg;
	while (outbox_pop(&cli->outbox, &msg) == 0)
	{
		sent = session_send(&cli->sess, msg.bytes, msg.len);
		free(msg.bytes);
		if (sent < 0)
		{
			shutdown(cli->fd, SHUT_RDWR);
			break ;
		}
	}
	return (NULL);
}

/**
 * @brief Ends a client: forfeit, unlink, stop the writer, free everything.
 *
 * The order is the lifetime rule: the player leaves the room first (a
 * disconnect mid-game is a forfeit, ADR-0002), then the client is unlinked
 * from the registry so nobody can enqueue into it, and only then are the
 * socket and memory released.
 *
 * @param cli Client to tear down; the pointer is invalid afterwards.
 */
static void	teardown(t_client *cli)
{
	t_server	*srv;

	srv = cli->srv;
	server_room_forfeit(srv, cli);
	registry_remove(&srv->reg, cli);
	shutdown(cli->fd, SHUT_RDWR);
	outbox_close(&cli->outbox);
	if (cli->writer_started)
		pthread_join(cli->writer, NULL);
	session_close(&cli->sess);
	close(cli->fd);
	outbox_destroy(&cli->outbox);
	logger_emit(&srv->log, CIPC_LOG_INFO, "client %s disconnected",
		cli->username[0] != '\0' ? cli->username : "(anonymous)");
	free(cli);
}

