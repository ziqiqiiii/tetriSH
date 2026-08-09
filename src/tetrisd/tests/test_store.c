/* ************************************************************************** */
/*                                                                            */
/*   test_store.c - the Marketplace half of an account                        */
/*                                                                            */
/*   libmacminidb has been able to sell and equip since it was written, but    */
/*   nothing could ask it to: tetrisu's Marketplace debited a wallet it kept   */
/*   in its own view model and forgot the moment the screen closed. These are  */
/*   the routes that close it - LIST /store, PROFILE, BUY, EQUIP - and what    */
/*   has to be true of them is that the server decides. The price comes from   */
/*   the catalogue, the wallet from the store, and ownership is the one thing  */
/*   a client can never assert, because the equipped character is what         */
/*   decides which abilities that player has.                                  */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_the_catalogue_is_what_the_config_holds(void);
static void	test_the_store_front_needs_a_session(void);
static void	test_a_fresh_account_owns_only_its_starters(void);
static void	test_another_players_profile_is_refused(void);
static void	test_a_free_theme_is_bought_with_an_empty_wallet(void);
static void	test_an_unaffordable_purchase_costs_nothing(void);
static void	test_a_credited_wallet_is_debited_by_the_price(void);
static void	test_an_unknown_item_is_not_found(void);
static void	test_equipping_what_you_do_not_own_is_refused(void);
static void	test_equipping_what_you_own_sticks(void);
static void	test_equipping_for_another_player_is_refused(void);
static void	test_playing_a_game_is_what_fills_the_wallet(void);

static int		player(t_fixture *fx, t_harness *hc, const char *name);
static int		play_until_top_out(t_harness *hc, uint64_t *score_out);
static void		nap(int ms);
static void		read_profile(t_harness *hc, t_body_profile *out);
static int		store_request(t_harness *hc, const char *method,
					const char *path, t_body_profile *out);
static bool		owns(const uint32_t *ids, size_t count, uint32_t id);
static bool		body_says(const t_htttp_message *resp, const char *text);
static void		credit(t_fixture *fx, t_player_id id, int64_t points);
static void		item_path(char *out, size_t cap, const char *kind, unsigned id);
static void		equip_path(char *out, size_t cap, t_player_id pid,
					const char *kind, unsigned id);

int	main(void)
{
	test_the_catalogue_is_what_the_config_holds();
	test_the_store_front_needs_a_session();
	test_a_fresh_account_owns_only_its_starters();
	test_another_players_profile_is_refused();
	test_a_free_theme_is_bought_with_an_empty_wallet();
	test_an_unaffordable_purchase_costs_nothing();
	test_a_credited_wallet_is_debited_by_the_price();
	test_an_unknown_item_is_not_found();
	test_equipping_what_you_do_not_own_is_refused();
	test_equipping_what_you_own_sticks();
	test_equipping_for_another_player_is_refused();
	test_playing_a_game_is_what_fills_the_wallet();
	return (0);
}

/*
** The store front is the config files, not a copy of them kept on the client.
** The two things a client cannot reconstruct on its own are here: the price,
** and the fact that theme ids skip 5 - the cut John Cena theme - so a client
** that treated the id as a position in the list would mislabel every theme
** after it.
*/
static void	test_the_catalogue_is_what_the_config_holds(void)
{
	t_body_catalogue	store;
	t_htttp_message		resp;
	t_fixture			fx;
	t_harness			hc;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_request(&hc, "LIST", TETRISD_ROUTE_STORE, NULL, &resp) == 0);
	assert(resp.status_code == 200);
	assert(body_catalogue_decode((const char *)resp.body, resp.body_len,
			&store) == 0);
	assert(store.character_count == 4);
	assert(store.characters[0].id == 1);
	assert(strcmp(store.characters[0].name, "Halloween") == 0);
	assert(store.characters[0].price == 10);
	assert(store.theme_count == 7);
	assert(store.themes[0].id == 1 && store.themes[0].price == 0);
	/* A name with spaces survives the wire; it is last on its line. */
	assert(strcmp(store.themes[2].name, "Do u wanna build a snowman?") == 0);
	assert(store.themes[2].price == 5);
	/* Ids are labels, not positions: the fifth theme listed is id 6. */
	assert(store.themes[4].id == 6);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_the_catalogue_is_what_the_config_holds\n");
}

/*
** Prices are not a secret, but a connection that has not said who it is has
** no business asking tetrisd for anything - the same rule every other read
** follows.
*/
static void	test_the_store_front_needs_a_session(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_request(&hc, "LIST", TETRISD_ROUTE_STORE, NULL, &resp) == 0);
	assert(resp.status_code == 401);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_the_store_front_needs_a_session\n");
}

