/* ************************************************************************** */
/*                                                                            */
/*   test_net_match_opponents.c - a frame unsays the seats it omits           */
/*                                                                            */
/*   A snapshot describes the room as it is now, and the opponents in it are  */
/*   whoever the server still has a board for. When one disconnects mid-match */
/*   tetrisd forfeits them and their game goes, so the very next frame        */
/*   carries one fewer opponent - and a client that wrote only the seats a    */
/*   frame named left the departed player standing there alive until the      */
/*   verdict arrived behind them.                                             */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

#include <assert.h>

// Static Functions
static void	test_an_opponent_that_leaves_the_frame_leaves_the_screen(void);
static void	fill_snapshot(t_body_state *snap, size_t opponents);

int	main(void)
{
	test_an_opponent_that_leaves_the_frame_leaves_the_screen();
	return (0);
}

static void	test_an_opponent_that_leaves_the_frame_leaves_the_screen(void)
{
	t_mp_match_state	state;
	t_net_client		net;

	memset(&net, 0, sizeof(net));
	memset(&state, 0, sizeof(state));
	net.has_state = true;
	fill_snapshot(&net.state_snapshot, 1);
	assert(net_match_apply(&net, &state));
	assert(state.opponents[0].present);
	assert(state.opponents[0].alive);
	assert(strcmp(state.opponent_name, "blake") == 0);
	assert(state.opponent_charge == 4);
	assert(state.players_alive == 2);
	/* the rival disconnects: the server forfeits them and the seat empties */
	fill_snapshot(&net.state_snapshot, 0);
	assert(net_match_apply(&net, &state));
	assert(!state.opponents[0].present);
	assert(!state.opponents[0].alive);
	assert(state.opponent_name[0] == '\0');
	assert(state.opponent_charge == 0);
	assert(state.opponent_character == 0);
	assert(state.players_alive == 1);
	printf("PASS test_an_opponent_that_leaves_the_frame_leaves_the_screen\n");
}

/**
 * @brief Builds one snapshot of a live board with n opponents beside it.
 *
 * @param snap Snapshot to overwrite.
 * @param opponents How many opponents the frame carries.
 */
static void	fill_snapshot(t_body_state *snap, size_t opponents)
{
	memset(snap, 0, sizeof(*snap));
	snap->seq = 1;
	snap->phase = BODY_PHASE_ACTIVE;
	snap->result = BODY_RESULT_NONE;
	snap->opponent_count = opponents;
	if (opponents == 0)
		return ;
	snap->opponents[0].slot = 2;
	snap->opponents[0].player_id = 42;
	snap->opponents[0].alive = true;
	snap->opponents[0].phase = BODY_PHASE_ACTIVE;
	snap->opponents[0].charge = 4;
	snap->opponents[0].character = 7;
	snprintf(snap->opponents[0].username,
		sizeof(snap->opponents[0].username), "%s", "blake");
}
