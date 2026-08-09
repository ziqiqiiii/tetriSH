#include "tetrisu.h"

/*
** Reading and changing the account behind the Settings and Marketplace
** screens: the profile, the store front, and the two writes that spend a
** wallet and choose a loadout.
**
** Nothing here decides anything. The price is the catalogue's, the balance is
** the store's, and whether a purchase or an equip is allowed is tetrisd's
** answer - this file only asks and decodes. That matters more here than
** anywhere else in the client, because the equipped character is what the
** server reads to decide which Gaiden abilities a player has, so a client
** that could assert its own loadout could grant itself another character's
** powers.
*/

// Static Functions
static int	write_request(t_net_client *net, const char *method,
				const char *path, t_body_profile *out, int *status);
static void	item_route(char *out, size_t cap, bool character,
				uint32_t item_id);
static void	equip_route(char *out, size_t cap, uint64_t player_id,
				bool character, uint32_t item_id);

/**
 * @brief PROFILE /player/<pid> - wallet, score, rank, inventory, loadout.
 *
 * @param net Signed-in client.
 * @param out Receives the decoded profile.
 * @return 0 on success, -1 on a transport failure, a refusal, or a body that
 *         will not decode.
 */
int	net_profile(t_net_client *net, t_body_profile *out)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	if (net == NULL || out == NULL || net->state < NET_AUTHED)
		return (-1);
	snprintf(path, sizeof(path), "%s%llu", TETRISU_ROUTE_PLAYER,
		(unsigned long long)net->player_id);
	memset(&result, 0, sizeof(result));
	if (net_request(net, "PROFILE", path, NULL, &result) != 0
		|| result.status != 200)
		return (-1);
	return (body_profile_decode(result.body, strlen(result.body), out));
}

/**
 * @brief LIST /store - every character and theme, with the prices tetrisd
 * charges.
 *
 * The client keeps no copy of this. Prices live in the store's config files,
 * and a client that guessed them would offer a purchase the server then
 * refuses at a different number.
 *
 * @param net Signed-in client.
 * @param out Receives the decoded catalogue.
 * @return 0 on success, -1 on a transport failure, a refusal, or a body that
 *         will not decode.
 */
int	net_catalogue(t_net_client *net, t_body_catalogue *out)
{
	t_net_result	result;

	if (net == NULL || out == NULL || net->state < NET_AUTHED)
		return (-1);
	memset(&result, 0, sizeof(result));
	if (net_request(net, "LIST", TETRISU_ROUTE_STORE, NULL, &result) != 0
		|| result.status != 200)
		return (-1);
	return (body_catalogue_decode(result.body, strlen(result.body), out));
}

/**
 * @brief BUY /store/character/<id> or /store/theme/<id> - spend the wallet.
 *
 * @param net Signed-in client.
 * @param character true to buy a character, false for a theme.
 * @param item_id The catalogue id to buy.
 * @param out Receives the profile as it stands afterwards, on 200 only.
 * @param status Receives the status tetrisd answered with; may be NULL.
 * @return 0 when the purchase was accepted, -1 otherwise.
 */
int	net_buy(t_net_client *net, bool character, uint32_t item_id,
		t_body_profile *out, int *status)
{
	char	path[NET_PATH_MAX];

	if (net == NULL || out == NULL)
		return (-1);
	item_route(path, sizeof(path), character, item_id);
	return (write_request(net, "BUY", path, out, status));
}

/**
 * @brief EQUIP /player/<pid>/character/<id> or /theme/<id> - set the loadout.
 *
 * @param net Signed-in client.
 * @param character true to equip a character, false for a theme.
 * @param item_id The catalogue id to equip.
 * @param out Receives the profile as it stands afterwards, on 200 only.
 * @param status Receives the status tetrisd answered with; may be NULL.
 * @return 0 when the equip was accepted, -1 otherwise.
 */
int	net_equip(t_net_client *net, bool character, uint32_t item_id,
		t_body_profile *out, int *status)
{
	char	path[NET_PATH_MAX];

	if (net == NULL || out == NULL)
		return (-1);
	equip_route(path, sizeof(path), net->player_id, character, item_id);
	return (write_request(net, "EQUIP", path, out, status));
}

/**
 * @brief Sends one store write and decodes the profile it answered with.
 *
 * The status is reported separately from the return value because a refusal
 * is an answer the screen has copy for - "you cannot afford that" reads
 * differently from "that is not for sale" - while both are a failure to the
 * caller waiting on a new profile.
 *
 * @param net Signed-in client.
 * @param method BUY or EQUIP.
 * @param path The route that method addresses.
 * @param out Receives the profile on 200.
 * @param status Receives the status; may be NULL.
 * @return 0 when the write was accepted, -1 otherwise.
 */
static int	write_request(t_net_client *net, const char *method,
		const char *path, t_body_profile *out, int *status)
{
	t_net_result	result;

	if (status != NULL)
		*status = 0;
	if (net->state < NET_AUTHED)
		return (-1);
	memset(&result, 0, sizeof(result));
	if (net_request(net, method, path, NULL, &result) != 0)
		return (-1);
	if (status != NULL)
		*status = result.status;
	if (result.status != 200)
		return (-1);
	return (body_profile_decode(result.body, strlen(result.body), out));
}

/**
 * @brief Builds a BUY route for one catalogue item.
 *
 * @param out Buffer receiving the route.
 * @param cap Size of out.
 * @param character true for the character catalogue, false for themes.
 * @param item_id The catalogue id.
 */
static void	item_route(char *out, size_t cap, bool character,
		uint32_t item_id)
{
	snprintf(out, cap, "%s/%s/%" PRIu32, TETRISU_ROUTE_STORE,
		character ? "character" : "theme", item_id);
}

/**
 * @brief Builds an EQUIP route for one catalogue item and one player.
 *
 * The subject rides in the path the way it does on every gameplay route, so
 * the request says whose loadout it is changing rather than leaving tetrisd
 * to assume it means this connection's.
 *
 * @param out Buffer receiving the route.
 * @param cap Size of out.
 * @param player_id The player whose loadout is addressed.
 * @param character true for the character catalogue, false for themes.
 * @param item_id The catalogue id.
 */
static void	equip_route(char *out, size_t cap, uint64_t player_id,
		bool character, uint32_t item_id)
{
	snprintf(out, cap, "%s%llu/%s/%" PRIu32, TETRISU_ROUTE_PLAYER,
		(unsigned long long)player_id,
		character ? "character" : "theme", item_id);
}
