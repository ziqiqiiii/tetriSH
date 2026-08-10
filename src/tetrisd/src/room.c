#include "tetrisd.h"

// Static Functions
static void		room_close(t_server_room *server_room);
static void		room_blank(t_server_room *server_room);
static void		bind_client(t_client *cli, t_server_room *server_room, int slot);
static bool		room_probe(void *ctx, t_player_id pid);
static int		deal_games(t_server_room *server_room);
static void		tick_room(t_server_room *server_room, int elapsed_ms);
static void		advance_and_push(t_server_room *server_room, int elapsed_ms);
static void		push_all(t_server_room *server_room, int n, const t_body_state *snaps, const t_player_id *pids);
static void		push_state(t_server_room *server_room, const char *room_name, t_player_id pid, const t_body_state *snap);
static int		tick_once(t_server_room *server_room, int elapsed_ms, t_body_state *snaps, t_player_id *pids);
static bool		room_is_over(t_server_room *server_room);
static void		record_and_reset(t_server_room *server_room);
static void		forfeit_slot(t_server_room *server_room, int slot, t_game *out);
static void		award_game(t_server *srv, const t_game *game, bool won);
static void		narrate_departure(t_server_room *server_room, const char *who, const char *name, const t_release_result *res);
static void		rehome_successor(t_server_room *server_room, t_client *leaver, const t_release_result *res);
static int		slot_holding(const t_server_room *server_room, t_player_id pid);
static int		game_holding(const t_server_room *server_room, t_player_id pid);

/*
** The Room, both halves of it. The domain library owns the pure t_room; the
** games, the dirty flags and `ticking` are tetrisd's, and this file is the
** only one that holds either. Rooms are created here and destroyed here -
** lobby_create_room and lobby_destroy_room have no other caller - because the
** two objects share an index and a lifetime, and the one time a destroyed
** room's runtime was left behind, the next player handed that index was
** evicted by a tick belonging to a game that had already finished
** (docs/bugs/room_runtime_outlived_its_room.md).
**
** Everything above this file therefore speaks in rooms rather than in the
** domain object: a handler opens one, seats a client in one, starts one, or
** feeds an input to one, and never reaches through to a t_room to do it.
**
** The client's half of a Slot - t_room_binding - is the same fact written
** down twice, so it is owned here too. bind_client and server_room_unbind are
** the only writers, and server_room_resolve is the only reader that decides
** anything: it answers which room a binding names *now* and writes nothing,
** so no caller has to sequence a validating call before an indexing one.
*/

/**
 * @brief Opens the lobby and pairs every room in it with its runtime.
 *
 * @param srv Server whose rooms are being prepared.
 * @param br_slots Slots a Battle Royale room is created with.
 * @return 0 on success, -1 when the lobby could not be initialised.
 */
int	server_rooms_init(t_server *srv, int br_slots)
{
	int	i;

	if (srv == NULL)
		return (-1);
	if (lobby_init(&srv->lobby, LOBBY_MAX_ROOMS, br_slots) != 0)
		return (-1);
	i = 0;
	while (i < LOBBY_MAX_ROOMS)
	{
		memset(&srv->rooms[i], 0, sizeof(srv->rooms[i]));
		srv->rooms[i].room = &srv->lobby.rooms[i];
		srv->rooms[i].srv = srv;
		srv->rooms[i].index = i;
		room_blank(&srv->rooms[i]);
		i++;
	}
	return (0);
}

/**
 * @brief Advances every room that is playing, by the same elapsed time.
 *
 * This is what the ninety-nine ticker threads became. One reading of the clock
 * drives every game, which is correct rather than merely cheap: t_game
 * accumulates elapsed against each player's own gravity_interval_ms(level), so
 * a uniform coarse tick still produces per-player speeds, and a tick that
 * arrived late or coalesced with another advances by what actually passed.
 *
 * A room that has just ended gets one final pass before it is recorded, so the
 * snapshot saying "you topped out" always reaches the player.
 *
 * @param srv Server whose rooms are advanced.
 * @param elapsed_ms Milliseconds since the previous tick.
 */
void	server_rooms_tick(t_server *srv, int elapsed_ms)
{
	int	i;

	if (srv == NULL)
		return ;
	i = 0;
	while (i < LOBBY_MAX_ROOMS)
	{
		if (srv->rooms[i].ticking)
			tick_room(&srv->rooms[i], elapsed_ms);
		i++;
	}
}

