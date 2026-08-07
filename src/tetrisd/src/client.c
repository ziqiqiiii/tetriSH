#include "tetrisd.h"

// Static Functions
static void	unwatch(t_client *cli);
static void	release(t_client *cli);

/**
 * @brief Takes ownership of an accepted connection and starts its handshake.
 *
 * The client is registered before its handshake runs, so the connection limit
 * is enforced at accept time - a peer beyond it is refused before any crypto
 * is spent on it - and so a shutdown can reach a peer that never finishes one.
 * From here the handshake pool owns the descriptor until it hands the
 * established session back to the reactor.
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
	cli->room_index = -1;
	cli->slot_index = -1;
	cli->state = CLI_HANDSHAKE;
	if (outbox_init(&cli->outbox) != 0 || registry_add(&srv->reg, cli) != 0)
	{
		outbox_destroy(&cli->outbox);
		return (close(fd), free(cli), -1);
	}
	if (handshake_pool_submit(&srv->pool, cli) != 0)
	{
		registry_remove(&srv->reg, cli);
		outbox_destroy(&cli->outbox);
		return (close(fd), free(cli), -1);
	}
	return (0);
}

/**
 * @brief Moves a handshaken connection into the event loop.
 *
 * This is the handoff: from here the socket is non-blocking and the reactor is
 * its only reader and its only writer, so the session's sequence counters have
 * exactly one owner. A client that cannot be adopted is ended rather than left
 * connected but unwatched.
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
	if (unixsock_set_nonblock(cli->fd) != 0
		|| epoll_ctl(cli->srv->epoll_fd, EPOLL_CTL_ADD, cli->fd, &ev) != 0)
	{
		logger_emit(&cli->srv->log, COREIPC_LOG_WARNING,
			"cannot watch fd %d: %s", cli->fd, strerror(errno));
		client_kill(cli);
		return ;
	}
	cli->watched = true;
	registry_mark_state(&cli->srv->reg, cli, CLI_ANONYMOUS);
}

/**
 * @brief Ends a client: forfeit, unlink, and park it for the reaper.
 *
 * The order is the lifetime rule: the player leaves the room first (a
 * disconnect mid-game is a forfeit, ADR-0002), then the client is unlinked
 * from the registry so no room ticker can enqueue into it, and only then is
 * the socket shut down.
 *
 * Nothing is freed here. A batch of epoll events may hold several pointers to
 * this client, so it goes on the zombie list and client_reap releases it once
 * the whole batch has been processed (docs/adr/0008).
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
 * This is the only free() site for a client, and calling it anywhere other
 * than between batches turns a stale epoll_event.data.ptr into a use-after
 * free - the one rule in the reactor that a later change can break without a
 * single test noticing.
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
	logger_emit(&cli->srv->log, COREIPC_LOG_INFO, "client %s disconnected",
		cli->username[0] != '\0' ? cli->username : "(anonymous)");
	session_close(&cli->sess);
	close(cli->fd);
	outbox_destroy(&cli->outbox);
	buffer_free(&cli->recv);
	buffer_free(&cli->send);
	free(cli);
}
