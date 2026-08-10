#include "tetrisd.h"

// Static Functions
static void			dump_header(t_server *srv);
static void			dump_clients(t_server *srv);
static void			dump_rooms(t_server *srv);
static void			dump_room(t_server *srv, t_server_room *server_room);
static const char	*state_name(t_client_state state);
static const char	*mode_name(t_game_mode mode);
static const char	*status_name(t_room_status status);

/*
** The answer to "what is this server doing right now", written to the log
** without restarting anything. Every line goes through logger_emit like any
** other record, so the dump lands wherever the logs land.
**
** It runs on the reactor, which owns everything it prints, so the picture it
** writes is consistent without any snapshot being coordinated - the one thing
** this was hardest to guarantee when several threads owned pieces of it.
*/

/**
 * @brief Writes the whole server state to the log.
 *
 * @param srv Server to describe.
 */
void	server_state_dump(t_server *srv)
{
	if (srv == NULL)
		return ;
	dump_header(srv);
	dump_clients(srv);
	dump_rooms(srv);
	logger_emit(&srv->log, COREIPC_LOG_INFO, "state dump: logs dropped %llu",
		(unsigned long long)logger_dropped_count(&srv->log));
}

/**
 * @brief Emits the settings the server is actually running with.
 *
 * @param srv Server to describe.
 */
static void	dump_header(t_server *srv)
{
	uint64_t	uptime;

	uptime = (clock_now_ms() - srv->started_ms) / 1000;
	logger_emit(&srv->log, COREIPC_LOG_INFO,
		"state dump: port %d, uptime %llus, tick %dms, max clients %d",
		srv->port, (unsigned long long)uptime,
		srv->tick_ms, srv->cfg.max_clients);
	logger_emit(&srv->log, COREIPC_LOG_INFO,
		"state dump: rc %s, data %s, log ipc %s",
		srv->cfg.rc_path, srv->cfg.data_dir, srv->cfg.log_ipc);
}

/**
 * @brief Emits one line per connection.
 *
 * @param srv Server whose clients are listed.
 */
static void	dump_clients(t_server *srv)
{
	t_client	*cli;
	size_t		i;

	logger_emit(&srv->log, COREIPC_LOG_INFO, "state dump: clients %zu of %zu",
		srv->reg.count, srv->reg.cap);
	i = 0;
	while (i < srv->reg.cap)
	{
		cli = srv->reg.slots[i];
		if (cli != NULL)
			logger_emit(&srv->log, COREIPC_LOG_INFO,
				"state dump:   client fd %d %s player %llu %s room %s slot %d "
				"chat-dropped %llu",
				cli->fd, state_name(cli->state),
				(unsigned long long)cli->player_id,
				cli->username[0] != '\0' ? cli->username : "-",
				cli->binding.room_name[0] != '\0'
				? cli->binding.room_name : "-",
				cli->binding.slot_index,
				(unsigned long long)cli->outbox.chat_dropped);
		i++;
	}
}

/**
 * @brief Emits every occupied room.
 *
 * @param srv Server whose lobby is listed.
 */
static void	dump_rooms(t_server *srv)
{
	int	i;

	logger_emit(&srv->log, COREIPC_LOG_INFO, "state dump: rooms %zu of %d",
		lobby_room_count(&srv->lobby), LOBBY_MAX_ROOMS);
	i = 0;
	while (i < LOBBY_MAX_ROOMS)
	{
		dump_room(srv, server_room_at(srv, i));
		i++;
	}
}

/**
 * @brief Emits one room and each game being played in it.
 *
 * @param srv Server holding the logger.
 * @param server_room Room to describe; skipped when nobody is sitting in it.
 */
static void	dump_room(t_server *srv, t_server_room *server_room)
{
	t_server_room_view	view;
	const t_game		*game;
	int					slot;

	if (!server_room_describe(server_room, &view))
		return ;
	logger_emit(&srv->log, COREIPC_LOG_INFO,
		"state dump:   room %s mode %s status %s players %d/%d ticking %s",
		view.name, mode_name(view.mode), status_name(view.status),
		view.players, view.slot_count, view.ticking ? "yes" : "no");
	slot = 0;
	while (slot < view.slot_count)
	{
		game = server_room_game_at(server_room, slot);
		if (game != NULL)
			logger_emit(&srv->log, COREIPC_LOG_INFO,
				"state dump:     slot %d player %llu score %d lines %d "
				"level %d %s", slot + 1,
				(unsigned long long)game->player_id,
				game->score.total, game->lines, game->level,
				game->active ? "playing" : "ended");
		slot++;
	}
}

/**
 * @brief Names a connection state for the dump.
 *
 * @param state State to name.
 * @return A short lower-case word, never NULL.
 */
static const char	*state_name(t_client_state state)
{
	if (state == CLI_HANDSHAKE)
		return ("handshaking");
	if (state == CLI_ANONYMOUS)
		return ("anonymous");
	if (state == CLI_AUTHED)
		return ("authed");
	return ("closing");
}

/**
 * @brief Names a room mode for the dump.
 *
 * @param mode Mode to name.
 * @return A short lower-case word, never NULL.
 */
static const char	*mode_name(t_game_mode mode)
{
	if (mode == MODE_SINGLE)
		return ("single");
	if (mode == MODE_DOUBLE)
		return ("double");
	return ("br");
}

/**
 * @brief Names a room status for the dump.
 *
 * @param status Status to name.
 * @return A short lower-case word, never NULL.
 */
static const char	*status_name(t_room_status status)
{
	if (status == ROOM_WAITING)
		return ("waiting");
	if (status == ROOM_READY)
		return ("ready");
	if (status == ROOM_IN_GAME)
		return ("in-game");
	return ("finished");
}