/*
** Signup grants item 1 of each kind and equips both (UC-01). The profile is
** what tells the Marketplace which tiles are owned, so it has to say exactly
** that and nothing more - a client that saw a fuller inventory than the store
** holds would offer an equip the server then refuses.
*/
static void	test_a_fresh_account_owns_only_its_starters(void)
{
	t_body_profile	profile;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	read_profile(&hc, &profile);
	assert(strcmp(profile.username, "amber") == 0);
	assert(profile.wallet == 0);
	assert(profile.score == 0);
	assert(profile.rank == 1);
	assert(profile.equipped_character == 1 && profile.equipped_theme == 1);
	assert(profile.owned_character_count == 1);
	assert(owns(profile.owned_characters, profile.owned_character_count, 1));
	assert(profile.owned_theme_count == 1);
	assert(owns(profile.owned_themes, profile.owned_theme_count, 1));
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_fresh_account_owns_only_its_starters\n");
}

/*
** A profile carries a wallet and an inventory, so it is not a public read:
** the subject in the path has to be the player bound to this connection.
*/
static void	test_another_players_profile_is_refused(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	snprintf(path, sizeof(path), "%s%llu", TETRISD_ROUTE_PLAYER_PREFIX,
		(unsigned long long)hc.player_id + 1);
	assert(hc_request(&hc, "PROFILE", path, NULL, &resp) == 0);
	assert(resp.status_code == 403);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_another_players_profile_is_refused\n");
}

/*
** Theme 2 is free for SUTDents, so it is the one purchase a brand-new account
** can make - and it proves the answer carries the account as it stands
** afterwards rather than a bare 200 the client would have to re-read.
*/
static void	test_a_free_theme_is_bought_with_an_empty_wallet(void)
{
	t_body_profile	profile;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	item_path(path, sizeof(path), "theme", 2);
	assert(store_request(&hc, "BUY", path, &profile) == 200);
	assert(profile.wallet == 0);
	assert(profile.owned_theme_count == 2);
	assert(owns(profile.owned_themes, profile.owned_theme_count, 2));
	/* Buying is not equipping: the loadout is a separate decision. */
	assert(profile.equipped_theme == 1);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_free_theme_is_bought_with_an_empty_wallet\n");
}

/*
** The refusal names its reason, because "you cannot afford that" and "there
** is no such item" are different problems for a player to act on. And it
** costs nothing: db_buy_* checks and deducts under one lock, so a refused
** purchase cannot have taken the money on the way out.
*/
static void	test_an_unaffordable_purchase_costs_nothing(void)
{
	t_body_profile	profile;
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	item_path(path, sizeof(path), "character", 2);
	assert(hc_request(&hc, "BUY", path, NULL, &resp) == 0);
	assert(resp.status_code == 403);
	assert(body_says(&resp, "insufficient-funds"));
	htttp_message_free(&resp);
	read_profile(&hc, &profile);
	assert(profile.wallet == 0);
	assert(profile.owned_character_count == 1);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_an_unaffordable_purchase_costs_nothing\n");
}

/*
** The price is the catalogue's. A character costs a flat 10, so a wallet
** credited with 25 comes back holding 15 - the client never says what
** anything costs, and could not make it cheaper by claiming otherwise.
*/
static void	test_a_credited_wallet_is_debited_by_the_price(void)
{
	t_body_profile	profile;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	credit(&fx, hc.player_id, 25);
	item_path(path, sizeof(path), "character", 2);
	assert(store_request(&hc, "BUY", path, &profile) == 200);
	assert(profile.wallet == 15);
	assert(owns(profile.owned_characters, profile.owned_character_count, 2));
	/* Buying it twice is a no-op the profile already describes, not a
	** second debit. */
	assert(store_request(&hc, "BUY", path, &profile) == 200);
	assert(profile.wallet == 15);
	assert(profile.owned_character_count == 2);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_credited_wallet_is_debited_by_the_price\n");
}

/*
** Theme 5 is the gap left by the cut John Cena theme. It is the case that
** matters most here: an id nobody sells has to be 404 rather than an equip
** refusal, or a player is told to buy something that does not exist.
*/
static void	test_an_unknown_item_is_not_found(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	item_path(path, sizeof(path), "theme", 5);
	assert(hc_request(&hc, "BUY", path, NULL, &resp) == 0);
	assert(resp.status_code == 404);
	htttp_message_free(&resp);
	item_path(path, sizeof(path), "character", 99);
	assert(hc_request(&hc, "BUY", path, NULL, &resp) == 0);
	assert(resp.status_code == 404);
	htttp_message_free(&resp);
	equip_path(path, sizeof(path), hc.player_id, "theme", 5);
	assert(hc_request(&hc, "EQUIP", path, NULL, &resp) == 0);
	assert(resp.status_code == 404);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_an_unknown_item_is_not_found\n");
}

