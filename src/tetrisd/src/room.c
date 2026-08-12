#include "tetrisd.h"

// Static Functions
static void		room_close(t_server_room *server_room);
static void		room_blank(t_server_room *server_room);
static void		bind_client(t_client *cli, t_server_room *server_room, int slot);
static bool		room_probe(void *ctx, t_player_id pid);
static int		deal_games(t_server_room *server_room);
static void		tick_room(t_server_room *server_room, int elapsed_ms);
static void		advance_and_push(t_server_room *server_room, int elapsed_ms);
static void		push_state(t_server_room *server_room, const char *room_name, t_player_id pid, const t_body_state *snap);
static void		advance_games(t_server_room *server_room, int elapsed_ms);
static void		build_snapshot(t_server_room *server_room, int slot, t_body_state *snap);
static void		number_snapshot(t_server_room *server_room, t_player_id pid, t_body_state *snap);
static void		decorate_snapshot(t_server_room *server_room, int slot, t_body_state *snap);
static void		fill_opponents(t_server_room *server_room, int subject, t_body_state *snap);
static void		project_opponent(t_server_room *server_room, int slot, t_body_opponent *out);
static t_body_phase	game_phase(const t_game *game);
static void		spend_selection(t_server_room *server_room, int elapsed_ms);
static int		spend_countdown(t_server_room *server_room, int elapsed_ms);
static void		mark_all_dirty(t_server_room *server_room);
static void		spread_dirty(t_server_room *server_room);
static void		settle_garbage(t_server_room *server_room);
static int		slot_of_player(const t_server_room *server_room,
					t_player_id pid);
static const t_participant	*participant_of(const t_server_room *server_room,
								t_player_id pid);
static t_participant		*participant_open(t_server_room *server_room,
								t_player_id pid);
static t_item_id			participant_character(
								const t_server_room *server_room,
								t_player_id pid);
static void					participant_close(t_server_room *server_room,
								t_player_id pid);
static int		count_live_games(const t_server_room *server_room);
static void		settle_results(t_server_room *server_room);
static bool		room_is_over(t_server_room *server_room);
static void		record_and_reset(t_server_room *server_room);
static void		forfeit_slot(t_server_room *server_room, int slot, t_game *out);
static void		award_game(t_server *srv, const t_game *game, bool won);
static void		narrate_departure(t_server_room *server_room, const char *who, const char *name, const t_release_result *res);
static void		rehome_successor(t_server_room *server_room, t_client *leaver, const t_release_result *res);
static int		slot_holding(const t_server_room *server_room, t_player_id pid);
static void		settle_selection(t_server_room *server_room);
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
 * Joining a room the caller is already sitting in gives back the seat they
 * have. The room refuses a duplicate player, which is right - one player is
 * not two - but that refusal used to come back as a failed JOIN, and a client
 * returning to its own room from a finished match read that as the room being
 * gone and went to the lobby. So the answer to "put me in this room" is the
 * seat, whether the caller had to be given one or already had it.
 *
 * The check comes before room_can_accept deliberately: a room in game is
 * closed to newcomers and is exactly where somebody already seated has the
 * most reason to ask.
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
	int				seated;

	*slot = -1;
	if (server_room == NULL || cli == NULL)
		return (JOIN_FULL);
	seated = slot_of_player(server_room, cli->player_id);
	if (seated >= 0)
	{
		*slot = seated + 1;
		bind_client(cli, server_room, *slot);
		return (JOIN_ACCEPTED);
	}
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
 * @brief Records whether one seated player has declared themselves ready.
 *
 * Readiness is the room's to hold, not the client's. A client that kept it
 * locally had it overwritten by its own next refresh - the server had never
 * had an opinion, so it kept answering with the one it was born with.
 *
 * @param server_room Room holding the seat.
 * @param cli Client declaring.
 * @param ready true to declare ready, false to withdraw it.
 * @return true when the declaration was recorded.
 */
bool	server_room_set_ready(t_server_room *server_room, t_client *cli,
			bool ready, t_item_id character)
{
	t_participant	*participant;

	if (server_room == NULL || cli == NULL)
		return (false);
	if (room_set_ready(server_room->room, cli->player_id, ready) != 0)
		return (false);
	/*
	 * The character rides with the declaration because that is the last
	 * moment it can be chosen: deal_games reads it, and after that the match
	 * is running. A body that omits it leaves whatever was declared before,
	 * which is what makes re-declaring readiness not also a way to lose it.
	 *
	 * It rides with a *declaration*, though, and not with a withdrawal. A
	 * seat naming a character is what server_room_all_locked reads as locked
	 * in, so recording one alongside `ready 0` let a player withdraw during
	 * the select window and have the room deal the match on the strength of
	 * it - the opposite of what they asked for. Withdrawing therefore takes
	 * the fighter back with it, and the seat is unlocked again.
	 *
	 * It is written against the player and not against the seat they are in.
	 * Filed by seat, a declaration made before an owner left was read back
	 * from the seat the successor was moved out of, which is the whole of the
	 * bug t_participant exists to close.
	 */
	participant = participant_open(server_room, cli->player_id);
	if (participant == NULL)
		return (true);
	if (!ready)
		participant->character = 0;
	else if (character != 0)
		participant->character = character;
	return (true);
}

/**
 * @brief Finds the 0-based slot a player is seated in.
 *
 * @param server_room Room to search.
 * @param pid The player to find.
 * @return The 0-based slot, or -1 when they are not seated here.
 */