/**
 * @brief Returns the room at a lobby index.
 *
 * @param srv Server holding the rooms.
 * @param index Room index, as assigned by the lobby.
 * @return The room, or NULL when the index is out of range.
 */
t_server_room	*server_room_at(t_server *srv, int index)
{
	if (srv == NULL || index < 0 || index >= LOBBY_MAX_ROOMS)
		return (NULL);
	return (&srv->rooms[index]);
}

/**
 * @brief Finds a room by its display name.
 *
 * Rooms are addressed by name on the wire because ids run per mode, so only
 * the prefixed name (S-01, D-02) is unique lobby-wide.
 *
 * @param srv Server to search.
 * @param name Room display name.
 * @return The room, or NULL when no room carries that name.
 */
t_server_room	*server_room_find(t_server *srv, const char *name)
{
	t_room	*room;

	if (srv == NULL || name == NULL || name[0] == '\0')
		return (NULL);
	room = lobby_find_room(&srv->lobby, name);
	if (room == NULL)
		return (NULL);
	return (server_room_at(srv, (int)(room - srv->lobby.rooms)));
}

/**
 * @brief Resolves a client's binding to the room it is actually sitting in.
 *
 * A Slot is one fact held in two places, and the client's copy can go out of
 * date on its own: a finished game clears every slot, so the binding outlives
 * the seat. This asks the rooms rather than trusting it - the index must still
 * carry that name, and that player must still be a member - and it answers
 * without writing anything, so a caller may resolve as often as it likes and
 * in whatever order suits it.
 *
 * Repairing a binding that no longer holds is server_room_unbind's job, at a
 * site that has decided to repair it.
 *
 * @param srv Server holding the rooms.
 * @param cli Client whose binding is being read.
 * @param name Room name the caller expects, or NULL to accept whichever room
 *        the client is in.
 * @return The room, or NULL when the binding no longer holds.
 */
t_server_room	*server_room_resolve(t_server *srv, const t_client *cli,
			const char *name)
{
	t_server_room	*server_room;

	if (srv == NULL || cli == NULL || cli->binding.room_index < 0)
		return (NULL);
	if (name != NULL && strcmp(name, cli->binding.room_name) != 0)
		return (NULL);
	server_room = server_room_at(srv, cli->binding.room_index);
	if (server_room == NULL
		|| strcmp(server_room->room->name, cli->binding.room_name) != 0
		|| room_find_member(server_room->room, cli->player_id) == NULL)
		return (NULL);
	return (server_room);
}

/**
 * @brief Clears a client's binding, so it is sitting in no room.
 *
 * The one place the client's half of a Slot is cleared: a fresh connection
 * starts here, a forfeit ends here, and a caller that has resolved a binding
 * to nothing repairs it here rather than blanking three fields of its own.
 *
 * @param cli Client to unbind.
 */
void	server_room_unbind(t_client *cli)
{
	if (cli == NULL)
		return ;
	cli->binding.room_index = -1;
	cli->binding.slot_index = -1;
	cli->binding.room_name[0] = '\0';
}

/**
 * @brief Creates a room and seats the client that asked for it as its owner.
 *
 * Creating and seating are one operation because a room nobody sits in has no
 * owner to name and nothing to end it: a room that could not seat its creator
 * is closed again here rather than left in the lobby.
 *
 * @param srv Server whose lobby gains the room.
 * @param cli Client creating it; bound to the room on success.
 * @param mode Mode the room is created with.
 * @return The 1-based slot the creator took, or -1 when the lobby is full or
 *         the seat was refused.
 */
int	server_room_open(t_server *srv, t_client *cli, t_game_mode mode)
{
	t_server_room	*server_room;
	t_room			*room;
	int				slot;

	if (srv == NULL || cli == NULL)
		return (-1);
	if (lobby_create_room(&srv->lobby, mode, &room) != 0)
		return (-1);
	server_room = server_room_at(srv, (int)(room - srv->lobby.rooms));
	if (server_room == NULL)
		return (-1);
	room_blank(server_room);
	slot = room_seat(room, cli->player_id, cli->username, room_probe, srv);
	if (slot < 0)
	{
		room_close(server_room);
		return (-1);
	}
	bind_client(cli, server_room, slot);
	/*
	 * Two lines, in this order, because the creator is both things at once
	 * and a feed that only said one of them would leave the room without a
	 * visible owner (UC-04.7).
	 */
	room_narrate(server_room, "PLAYER %s joined the room %s", cli->username,
		room->name);
	room_narrate(server_room, "PLAYER %s set as owner", cli->username);
	return (slot);
}