/*
** The rule the whole feature rests on. handlers_game.c reads the equipped
** character out of the store to decide what an ABILITY level means, so a
** client that could equip what it has not bought would be granting itself
** another character's powers.
*/
static void	test_equipping_what_you_do_not_own_is_refused(void)
{
	t_body_profile	profile;
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	equip_path(path, sizeof(path), hc.player_id, "character", 4);
	assert(hc_request(&hc, "EQUIP", path, NULL, &resp) == 0);
	assert(resp.status_code == 403);
	assert(body_says(&resp, "not-owned"));
	htttp_message_free(&resp);
	read_profile(&hc, &profile);
	assert(profile.equipped_character == 1);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_equipping_what_you_do_not_own_is_refused\n");
}

/*
** An equip is persisted, not remembered by the screen that made it: the
** whole point of moving this off the client is that signing back in finds
** the loadout you chose.
*/
static void	test_equipping_what_you_own_sticks(void)
{
	t_body_profile	profile;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	item_path(path, sizeof(path), "theme", 2);
	assert(store_request(&hc, "BUY", path, &profile) == 200);
	equip_path(path, sizeof(path), hc.player_id, "theme", 2);
	assert(store_request(&hc, "EQUIP", path, &profile) == 200);
	assert(profile.equipped_theme == 2);
	hc_close(&hc);
	/* A new connection for the same account reads the same loadout. */
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	read_profile(&hc, &profile);
	assert(profile.equipped_theme == 2);
	assert(profile.equipped_character == 1);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_equipping_what_you_own_sticks\n");
}

/*
** The subject rides in the path on this route the same way it does on the
** input routes, and is checked the same way.
*/
static void	test_equipping_for_another_player_is_refused(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	equip_path(path, sizeof(path), hc.player_id + 1, "theme", 1);
	assert(hc_request(&hc, "EQUIP", path, NULL, &resp) == 0);
	assert(resp.status_code == 403);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_equipping_for_another_player_is_refused\n");
}

/*
** Every other test here credits the wallet through the store directly, which
** is convenient and proves nothing about where the money comes from. This is
** the only way a player can earn it: play a game and top out.
**
** Two games, because the two numbers a game moves are two different rules and
** one game cannot tell either of them from its alternative. The score on the
** profile is the *best* game, so after the second it is the larger of the two
** and not their sum - that is the assertion that would have caught the board
** ranking whoever played most. The wallet is charged against the running
** total of both games, so it is worth their sum divided once, not each of
** them divided and then added - that is the assertion that would have caught
** a game worth less than the exchange rate rounding away to nothing.
*/
static void	test_playing_a_game_is_what_fills_the_wallet(void)
{
	t_body_profile	profile;
	t_fixture		fx;
	t_harness		hc;
	uint64_t		first;
	uint64_t		second;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	read_profile(&hc, &profile);
	assert(profile.wallet == 0 && profile.score == 0);
	assert(play_until_top_out(&hc, &first) == 0);
	nap(300);
	read_profile(&hc, &profile);
	assert(first > 0);
	assert(profile.score == first);
	assert(profile.wallet == first / 100);
	assert(play_until_top_out(&hc, &second) == 0);
	nap(300);
	read_profile(&hc, &profile);
	if (second > first)
		assert(profile.score == second);
	else
		assert(profile.score == first);
	assert(profile.wallet == (first + second) / 100);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_playing_a_game_is_what_fills_the_wallet\n");
}

/**
 * @brief Connects, registers and signs in one throwaway player.
 *
 * @param fx Running fixture.
 * @param hc Harness to bring up.
 * @param name Username to register.
 * @return 0 on success, -1 otherwise.
 */
static int	player(t_fixture *fx, t_harness *hc, const char *name)
{
	if (hc_connect(hc, fx) != 0)
		return (-1);
	if (hc_signup(hc, name, "hunter2") != 201)
		return (-1);
	if (hc_login(hc, name, "hunter2") != 200)
		return (-1);
	return (0);
}

/**
 * @brief Plays one Single room out by hard-dropping until the stack tops out.
 *
 * Hard drop is the only input needed: it always locks, so the board fills
 * without depending on gravity arriving on any particular schedule. The refusal
 * that ends the loop is the game telling this connection it is over.
 *
 * @param hc Signed-in harness.
 * @param score_out Receives the score the final snapshot reported.
 * @return 0 once the game has ended, -1 if it never did.
 */