static int	slot_of_player(const t_server_room *server_room, t_player_id pid)
{
	int	slot;

	if (server_room == NULL || server_room->room == NULL)
		return (-1);
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->room->slots[slot].occupied
			&& server_room->room->slots[slot].membership.player_id == pid)
			return (slot);
		slot++;
	}
	return (-1);
}

/**
 * @brief Finds the match record belonging to a player, if they have one.
 *
 * The one lookup every match fact goes through, and the reason nothing in this
 * file indexes a result, a placing or a fighter by seat.
 *
 * A player with no record is not an error and is answered with NULL: records
 * are opened when a match is set up, so anyone asking before that - a lobby
 * snapshot of a room nobody has readied in - is asking about a match that does
 * not exist yet.
 *
 * @param server_room Room to search.
 * @param pid The player to find.
 * @return Their record, or NULL when this room holds none for them.
 */
static const t_participant	*participant_of(const t_server_room *server_room,
	t_player_id pid)
{
	int	index;

	if (server_room == NULL || pid == 0)
		return (NULL);
	index = 0;
	while (index < TD_MAX_GAMES)
	{
		if (server_room->participants[index].player_id == pid)
			return (&server_room->participants[index]);
		index++;
	}
	return (NULL);
}

/**
 * @brief Finds a player's match record, opening a blank one if they have none.
 *
 * Every writer goes through this rather than through participant_of, so a fact
 * about a match can be written down the first time there is one to write -
 * there is no separate moment at which the room has to remember to create the
 * record first.
 *
 * The array is TD_MAX_GAMES wide and a room seats at most that many players, so
 * the full answer is unreachable while every record belongs to somebody seated.
 * It is still answered rather than assumed: the caller has a player id and no
 * room to put it in, and inventing a seat for them would be worse than saying
 * so.
 *
 * @param server_room Room to search.
 * @param pid The player to find or admit.
 * @return Their record, or NULL when the room has no free one.
 */
static t_participant	*participant_open(t_server_room *server_room,
	t_player_id pid)
{
	int	index;
	int	free_slot;

	if (server_room == NULL || pid == 0)
		return (NULL);
	free_slot = -1;
	index = 0;
	while (index < TD_MAX_GAMES)
	{
		if (server_room->participants[index].player_id == pid)
			return (&server_room->participants[index]);
		if (free_slot < 0 && server_room->participants[index].player_id == 0)
			free_slot = index;
		index++;
	}
	if (free_slot < 0)
		return (NULL);
	memset(&server_room->participants[free_slot], 0, sizeof(t_participant));
	server_room->participants[free_slot].player_id = pid;
	return (&server_room->participants[free_slot]);
}

/**
 * @brief Reads the fighter a player declared for this match.
 *
 * Its own function because the answer for a player who has declared nothing and
 * the answer for a player with no record at all are the same one - 0, meaning
 * "whatever the account has equipped" - and every reader wants that collapse.
 *
 * @param server_room Room holding the match.
 * @param pid The player to ask about.
 * @return The declared character id, or 0 when none was declared.
 */
static t_item_id	participant_character(const t_server_room *server_room,
	t_player_id pid)
{
	const t_participant	*participant;

	participant = participant_of(server_room, pid);
	if (participant == NULL)
		return (0);
	return (participant->character);
}

/**
 * @brief Releases a player's match record.
 *
 * Records are bounded, so one belonging to a player who is no longer in the
 * room is not merely stale - it is a record the next player to sit down cannot
 * have. A room whose seats turn over enough times would run out and quietly
 * stop recording what anybody declared.
 *
 * @param server_room Room holding the record.
 * @param pid The player whose record is released.
 */
static void	participant_close(t_server_room *server_room, t_player_id pid)
{
	int	index;

	if (server_room == NULL || pid == 0)
		return ;
	index = 0;
	while (index < TD_MAX_GAMES)
	{
		if (server_room->participants[index].player_id == pid)
		{
			memset(&server_room->participants[index], 0,
				sizeof(t_participant));
			return ;
		}
		index++;
	}
}

/**
 * @brief Reports whether every seat in a full room has declared ready.
 *
 * What a Double room starts on. It is asked of the Room because both halves
 * of the answer - how many seats are taken and what each of them has declared
 * - are this file's.
 *
 * @param server_room Room to ask.
 * @return true when the room is full and every occupant is ready.
 */
bool	server_room_all_ready(const t_server_room *server_room)
{
	int	slot;

	if (server_room == NULL || server_room->room == NULL)
		return (false);
	if (server_room->room->number_of_players < server_room->room->min_to_start)
		return (false);
	slot = 0;
	while (slot < server_room->room->slot_count)
	{
		if (server_room->room->slots[slot].occupied
			&& server_room->room->slots[slot].status != SLOT_READY)
			return (false);
		slot++;
	}
	return (true);
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
 * Only a Single room is dealt from here. Anywhere two people are playing, the
 * owner's start opens the character-select window instead, exactly as the
 * last readiness does - otherwise the same room reached a match by two
 * different routes, and the one the owner took skipped the moment in which
 * either of them was allowed to choose a fighter. The match still begins from
 * this request; it is the window that begins first.
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
	if (!server_room_is_solo(server_room))
	{
		verdict = room_can_start(server_room->room, cli->player_id);
		if (verdict != START_ACCEPTED)
			return (verdict);
		/*
		 * A window that will not open is one that is already open: the domain
		 * takes a READY room and nothing else, so the only way past the
		 * verdict above and into this refusal is a room that is already
		 * choosing. That is a start the owner has already made.
		 */
		if (!server_room_begin_selection(server_room))
			return (START_ALREADY_STARTED);
		return (START_ACCEPTED);
	}
	verdict = room_start(server_room->room, cli->player_id);
	if (verdict != START_ACCEPTED)
		return (verdict);
	deal_games(server_room);
	if (!server_room_is_solo(server_room))
	{
		server_room->countdown_ms = TETRISD_MATCH_COUNTDOWN_MS;
		server_room->countdown_second = -1;
	}
	server_room->ticking = true;
	return (verdict);
}