/**
 * @brief Seats a client in a room that already exists.
 *
 * @param server_room Room being joined.
 * @param cli Client joining; bound to the room when it takes a slot.
 * @param slot Receives the 1-based slot taken, or -1 when none was.
 * @return The room's verdict; JOIN_ACCEPTED with a slot of -1 means the room
 *         agreed to accept the player but had no seat to give.
 */
t_join_verdict	server_room_seat(t_server_room *server_room, t_client *cli,
			int *slot)
{
	t_join_verdict	verdict;

	*slot = -1;
	if (server_room == NULL || cli == NULL)
		return (JOIN_FULL);
	verdict = room_can_accept(server_room->room);
	if (verdict != JOIN_ACCEPTED)
		return (verdict);
	*slot = room_seat(server_room->room, cli->player_id, cli->username,
			room_probe, server_room->srv);
	if (*slot >= 0)
	{
		bind_client(cli, server_room, *slot);
		room_narrate(server_room, "PLAYER %s joined the room %s",
			cli->username, server_room->room->name);
	}
	return (verdict);
}

/**
 * @brief Starts the game in a room, on its owner's request.
 *
 * Beginning a game used to mean creating a thread, which could fail and had to
 * be joined before the next game could start in the same room. It is now one
 * flag the server's timer reads, so there is nothing left to fail and nothing
 * left to leak - and dealing the boards happens here, with the flag, rather
 * than a caller away from it.
 *
 * @param server_room Room whose game is starting.
 * @param cli Client asking to start; the room decides whether it may.
 * @return The room's verdict; the game runs only on START_ACCEPTED.
 */
t_start_verdict	server_room_start(t_server_room *server_room, t_client *cli)
{
	t_start_verdict	verdict;

	if (server_room == NULL || cli == NULL)
		return (START_NOT_OWNER);
	verdict = room_start(server_room->room, cli->player_id);
	if (verdict != START_ACCEPTED)
		return (verdict);
	deal_games(server_room);
	server_room->ticking = true;
	return (verdict);
}

/**
 * @brief Applies one input to the caller's own game.
 *
 * The move is marked dirty rather than pushed: the tick owns the outgoing
 * snapshots, so inputs and gravity produce one STATE stream instead of two
 * racing ones. They also run on the same thread, so they cannot interleave
 * inside a move at all.
 *
 * @param server_room Room holding the game.
 * @param cli Client whose own board is being driven.
 * @param action Which input to apply.
 * @param argument Direction for a move or rotation, hard flag for a drop.
 * @return true when the input changed the board, false when it was refused.
 */
bool	server_room_input(t_server_room *server_room, t_client *cli,
			t_input_action action, int argument)
{
	t_game	*game;
	bool	ok;

	game = server_room_game_of(server_room, cli);
	if (game == NULL || !game->active)
		return (false);
	if (action == INPUT_MOVE)
		ok = game_move(game, argument);
	else if (action == INPUT_ROTATE)
		ok = game_rotate(game, argument);
	else if (action == INPUT_HOLD)
		ok = game_hold(game);
	else if (action == INPUT_PAUSE)
		ok = game_pause(game, argument != 0);
	else if (action == INPUT_RESTART)
		ok = game_restart(game);
	else
		ok = game_drop(game, argument != 0);
	if (ok)
		server_room->dirty[cli->binding.slot_index - 1] = true;
	return (ok);
}

/**
 * @brief Marks the caller's own game as owing a snapshot.
 *
 * An ability is applied through game_ability rather than server_room_input,
 * because its verdict is richer than "it happened" - but the snapshot it owes
 * is queued exactly the same way, by the tick and not by the handler.
 *
 * @param server_room Room holding the game.
 * @param cli Client whose game changed.
 */
