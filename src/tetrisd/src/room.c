#include "tetrisd.h"

// Static Functions
static void		*ticker_main(void *arg);
static int		tick_once(t_room_rt *rt, t_sb_state *snaps, t_player_id *pids);
static bool		room_is_over(t_room_rt *rt);
static void		record_and_reset(t_room_rt *rt);
static void		forfeit_slot(t_room_rt *rt, int slot, t_game *out);
static void		destroy_if_empty(t_room_rt *rt);
static int64_t	coins_earned(const t_game *game);

/**
 * @brief Pairs every lobby room with the runtime state tetrisd keeps beside it.
 *
 * The domain library owns the pure room; the mutex, the per-slot games, and
 * the ticker live here, one runtime per room, addressed by the same index.
 *
 * @param srv Server whose room runtimes are being prepared.
 */
void	room_rt_init_all(t_server *srv)
{
	int	i;

	i = 0;
	while (i < LOBBY_MAX_ROOMS)
	{
		memset(&srv->rooms[i], 0, sizeof(srv->rooms[i]));
		pthread_mutex_init(&srv->rooms[i].mutex, NULL);
		srv->rooms[i].room = &srv->lobby.rooms[i];
		srv->rooms[i].srv = srv;
		srv->rooms[i].index = i;
		atomic_store(&srv->rooms[i].running, false);
		i++;
	}
}

/**
 * @brief Returns the runtime for a room index.
 *
 * @param srv Server holding the runtimes.
 * @param index Room index, as assigned by the lobby.
 * @return The runtime, or NULL when the index is out of range.
 */
t_room_rt	*room_rt_at(t_server *srv, int index)
{
	if (srv == NULL || index < 0 || index >= LOBBY_MAX_ROOMS)
		return (NULL);
	return (&srv->rooms[index]);
}

/**
 * @brief Finds a room runtime by the room's display name.
 *
 * Rooms are addressed by name on the wire because ids run per mode, so only
 * the prefixed name (S-01, D-02) is unique lobby-wide.
 *
 * @param srv Server to search.
 * @param name Room display name.
 * @return The runtime, or NULL when no room carries that name.
 */
t_room_rt	*room_rt_find(t_server *srv, const char *name)
{
	t_room	*room;

	if (srv == NULL || name == NULL || name[0] == '\0')
		return (NULL);
	pthread_mutex_lock(&srv->lobby_mutex);
	room = lobby_find_room(&srv->lobby, name);
	pthread_mutex_unlock(&srv->lobby_mutex);
	if (room == NULL)
		return (NULL);
	return (room_rt_at(srv, (int)(room - srv->lobby.rooms)));
}

/**
 * @brief Answers the room domain's liveness question from the registry.
 *
 * libtetrisroom never touches a socket, so ownership succession and seating
 * ask the caller whether a player is still connected; this is that answer.
 *
 * @param ctx The server, passed through as the probe context.
 * @param pid Player being asked about.
 * @return true when that player still has a live connection.
 */
bool	room_rt_probe(void *ctx, t_player_id pid)
{
	t_server	*srv;

	srv = ctx;
	if (srv == NULL)
		return (false);
	return (reg_player_online(&srv->reg, pid));
}

/**
 * @brief Reports whether a client still holds the slot it thinks it holds.
 *
 * A finished game clears every slot, so a client's room binding outlives its
 * seat. Rather than leave the connection wedged - refused a new room because
 * it "is already in one" that no longer exists - a stale binding is cleared
 * here, on the client's own thread.
 *
 * @param srv Server holding the rooms.
 * @param cli Client whose binding is checked; cleared when stale.
 * @return true when the client is still seated where it thinks it is.
 */