/**
 * @brief Opens the character-select window on a room that has readied itself.
 *
 * What readiness now completes, in place of the match. A Double room used to
 * be dealt the instant its last seat declared, which meant the fighter a
 * player took into a match was whichever their account happened to have
 * equipped - there was no moment between committing and playing in which to
 * choose one. This is that moment, and the room owns its clock so both players
 * are shown the same number and are dealt in at the same instant.
 *
 * Every seat's declared character is cleared as the window opens. That is what
 * makes "locked in" a fact about this match rather than a leftover from the
 * last one, and it is why the room can decide to start early by asking whether
 * every seat has named one.
 *
 * @param server_room Room to hold open.
 * @return true when the window is now running.
 */
bool	server_room_begin_selection(t_server_room *server_room)
{
	int	index;

	if (server_room == NULL || server_room->room == NULL)
		return (false);
	if (room_begin_selection(server_room->room) != 0)
		return (false);
	index = 0;
	while (index < TD_MAX_GAMES)
	{
		server_room->participants[index].character = 0;
		index++;
	}
	server_room->select_ms = TETRISD_MATCH_SELECT_MS;
	server_room->select_second = -1;
	server_room->ticking = true;
	room_narrate(server_room, "ROOM choosing fighters");
	return (true);
}

/**
 * @brief Reports whether every seated player has settled on a fighter.
 *
 * A seat is locked exactly when it names a character, so there is no second
 * flag that could disagree with this one. An empty room is not locked in -
 * there is nobody in it to have chosen.
 *
 * Only a room with a window open can answer yes. Outside one the question has
 * no meaning: a Single player who equipped a fighter last week would otherwise
 * be "all locked" the moment they declared, and the room would deal itself a
 * match nobody asked it for.
 *
 * @param server_room Room to ask.
 * @return true when a select window is open and every occupied seat has
 *         declared a character.
 */
bool	server_room_all_locked(const t_server_room *server_room)
{
	int	slot;

	if (server_room == NULL || server_room->room == NULL)
		return (false);
	if (server_room->room->status != ROOM_SELECTING)
		return (false);
	if (server_room->room->number_of_players < server_room->room->min_to_start)
		return (false);
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->room->slots[slot].occupied
			&& participant_character(server_room,
				server_room->room->slots[slot].membership.player_id) == 0)
			return (false);
		slot++;
	}
	return (true);
}

/**
 * @brief Starts a room that has readied itself, on nobody's behalf.
 *
 * The room decides this one, so it cannot go through server_room_start: that
 * asks whether the requester owns the room, and the player who completes a
 * readiness is whoever declared last - as often the joiner as the owner. A
 * Double room that refused to start because the wrong person finished getting
 * ready would be enforcing a rule nobody wrote.
 *
 * The owner is still named to the domain, because a room's start is the
 * owner's in every other mode and the verdict is the same one.
 *
 * @param server_room Room to start.
 * @return true when the match is now running.
 */