void	server_room_mark_dirty(t_server_room *server_room, const t_client *cli)
{
	if (server_room_game_of(server_room, cli) == NULL)
		return ;
	server_room->dirty[cli->binding.slot_index - 1] = true;
}

/**
 * @brief Borrows the caller's own game out of the room it is sitting in.
 *
 * The slot a client believes it holds is a copy of the room's fact, so the
 * player id on the game is checked against the connection before the game is
 * handed back - a stale binding names a slot somebody else may now be in.
 *
 * @param server_room Room holding the game.
 * @param cli Client whose own game is wanted.
 * @return The game, or NULL when this client has none in this room.
 */
t_game	*server_room_game_of(t_server_room *server_room, const t_client *cli)
{
	int	index;

	if (server_room == NULL || cli == NULL)
		return (NULL);
	index = cli->binding.slot_index - 1;
	if (index < 0 || index >= TD_MAX_GAMES
		|| index >= server_room->room->slot_count)
		return (NULL);
	if (server_room->games[index].player_id != cli->player_id)
		return (NULL);
	return (&server_room->games[index]);
}

/**
 * @brief Reports whether this room is played alone.
 *
 * Pausing and restarting are one-player affordances: in a room with anybody
 * else in it, one player stopping their own clock is an advantage over
 * everyone whose clock keeps running. Abilities ask the same question for a
 * different reason - a room with one player in it has no Target
 * (docs/CONTEXT.md), so most of the catalogue has nothing to act on.
 *
 * @param server_room Room to ask about.
 * @return true when the room's mode is Single.
 */
bool	server_room_is_solo(const t_server_room *server_room)
{
	return (server_room != NULL && server_room->room != NULL
		&& server_room->room->mode == MODE_SINGLE);
}

/**
 * @brief Removes a client from its room, forfeiting any game in progress.
 *
 * Leaving, topping out, and losing the connection are the same event:
 * the game is recorded on the spot, the slot is released, and
 * ownership passes to a successor when the owner was the one who left.
 *
 * @param srv Server the client belongs to.
 * @param cli Client leaving; its room binding is cleared.
 */
void	server_room_forfeit(t_server *srv, t_client *cli)
{
	t_release_result	res;
	t_server_room		*server_room;
	t_game				finished;
	char				name[ROOM_NAME_MAX];

	if (srv == NULL || cli == NULL)
		return ;
	server_room = server_room_resolve(srv, cli, NULL);
	game_reset(&finished);
	if (server_room != NULL)
	{
		snprintf(name, sizeof(name), "%s", server_room->room->name);
		forfeit_slot(server_room, cli->binding.slot_index, &finished);
		memset(&res, 0, sizeof(res));
		room_release(server_room->room, cli->player_id, room_probe, srv, &res);
		rehome_successor(server_room, cli, &res);
		narrate_departure(server_room, cli->username, name, &res);
	}
	if (finished.player_id != 0)
		award_game(srv, &finished, false);
	if (server_room != NULL)
		room_close(server_room);
	server_room_unbind(cli);
}

/**
 * @brief Describes a room to a caller that does not hold one.
 *
 * Everything an outside reader is entitled to know, read once under this
 * file's ownership rather than field by field through the domain object.
 *
 * @param server_room Room to describe.
 * @param out Receives the description; untouched when there is nothing to say.
 * @return true when the room exists and somebody is sitting in it.
 */
bool	server_room_describe(const t_server_room *server_room,
			t_server_room_view *out)
{
	if (server_room == NULL || out == NULL
		|| server_room->room->number_of_players == 0)
		return (false);
	out->name = server_room->room->name;
	out->mode = server_room->room->mode;
	out->status = server_room->room->status;
	out->players = server_room->room->number_of_players;
	out->slot_count = server_room->room->slot_count;
	out->ticking = server_room->ticking;
	return (true);
}

/**
 * @brief Projects an authoritative room and its occupied seats for a client.
 *
 * The wire model is compact but preserves server slot order. Keeping the
 * projection here maintains room.c's ownership of the domain object while
 * allowing the waiting-room handler to encode a stable snapshot.
 *
 * @param server_room Room to project.
 * @param out Receives the complete wire-facing snapshot.
 * @return true when the room exists and the projection fits.
 */