bool	room_rt_seated(t_server *srv, t_client *cli)
{
	t_room_rt	*rt;
	bool		seated;

	if (srv == NULL || cli == NULL || cli->room_index < 0)
		return (false);
	rt = room_rt_at(srv, cli->room_index);
	seated = false;
	if (rt != NULL)
	{
		pthread_mutex_lock(&rt->mutex);
		seated = strcmp(rt->room->name, cli->room_name) == 0
			&& room_find_member(rt->room, cli->player_id) != NULL;
		pthread_mutex_unlock(&rt->mutex);
	}
	if (!seated)
	{
		cli->room_index = -1;
		cli->slot_index = -1;
		cli->room_name[0] = '\0';
	}
	return (seated);
}

/**
 * @brief Starts the ticker thread that drives one room's games.
 *
 * A previous game's ticker is joined first, so a room can host game after
 * game without leaking a thread each time.
 *
 * @param rt Room runtime to start; its room must already be IN_GAME.
 * @param srv Server the room belongs to.
 * @return 0 on success, -1 when the thread could not be created.
 */
int	room_rt_begin(t_room_rt *rt, t_server *srv)
{
	if (rt == NULL || srv == NULL)
		return (-1);
	if (rt->ticker_started)
	{
		pthread_join(rt->ticker, NULL);
		rt->ticker_started = false;
	}
	rt->srv = srv;
	clock_gettime(CLOCK_MONOTONIC, &rt->last_tick);
	atomic_store(&rt->running, true);
	if (pthread_create(&rt->ticker, NULL, ticker_main, rt) != 0)
	{
		atomic_store(&rt->running, false);
		return (-1);
	}
	rt->ticker_started = true;
	return (0);
}

/**
 * @brief Stops a room's ticker and waits for it to finish.
 *
 * @param rt Room runtime to stop; safe when no ticker ever ran.
 */
void	room_rt_stop(t_room_rt *rt)
{
	if (rt == NULL)
		return ;
	atomic_store(&rt->running, false);
	if (rt->ticker_started)
		pthread_join(rt->ticker, NULL);
	rt->ticker_started = false;
}

/**
 * @brief Removes a client from its room, forfeiting any game in progress.
 *
 * Leaving, topping out, and losing the connection are the same event
 * (ADR-0002): the game is recorded on the spot, the slot is released, and
 * ownership passes to a successor when the owner was the one who left.
 *
 * @param srv Server the client belongs to.
 * @param cli Client leaving; its room binding is cleared.
 */
void	room_rt_forfeit(t_server *srv, t_client *cli)
{
	t_release_result	res;
	t_room_rt			*rt;
	t_game				finished;

	if (srv == NULL || cli == NULL || cli->room_index < 0)
		return ;
	rt = room_rt_at(srv, cli->room_index);
	game_reset(&finished);
	if (rt != NULL)
	{
		pthread_mutex_lock(&rt->mutex);
		if (strcmp(rt->room->name, cli->room_name) == 0
			&& room_find_member(rt->room, cli->player_id) != NULL)
		{
			forfeit_slot(rt, cli->slot_index, &finished);
			memset(&res, 0, sizeof(res));
			room_release(rt->room, cli->player_id, room_rt_probe, srv, &res);
		}
		pthread_mutex_unlock(&rt->mutex);
	}
	if (finished.player_id != 0)
		db_record_game(srv->db, finished.player_id,
			(int64_t)finished.score.total, coins_earned(&finished), false);
	if (rt != NULL)
		destroy_if_empty(rt);
	cli->room_index = -1;
	cli->slot_index = -1;
	cli->room_name[0] = '\0';
}

/**
 * @brief Pushes one player's snapshot as a server-originated STATE message.
 *
 * The subject rides in the request path (ADR-0003), so the body stays a pure
 * projection of one game and says nothing about whose it is.
 *
 * @param rt Room runtime the snapshot came from (unused beyond context).
 * @param room_name Room the subject is playing in.
 * @param pid The subject player.
 * @param snap Snapshot to encode and push.
 */