bool	server_room_autostart(t_server_room *server_room)
{
	t_player_id	owner;
	int			slot;

	if (server_room == NULL || server_room->room == NULL)
		return (false);
	owner = 0;
	slot = 0;
	while (slot < server_room->room->slot_count)
	{
		if (server_room->room->slots[slot].occupied
			&& membership_is_owner(&server_room->room->slots[slot].membership))
			owner = server_room->room->slots[slot].membership.player_id;
		slot++;
	}
	if (owner == 0 || room_start(server_room->room, owner) != START_ACCEPTED)
		return (false);
	deal_games(server_room);
	if (!server_room_is_solo(server_room))
	{
		server_room->countdown_ms = TETRISD_MATCH_COUNTDOWN_MS;
		server_room->countdown_second = -1;
	}
	server_room->ticking = true;
	return (true);
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
	/*
	 * A board being held before the match starts is not one to play on. The
	 * game is active and unpaused for those three seconds - that is what makes
	 * the hold the room's rather than the game's - so the game itself would
	 * accept these, and a player who kept dropping through their own countdown
	 * would begin the match with a stack.
	 */
	if (server_room->countdown_ms > 0)
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
 * @brief Reports whether readiness alone is enough to commit this room.
 *
 * Double, and only Double. Two players readying is the whole of a Double room's
 * agreement - there is nobody else to wait for, and the player who declares
 * last is as often the joiner as the owner, so requiring the owner to then
 * press start would be a second confirmation of a decision already unanimous.
 *
 * A Battle Royale is the opposite case. It starts below capacity by design, so
 * "everybody who is here is ready" is true of four people in a forty-seat room
 * and says nothing about whether the match should begin - it is the owner's
 * call, which is what docs/use_cases.md has always specified and what the
 * client's waiting room has always drawn. Readiness there is a signal to the
 * owner rather than a trigger.
 *
 * Single has no readiness to speak of and reaches its game through START.
 *
 * @param server_room Room to ask.
 * @return true when the last readiness should commit the room by itself.
 */
bool	server_room_starts_on_ready(const t_server_room *server_room)
{
	return (server_room != NULL && server_room->room != NULL
		&& server_room->room->mode == MODE_DOUBLE);
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
		/*
		 * The match record leaves with the player, unless a match is running -
		 * then it is theirs until that match ends. forfeit_slot has just reset
		 * their board, so during a game the record is the only thing left
		 * holding what they did in it. Outside one there is nothing to hold,
		 * and it is asked before room_release because that is the last moment
		 * the room still says what it was doing.
		 */
		if (server_room->room->status != ROOM_IN_GAME)
			participant_close(server_room, cli->player_id);
		memset(&res, 0, sizeof(res));
		room_release(server_room->room, cli->player_id, room_probe, srv, &res);
		rehome_successor(server_room, cli, &res);
		narrate_departure(server_room, cli->username, name, &res);
		if (server_room->room->status == ROOM_SELECTING)
			settle_selection(server_room);
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
	out->select_ms = server_room->select_ms;
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
			out->members[out->member_count].character
				= (uint32_t)participant_character(server_room,
					slot->membership.player_id);
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
	server_room->countdown_ms = 0;
	server_room->countdown_second = -1;
	server_room->select_ms = 0;
	server_room->select_second = -1;
	memset(server_room->participants, 0, sizeof(server_room->participants));
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
 * A match record is opened here as well as a board, and this is what makes
 * every player in a running match have one: readiness opens a record only for
 * those who declared, and a Battle Royale is started by its owner over seats
 * that need never have. settle_results then has somewhere to write a verdict
 * for each of them without deciding, at the end of a match, who was in it.
 *
 * @param server_room Room whose game is starting.
 * @return The number of games started.
 */
static int	deal_games(t_server_room *server_room)
{
	t_participant	*participant;
	t_slot			*slots;
	uint32_t		seed;
	int				started;
	int				i;

	slots = server_room->room->slots;
	started = 0;
	i = 0;
	while (i < server_room->room->slot_count && i < TD_MAX_GAMES)
	{
		if (slots[i].occupied)
		{
			participant = participant_open(server_room,
					slots[i].membership.player_id);
			seed = (uint32_t)(clock_now_ms() + (uint64_t)i * 7919u
					+ slots[i].membership.player_id);
			game_start(&server_room->games[i], slots[i].membership.player_id,
				seed);
			if (participant != NULL)
				server_room->games[i].character_id = participant->character;
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
	if (server_room->room->status == ROOM_SELECTING)
	{
		spend_selection(server_room, elapsed_ms);
		return ;
	}
	if (server_room->countdown_ms > 0)
		elapsed_ms = spend_countdown(server_room, elapsed_ms);
	advance_and_push(server_room, elapsed_ms);
	if (!room_is_over(server_room))
		return ;
	settle_results(server_room);
	advance_and_push(server_room, 0);
	server_room->ticking = false;
	record_and_reset(server_room);
	room_close(server_room);
}

/**
 * @brief Runs the character-select window, and deals the match when it ends.
 *
 * The window ends either way it can: every seat has locked in, or the clock
 * ran out. Locking in early is therefore worth something - the two players who
 * both know who they are playing are not made to wait out fifteen seconds -
 * and a player who never chooses still gets a match, played as whatever their
 * account has equipped.
 *
 * No game exists yet, so there is nothing to advance and nothing to push but
 * the room itself; the clients read the remaining milliseconds out of the room
 * snapshot they are already polling.
 *
 * @param server_room Room whose window is running.
 * @param elapsed_ms Milliseconds this tick is worth.
 */
static void	spend_selection(t_server_room *server_room, int elapsed_ms)
{
	int	second;

	if (server_room->select_ms > elapsed_ms
		&& !server_room_all_locked(server_room))
	{
		server_room->select_ms -= elapsed_ms;
		second = (server_room->select_ms + 999) / 1000;
		if (second != server_room->select_second)
			server_room->select_second = second;
		return ;
	}
	server_room->select_ms = 0;
	server_room->select_second = -1;
	if (!server_room_autostart(server_room))
	{
		/*
		 * Nothing left to start - the room emptied under the window, or fell
		 * below its minimum while it was open. Closing it puts the room back
		 * where the remaining player can wait for another opponent rather
		 * than sitting on a roster screen forever.
		 */
		room_abort_selection(server_room->room);
		server_room->ticking = false;
	}
}

/**
 * @brief Holds a dealt match still, and hands back whatever time is left over.
 *
 * The countdown is spent before gravity rather than instead of it, so the tick
 * it runs out on still advances the game by its remainder - the match begins
 * exactly three seconds after it was dealt rather than at the start of the
 * next tick, which is the same reasoning game_gravity already applies to a
 * clear that finishes partway through a late tick.
 *
 * A snapshot is owed only when the second on screen changes. Three seconds at
 * the tick rate is a couple of hundred frames of a number the client can
 * interpolate between four of, and the frames a match needs are the ones after
 * it starts.
 *
 * @param server_room Room being held.
 * @param elapsed_ms Milliseconds this tick is worth.
 * @return Milliseconds left after the countdown took its share.
 */
static int	spend_countdown(t_server_room *server_room, int elapsed_ms)
{
	int	second;

	if (elapsed_ms >= server_room->countdown_ms)
	{
		elapsed_ms -= server_room->countdown_ms;
		server_room->countdown_ms = 0;
		mark_all_dirty(server_room);
		return (elapsed_ms);
	}
	server_room->countdown_ms -= elapsed_ms;
	second = (server_room->countdown_ms + 999) / 1000;
	if (second != server_room->countdown_second)
	{
		server_room->countdown_second = second;
		mark_all_dirty(server_room);
	}
	return (0);
}

/**
 * @brief Marks every live game in the room as owing a snapshot.
 *
 * @param server_room Room whose games are marked.
 */
static void	mark_all_dirty(t_server_room *server_room)
{
	int	slot;

	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->games[slot].player_id != 0)
			server_room->dirty[slot] = true;
		slot++;
	}
}

/**
 * @brief Advances one room by an elapsed and pushes whatever changed.
 *
 * The final pass a finished room gets is advanced by zero: every game is
 * already inactive, so there is no gravity left to apply and the pass exists
 * only to carry out the snapshot that says so.
 *
 * Three passes, and the order between them is the point. Every board is
 * advanced before any garbage is settled, and all of that happens before any
 * board is projected - so no player is ever shown a room half a tick old, with
 * their own board advanced and a rival's not. That guarantee is about the three
 * passes and not about collecting anything, which is why the third one is free
 * to encode and push each snapshot as it builds it.
 *
 * It used to collect instead: one t_body_state per seat into an array on this
 * stack, and a second walk to push them. At sixteen seats that is 17 KB and
 * unremarkable, and at ninety-nine it is 103 KB - survivable, but paid on the
 * reactor's own stack on every tick of every room, to hold snapshots that are
 * each read exactly once by the line that would have followed. It also grows
 * with the body: the arena section adds ~4 KB to a t_body_state, which takes
 * the array past half a megabyte without anything about this function
 * changing. One snapshot, reused, is a kilobyte whatever the room holds and
 * whatever the body grows into.
 *
 * @param server_room Room to advance.
 * @param elapsed_ms Milliseconds to advance by.
 */
static void	advance_and_push(t_server_room *server_room, int elapsed_ms)
{
	t_body_state	snap;
	char			name[ROOM_NAME_MAX];
	int				slot;

	advance_games(server_room, elapsed_ms);
	settle_garbage(server_room);
	spread_dirty(server_room);
	snprintf(name, sizeof(name), "%s", server_room->room->name);
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->games[slot].player_id != 0
			&& server_room->dirty[slot])
		{
			build_snapshot(server_room, slot, &snap);
			push_state(server_room, name,
				server_room->games[slot].player_id, &snap);
			server_room->dirty[slot] = false;
		}
		slot++;
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
 * @brief Applies gravity to every live game in the room.
 *
 * The first of advance_and_push's three passes. It is its own function because
 * the ordering guarantee depends on it finishing before the next one starts,
 * and a pass with a name is harder to fold into the loop that follows it.
 *
 * @param server_room Room to advance.
 * @param elapsed_ms Milliseconds since the previous tick.
 */
static void	advance_games(t_server_room *server_room, int elapsed_ms)
{
	int	slot;

	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->games[slot].player_id != 0
			&& game_gravity(&server_room->games[slot], elapsed_ms))
			server_room->dirty[slot] = true;
		slot++;
	}
}