bool	server_room_snapshot(const t_server_room *server_room,
	t_body_room *out)
{
	const t_slot	*slot;
	int			index;

	if (server_room == NULL || out == NULL || server_room->room == NULL)
		return (false);
	memset(out, 0, sizeof(*out));
	snprintf(out->name, sizeof(out->name), "%s", server_room->room->name);
	out->mode = (t_body_mode)server_room->room->mode;
	out->status = (t_body_room_status)server_room->room->status;
	out->min_to_start = server_room->room->min_to_start;
	out->slot_count = server_room->room->slot_count;
	index = 0;
	while (index < server_room->room->slot_count)
	{
		slot = &server_room->room->slots[index];
		if (slot->occupied)
		{
			if (out->member_count >= BODY_ROOM_MEMBERS_MAX)
				return (false);
			out->members[out->member_count].slot = slot->index;
			out->members[out->member_count].player_id
				= slot->membership.player_id;
			out->members[out->member_count].owner
				= membership_is_owner(&slot->membership);
			out->members[out->member_count].ready
				= slot->status == SLOT_READY;
			snprintf(out->members[out->member_count].username,
				sizeof(out->members[out->member_count].username), "%s",
				slot->membership.username);
			out->member_count++;
		}
		index++;
	}
	return (true);
}

/**
 * @brief Reports whether a room has silenced one of its members.
 *
 * Asked of the Room rather than read off the domain object, because muting is
 * a fact about a seat and seats are this file's - the same reason every other
 * question about a membership is answered here (UC-09.4b).
 *
 * @param server_room Room to ask.
 * @param pid Player whose seat is in question.
 * @return true when that player is seated here and muted.
 */
bool	server_room_is_muted(const t_server_room *server_room, t_player_id pid)
{
	const t_slot	*slot;
	int				index;

	if (server_room == NULL || server_room->room == NULL)
		return (false);
	index = 0;
	while (index < server_room->room->slot_count)
	{
		slot = &server_room->room->slots[index];
		if (slot->occupied && slot->membership.player_id == pid)
			return (slot->membership.muted);
		index++;
	}
	return (false);
}

/**
 * @brief Borrows the game being played in one slot, for reading only.
 *
 * @param server_room Room holding the slot.
 * @param slot 0-based slot index.
 * @return The game, or NULL when the slot is out of range or holds nobody.
 */
const t_game	*server_room_game_at(const t_server_room *server_room, int slot)
{
	if (server_room == NULL || slot < 0 || slot >= TD_MAX_GAMES
		|| slot >= server_room->room->slot_count)
		return (NULL);
	if (server_room->games[slot].player_id == 0)
		return (NULL);
	return (&server_room->games[slot]);
}

/**
 * @brief Returns an emptied room to the lobby, and blanks it with the same
 *        call.
 *
 * Rooms outlive their games, but not their players: a room nobody is sitting
 * in is removed, which is what keeps a long-running server from filling its
 * lobby with the ghosts of finished games. The lobby hands that index straight
 * back out, so the runtime beside it has to go at the same moment - this is
 * the one place a room dies, precisely so there is no second place to forget.
 *
 * @param server_room Room to close; kept when a player is still sitting in it.
 */
static void	room_close(t_server_room *server_room)
{
	char	name[ROOM_NAME_MAX];

	if (server_room->room->number_of_players != 0)
		return ;
	snprintf(name, sizeof(name), "%s", server_room->room->name);
	lobby_destroy_room(&server_room->srv->lobby, name);
	room_blank(server_room);
}

/**
 * @brief Blanks a room's runtime so nothing of the last game is readable.
 *
 * Leaving `ticking` set is the sharp edge: the timer would advance a room the
 * lobby had already handed to somebody else, find no active games, and end a
 * game that had just been created - evicting whoever created it. Stale boards
 * were harmless on their own; the flag that decides whether anything reads
 * them was not.
 *
 * @param server_room Room whose runtime is cleared.
 */
static void	room_blank(t_server_room *server_room)
{
	int	slot;

	server_room->ticking = false;
	server_room->chat_seq = 0;
	slot = 0;
	while (slot < TD_MAX_GAMES)
	{
		game_reset(&server_room->games[slot]);
		server_room->dirty[slot] = false;
		slot++;
	}
}

/**
 * @brief Records which room and slot a connection now occupies.
 *
 * @param cli Client that was seated.
 * @param server_room Room it was seated in.
 * @param slot The 1-based slot index.
 */
