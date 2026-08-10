#include "tetrisu.h"

/*
** Registering and signing in. Both are one request with a two-line body, and
** both end with the connection knowing which player it is - the id tetrisd
** answers with is what rides in Player-Id on everything afterwards.
**
** The password is written into the request body and nowhere else: it is not
** kept on the client after the request is built, because the only thing
** tetrisu ever needs again is the id.
*/

// Static Functions
static int	credentials_request(t_net_client *net, const char *method,
				const char *path, const char *username,
				const char *password, t_net_result *out);
static void	adopt_identity(t_net_client *net, const char *username,
				const t_net_result *result);

/**
 * @brief SIGNUP /account - register a player and bind this connection to it.
 *
 * @param net Connected client.
 * @param username Name to register.
 * @param password Password to register with.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_signup(t_net_client *net, const char *username,
		const char *password, t_net_result *out)
{
	return (credentials_request(net, "SIGNUP", TETRISU_ROUTE_ACCOUNT,
			username, password, out));
}

/**
 * @brief LOGIN /session - bind this connection to an existing player.
 *
 * @param net Connected client.
 * @param username Name to sign in as.
 * @param password Password to sign in with.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_login(t_net_client *net, const char *username,
		const char *password, t_net_result *out)
{
	return (credentials_request(net, "LOGIN", TETRISU_ROUTE_SESSION,
			username, password, out));
}

/**
 * @brief Sends one credentials request and adopts the identity it returns.
 *
 * @param net Connected client.
 * @param method SIGNUP or LOGIN.
 * @param path The route that method addresses.
 * @param username Name being registered or signed in as.
 * @param password Password to send.
 * @param out Receives the status and reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
static int	credentials_request(t_net_client *net, const char *method,
			const char *path, const char *username,
			const char *password, t_net_result *out)
{
	t_net_result	result;
	char			body[NET_USER_MAX + TETRISU_PASSWORD_MAX + 32];

	if (net == NULL || username == NULL || password == NULL)
		return (-1);
	if (snprintf(body, sizeof(body), "username %s\npassword %s\n",
			username, password) >= (int)sizeof(body))
		return (-1);
	memset(&result, 0, sizeof(result));
	if (net_request(net, method, path, body, &result) != 0)
	{
		memset(body, 0, sizeof(body));
		return (-1);
	}
	memset(body, 0, sizeof(body));
	adopt_identity(net, username, &result);
	if (out != NULL)
		*out = result;
	return (0);
}

/**
 * @brief Records who this connection now is, when the server accepted it.
 *
 * The player id is not read out of the response body: tetrisd puts it in the
 * Player-Id header of every authenticated answer, and net_request has already
 * seen it. Taking it from one place keeps the client from having two ideas of
 * who it is.
 *
 * That same id is what decides NET_AUTHED, rather than the status code.
 * SIGNUP answers 201 and creates an account, but it binds nothing: identity
 * belongs to the connection and only LOGIN claims it, so a signup
 * comes back with no Player-Id at all. Reading 201 as "signed in" left the
 * client sure it was authenticated on a socket tetrisd still considered
 * anonymous, and every route it then called answered 401.
 *
 * @param net Client to bind.
 * @param username Name that was accepted.
 * @param result The server's answer.
 */
static void	adopt_identity(t_net_client *net, const char *username,
			const t_net_result *result)
{
	if (result->status != 200 && result->status != 201)
		return ;
	snprintf(net->username, sizeof(net->username), "%s", username);
	if (net->player_id != 0 && net->state < NET_AUTHED)
		net->state = NET_AUTHED;
}