void	room_rt_push_state(t_room_rt *rt, const char *room_name,
		t_player_id pid, const t_sb_state *snap)
{
	t_htttp_message	msg;
	unsigned char	*bytes;
	char			path[TD_LINE_MAX];
	char			body[TD_BODY_MAX];
	size_t			len;
	int				body_len;

	body_len = sb_state_encode(snap, body, sizeof(body));
	if (rt == NULL || body_len <= 0)
		return ;
	snprintf(path, sizeof(path), "/room/%s/player/%llu", room_name,
		(unsigned long long)pid);
	htttp_message_init(&msg);
	if (htttp_message_make_request(&msg, "STATE", path) == HTTTP_OK
		&& htttp_message_set_header(&msg, "Content-Type",
			HTTTP_CONTENT_TYPE_STATE) == HTTTP_OK
		&& htttp_message_set_body(&msg, body, (size_t)body_len) == HTTTP_OK
		&& htttp_serialize(&msg, &bytes, &len) == HTTTP_OK)
	{
		if (reg_enqueue(&rt->srv->reg, pid, bytes, len, true) != 0)
			free(bytes);
	}
	htttp_message_free(&msg);
}

/**
 * @brief Ticker thread: gravity for every game in one room, then snapshots.
 *
 * The room's mutex is held only while the games advance; encoding and
 * enqueueing happen outside it, so one stalled client can never hold up
 * another player's game. One last pass runs after the game ends, so the
 * snapshot that says "you topped out" is always sent before the room resets.
 * The tick period is re-read each round, so SIGHUP reaches running games.
 *
 * @param arg The room runtime.
 * @return Always NULL.
 */
static void	*ticker_main(void *arg)
{
	t_sb_state		snaps[TD_MAX_GAMES];
	t_player_id		pids[TD_MAX_GAMES];
	struct timespec	period;
	t_room_rt		*rt;
	char			name[ROOM_NAME_MAX];
	int				n;

	rt = arg;
	period.tv_sec = 0;
	while (atomic_load(&rt->running) && atomic_load(&rt->srv->running))
	{
		period.tv_nsec = (long)atomic_load(&rt->srv->tick_ms) * 1000000L;
		nanosleep(&period, NULL);
		pthread_mutex_lock(&rt->mutex);
		snprintf(name, sizeof(name), "%s", rt->room->name);
		pthread_mutex_unlock(&rt->mutex);
		n = tick_once(rt, snaps, pids);
		while (n > 0)
		{
			n--;
			room_rt_push_state(rt, name, pids[n], &snaps[n]);
		}
		if (room_is_over(rt))
		{
			n = tick_once(rt, snaps, pids);
			while (n > 0)
			{
				n--;
				room_rt_push_state(rt, name, pids[n], &snaps[n]);
			}
			break ;
		}
	}
	record_and_reset(rt);
	destroy_if_empty(rt);
	atomic_store(&rt->running, false);
	return (NULL);
}

/**
 * @brief Advances every live game in the room and collects what changed.
 *
 * @param rt Room runtime to advance.
 * @param snaps Receives one snapshot per changed game.
 * @param pids Receives the matching subject player ids.
 * @return Number of snapshots collected.
 */
static int	tick_once(t_room_rt *rt, t_sb_state *snaps, t_player_id *pids)
{
	int	elapsed;
	int	slot;
	int	n;

	n = 0;
	pthread_mutex_lock(&rt->mutex);
	elapsed = net_elapsed_ms(&rt->last_tick);
	slot = 0;
	while (slot < rt->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (rt->games[slot].player_id != 0)
		{
			if (game_gravity(&rt->games[slot], elapsed))
				rt->dirty[slot] = true;
			if (rt->dirty[slot])
			{
				game_snapshot(&rt->games[slot], &snaps[n]);
				pids[n] = rt->games[slot].player_id;
				rt->dirty[slot] = false;
				n++;
			}
		}
		slot++;
	}
	pthread_mutex_unlock(&rt->mutex);
	return (n);
}

/**
 * @brief Reports whether the room has nothing left to tick.
 *
 * @param rt Room runtime to check.
 * @return true when every game has ended or every player has left.
 */