static void	bind_client(t_client *cli, t_server_room *server_room, int slot)
{
	cli->binding.room_index = server_room->index;
	cli->binding.slot_index = slot;
	snprintf(cli->binding.room_name, sizeof(cli->binding.room_name), "%s",
		server_room->room->name);
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
static bool	room_probe(void *ctx, t_player_id pid)
{
	t_server	*srv;

	srv = ctx;
	if (srv == NULL)
		return (false);
	return (registry_player_online(&srv->reg, pid));
}

/**
 * @brief Deals every seated player a board.
 *
 * @param server_room Room whose game is starting.
 * @return The number of games started.
 */
static int	deal_games(t_server_room *server_room)
{
	t_slot		*slots;
	uint32_t	seed;
	int			started;
	int			i;

	slots = server_room->room->slots;
	started = 0;
	i = 0;
	while (i < server_room->room->slot_count && i < TD_MAX_GAMES)
	{
		if (slots[i].occupied)
		{
			seed = (uint32_t)(clock_now_ms() + (uint64_t)i * 7919u
					+ slots[i].membership.player_id);
			game_start(&server_room->games[i], slots[i].membership.player_id,
				seed);
			server_room->dirty[i] = true;
			started++;
		}
		i++;
	}
	return (started);
}

/**
 * @brief Advances one playing room and pushes whatever changed.
 *
 * @param server_room Room to advance.
 * @param elapsed_ms Milliseconds since the previous tick.
 */
static void	tick_room(t_server_room *server_room, int elapsed_ms)
{
	advance_and_push(server_room, elapsed_ms);
	if (!room_is_over(server_room))
		return ;
	advance_and_push(server_room, 0);
	server_room->ticking = false;
	record_and_reset(server_room);
	room_close(server_room);
}

/**
 * @brief Advances one room by an elapsed and pushes whatever changed.
 *
 * The final pass a finished room gets is advanced by zero: every game is
 * already inactive, so there is no gravity left to apply and the pass exists
 * only to carry out the snapshot that says so.
 *
 * @param server_room Room to advance.
 * @param elapsed_ms Milliseconds to advance by.
 */
static void	advance_and_push(t_server_room *server_room, int elapsed_ms)
{
	t_body_state	snaps[TD_MAX_GAMES];
	t_player_id		pids[TD_MAX_GAMES];

	push_all(server_room, tick_once(server_room, elapsed_ms, snaps, pids),
		snaps, pids);
}

/**
 * @brief Pushes a tick's snapshots to the players they belong to.
 *
 * @param server_room Room the snapshots came from.
 * @param n Number of snapshots collected.
 * @param snaps The snapshots.
 * @param pids The matching subject player ids.
 */
static void	push_all(t_server_room *server_room, int n,
				const t_body_state *snaps, const t_player_id *pids)
{
	char	name[ROOM_NAME_MAX];

	snprintf(name, sizeof(name), "%s", server_room->room->name);
	while (n > 0)
	{
		n--;
		push_state(server_room, name, pids[n], &snaps[n]);
	}
}

/**
 * @brief Pushes one player's snapshot as a server-originated STATE message.
 *
 * The subject rides in the request path, so the body stays a pure
 * projection of one game and says nothing about whose it is.
 *
 * @param server_room Room the snapshot came from.
 * @param room_name Room the subject is playing in.
 * @param pid The subject player.
 * @param snap Snapshot to encode and push.
 */
static void	push_state(t_server_room *server_room, const char *room_name,
				t_player_id pid, const t_body_state *snap)
{
	t_htttp_message	msg;
	unsigned char	*bytes;
	char			path[TETRISD_CONFIG_LINE_MAX];
	char			body[TETRISD_BODY_MAX_BYTES];
	size_t			len;
	int				body_len;

	body_len = body_state_encode(snap, body, sizeof(body));
	if (body_len <= 0)
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
		if (registry_enqueue(&server_room->srv->reg, pid, bytes, len, true) != 0)
			free(bytes);
	}
	htttp_message_free(&msg);
}

/**
 * @brief Advances every live game in the room and collects what changed.
 *
 * @param server_room Room to advance.
 * @param elapsed_ms Milliseconds since the previous tick.
 * @param snaps Receives one snapshot per changed game.
 * @param pids Receives the matching subject player ids.
 * @return Number of snapshots collected.
 */
