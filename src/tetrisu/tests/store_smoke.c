/* ************************************************************************** */
/*                                                                            */
/*   store_smoke.c - the Marketplace and Settings, against a live tetrisd     */
/*                                                                            */
/*   Not a unit test: this needs a server, so tests/integration/              */
/*   test_net_store.sh starts one and runs this against it.                   */
/*                                                                            */
/*   What it guards is the claim that moved these two screens off the client. */
/*   The Marketplace used to debit a wallet it kept in its own view model and  */
/*   forgot when the screen closed, and Settings equipped a character by       */
/*   setting a flag nobody else could see. Prices, balances, ownership and     */
/*   the loadout are all tetrisd's now, and the checks below are the ones      */
/*   that can only pass if that is true: a price the client never wrote down,  */
/*   a refusal it could not have issued, and an equip that is still there on   */
/*   a connection that has not seen it happen.                                 */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

// Static Functions
static int	check_catalogue_comes_from_the_server(t_app_data_provider *provider,
				t_app_net_session *session);
static int	check_a_fresh_account_owns_only_starters(
				t_app_data_provider *provider, t_app_net_session *session);
static int	check_a_free_theme_can_be_bought(t_app_data_provider *provider,
				t_app_net_session *session);
static int	check_an_unaffordable_buy_is_refused(t_app_data_provider *provider,
				t_app_net_session *session);
static int	check_an_equip_outlives_the_screen(t_app_data_provider *provider,
				t_app_net_session *session);
static int	check_equipping_what_is_not_owned_is_refused(
				t_app_data_provider *provider, t_app_net_session *session);
static int	check_the_equipped_theme_picks_the_artwork(
				t_app_data_provider *provider, t_app_net_session *session);
static int	sign_up_and_in(t_app_data_provider *provider,
				t_app_net_session *session, const char *name);
static const t_app_catalogue_item_view_model	*find_item(
				const t_app_catalogue_view_model *catalogue, uint32_t item_id);
static void	report(const char *name, int ok, int *failures);

/**
 * @brief Entry point - connect, register, then drive the store end to end.
 *
 * @return 0 when every check passed, 1 otherwise.
 */
int	main(void)
{
	t_net_config		cfg;
	t_app_net_session	session;
	t_app_data_provider	provider;
	char				name[NET_USER_MAX];
	int					failures;

	net_config_load(&cfg);
	memset(&session, 0, sizeof(session));
	if (net_connect(&session.net, &cfg) != 0)
	{
		printf("FAIL: no tetrisd at %s:%d (%s)\n", cfg.host, cfg.port,
			session.net.error);
		return (1);
	}
	session.connected = true;
	app_net_provider_init(&provider, &session);
	snprintf(name, sizeof(name), "shop%d", (int)getpid());
	failures = 0;
	if (!sign_up_and_in(&provider, &session, name))
	{
		printf("FAIL: could not register %s\n", name);
		return (1);
	}
	report("the catalogue is the server's, prices included",
		check_catalogue_comes_from_the_server(&provider, &session), &failures);
	report("a fresh account owns only its starters",
		check_a_fresh_account_owns_only_starters(&provider, &session),
		&failures);
	report("a free theme can be bought with an empty wallet",
		check_a_free_theme_can_be_bought(&provider, &session), &failures);
	report("an unaffordable purchase is refused and costs nothing",
		check_an_unaffordable_buy_is_refused(&provider, &session), &failures);
	report("an equip outlives the screen that made it",
		check_an_equip_outlives_the_screen(&provider, &session), &failures);
	report("equipping what is not owned is refused",
		check_equipping_what_is_not_owned_is_refused(&provider, &session),
		&failures);
	report("the equipped theme picks the artwork",
		check_the_equipped_theme_picks_the_artwork(&provider, &session),
		&failures);
	net_disconnect(&session.net);
	return (failures != 0);
}

/**
 * @brief The store front is read, not held locally.
 *
 * The prices are the assertion that matters. They live in the server's config
 * files and were never written down on this side, so a client that had kept
 * its own catalogue would disagree with these numbers - as the old fixture
 * did, offering Wolf-man at 1800 against a real price of 10.
 *
 * The theme ids are the second: 5 was cut, so a client that treated an id as
 * a position would mislabel every theme after it.
 *
 * @param provider Provider bound to a signed-in session.
 * @param session The signed-in session.
 * @return 1 when the catalogue matched the server's, 0 otherwise.
 */