/**
 * @brief Projects one seat's board into the snapshot its player is owed.
 *
 * The sequence number is stepped here rather than by the caller, because it
 * counts snapshots taken of this board and this is the only place one is.
 *
 * @param server_room Room holding the seat.
 * @param slot 0-based slot to project.
 * @param snap Receives the snapshot, overwriting whatever it held.
 */
static void	build_snapshot(t_server_room *server_room, int slot,
			t_body_state *snap)
{
	server_room->games[slot].seq++;
	game_snapshot(&server_room->games[slot], snap);
	decorate_snapshot(server_room, slot, snap);
	number_snapshot(server_room, server_room->games[slot].player_id, snap);
}

/**
 * @brief Makes one player's move owe everybody in the room a snapshot.
 *
 * Every snapshot now carries every board in the room, so a frame is out of
 * date the moment anybody moves - not only its own subject. Marking only the
 * player who acted would leave the other watching a board that froze whenever
 * they themselves stopped playing.
 *
 * This is the cost of carrying both boards in one message rather than two: a
 * Double room pushes two frames where it used to push one. It is the right
 * trade, because the alternative is a client drawing two boards from two
 * different instants, and the STATE lane is a latest-wins mailbox - a client
 * that cannot keep up drops frames rather than delaying anybody.
 *
 * Single is left alone: there is nobody else in the room to tell.
 *
 * @param server_room Room whose dirty flags are spread.
 */
static void	spread_dirty(t_server_room *server_room)
{
	int	slot;

	if (server_room_is_solo(server_room))
		return ;
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->dirty[slot])
		{
			mark_all_dirty(server_room);
			return ;
		}
		slot++;
	}
}