static bool	room_is_over(t_room_rt *rt)
{
	bool	over;
	int		slot;

	over = true;
	pthread_mutex_lock(&rt->mutex);
	if (rt->room->number_of_players == 0)
	{
		pthread_mutex_unlock(&rt->mutex);
		return (true);
	}
	slot = 0;
	while (slot < rt->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (rt->games[slot].active)
			over = false;
		slot++;
	}
	pthread_mutex_unlock(&rt->mutex);
	return (over);
}

/**
 * @brief Records every finished game and empties the room.
 *
 * Results are written to the store outside the room's mutex - no database
 * call ever happens under a lock. Finishing a game clears every slot (the
 * room domain's rule), so the room ends empty and is handed back to the
 * lobby; players who want another game join a fresh one.
 *
 * @param rt Room runtime whose game has ended.
 */
static void	record_and_reset(t_room_rt *rt)
{
	t_game	played[TD_MAX_GAMES];
	int		n;
	int		slot;

	n = 0;
	pthread_mutex_lock(&rt->mutex);
	slot = 0;
	while (slot < rt->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (rt->games[slot].player_id != 0 && !rt->games[slot].recorded)
		{
			rt->games[slot].recorded = true;
			played[n++] = rt->games[slot];
		}
		game_reset(&rt->games[slot]);
		rt->dirty[slot] = false;
		slot++;
	}
	room_finish(rt->room);
	pthread_mutex_unlock(&rt->mutex);
	while (n > 0)
	{
		n--;
		db_record_game(rt->srv->db, played[n].player_id,
			(int64_t)played[n].score.total, coins_earned(&played[n]),
			!played[n].topped_out);
	}
}

/**
 * @brief Ends one slot's game so the leaver's result can be recorded.
 *
 * @param rt Room runtime, with its mutex already held.
 * @param slot 1-based slot index the player occupied.
 * @param out Receives the finished game, or a blank game when there was none.
 */
static void	forfeit_slot(t_room_rt *rt, int slot, t_game *out)
{
	int	index;

	index = slot - 1;
	if (index < 0 || index >= TD_MAX_GAMES)
		return ;
	if (rt->games[index].player_id == 0 || rt->games[index].recorded)
		return ;
	rt->games[index].active = false;
	rt->games[index].recorded = true;
	*out = rt->games[index];
	game_reset(&rt->games[index]);
	rt->dirty[index] = false;
}

/**
 * @brief Returns an emptied room to the lobby so its slot can be reused.
 *
 * Rooms outlive their games, but not their players: a room nobody is sitting
 * in is removed, which is what keeps a long-running server from filling its
 * lobby with the ghosts of finished games.
 *
 * The room's mutex is held across the destroy, not just across the test.
 * Destroying a room rewrites the very fields a ticker reads under that mutex,
 * so releasing it first would leave the same room guarded by two different
 * locks depending on who was asking - which is a data race, not a lock order.
 * Taking the lobby first keeps the documented order intact.
 *
 * @param rt Room runtime to check; its mutex must not be held.
 */
static void	destroy_if_empty(t_room_rt *rt)
{
	char	name[ROOM_NAME_MAX];
	bool	empty;

	pthread_mutex_lock(&rt->srv->lobby_mutex);
	pthread_mutex_lock(&rt->mutex);
	empty = rt->room->number_of_players == 0;
	snprintf(name, sizeof(name), "%s", rt->room->name);
	if (empty)
		lobby_destroy_room(&rt->srv->lobby, name);
	pthread_mutex_unlock(&rt->mutex);
	pthread_mutex_unlock(&rt->srv->lobby_mutex);
}

/**
 * @brief Converts a finished game's score into wallet points.
 *
 * The economy's one rule, in one place: a thousand points buys one wallet
 * point (docs/game-economics.md).
 *
 * @param game The finished game.
 * @return Wallet points earned by that game.
 */
static int64_t	coins_earned(const t_game *game)
{
	return ((int64_t)(game->score.total / 1000));
}