static int	tick_once(t_server_room *server_room, int elapsed_ms,
			t_body_state *snaps, t_player_id *pids)
{
	int	slot;
	int	n;

	n = 0;
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->games[slot].player_id != 0)
		{
			if (game_gravity(&server_room->games[slot], elapsed_ms))
				server_room->dirty[slot] = true;
			if (server_room->dirty[slot])
			{
				server_room->games[slot].seq++;
				game_snapshot(&server_room->games[slot], &snaps[n]);
				pids[n] = server_room->games[slot].player_id;
				server_room->dirty[slot] = false;
				n++;
			}
		}
		slot++;
	}
	return (n);
}

/**
 * @brief Reports whether the room has nothing left to tick.
 *
 * @param server_room Room to check.
 * @return true when every game has ended or every player has left.
 */
static bool	room_is_over(t_server_room *server_room)
{
	bool	over;
	int		slot;

	over = true;
	if (server_room->room->number_of_players == 0)
		return (true);
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->games[slot].active)
			over = false;
		slot++;
	}
	return (over);
}

/**
 * @brief Records every finished game and empties the room.
 *
 * Finishing a game clears every slot (the room domain's rule), so the room
 * ends empty and is handed back to the lobby; players who want another game
 * join a fresh one.
 *
 * @param server_room Room whose game has ended.
 */
static void	record_and_reset(t_server_room *server_room)
{
	t_game	played[TD_MAX_GAMES];
	int		n;
	int		slot;

	n = 0;
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->games[slot].player_id != 0 && !server_room->games[slot].recorded)
		{
			server_room->games[slot].recorded = true;
			played[n++] = server_room->games[slot];
		}
		game_reset(&server_room->games[slot]);
		server_room->dirty[slot] = false;
		slot++;
	}
	room_finish(server_room->room);
	while (n > 0)
	{
		n--;
		award_game(server_room->srv, &played[n], !played[n].topped_out);
	}
}

/**
 * @brief Ends one slot's game so the leaver's result can be recorded.
 *
 * @param server_room Room holding the slot.
 * @param slot 1-based slot index the player occupied.
 * @param out Receives the finished game, or a blank game when there was none.
 */
static void	forfeit_slot(t_server_room *server_room, int slot, t_game *out)
{
	int	index;

	index = slot - 1;
	if (index < 0 || index >= TD_MAX_GAMES)
		return ;
	if (server_room->games[index].player_id == 0 || server_room->games[index].recorded)
		return ;
	server_room->games[index].active = false;
	server_room->games[index].recorded = true;
	*out = server_room->games[index];
	game_reset(&server_room->games[index]);
	server_room->dirty[index] = false;
}

/**
 * @brief Records one finished game and pays its player what it earned.
 *
 * The economy's one rule, in one place (docs/game-economics.md), and it is
 * charged against the player's running total rather than each game on its own.
 * That difference is the whole reason this is two divisions and not one: a
 * game worth less than the exchange rate would otherwise round to nothing and
 * stay nothing, so a player having a bad night could play all evening and earn
 * zero. Taking the difference between the total before and after leaves the
 * remainder on the account, where the next game picks it up.
 *
 * The running total it charges against is `lifetime_points`, read back out of
 * the store. It is deliberately not `leaderboard_score`: that one is the
 * player's best single game, so it stops moving the moment they stop beating
 * it, and a wallet charged against it would stop paying at the same moment.
 *
 * The read and the write are separate locks, and that is safe because a player
 * holds one connection and the reactor is the only thread that
 * records a game - nobody else can be crediting this account in between.
 *
 * @param srv Server holding the store.
 * @param game The finished game.
 * @param won Whether this game counts as a win.
 */
static void	award_game(t_server *srv, const t_game *game, bool won)
{
	t_player	before;
	int64_t		scored;
	int64_t		earned;

	scored = (int64_t)game->score.total;
	earned = scored / TETRISD_POINTS_PER_WALLET_POINT;
	if (db_get_player(srv->db, game->player_id, &before) == DB_OK)
		earned = (before.lifetime_points + scored)
			/ TETRISD_POINTS_PER_WALLET_POINT
			- before.lifetime_points / TETRISD_POINTS_PER_WALLET_POINT;
	db_record_game(srv->db, game->player_id, scored, earned, won);
}