/**
 * @brief Turns the lines each game cleared into garbage against its Target.
 *
 * This is the one place a clear on one board becomes rows on another, and it
 * is in room.c because that is the only module holding both halves of a Room:
 * how many rows a clear is worth belongs to libtetrisbrain
 * (garbage_lines_from_clear, N-1) and who owes them to whom belongs to the
 * room's seating, and neither knows the other.
 *
 * It runs after every game has been advanced and before any snapshot is taken,
 * so the frame that shows a clear is the same frame that shows the pending
 * count it caused. The rows themselves land later still - at the Target's next
 * lock - which is what makes the count a warning rather than a surprise.
 *
 * Single takes the whole function out: a player with no Target is not owed a
 * queue, and game_take_cleared is still called so nothing accumulates against
 * a mode change that never comes.
 *
 * @param server_room Room whose clears are being charged.
 */
static void	settle_garbage(t_server_room *server_room)
{
	int	slot;
	int	target;
	int	cleared;
	int	fry;

	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		cleared = game_take_cleared(&server_room->games[slot]);
		fry = game_take_fry(&server_room->games[slot]);
		target = server_room_target_of(server_room, slot);
		if (target < 0)
		{
			slot++;
			continue ;
		}
		if (cleared > 0)
			game_queue_garbage(&server_room->games[target],
				garbage_lines_from_clear(cleared));
		/*
		 * Fry's rows go on whole rather than through
		 * garbage_lines_from_clear's N-1: they are not a clear being
		 * converted, they are three rows the sender put on their own floor
		 * and burned in order to hand over. They ride the ability lane, so
		 * Pals does not absorb them - the text excludes garbage an ability
		 * made.
		 */
		if (fry > 0)
			game_queue_ability_garbage(&server_room->games[target], fry);
		if (cleared > 0 || fry > 0)
		{
			server_room->dirty[target] = true;
			server_room->dirty[slot] = true;
		}
		slot++;
	}
}

/**
 * @brief Names the player a slot's clears and abilities are aimed at.
 *
 * Double's answer is the whole of it today: the other occupied slot, if
 * somebody is still playing in it. Single answers -1, which is the same answer
 * as an opponent who has already topped out, and both mean "nothing crosses" -
 * so no caller needs to know which of the two it got.
 *
 * Battle Royale is the reason this is a function rather than an expression.
 * Its four targeting modes (docs/CONTEXT.md) all reduce to "which slot", and
 * this is where they will land; nothing above it will have to change.
 *
 * @param server_room Room to resolve within.
 * @param from_slot The 0-based slot acting.
 * @return The 0-based Target slot, or -1 when there is none.
 */
int	server_room_target_of(t_server_room *server_room, int from_slot)
{
	int	slot;

	if (server_room == NULL || server_room->room == NULL
		|| server_room_is_solo(server_room))
		return (-1);
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (slot != from_slot && server_room->games[slot].player_id != 0
			&& server_room->games[slot].active)
			return (slot);
		slot++;
	}
	return (-1);
}

/**
 * @brief The game one client's abilities land on.
 *
 * The seat-to-game step kept out of the handlers, which know a connection and
 * a room and have no business knowing that a Room holds an array.
 *
 * @param server_room Room to resolve within.
 * @param cli The connection acting.
 * @return The Target's game, or NULL when there is no Target.
 */
t_game	*server_room_target_game(t_server_room *server_room,
		const t_client *cli)
{
	int	from;
	int	target;

	if (server_room == NULL || cli == NULL)
		return (NULL);
	from = slot_of_player(server_room, cli->player_id);
	if (from < 0)
		return (NULL);
	target = server_room_target_of(server_room, from);
	if (target < 0)
		return (NULL);
	return (&server_room->games[target]);
}

/**
 * @brief Writes onto a snapshot the facts that belong to the room, not to the
 *        game it came from.
 *
 * game_snapshot projects one board and deliberately knows nothing about the
 * room around it. Two things a player has to be told are the room's alone: the
 * countdown, which is every game in the room being held still at once, and the
 * result, which is a fact about who else is left rather than about this board.
 *
 * The countdown overrides the phase because during it the game is genuinely
 * active and unpaused - it is simply not being advanced - so the game has no
 * way to describe itself as held.
 *
 * The verdict is looked up by the player whose board this is and not by the
 * seat holding it. The two can disagree - a promoted successor's board follows
 * them into the seat they were moved to - and of the two it is the player the
 * result was ever about.
 *
 * @param server_room Room the snapshot came from.
 * @param slot 0-based slot the snapshot belongs to.
 * @param snap Snapshot to decorate.
 */
static void	decorate_snapshot(t_server_room *server_room, int slot,
		t_body_state *snap)
{
	const t_participant	*participant;

	snap->countdown_ms = server_room->countdown_ms;
	if (server_room->countdown_ms > 0)
		snap->phase = BODY_PHASE_COUNTDOWN;
	participant = participant_of(server_room,
			server_room->games[slot].player_id);
	if (participant != NULL)
	{
		snap->result = participant->result;
		snap->rank = participant->rank;
	}
	fill_opponents(server_room, slot, snap);
}

/**
 * @brief Puts every other seat's board into one player's snapshot.
 *
 * The opponent rides inside the recipient's own snapshot because a client
 * holds one STATE mailbox slot and a second push would free the first before
 * it was written. It also makes the two boards the same instant by
 * construction, which two messages could not promise: a client would
 * otherwise be free to draw its own board from this tick beside its
 * opponent's from three ticks ago.
 *
 * A seat with no game in it is skipped rather than sent empty. An opponent who
 * has topped out is not skipped - they are still in the match until it ends,
 * and their final board is what the winner is looking at.
 *
 * @param server_room Room being projected.
 * @param subject The 0-based slot the snapshot belongs to.
 * @param snap Snapshot receiving the opponents.
 */
