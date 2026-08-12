/* ************************************************************************** */
/*                                                                            */
/*   test_match_authority_lost.c - a dropped match does not play on           */
/*                                                                            */
/*   Solo falls back to the local rules when its session goes: the game it    */
/*   was playing is still a game without a server. A match is not - the       */
/*   opponent was the point of it - so the authority says so and stops.       */
/*   Clearing `online` alone was not stopping: every entry point simply took  */
/*   its offline branch, gravity resumed under the local rules, the keys      */
/*   started driving the board again, and the fixture rival this screen had   */
/*   blanked on the way in stood up and played on against a player who had    */
/*   just been told the match was over.                                       */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

#include <assert.h>

// Static Functions
static void	test_a_lost_match_neither_falls_nor_answers_a_key(void);
static void	load_fixture(t_app_profile_view_model *profile,
				t_app_catalogue_view_model *characters);

int	main(void)
{
	test_a_lost_match_neither_falls_nor_answers_a_key();
	return (0);
}

static void	test_a_lost_match_neither_falls_nor_answers_a_key(void)
{
	t_app_profile_view_model	profile;
	t_app_catalogue_view_model	characters;
	t_match_authority			authority;
	t_mp_match_state			state;
	t_board						local_before;
	t_board						rival_before;

	load_fixture(&profile, &characters);
	mp_match_state_init(&state, APP_GAME_MODE_DOUBLE, "duel-42", &profile,
		&characters, 42u);
	state.phase = MP_MATCH_PLAYING;
	state.local_game.countdown_active = false;
	state.opponent_game.countdown_active = false;
	match_authority_open(&authority, NULL, &state);
	/* offline is a rehearsal and still plays: this is the control */
	assert(match_authority_update(&authority, &state, 5000));
	/* the session drops mid-match */
	authority.online = false;
	authority.lost = true;
	board_copy(&local_before, &state.local_game.board);
	board_copy(&rival_before, &state.opponent_game.board);
	assert(!match_authority_update(&authority, &state, 60000));
	assert(!match_authority_action(&authority, &state, SOLO_HARD_DROP));
	assert(!match_authority_ability(&authority, &state, SOLO_ABILITY_MIRURUN));
	assert(memcmp(&local_before, &state.local_game.board,
			sizeof(local_before)) == 0);
	assert(memcmp(&rival_before, &state.opponent_game.board,
			sizeof(rival_before)) == 0);
	assert(state.phase == MP_MATCH_PLAYING);
	printf("PASS test_a_lost_match_neither_falls_nor_answers_a_key\n");
}

/**
 * @brief Loads the fixture profile and character roster the screen is built on.
 *
 * @param profile Receives the fixture profile.
 * @param characters Receives the fixture character catalogue.
 */
static void	load_fixture(t_app_profile_view_model *profile,
		t_app_catalogue_view_model *characters)
{
	t_app_data_provider	provider;

	app_fixture_provider_init(&provider);
	memset(profile, 0, sizeof(*profile));
	memset(characters, 0, sizeof(*characters));
	assert(provider.load_profile(provider.userdata, profile)
		== APP_PROVIDER_OK);
	assert(provider.load_catalogue(provider.userdata,
			APP_CATALOGUE_CHARACTERS, characters) == APP_PROVIDER_OK);
}