/**
 * @brief Tells the room who left it and who owns it now.
 *
 * Called after room_release and before room_close, which is the only window
 * where both facts are known and the room still exists: release is what fills
 * in the successor, and close is what destroys a room the last player just
 * left. The leaver is already out of their seat and so does not hear either
 * line - they know.
 *
 * A room emptied by this departure has nobody to tell, and the broadcast is
 * simply delivered to no one rather than guarded against here.
 *
 * @param server_room Room the player left.
 * @param who The departing player's name.
 * @param name The room's name, read before release in case it is destroyed.
 * @param res What release decided, including any owner succession.
 */
static void	narrate_departure(t_server_room *server_room, const char *who,
	const char *name, const t_release_result *res)
{
	if (!res->released)
		return ;
	room_narrate(server_room, "PLAYER %s left the room %s", who, name);
	if (res->owner_changed)
		room_narrate(server_room, "PLAYER %s set as the owner",
			res->new_owner_name);
}

/**
 * @brief Follows a promoted owner into their new seat with everything of theirs
 *        that is indexed by it.
 *
 * The room domain does not hand the successor the owner's role where they sit:
 * room_release *moves* them into the seat the owner vacated and clears the one
 * they were in (UC-07a asks for the lower slot). Two things on this side are
 * indexed by that seat and knew nothing about the move - the room's games and
 * dirty flags, and the client's own copy of its slot - so after a mid-game
 * owner leave, room->slots[i] and games[i] described different players.
 *
 * Nothing broke immediately only because both of those wrong answers agreed
 * with each other: server_room_game_of matches games[index].player_id against
 * the connection, so a board reached its player through a stale index. The
 * moment anything pairs a seat with the game in it - an opponent view, garbage
 * against a Target - it would attribute a board to the wrong player, so the
 * seat is made to mean one thing again here.
 *
 * The leaver's own binding is deliberately not consulted: both indices are
 * looked up by player id, so this is correct even for a successor who was
 * themselves promoted earlier, and it is what stops that ever being true again.
 *
 * @param server_room Room whose owner has just been replaced.
 * @param leaver The departing client, excluded when finding the successor's.
 * @param res What release decided; nothing is done unless it promoted somebody.
 */
static void	rehome_successor(t_server_room *server_room, t_client *leaver,
	const t_release_result *res)
{
	t_client	*successor;
	int			to;
	int			from;

	if (!res->owner_changed)
		return ;
	to = slot_holding(server_room, res->new_owner);
	from = game_holding(server_room, res->new_owner);
	successor = registry_find_other(&server_room->srv->reg, res->new_owner,
			leaver);
	if (successor != NULL && to >= 0)
		successor->binding.slot_index = to + 1;
	if (to < 0 || from < 0 || to == from || to >= TD_MAX_GAMES
		|| server_room->games[to].player_id != 0)
		return ;
	server_room->games[to] = server_room->games[from];
	server_room->dirty[to] = server_room->dirty[from];
	game_reset(&server_room->games[from]);
	server_room->dirty[from] = false;
}

/**
 * @brief Finds which seat a player occupies now.
 *
 * @param server_room Room to search.
 * @param pid Player to look for.
 * @return The 0-based slot index, or -1 when that player holds no seat.
 */
static int	slot_holding(const t_server_room *server_room, t_player_id pid)
{
	int	index;

	index = 0;
	while (index < server_room->room->slot_count)
	{
		if (server_room->room->slots[index].occupied
			&& server_room->room->slots[index].membership.player_id == pid)
			return (index);
		index++;
	}
	return (-1);
}

/**
 * @brief Finds which of the room's boards belongs to a player.
 *
 * Asked separately from slot_holding precisely because the two can disagree,
 * which is the whole reason rehome_successor exists.
 *
 * @param server_room Room to search.
 * @param pid Player to look for.
 * @return The 0-based game index, or -1 when that player has no board here.
 */
static int	game_holding(const t_server_room *server_room, t_player_id pid)
{
	int	index;

	index = 0;
	while (index < TD_MAX_GAMES)
	{
		if (server_room->games[index].player_id == pid)
			return (index);
		index++;
	}
	return (-1);
}