static int	check_catalogue_comes_from_the_server(t_app_data_provider *provider,
			t_app_net_session *session)
{
	t_app_settings_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->load_settings(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (view.characters.count != 4 || view.themes.count != 7)
		return (0);
	if (find_item(&view.characters, 1) == NULL
		|| find_item(&view.characters, 1)->price != 10)
		return (0);
	if (strcmp(find_item(&view.characters, 1)->name, "Halloween") != 0)
		return (0);
	if (find_item(&view.themes, 3) == NULL
		|| find_item(&view.themes, 3)->price != 5)
		return (0);
	/* The gap, and the row after it. */
	if (find_item(&view.themes, 5) != NULL)
		return (0);
	if (find_item(&view.themes, 6) == NULL
		|| find_item(&view.themes, 6)->price != 7)
		return (0);
	/* Ability copy is local, attached by id - the server never sends it. */
	if (strcmp(find_item(&view.characters, 1)->abilities[0].name, "Fry") != 0)
		return (0);
	if (strcmp(find_item(&view.characters, 4)->abilities[3].name,
			"Thwack") != 0)
		return (0);
	return (1);
}

/**
 * @brief Signup grants item 1 of each kind and equips both (UC-01).
 *
 * @param provider Provider bound to a signed-in session.
 * @param session The signed-in session.
 * @return 1 when only the starters are owned, 0 otherwise.
 */
static int	check_a_fresh_account_owns_only_starters(
			t_app_data_provider *provider, t_app_net_session *session)
{
	t_app_settings_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->load_settings(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (view.profile.wallet_points != 0 || view.profile.rank < 1)
		return (0);
	if (!find_item(&view.characters, 1)->owned
		|| !find_item(&view.characters, 1)->equipped)
		return (0);
	if (find_item(&view.characters, 2)->owned
		|| find_item(&view.characters, 4)->owned)
		return (0);
	if (!find_item(&view.themes, 1)->owned
		|| !find_item(&view.themes, 1)->equipped)
		return (0);
	if (find_item(&view.themes, 3)->owned)
		return (0);
	return (1);
}

/**
 * @brief Theme 2 is free, so it is the one purchase a new account can make.
 *
 * Buying is not equipping: the tile becomes owned and the loadout does not
 * move, because those are two decisions.
 *
 * @param provider Provider bound to a signed-in session.
 * @param session The signed-in session.
 * @return 1 when the purchase landed, 0 otherwise.
 */
static int	check_a_free_theme_can_be_bought(t_app_data_provider *provider,
			t_app_net_session *session)
{
	t_app_settings_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->buy_item(session, APP_CATALOGUE_THEMES, 2, &view)
		!= APP_PROVIDER_OK)
		return (0);
	if (!find_item(&view.themes, 2)->owned)
		return (0);
	if (find_item(&view.themes, 2)->equipped)
		return (0);
	if (!find_item(&view.themes, 1)->equipped)
		return (0);
	if (view.profile.wallet_points != 0)
		return (0);
	return (1);
}

/**
 * @brief A character costs 10 and the wallet holds nothing.
 *
 * The refusal is the server's - db_buy_* checks and deducts under one lock -
 * and the wallet afterwards proves it did not take the money on the way out.
 *
 * @param provider Provider bound to a signed-in session.
 * @param session The signed-in session.
 * @return 1 when the purchase was refused and nothing changed, 0 otherwise.
 */
static int	check_an_unaffordable_buy_is_refused(t_app_data_provider *provider,
			t_app_net_session *session)
{
	t_app_settings_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->buy_item(session, APP_CATALOGUE_CHARACTERS, 2, &view)
		== APP_PROVIDER_OK)
		return (0);
	memset(&view, 0, sizeof(view));
	if (provider->load_settings(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (find_item(&view.characters, 2)->owned)
		return (0);
	if (view.profile.wallet_points != 0)
		return (0);
	return (1);
}

/**
 * @brief The point of the whole change: an equip is persisted, not remembered.
 *
 * Equip theme 2, then load the screen again. The reload goes back to tetrisd,
 * so what comes back is what the account holds rather than what this process
 * last drew - which is exactly what the old in-model equip could not do.
 *
 * @param provider Provider bound to a signed-in session.
 * @param session The signed-in session.
 * @return 1 when the equip survived a reload, 0 otherwise.
 */
static int	check_an_equip_outlives_the_screen(t_app_data_provider *provider,
			t_app_net_session *session)
{
	t_app_settings_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->equip_item(session, APP_CATALOGUE_THEMES, 2, &view)
		!= APP_PROVIDER_OK)
		return (0);
	if (!find_item(&view.themes, 2)->equipped
		|| find_item(&view.themes, 1)->equipped)
		return (0);
	memset(&view, 0, sizeof(view));
	if (provider->load_settings(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (!find_item(&view.themes, 2)->equipped)
		return (0);
	/* Equipping a theme must not have moved the character (a real bug once:
	** db_equip_theme wrote current_equipped_character). */
	if (!find_item(&view.characters, 1)->equipped)
		return (0);
	return (1);
}

/**
 * @brief The rule the whole feature rests on.
 *
 * tetrisd reads the equipped character to decide which Gaiden abilities a
 * player has, so equipping one that was never bought would be a client
 * granting itself powers.
 *
 * @param provider Provider bound to a signed-in session.
 * @param session The signed-in session.
 * @return 1 when the equip was refused and the loadout held, 0 otherwise.
 */
static int	check_equipping_what_is_not_owned_is_refused(
			t_app_data_provider *provider, t_app_net_session *session)
{
	t_app_settings_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->equip_item(session, APP_CATALOGUE_CHARACTERS, 4, &view)
		== APP_PROVIDER_OK)
		return (0);
	memset(&view, 0, sizeof(view));
	if (provider->load_settings(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (find_item(&view.characters, 4)->equipped)
		return (0);
	if (!find_item(&view.characters, 1)->equipped)
		return (0);
	return (1);
}

/**
 * @brief The equipped theme decides which artwork the screens load.
 *
 * Theme 2 is equipped by now, so the portrait the profile carries has to come
 * out of that theme's directory rather than the default one. This is the step
 * that only works because the artwork is keyed by catalogue id: matching on
 * the display name would have to know that the store spells this theme
 * "Design and AI" while the asset directory is design_ai_university_theme.
 *
 * Every shelf tile is checked too, not just the profile's one portrait. The
 * tiles are what the Settings and Marketplace panels actually draw, and they
 * were shipped empty: the row carried an id, a name and a price and no path,
 * so both screens rendered characters with no art at all while this check -
 * looking only at the profile - passed.
 *
 * @param provider Provider bound to a signed-in session.
 * @param session The signed-in session.
 * @return 1 when the portrait came from the equipped theme, 0 otherwise.
 */
static int	check_the_equipped_theme_picks_the_artwork(
			t_app_data_provider *provider, t_app_net_session *session)
{
	t_app_settings_view_model	view;
	int							index;

	memset(&view, 0, sizeof(view));
	if (provider->load_settings(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (!find_item(&view.themes, 2)->equipped)
		return (0);
	if (strstr(view.profile.portrait_asset,
			"design_ai_university_theme") == NULL)
		return (0);
	if (strcmp(view.profile.theme, "Design and AI") != 0)
		return (0);
	if (strcmp(view.profile.character, "Halloween") != 0)
		return (0);
	index = 0;
	while (index < view.characters.count)
	{
		if (strstr(view.characters.items[index].portrait_asset,
				"design_ai_university_theme") == NULL)
			return (0);
		index++;
	}
	index = 0;
	while (index < view.themes.count)
	{
		if (strstr(view.themes.items[index].portrait_asset,
				"settings_previews/") == NULL)
			return (0);
		index++;
	}
	return (1);
}

/**
 * @brief Registers a throwaway account and signs in as it.
 *
 * @param provider Provider bound to the session.
 * @param session Connected session.
 * @param name Username to register.
 * @return 1 on success, 0 otherwise.
 */
static int	sign_up_and_in(t_app_data_provider *provider,
			t_app_net_session *session, const char *name)
{
	t_app_auth_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->sign_up(session, name, "hunter2", "127.0.0.1", &view)
		!= APP_PROVIDER_OK)
		return (0);
	if (provider->login(session, name, "hunter2", "127.0.0.1", &view)
		!= APP_PROVIDER_OK)
		return (0);
	return (1);
}

/**
 * @brief Finds one shelf tile by the catalogue id tetrisd sells it under.
 *
 * A scan, because ids carry gaps and are therefore not positions.
 *
 * @param catalogue The panel to search.
 * @param item_id The catalogue id to look for.
 * @return The tile, or NULL when the panel does not carry it.
 */
static const t_app_catalogue_item_view_model	*find_item(
			const t_app_catalogue_view_model *catalogue, uint32_t item_id)
{
	int	index;

	index = 0;
	while (index < catalogue->count && index < APP_CATALOGUE_MAX_ITEMS)
	{
		if (catalogue->items[index].item_id == item_id)
			return (&catalogue->items[index]);
		index++;
	}
	return (NULL);
}

/**
 * @brief Prints one check in the integration runner's protocol.
 *
 * @param name What was checked.
 * @param ok Non-zero when it passed.
 * @param failures Running failure count, incremented on a failure.
 */
static void	report(const char *name, int ok, int *failures)
{
	if (ok)
		printf("PASS: %s\n", name);
	else
	{
		printf("FAIL: %s\n", name);
		(*failures)++;
	}
}
