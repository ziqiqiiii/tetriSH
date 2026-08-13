/* ************************************************************************** */
/*                                                                            */
/*   test_narrate.c - broadcasting one line of a room's feed                  */
/*                                                                            */
/*   Both cases here are about what a broadcast does when it cannot finish.   */
/*   The feed is best-effort by specification, so losing a line is allowed -  */
/*   what is not allowed is spending a sequence number on a line nobody got,  */
/*   or letting one seat's failure decide what the seats after it receive.    */
/*                                                                            */
/*   Driven in-process rather than through the harness: neither failure has   */
/*   a route through a socket. A sender the body cannot carry is refused at   */
/*   signup now (db_username_valid), and an allocation failure has no route   */
/*   at all without the trap below.                                           */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisd.h"

#include <assert.h>

/*
** Allocation trap. malloc is interposed for the whole binary and passes
** straight through to glibc's until a test arms it, so nothing else in the
** suite is affected by its presence.
**
** Arming is by exact size and ordinal, because deliver's per-seat copy is
** malloc(len) and htttp_serialize's wire buffer is malloc(len) too - the same
** size, and the one allocated first. Counting them apart is what lets the
** test fail exactly the middle seat's copy and nothing else.
**
** ASan replaces malloc itself, so the trap is compiled out under it and the
** case that needs it skips rather than fights the interceptor.
*/
# ifndef __SANITIZE_ADDRESS__
#  define ALLOC_TRAP_AVAILABLE 1
extern void	*__libc_malloc(size_t size);

static size_t	g_trap_size = 0;
static int		g_trap_ordinal = -1;
static int		g_trap_seen = 0;
static size_t	g_largest = 0;

void	*malloc(size_t size)
{
	if (size > g_largest)
		g_largest = size;
	if (g_trap_ordinal >= 0 && size == g_trap_size
		&& ++g_trap_seen == g_trap_ordinal)
		return (NULL);
	return (__libc_malloc(size));
}
# else
#  define ALLOC_TRAP_AVAILABLE 0
static size_t	g_largest = 0;
# endif

// Static Functions
static void	test_a_line_that_cannot_be_built_does_not_spend_a_seq(void);
static void	test_one_seat_that_cannot_be_copied_does_not_stop_the_rest(void);

static void	room_of(t_server *srv, t_server_room *server_room, t_room *room,
				int seats);
static void	seat(t_server *srv, t_client *cli, t_room *room, int index);
static void	say(t_server_room *server_room, const char *sender,
				const char *text, bool *sent);
static void	arm_trap(size_t size, int ordinal);
static void	disarm_trap(void);

int	main(void)
{
	test_a_line_that_cannot_be_built_does_not_spend_a_seq();
	test_one_seat_that_cannot_be_copied_does_not_stop_the_rest();
	return (0);
}

/*
** A 200 carrying a seq is a promise that the line went out under that number.
** The counter was being advanced before the body was built, so a line the
** codec refused took its number with it: the sender was told the seq of a
** message nobody received, and the feed kept a permanent hole where it had
** been. The next line must get the number the failed one did not.
**
** A sender with a space in it is the refusal used here because the body puts
** the sender on its own line. The room has no seats, so nothing is delivered
** and the assertion is only about the counter.
*/
static void	test_a_line_that_cannot_be_built_does_not_spend_a_seq(void)
{
	t_server		*srv;
	t_server_room	server_room;
	t_room			room;
	bool			sent;

	srv = calloc(1, sizeof(*srv));
	assert(srv != NULL);
	room_of(srv, &server_room, &room, 0);
	say(&server_room, "amber lee", "can anyone hear me", &sent);
	assert(!sent);
	assert(server_room.chat_seq == 0);
	// Narration is the same call, so text that will not fit spends nothing.
	assert(server_room.chat_seq == 0);
	say(&server_room, "amber", "now with a sayable name", &sent);
	assert(sent);
	assert(server_room.chat_seq == 1);
	say(&server_room, "blake", "and the next line follows it", &sent);
	assert(sent);
	assert(server_room.chat_seq == 2);
	free(srv);
	printf("PASS test_a_line_that_cannot_be_built_does_not_spend_a_seq\n");
}

