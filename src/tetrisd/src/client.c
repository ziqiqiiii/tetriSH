#include "tetrisd.h"

// Static Functions
static void	unwatch(t_client *cli);
static void	release(t_client *cli);

/**
 * @brief Takes ownership of an accepted connection and starts its handshake.
 *
 * The connection id is claimed, and its accept line written, before the pool is
 * signalled: a worker can fail and log the instant it is woken.
 *
 * @param srv Server the connection belongs to.
 * @param fd Accepted socket descriptor; closed here on failure.
 * @return 0 when the pool took over, -1 when the client was refused.
 */
int	client_spawn(t_server *srv, int fd)
{
	t_client	*cli;

	if (srv == NULL || fd < 0)
		return (-1);
	cli = calloc(1, sizeof(*cli));
	if (cli == NULL)
		return (close(fd), -1);
	cli->tag.source = EVENT_CLIENT;
	cli->fd = fd;
	cli->srv = srv;
	cli->index = -1;
	cli->state = CLI_HANDSHAKE;
	server_room_unbind(cli);
	if (outbox_init(&cli->outbox) != 0 || registry_add(&srv->reg, cli) != 0)
	{
		outbox_destroy(&cli->outbox);
		return (close(fd), free(cli), -1);
	}
	cli->conn_id = ++srv->next_conn_id;
	logger_emit(&srv->log, COREIPC_LOG_INFO, "conn %u accepted", cli->conn_id);
	if (handshake_pool_submit(&srv->pool, cli) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_WARNING, "conn %u refused: the handshake pool would not take it", cli->conn_id);
		registry_remove(&srv->reg, cli);
		outbox_destroy(&cli->outbox);
		return (close(fd), free(cli), -1);
	}
	return (0);
}

/**
 * @brief Moves a handshaken connection into the event loop.
 *
 * From here the reactor is the socket's only reader and only writer.
 *
 * @param cli Client whose handshake succeeded.
 */
void	client_adopt(t_client *cli)
{
	struct epoll_event	ev;

	if (cli == NULL || cli->dead)
		return ;
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.ptr = cli;
	if (unixsock_set_nonblock(cli->fd) != 0 || epoll_ctl(cli->srv->epoll_fd, EPOLL_CTL_ADD, cli->fd, &ev) != 0)
	{
		logger_emit(&cli->srv->log, COREIPC_LOG_WARNING, "conn %u cannot watch fd %d: %s", cli->conn_id, cli->fd, strerror(errno));
		client_kill(cli);
		return ;
	}
	cli->watched = true;
	registry_mark_state(&cli->srv->reg, cli, CLI_ANONYMOUS);
	logger_emit(&cli->srv->log, COREIPC_LOG_INFO, "conn %u handshake ok", cli->conn_id);
}

/**
 * @brief Ends a client: forfeit, unlink, and park it for the reaper.
 *
 * The order is the lifetime rule: forfeit the room, unlink from the registry,
 * then shut the socket down. Nothing is freed here - a batch of epoll events
 * may still hold pointers to this client, so client_reap frees it.
 *
 * @param cli Client to end; a second call and a NULL are both ignored.
 */
void	client_kill(t_client *cli)
{
	t_server	*srv;

	if (cli == NULL || cli->dead)
		return ;
	srv = cli->srv;
	cli->dead = true;
	unwatch(cli);
	server_room_forfeit(srv, cli);
	registry_remove(&srv->reg, cli);
	shutdown(cli->fd, SHUT_RDWR);
	outbox_close(&cli->outbox);
	cli->next_zombie = srv->zombies;
	srv->zombies = cli;
}

/**
 * @brief Frees every client killed during the batch that has just finished.
 *
 * The only free() site for a client: calling it anywhere but between batches
 * turns a stale epoll_event.data.ptr into a use-after-free.
 *
 * @param srv Server whose zombie list is drained.
 */
void	client_reap(t_server *srv)
{
	t_client	*cli;

	if (srv == NULL)
		return ;
	while (srv->zombies != NULL)
	{
		cli = srv->zombies;
		srv->zombies = cli->next_zombie;
		release(cli);
	}
}

/**
 * @brief Removes a client's descriptor from the epoll set.
 *
 * @param cli Client being unwatched; safe when it never was.
 */
static void	unwatch(t_client *cli)
{
	if (!cli->watched)
		return ;
	epoll_ctl(cli->srv->epoll_fd, EPOLL_CTL_DEL, cli->fd, NULL);
	cli->watched = false;
	cli->writable_armed = false;
}

/**
 * @brief Releases one dead client's descriptor, buffers, and memory.
 *
 * @param cli Client to release; the pointer is invalid afterwards.
 */
static void	release(t_client *cli)
{
	logger_emit(&cli->srv->log, COREIPC_LOG_INFO, "conn %u %s disconnected", cli->conn_id, cli->username[0] != '\0' ? cli->username : "(anonymous)");
	session_close(&cli->sess);
	close(cli->fd);
	outbox_destroy(&cli->outbox);
	buffer_free(&cli->recv);
	buffer_free(&cli->send);
	free(cli);
}
