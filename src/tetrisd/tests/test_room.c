/* ************************************************************************** */
/*                                                                            */
/*   test_room.c - one connection, two games, one unbroken STATE stream       */
/*                                                                            */
/*   Driven in-process so the match boundary is deterministic; the assertion */
/*   still decodes the serialized STATE body the client receives.             */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisd.h"

#include <assert.h>

// Static Functions
static void		test_a_second_game_keeps_the_stream_numbering(void);

static void		seat_one(t_server *srv, t_client *cli);
static uint64_t	pushed_seq(t_client *cli);

int	main(void)
{
	test_a_second_game_keeps_the_stream_numbering();
	return (0);
}

/*
** The client drops any STATE numbered below the last it saw on its
** connection. A finished match destroys the room and its games with it, so a
** stream numbered by the game went back to zero on the next deal - and the
** next room the lobby hands out may even carry the same name. A player who
** never sent LEAVE in between then had every frame of their second match
** thrown away as replays, and the board sat frozen for the whole of it.
**
** The number on the wire must therefore come from the connection, not the
** game: the second match's first frame is numbered after the first match's
** last one, whatever rooms either of them lived in.
*/
static void	test_a_second_game_keeps_the_stream_numbering(void)
{
	t_server		*srv;
	t_client		cli;
	t_server_room	*server_room;
	uint64_t		first_seq;
	uint64_t		second_seq;

	srv = calloc(1, sizeof(*srv));
	assert(srv != NULL);
	assert(registry_init(&srv->reg, 8) == 0);
	assert(server_rooms_init(srv, 4) == 0);
	seat_one(srv, &cli);
	assert(server_room_open(srv, &cli, MODE_SINGLE) == 1);
	server_room = server_room_at(srv, cli.binding.room_index);
	assert(server_room != NULL);
	assert(server_room_start(server_room, &cli) == START_ACCEPTED);
	server_rooms_tick(srv, 1000);
	first_seq = pushed_seq(&cli);
	assert(first_seq > 0);
	/* a top-out ends the match, and the domain ends the room with it */
	server_room->games[0].active = false;
	server_room->games[0].topped_out = true;
	server_rooms_tick(srv, 0);
	server_room_unbind(&cli);
	assert(server_room_open(srv, &cli, MODE_SINGLE) == 1);
	server_room = server_room_at(srv, cli.binding.room_index);
	assert(server_room != NULL);
	assert(server_room_start(server_room, &cli) == START_ACCEPTED);
	server_rooms_tick(srv, 1000);
	second_seq = pushed_seq(&cli);
	assert(second_seq > first_seq);
	outbox_destroy(&cli.outbox);
	registry_remove(&srv->reg, &cli);
	registry_destroy(&srv->reg);
	free(srv);
	printf("PASS test_a_second_game_keeps_the_stream_numbering\n");
}

/**
 * @brief Registers one authenticated client the room can seat.
 *
 * @param srv Server whose registry receives it.
 * @param cli Client to prepare.
 */
static void	seat_one(t_server *srv, t_client *cli)
{
	memset(cli, 0, sizeof(*cli));
	cli->fd = -1;
	cli->srv = srv;
	cli->state = CLI_AUTHED;
	cli->player_id = 7;
	snprintf(cli->username, sizeof(cli->username), "rematcher");
	assert(outbox_init(&cli->outbox) == 0);
	assert(registry_add(&srv->reg, cli) == 0);
}

/**
 * @brief Decodes the STATE waiting in the client's mailbox and reads its seq.
 *
 * The assertion belongs on the wire form, not on the counter behind it: what
 * the client would filter on is the number in the frame, however room.c chose
 * to put it there.
 *
 * @param cli Client whose outbox holds the latest STATE.
 * @return The sequence number the frame carries.
 */
static uint64_t	pushed_seq(t_client *cli)
{
	t_htttp_message	parsed;
	t_body_state	snap;
	uint64_t		seq;

	assert(cli->outbox.state_pending);
	htttp_message_init(&parsed);
	assert(htttp_parse(cli->outbox.state.bytes, cli->outbox.state.len,
			&parsed) == HTTTP_OK);
	assert(parsed.method != NULL && strcmp(parsed.method, "STATE") == 0);
	assert(body_state_decode((const char *)parsed.body, parsed.body_len,
			&snap) == 0);
	seq = snap.seq;
	htttp_message_free(&parsed);
	return (seq);
}