static void	fill_opponents(t_server_room *server_room, int subject,
		t_body_state *snap)
{
	int	slot;

	snap->opponent_count = 0;
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (slot != subject && server_room->games[slot].player_id != 0
			&& snap->opponent_count < BODY_OPPONENTS_MAX)
		{
			project_opponent(server_room, slot,
				&snap->opponents[snap->opponent_count]);
			snap->opponent_count++;
		}
		slot++;
	}
}

/**
 * @brief Projects one seat's game onto the compact opponent view.
 *
 * Only what the other player is entitled to see: the board, the piece on it,
 * how they are doing and what is queued against them. Deliberately absent are
 * the next queue and the hold slot - knowing which piece an opponent is about
 * to be dealt is not watching their board, it is reading their hand. The
 * charge meter and the fighter they chose are on the other side of that line
 * and are sent.
 *
 * The name is taken from the seat rather than from the game, because a game
 * knows a player id and nothing else about who is playing it.
 *
 * @param server_room Room holding the seat.
 * @param slot The 0-based slot to project.
 * @param out Receives the projection.
 */
static void	project_opponent(t_server_room *server_room, int slot,
		t_body_opponent *out)
{
	const t_game	*game;
	int				row;
	int				col;

	game = &server_room->games[slot];
	memset(out, 0, sizeof(*out));
	out->slot = server_room->room->slots[slot].index;
	out->player_id = game->player_id;
	out->alive = game->active;
	out->phase = game_phase(game);
	out->score = game->score.total;
	out->lines = game->lines;
	/*
	 * Both queues, exactly as game_snapshot reports them to the player who
	 * owes them. Counting only the ordinary kind left every row an ability
	 * sent - Fry's three, Pentaris's - off the opponent's panel, so the one
	 * garbage a player cannot see coming was also the one they were not told
	 * about.
	 */
	out->pending = game->pending_garbage + game->pending_ability_garbage;
	out->piece.type = (int)game->piece.type;
	out->piece.rotation = game->piece.rotation;
	out->piece.col = game->piece.col;
	out->piece.row = game->piece.row;
	/*
	 * The charge is the exception to what is withheld, and the character with
	 * it. Both are what the opponent's side of the screen is drawn from - a
	 * meter filling opposite you is the warning that an ability is coming, and
	 * a match where you cannot see who you are fighting reads as a board with
	 * nobody behind it. Neither says what they will do with it, which is the
	 * line the next queue and the hold slot are still on the wrong side of.
	 */
	out->charge = game->charge.charges;
	if (out->charge > BODY_CHARGE_MAX)
		out->charge = BODY_CHARGE_MAX;
	out->character = (uint32_t)participant_character(server_room,
			game->player_id);
	snprintf(out->username, sizeof(out->username), "%s",
		server_room->room->slots[slot].membership.username);
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			out->cells[row][col].type
				= (uint8_t)board_get(&game->board, col, row).type;
			out->cells[row][col].color
				= (uint8_t)(board_get(&game->board, col, row).color & 0x0F);
			col++;
		}
		row++;
	}
}

/**
 * @brief Names the phase one game is in, for an onlooker.
 *
 * The same mapping game_snapshot makes for the game's own player, minus the
 * countdown - that one belongs to the room and is written onto the snapshot
 * as a whole, not per opponent.
 *
 * @param game Game to describe.
 * @return The phase to put on the wire.
 */
static t_body_phase	game_phase(const t_game *game)
{
	if (game->topped_out)
		return (BODY_PHASE_TOP_OUT);
	if (game->paused)
		return (BODY_PHASE_PAUSED);
	if (game->clearing_count > 0)
		return (BODY_PHASE_CLEARING);
	return (BODY_PHASE_ACTIVE);
}

/**
 * @brief Numbers one outgoing snapshot on its connection's STATE stream.
 *
 * The staleness check this number feeds is per connection: a client drops
 * any snapshot numbered below the last it saw on this stream. The game that
 * produced the frame does not outlive a finished match, though - the room is
 * destroyed with it and the next game sits in a fresh one - so numbering
 * from the game reset the stream to zero on every new game, and a client
 * that had never sent LEAVE in between dropped the whole of its next match
 * as replayed frames. The counter therefore lives on the connection, beside
 * the check that reads it.
 *
 * @param server_room Room the snapshot came from.
 * @param pid Player the snapshot is addressed to.
 * @param snap Snapshot to number; kept as the game numbered it when the
 *        player's connection is already gone and the frame will not send.
 */
static void	number_snapshot(t_server_room *server_room, t_player_id pid,
		t_body_state *snap)
{
	t_client	*cli;

	cli = registry_find_other(&server_room->srv->reg, pid, NULL);
	if (cli == NULL)
		return ;
	cli->state_seq++;
	snap->seq = cli->state_seq;
}

/**
 * @brief Reports whether the room has nothing left to tick.
 *
 * @param server_room Room to check.
 * @return true when every game has ended or every player has left.
 */
static bool	room_is_over(t_server_room *server_room)
{
	if (server_room->room->number_of_players == 0)
		return (true);
	if (server_room_is_solo(server_room))
		return (count_live_games(server_room) == 0);
	return (count_live_games(server_room) < 2
		|| server_room->room->number_of_players < 2);
}