static int	play_until_top_out(t_harness *hc, uint64_t *score_out)
{
	t_body_state	state;
	t_htttp_message	resp;
	char			room[ROOM_NAME_MAX];
	char			path[96];
	int				status;
	int				guard;

	if (hc_join_new(hc, "single", room, sizeof(room)) != 201)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(hc_request(hc, "START", path, NULL, &resp) == 0);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	if (status != 200)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s/player/%llu", room,
		(unsigned long long)hc->player_id);
	guard = 0;
	while (status == 200 && guard < 400)
	{
		assert(hc_request(hc, "DROP", path, "HARD", &resp) == 0);
		status = (int)resp.status_code;
		htttp_message_free(&resp);
		guard++;
	}
	if (status == 200)
		return (-1);
	guard = 0;
	while (guard < 20)
	{
		/* The snapshot saying "you topped out" is usually already in hand:
		** the harness files away every STATE it drains while sending a
		** request, and the last drop is what ended the game. */
		if (hc->has_state && hc->last_state.phase == BODY_PHASE_TOP_OUT)
		{
			*score_out = hc->last_state.score;
			return (0);
		}
		if (hc_wait_state(hc, &state, 300) != 0)
			return (-1);
		hc->last_state = state;
		hc->has_state = true;
		guard++;
	}
	return (-1);
}

/**
 * @brief Sleeps for a while, so a tick the reactor owes can land.
 *
 * @param ms Milliseconds to sleep.
 */
static void	nap(int ms)
{
	struct timespec	ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}

/**
 * @brief Reads this connection's profile and decodes it.
 *
 * @param hc Signed-in harness.
 * @param out Receives the decoded profile.
 */
static void	read_profile(t_harness *hc, t_body_profile *out)
{
	char	path[64];

	snprintf(path, sizeof(path), "%s%llu", TETRISD_ROUTE_PLAYER_PREFIX,
		(unsigned long long)hc->player_id);
	assert(store_request(hc, "PROFILE", path, out) == 200);
}

/**
 * @brief Sends one store request and decodes the profile it answered with.
 *
 * @param hc Signed-in harness.
 * @param method PROFILE, BUY or EQUIP.
 * @param path The route that method addresses.
 * @param out Receives the decoded profile when the status was 200.
 * @return The status answered with.
 */
static int	store_request(t_harness *hc, const char *method, const char *path,
		t_body_profile *out)
{
	t_htttp_message	resp;
	int				status;

	assert(hc_request(hc, method, path, NULL, &resp) == 0);
	status = (int)resp.status_code;
	if (status == 200)
		assert(body_profile_decode((const char *)resp.body, resp.body_len,
				out) == 0);
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Reports whether an owned-id list holds one id.
 *
 * @param ids The owned ids.
 * @param count How many ids the list holds.
 * @param id The id to look for.
 * @return true when the id is present.
 */
static bool	owns(const uint32_t *ids, size_t count, uint32_t id)
{
	size_t	i;

	i = 0;
	while (i < count)
	{
		if (ids[i] == id)
			return (true);
		i++;
	}
	return (false);
}

/**
 * @brief Reports whether a response body contains one word.
 *
 * @param resp The server's answer.
 * @param text The reason word to look for.
 * @return true when the body carries it.
 */
static bool	body_says(const t_htttp_message *resp, const char *text)
{
	char	buf[256];

	if (resp->body == NULL || resp->body_len == 0
		|| resp->body_len >= sizeof(buf))
		return (false);
	memcpy(buf, resp->body, resp->body_len);
	buf[resp->body_len] = '\0';
	return (strstr(buf, text) != NULL);
}

/**
 * @brief Puts wallet points on an account without playing for them.
 *
 * The suite runs the real server in-process, so the store is reachable
 * directly. This arranges the precondition a purchase test needs; the
 * purchase itself still goes over the wire like any client's would.
 *
 * @param fx Running fixture holding the store.
 * @param id Player to credit.
 * @param points Wallet points to add.
 */
static void	credit(t_fixture *fx, t_player_id id, int64_t points)
{
	assert(db_record_game(fx->srv->db, id, 0, points, false) == DB_OK);
}

/**
 * @brief Builds a BUY path for one catalogue item.
 *
 * @param out Buffer receiving the path.
 * @param cap Size of out.
 * @param kind "character" or "theme".
 * @param id The item id.
 */
static void	item_path(char *out, size_t cap, const char *kind, unsigned id)
{
	snprintf(out, cap, "%s%s/%u", TETRISD_ROUTE_STORE_PREFIX, kind, id);
}

/**
 * @brief Builds an EQUIP path for one catalogue item and one player.
 *
 * @param out Buffer receiving the path.
 * @param cap Size of out.
 * @param pid The player whose loadout is addressed.
 * @param kind "character" or "theme".
 * @param id The item id.
 */
static void	equip_path(char *out, size_t cap, t_player_id pid,
		const char *kind, unsigned id)
{
	snprintf(out, cap, "%s%llu/%s/%u", TETRISD_ROUTE_PLAYER_PREFIX,
		(unsigned long long)pid, kind, id);
}
