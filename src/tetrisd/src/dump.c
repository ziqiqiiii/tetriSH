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
** Records are emitted while the locks are held. logger_emit is a non-blocking
** ring push - not a database call, a session write, or an IPC send - and the
** ring is a leaf that never reaches back for one of these locks, so this does
** not break the rule about what may happen under a lock. Copying the whole
** server into local buffers first would need an unbounded stack instead.
*/

/**
 * @brief Writes the whole server state to the log, in lock order.
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
	logger_emit(&srv->log, CIPC_LOG_INFO, "state dump: logs dropped %llu",
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
	logger_emit(&srv->log, CIPC_LOG_INFO,
		"state dump: port %d, uptime %llus, tick %dms, max clients %d",
		srv->port, (unsigned long long)uptime,
		atomic_load(&srv->tick_ms), srv->cfg.max_clients);
	logger_emit(&srv->log, CIPC_LOG_INFO,
		"state dump: rc %s, data %s, log ipc %s",
		srv->cfg.rc_path, srv->cfg.data_dir, srv->cfg.log_ipc);
}

/**
 * @brief Emits one line per connection, under the registry read lock.
 *
 * @param srv Server whose clients are listed.
 */
static void	dump_clients(t_server *srv)
{
	t_client	*cli;
	size_t		i;

	pthread_rwlock_rdlock(&srv->reg.lock);
	logger_emit(&srv->log, CIPC_LOG_INFO, "state dump: clients %zu of %zu",
		srv->reg.count, srv->reg.cap);
	i = 0;
	while (i < srv->reg.cap)
	{
		cli = srv->reg.slots[i];
		if (cli != NULL)
			logger_emit(&srv->log, CIPC_LOG_INFO,
				"state dump:   client fd %d %s player %llu %s room %s slot %d",
				cli->fd, state_name(cli->state),
				(unsigned long long)cli->player_id,
				cli->username[0] != '\0' ? cli->username : "-",
				cli->room_name[0] != '\0' ? cli->room_name : "-",
				cli->slot_index);
		i++;
	}
	pthread_rwlock_unlock(&srv->reg.lock);
}

/**
 * @brief Emits every occupied room, taking the lobby before each room.
 *
 * @param srv Server whose lobby is listed.
 */
static void	dump_rooms(t_server *srv)
{
	int	i;

	pthread_mutex_lock(&srv->lobby_mutex);
	logger_emit(&srv->log, CIPC_LOG_INFO, "state dump: rooms %zu of %d",
		lobby_room_count(&srv->lobby), LOBBY_MAX_ROOMS);
	i = 0;
	while (i < LOBBY_MAX_ROOMS)
	{
		dump_room(srv, &srv->rooms[i]);
		i++;
	}
	pthread_mutex_unlock(&srv->lobby_mutex);
}

/**
 * @brief Emits one room and each game being played in it.
 *
 * @param srv Server holding the logger.
 * @param server_room Room runtime to describe.
 */
static void	dump_room(t_server *srv, t_server_room *server_room)
{
	int	slot;

	pthread_mutex_lock(&server_room->mutex);
	if (server_room->room->number_of_players > 0)
	{
		logger_emit(&srv->log, CIPC_LOG_INFO,
			"state dump:   room %s mode %s status %s players %d/%d ticking %s",
			server_room->room->name, mode_name(server_room->room->mode),
			status_name(server_room->room->status), server_room->room->number_of_players,
			server_room->room->slot_count,
			atomic_load(&server_room->running) ? "yes" : "no");
		slot = 0;
		while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
		{
			if (server_room->games[slot].player_id != 0)
				logger_emit(&srv->log, CIPC_LOG_INFO,
					"state dump:     slot %d player %llu score %d lines %d "
					"level %d %s", slot + 1,
					(unsigned long long)server_room->games[slot].player_id,
					server_room->games[slot].score.total, server_room->games[slot].lines,
					server_room->games[slot].level,
					server_room->games[slot].active ? "playing" : "ended");
			slot++;
		}
	}
	pthread_mutex_unlock(&server_room->mutex);
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