/*
** Delivery is per-seat, and a seat it cannot serve is skipped rather than
** returned on. Giving up mid-room would hand the feed to whoever happened to
** be seated before the failure and silently cut off everyone after them -
** in a Battle Royale room, most of it.
**
** The trap fails the copy for seat 1 only. Seat 0 is what proves the
** broadcast was running, and seat 2 is the assertion: before the fix it
** received nothing, because one failed malloc ended the loop.
*/
static void	test_one_seat_that_cannot_be_copied_does_not_stop_the_rest(void)
{
	t_server		*srv;
	t_server_room	server_room;
	t_room			room;
	t_client		cli[3];
	bool			sent;

	if (!ALLOC_TRAP_AVAILABLE)
		return ((void)printf("SKIP test_one_seat_that_cannot_be_copied"
				"_does_not_stop_the_rest (needs the malloc trap)\n"));
	srv = calloc(1, sizeof(*srv));
	assert(srv != NULL);
	assert(registry_init(&srv->reg, 8) == 0);
	room_of(srv, &server_room, &room, 3);
	seat(srv, &cli[0], &room, 0);
	seat(srv, &cli[1], &room, 1);
	seat(srv, &cli[2], &room, 2);
	/*
	 * One unarmed broadcast first, to learn the size the trap must watch for:
	 * the largest allocation a broadcast makes is the serialised message, and
	 * every per-seat copy is that same size.
	 */
	g_largest = 0;
	say(&server_room, "amber", "calibration line, long enough to be the "
		"largest allocation this broadcast makes by a clear margin", &sent);
	assert(sent);
	assert(cli[0].outbox.chat_count == 1 && cli[1].outbox.chat_count == 1
		&& cli[2].outbox.chat_count == 1);
	arm_trap(g_largest, 3);
	say(&server_room, "amber", "calibration line, long enough to be the "
		"largest allocation this broadcast makes by a clear margin", &sent);
	disarm_trap();
	assert(sent);
	// Seat 1's copy was refused; the seats either side of it still got theirs.
	assert(cli[0].outbox.chat_count == 2);
	assert(cli[1].outbox.chat_count == 1);
	assert(cli[2].outbox.chat_count == 2);
	outbox_destroy(&cli[0].outbox);
	outbox_destroy(&cli[1].outbox);
	outbox_destroy(&cli[2].outbox);
	free(srv->reg.slots);
	free(srv);
	printf("PASS test_one_seat_that_cannot_be_copied_does_not_stop_the_rest\n");
}

/**
 * @brief Builds a named room with a given number of occupied seats.
 *
 * @param srv Server the room belongs to.
 * @param server_room Runtime half to fill in.
 * @param room Domain half to fill in.
 * @param seats How many slots to mark occupied.
 */
static void	room_of(t_server *srv, t_server_room *server_room, t_room *room,
		int seats)
{
	int	index;

	memset(server_room, 0, sizeof(*server_room));
	memset(room, 0, sizeof(*room));
	snprintf(room->name, sizeof(room->name), "D-01");
	room->slot_count = seats;
	room->number_of_players = seats;
	index = 0;
	while (index < seats)
	{
		room->slots[index].index = index + 1;
		room->slots[index].occupied = true;
		room->slots[index].membership.player_id = (t_player_id)(index + 1);
		snprintf(room->slots[index].membership.username,
			sizeof(room->slots[index].membership.username), "player%d", index);
		index++;
	}
	server_room->room = room;
	server_room->srv = srv;
}

/**
 * @brief Registers one authenticated client for the player in a given slot.
 *
 * @param srv Server whose registry receives it.
 * @param cli Client to prepare.
 * @param room Room the player is seated in.
 * @param index Slot the client answers to.
 */
static void	seat(t_server *srv, t_client *cli, t_room *room, int index)
{
	memset(cli, 0, sizeof(*cli));
	cli->fd = -1;
	cli->srv = srv;
	cli->state = CLI_AUTHED;
	cli->player_id = room->slots[index].membership.player_id;
	snprintf(cli->username, sizeof(cli->username), "%s",
		room->slots[index].membership.username);
	assert(outbox_init(&cli->outbox) == 0);
	assert(registry_add(&srv->reg, cli) == 0);
}

/**
 * @brief Broadcasts one player line and reports whether it was built.
 *
 * @param server_room Room to broadcast into.
 * @param sender Name to attribute the line to.
 * @param text The message.
 * @param sent Receives what room_chat_broadcast answered.
 */
static void	say(t_server_room *server_room, const char *sender,
		const char *text, bool *sent)
{
	t_body_chat	chat;

	memset(&chat, 0, sizeof(chat));
	snprintf(chat.sender, sizeof(chat.sender), "%s", sender);
	snprintf(chat.text, sizeof(chat.text), "%s", text);
	*sent = room_chat_broadcast(server_room, &chat);
}

/**
 * @brief Arms the allocation trap for one allocation of a given size.
 *
 * @param size Exact allocation size to watch for.
 * @param ordinal Which allocation of that size to refuse, counting from one.
 */
static void	arm_trap(size_t size, int ordinal)
{
# if ALLOC_TRAP_AVAILABLE
	g_trap_size = size;
	g_trap_ordinal = ordinal;
	g_trap_seen = 0;
# else
	(void)size;
	(void)ordinal;
# endif
}

/**
 * @brief Disarms the allocation trap so later allocations pass through.
 */
static void	disarm_trap(void)
{
# if ALLOC_TRAP_AVAILABLE
	g_trap_ordinal = -1;
	g_trap_seen = 0;
# endif
}