/**
 * @brief Counts the games in this room that are still being played.
 *
 * @param server_room Room to count.
 * @return How many of its games are active.
 */
static int	count_live_games(const t_server_room *server_room)
{
	int	live;
	int	slot;

	live = 0;
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		if (server_room->games[slot].active)
			live++;
		slot++;
	}
	return (live);
}

/**
 * @brief Decides how the match ended for each player, once, before the last
 *        snapshot goes out.
 *
 * Surviving is the whole of winning: a player is the winner because everybody
 * else stopped, not because of anything their own board did. That is also why
 * the verdict has to be written down here rather than read off the game when
 * the snapshot is built - the winner's board is active with a piece on it,
 * exactly like a board mid-match.
 *
 * Single has no verdict to give. Its game ends by topping out and the top-out
 * phase already says so, so the result stays NONE and its snapshot is
 * unchanged.
 *
 * A seat with no game in it is skipped by the lookup rather than by a test of
 * its own: it has no player id, and no record can be opened for player 0.
 *
 * @param server_room Room whose match has just ended.
 */
static void	settle_results(t_server_room *server_room)
{
	t_participant	*participant;
	int				slot;

	if (server_room_is_solo(server_room))
		return ;
	slot = 0;
	while (slot < server_room->room->slot_count && slot < TD_MAX_GAMES)
	{
		participant = participant_open(server_room,
				server_room->games[slot].player_id);
		if (participant != NULL)
		{
			if (server_room->games[slot].active)
			{
				participant->result = BODY_RESULT_WON;
				participant->rank = 1;
			}
			else
			{
				participant->result = BODY_RESULT_LOST;
				participant->rank = count_live_games(server_room) + 1;
			}
			server_room->dirty[slot] = true;
		}
		slot++;
	}
}

/**
 * @brief Records every finished game and returns the room to its players.
 *
 * The room outlives the match. It used to be finished instead, which cleared
 * every slot and handed the room back to the lobby the moment the last board
 * stopped - so two players who had just played each other came back from the
 * results screen to a room that had been destroyed underneath them, and were
 * dropped in the lobby to find one another again. room_rematch keeps the seats
 * and the owner and withdraws only readiness, so the same room is waiting when
 * they get back and either of them can ask for another match.
 *
 * The room still dies when the last player leaves it - that rule is
 * server_room_forfeit's, and it is the one that keeps the lobby from filling
 * with the ghosts of finished games.
 *
 * A game is a win if it was still being played when the match ended, which is
 * the same question settle_results asked a moment earlier and the same answer.
 * It is deliberately not "did not top out": in Single those two agree, because
 * the only way a solo game ends is by topping out, but in a match the winner
 * is the player whose board never stopped and there is nothing on that board
 * to say so.
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
	/*
	 * Every match fact goes with the match it was about, and it goes here
	 * rather than in room_blank, which a room that survives its match never
	 * reaches. Left standing, the verdicts would open the next match with the
	 * last one's WON or LOST in its first snapshot - advance_and_push sent
	 * them a moment ago and this is the last thing the match does - and the
	 * declared fighters would open the next select window with every seat
	 * already locked, starting it again before anybody had looked at the
	 * roster.
	 */
	memset(server_room->participants, 0, sizeof(server_room->participants));
	server_room->select_ms = 0;
	server_room->select_second = -1;
	/*
	 * Single is the exception, and it is not an oversight: a solo room has
	 * nobody to play again, restarting a solo game is RESTART's job and never
	 * needed a room to survive for it, and the player who tops out is expected
	 * to come out of that room free to open another. Only a match keeps its
	 * room, because only a match has someone on the other side of it.
	 */
	if (server_room_is_solo(server_room))
		room_finish(server_room->room);
	else
		room_rematch(server_room->room);
	while (n > 0)
	{
		n--;
		award_game(server_room->srv, &played[n], played[n].active);
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
 * @brief Decides what a departure does to an open character-select window.
 *
 * The window used to close unconditionally, which is right for Double and
 * hands a Battle Royale one player a cancel button: thirty people join, the
 * owner starts, and one disconnect two seconds into the roster ends it for
 * everybody. The rule is the one the room already uses everywhere else - is it
 * still startable?
 *
 * Below min_to_start the window closes, because the room can no longer deal
 * the match it was setting up and whoever is left would sit on a roster screen
 * waiting out a clock with nothing behind it. Losing one of two players is
 * exactly that case, so Double's behaviour falls out of this rather than being
 * a special case beside it.
 *
 * Above it the window keeps running on its own clock - and may now be over,
 * which is why this asks. The player who left took their declared fighter with
 * them (server_room_forfeit closed their record), so a room waiting on that one
 * seat is waiting for nobody, and without this check it would sit out the full
 * TETRISD_MATCH_SELECT_MS before dealing a match everybody had already chosen
 * for.
 *
 * Nothing here is the owner's: a window is the room's once it is open, so a
 * departure that also changes the owner changes nothing about this.
 *
 * @param server_room Room whose window is being reconsidered.
 */
static void	settle_selection(t_server_room *server_room)
{
	if (server_room->room->number_of_players
		< server_room->room->min_to_start)
	{
		room_abort_selection(server_room->room);
		server_room->select_ms = 0;
		server_room->select_second = -1;
		server_room->ticking = false;
		return ;
	}
	if (server_room_all_locked(server_room))
		(void)server_room_autostart(server_room);
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
