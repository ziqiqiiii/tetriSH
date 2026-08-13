/* ************************************************************************** */
/*                                                                            */
/*   bot_pool.c — the accounts a player's bots log in as                       */
/*                                                                            */
/*   A bot is an ordinary client: it signs in over the same port, completes    */
/*   the same handshake, and sends the same JOIN, READY and MOVE as a person.  */
/*   What it needs from the server is an account to be, and this is where      */
/*   those come from - BOT_01 upward, created at boot if they are not already  */
/*   in the store, never ranked, and never available to a person because       */
/*   db_signup refuses the prefix.                                            */
/*                                                                            */
/*   There is no claim table, and that is the point. A player holds at most    */
/*   one connection, which the registry already knows, so `is this account     */
/*   taken` is `registry_find_other`. A claim ends exactly when the connection */
/*   does - including when it dies - so there is nothing to release, nothing   */
/*   to leak, and no state that can disagree with what is actually connected.  */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisd.h"

/**
 * @brief Writes the name of the nth pool account.
 *
 * One-based and zero-padded to two digits, so the ordinary pool sorts the way
 * it reads. Beyond 99 the number simply grows; the names stay unique, which is
 * all the store asks of them.
 *
 * @param index Zero-based account number.
 * @param out Buffer receiving the name.
 * @param cap Size of out.
 */
void	bot_pool_name(int index, char *out, size_t cap)
{
	if (out == NULL || cap == 0)
		return ;
	snprintf(out, cap, "%s%02d", DB_RESERVED_PREFIX, index + 1);
}

/**
 * @brief Creates the pool's accounts, and leaves the ones already there alone.
 *
 * Run once at boot. DB_EXISTS is the ordinary answer on every start but the
 * first, and is not a failure: the accounts live in the store like anybody
 * else's and outlive the process that made them.
 *
 * A failure to create one is logged and not fatal. The pool is a convenience -
 * a server that cannot make bot accounts is a server without bots, not a
 * server that should refuse to start and take everybody's game with it.
 *
 * @param srv The server, for its store and its log.
 * @param count How many accounts to make.
 * @return How many the pool holds, which may be fewer than asked for.
 */
int	bot_pool_open(t_server *srv, int count)
{
	char		name[DB_MAX_USERNAME];
	char		hash[DB_HASH_LEN + 1];
	t_player_id	id;
	t_db_result	result;
	int			index;

	if (srv == NULL || count <= 0)
		return (0);
	index = 0;
	while (index < count)
	{
		bot_pool_name(index, name, sizeof(name));
		if (bot_pool_hash(name, hash, sizeof(hash)) != 0)
			break ;
		result = db_signup_reserved(srv->db, name, hash, TETRISD_BOT_SALT, &id);
		if (result != DB_OK && result != DB_EXISTS)
		{
			logger_emit(&srv->log, COREIPC_LOG_WARNING,
				"bot account %s unavailable", name);
			break ;
		}
		index++;
	}
	logger_emit(&srv->log, COREIPC_LOG_INFO, "bot pool holds %d accounts",
		index);
	return (index);
}

/**
 * @brief Hashes a pool account's password the way login will hash it.
 *
 * Every pool account shares one password, derived from its name so that the
 * accounts are at least not interchangeable, and salted with a fixed salt so
 * that the client can compute the same thing without asking the server for it.
 *
 * This is not a secret and is not pretending to be one: the bot binary has to
 * know it, so anybody holding the binary can log in as a pool account and sit
 * in a seat. On a LAN, for a course project, that is the right trade - the
 * alternative is a route that hands out credentials, which is a genuine
 * authentication surface built to protect accounts that own nothing, cannot be
 * ranked, and are handed out for free. It is not a property to rely on if this
 * were ever exposed.
 *
 * @param name The account's username.
 * @param out Buffer receiving DB_HASH_LEN hex characters plus a terminator.
 * @param cap Size of out.
 * @return 0 on success, -1 on failure.
 */
int	bot_pool_hash(const char *name, char *out, size_t cap)
{
	char	password[TETRISD_PASSWORD_MAX];

	if (name == NULL)
		return (-1);
	snprintf(password, sizeof(password), "%s%s", TETRISD_BOT_SECRET, name);
	return (password_hash(password, TETRISD_BOT_SALT, out, cap));
}
